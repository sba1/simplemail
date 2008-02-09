/***************************************************************************
 SimpleMail - Copyright (C) 2000 Hynek Schlawack and Sebastian Bauer

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
***************************************************************************/

/*
** datatypescache.c
*/

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <datatypes/pictureclass.h>

#include <clib/alib_protos.h>

#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/datatypes.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "lists.h"
#include "debug.h"
#include "support_indep.h"
#include "configuration.h"

#include "amigasupport.h"
#include "datatypescache.h"

#ifdef __AMIGAOS4__
#ifndef WritePixelArrayAlpha
#define WritePixelArrayAlpha(srcRect, SrcX, SrcY, SrcMod, RastPort, DestX, DestY, SizeX, SizeY, globalAlpha) ICyberGfx->WritePixelArrayAlpha(srcRect, SrcX, SrcY, SrcMod, RastPort, DestX, DestY, SizeX, SizeY, globalAlpha)
#endif
#elif defined(__MORPHOS__)
#ifndef WritePixelArrayAlpha
#define WritePixelArrayAlpha(__p0, __p1, __p2, __p3, __p4, __p5, __p6, __p7, __p8, __p9) \
    LP10(216, ULONG , WritePixelArrayAlpha, \
        APTR , __p0, a0, \
        UWORD , __p1, d0, \
        UWORD , __p2, d1, \
        UWORD , __p3, d2, \
        struct RastPort *, __p4, a1, \
        UWORD , __p5, d3, \
        UWORD , __p6, d4, \
        UWORD , __p7, d5, \
        UWORD , __p8, d6, \
        ULONG , __p9, d7, \
        , CYBERGRAPHICS_BASE_NAME, 0, 0, 0, 0, 0, 0)
#endif
#endif

/**************************************************************/

/* An icon description */
struct icon_desc
{
	struct node node;
	char *filename;
	char *masonname;

	/* Coordinates within the big image */
	int x1,x2,y1,y2;
};

struct dt_node
{
	struct node node;
	char *name;
	struct icon_desc *desc;
	int count;

	void *argb;

	Object *o;
	int depth;
	int trans;

	int x1,x2,y1,y2;

	struct Screen *scr;
};

/**************************************************************/

static void dt_put_rect_on_argb(struct dt_node *node, int srcx, int srcy, int src_width, int src_height, void *dest, int dest_width, int x, int y);

/**************************************************************/

static struct dt_node *img;

static int mason_available;

static struct list desc_list;
static struct list dt_list;

/**************************************************************/

#ifndef __AMIGAOS4__
APTR MySetProcWindow(void *newvalue)
{
	struct Process *myproc;
	APTR oldwindowptr;

	myproc = (struct Process *)FindTask(NULL);
	oldwindowptr = myproc->pr_WindowPtr;
	myproc->pr_WindowPtr = (APTR)newvalue;

	return oldwindowptr;
}
#else
#define MySetProcWindow SetProcWindow
#endif

/*************************************************************
 Loads the picture and maps it to the given screen. Returns
 a datatypes object which needs to be freed with
 DisposeDTObject(). Note the picture cannot be remapped
 to another screen.
**************************************************************/
Object *LoadAndMapPicture(char *filename, struct Screen *scr)
{
	Object *o;
	APTR oldwindowptr;

	/* tell DOS not to bother us with requesters */
	oldwindowptr = MySetProcWindow((APTR)-1);

	o = NewDTObject(filename,
			DTA_GroupID          , GID_PICTURE,
			OBP_Precision        , PRECISION_EXACT,
			PDTA_Screen          , scr,
			PDTA_FreeSourceBitMap, TRUE,
			PDTA_DestMode        , PMODE_V43,
			PDTA_UseFriendBitMap , TRUE,
			TAG_DONE);
	
	MySetProcWindow(oldwindowptr);
	
	/* do all the setup/layout stuff that's necessary to get a bitmap from the dto    */
	/* note that when using V43 datatypes, this might not be a real "struct BitMap *" */
	
	if (o)
	{
		struct FrameInfo fri = {0};
		DoMethod(o,DTM_FRAMEBOX,NULL,(ULONG)&fri,(ULONG)&fri,sizeof(struct FrameInfo),0);
	
		if (fri.fri_Dimensions.Depth>0)
		{
			if (DoMethod(o,DTM_PROCLAYOUT,NULL,1))
			{
				return o;
			}
		}
		DisposeDTObject(o);
	}
	return NULL;
}

/*************************************************************
 Loads the picture. Returns a datatypes object which needs to
 be freed with DisposeDTObject().
**************************************************************/
static Object *LoadPicture(char *filename)
{
	Object *o;
	APTR oldwindowptr;

	/* tell DOS not to bother us with requesters */
	oldwindowptr = MySetProcWindow((APTR)-1);

	o = NewDTObject(filename,
			DTA_GroupID          , GID_PICTURE,
			OBP_Precision        , PRECISION_EXACT,
			PDTA_FreeSourceBitMap, FALSE,
			PDTA_DestMode        , PMODE_V43,
			PDTA_UseFriendBitMap , TRUE,
			TAG_DONE);
	
	MySetProcWindow(oldwindowptr);

	return o;
}

/*************************************************************
 Maps the given object to the screen.
**************************************************************/
static int MapPicture(Object *obj, struct Screen *scr)
{
	struct FrameInfo fri = {0};
	
	SetAttrs(obj, PDTA_Screen, scr, TAG_DONE);
	
	DoMethod(obj,DTM_FRAMEBOX,NULL,(ULONG)&fri,(ULONG)&fri,sizeof(struct FrameInfo),0);

	if (fri.fri_Dimensions.Depth>0)
	{
		if (DoMethod(obj,DTM_PROCLAYOUT,NULL,1))
		{
			return 1;
		}
	}
	return 0;
}

/**************************************************************/

/*************************************************************
 Initialized the cache.
**************************************************************/
void dt_init(void)
{
	BPTR file;
	BPTR lock;
	APTR oldwindowptr;

	list_init(&desc_list);
	list_init(&dt_list);

	mason_available = 0;
	if (!user.config.dont_use_aiss)
	{
		/* Test for general mason availability */
		oldwindowptr = MySetProcWindow((APTR)-1);
		if ((lock = Lock("TBIMAGES:",ACCESS_READ)))
		{
			mason_available = 1;
			UnLock(lock);
		}
		MySetProcWindow(oldwindowptr);
	}

	if ((file = Open("PROGDIR:Images/images.list",MODE_OLDFILE)))
	{
		char buf[256];
		while((FGets(file,buf,sizeof(buf))))
		{
			char *filename_end;
			char *filename;

			if (*buf==';') continue;

			/* trim ending white spaces */
			{
				int buf_len;
				
				buf_len = strlen(buf);
				while (buf_len && isspace((unsigned char)buf[buf_len-1]))
					buf_len--;

				buf[buf_len] = 0;
			}

			if ((filename_end = strchr(buf,',')))
			{
				if ((filename = malloc(filename_end-buf+50)))
				{
					struct icon_desc *node = malloc(sizeof(struct icon_desc));
					if (node)
					{
						char *lastchar;
						memset(node,0,sizeof(struct icon_desc));
						node->filename = filename;
						strcpy(filename,"PROGDIR:Images/"); /* 15 chars */
						strncpy(&filename[15],buf,filename_end-buf);
						filename[15+filename_end-buf]=0;
						node->x1 = strtol(filename_end+1,&lastchar,10);
						node->y1 = strtol(lastchar+1,&lastchar,10);
						node->x2 = strtol(lastchar+1,&lastchar,10);
						node->y2 = strtol(lastchar+1,&lastchar,10);

						/* Optional mason name */
						if (*lastchar == ',')
						{
							char *masonname = malloc(strlen(lastchar) + 12);
							if (masonname)
							{
								strcpy(masonname,"TBIMAGES:");
								strcat(masonname,lastchar+1);
								node->masonname = masonname;
							}
						}
						list_insert_tail(&desc_list,&node->node);
					}
				}
			}
			
		}
		Close(file);
	}
}

/*************************************************************
 Free all ressources associated with the cache.
**************************************************************/
void dt_cleanup(void)
{
	struct dt_node *dt;
	struct icon_desc *icon;
	
	while ((dt = (struct dt_node*)list_remove_tail(&dt_list)))
	{
		if (dt->o) DisposeDTObject(dt->o);
		if (dt->name) free(dt->name);
		free(dt);
	}

	while ((icon = (struct icon_desc*)list_remove_tail(&desc_list)))
	{
		if (icon->filename) free(icon->filename);
		if (icon->masonname) free(icon->masonname);
		free(icon);
	}

	if (img)
	{
		dt_dispose_picture(img);
		img = NULL;
	}
}

/****************************************************************
 Create the dt object directly from filename
****************************************************************/
static struct dt_node *dt_create_from_filename(char *filename)
{
	struct dt_node *node;

	if ((node = (struct dt_node*)malloc(sizeof(struct dt_node))))
	{
		/* Clear the data */
		memset(node,0,sizeof(struct dt_node));

		/* create the datatypes object */
		if ((node->o = LoadPicture(filename)))
		{
			struct BitMapHeader *bmhd = NULL;
			GetDTAttrs(node->o,PDTA_BitMapHeader,&bmhd,TAG_DONE);

			if (bmhd)
			{
				int w = bmhd->bmh_Width;
				int h = bmhd->bmh_Height;

				node->x2 = w - 1;
				node->y2 = h - 1;
				node->depth = bmhd->bmh_Depth;
				node->trans = bmhd->bmh_Transparent;
			}
			return node;
		}
		free(node);
	}
	return NULL;
}

/****************************************************************
 Load the dt object. A given filename is instanciated only
 once. 
****************************************************************/
struct dt_node *dt_load_unmapped_picture(char *filename)
{
	struct icon_desc *icon; 
	struct dt_node *node;

	SM_ENTER;

	/* Already loaded? */
	node = (struct dt_node*)list_first(&dt_list);
	while (node)
	{
		if (!mystricmp(filename,node->name))
		{
			node->count++;
			SM_RETURN(node,"%p");
		}
		node = (struct dt_node*)node_next(&node->node);
	}

	/* Try if we can match an icon description first */
	icon = (struct icon_desc*)list_first(&desc_list);
	while (icon)
	{
		if (!mystricmp(filename,icon->filename))
		{
			/* We can, now check if we can find the mason icon */
			if (mason_available && icon->masonname)
			{
				SM_DEBUGF(15,("Trying to load mason icon \"%s\"\n",icon->masonname));
				if ((node = dt_create_from_filename(icon->masonname)))
				{
					node->count = 1;
					node->desc = icon;
					node->name = mystrdup(filename);
					list_insert_tail(&dt_list,&node->node);
					SM_DEBUGF(15,("Mason found\n"));
					SM_RETURN(node,"%p");
				}
				SM_DEBUGF(15,("Mason not found\n"));
			}

			/* No mason, use the big image */
			if (!img)
				img = dt_create_from_filename("PROGDIR:Images/images");
			if (!img)
				break;
			img->count++;
			
			SM_DEBUGF(15,("Increased usage count of strip image to %ld\n",img->count));

			if ((node = (struct dt_node*)malloc(sizeof(struct dt_node))))
			{
				memset(node,0,sizeof(struct dt_node));
				node->count = 1;
				node->desc = icon;
				node->name = mystrdup(filename);
				node->x1 = node->desc->x1;
				node->x2 = node->desc->x2;
				node->y1 = node->desc->y1;
				node->y2 = node->desc->y2;
				list_insert_tail(&dt_list,&node->node);

				SM_RETURN(node,"%p");
			}
		}
		icon = (struct icon_desc*)node_next(&icon->node);
	}


	/* Try loading the specified image as it is */
	if ((node = dt_create_from_filename(filename)))
	{
		node->count = 1;
		node->name = mystrdup(filename);
		list_insert_tail(&dt_list,&node->node);
	}

	SM_RETURN(node,"%p");
}

/****************************************************************
 Load the dt object. A given filename is instanciated only
 once. 
****************************************************************/
struct dt_node *dt_load_picture(char *filename, struct Screen *scr)
{
	struct dt_node *node;

	SM_ENTER;

	node = dt_load_unmapped_picture(filename);

	if (!node->o)
	{
		if (img->scr != scr)
		{
			if (img->scr)
			{
				/* This has debugging purposes only, it should never happen */
				SM_DEBUGF(1,("The given screen differs from the screen for which the image was initially loaded for!\n"));
			}

			if (!(MapPicture(img->o, scr)))
				SM_DEBUGF(5,("Could not map the strip image!\n"));
			img->scr = scr;

#if 0
			if (img->argb)
			{
//				if (img->trans != mskHasAlpha)
				{
					APTR mask = NULL;
					struct BitMapHeader *bmhd = NULL;

					GetDTAttrs(img->o,PDTA_MaskPlane,&mask,TAG_DONE);
					GetDTAttrs(img->o,PDTA_BitMapHeader,&bmhd,TAG_DONE);

					SM_DEBUGF(1,("Mask %lx   Trans = %ld\n",mask,bmhd->bmh_Transparent));

					
					if (mask)
					{
						int i,j;
						ULONG *argb = (ULONG*)img->argb;
						int w = dt_width(img);
						int h = dt_height(img);

						for (j=0;j<h;j++)
						{
							for (i=0;i<w;i++)
							{
								argb[j*w+i] |= 0xff000000;
							}
						}
					}
				}

			}
#endif
		}
	} else
	{
		if (node->scr != scr)
		{
			if (!(MapPicture(node->o, scr)))
				SM_DEBUGF(5,("Could not map the image \"%s\"!\n",node->name));
			node->scr = scr;
		}
	}

	SM_RETURN(node,"%p");
}

/****************************************************************
 Dispose the given dt
****************************************************************/
void dt_dispose_picture(struct dt_node *node)
{
	SM_ENTER;

	if (node && node->count)
	{
		node->count--;
		if (!node->count)
		{
			if (node->o)
			{
				SM_DEBUGF(15,("Disposing dt object at %p\n",node->o));
				DisposeDTObject(node->o);
				if (node->argb) FreeVec(node->argb);
			} else
			{
				img->count--;
				SM_DEBUGF(15,("Reduced usage count of strip image to %ld\n",img->count));
				if (!img->count)
				{
					DisposeDTObject(img->o);
					if (img->argb) FreeVec(img->argb);
					free(img);
					img = NULL;
				}
			}	

			node_remove(&node->node);
			free(node);
		}
	}

	SM_LEAVE;
}

int dt_width(struct dt_node *node)
{
	return node->x2 - node->x1 + 1;
}

int dt_height(struct dt_node *node)
{
	return node->y2 - node->y1 + 1;
}

/***************************************************************
 Puts the dt node onto the given rastport. 
****************************************************************/
void dt_put_on_rastport(struct dt_node *node, struct RastPort *rp, int x, int y)
{
	struct BitMap *bitmap = NULL;
	Object *o;

	o = node->o;
	if (!o) o = img->o;
	if (!o) return;

	GetDTAttrs(o,PDTA_DestBitMap,&bitmap,TAG_DONE);
	if (!bitmap) GetDTAttrs(o,PDTA_BitMap,&bitmap,TAG_DONE);
	if (bitmap)
	{
		APTR mask = NULL;

		GetDTAttrs(o,PDTA_MaskPlane,&mask,TAG_DONE);
#ifndef __AROS__ /*don't know how to port this*/

		if (mask)
		{
			MyBltMaskBitMapRastPort(bitmap,node->x1,node->y1,rp,x,y,dt_width(node),dt_height(node),0xe2,(PLANEPTR)mask);
		} else
		{
#ifndef __SASC
			if (node->argb)
			{
				WritePixelArrayAlpha(node->argb,0,0,dt_width(node)*4,rp,x,y,dt_width(node),dt_height(node),0xffffffff);
			} else
#endif 
			{
				BltBitMapRastPort(bitmap,node->x1,node->y1,rp,x,y,dt_width(node),dt_height(node),0xc0);
			}
		}
#else /*AROS BLT*/
		BltBitMapRastPort(bitmap,node->x1,node->y1,rp,x,y,dt_width(node),dt_height(node),0xc0);
#endif
	}
}

/***************************************************************
 Returns the ARGB buffer solely of this image. You can use
 dt_width() and dt_height() to query the dimension.
****************************************************************/
void *dt_argb(struct dt_node *node)
{
	int w,h;

	SM_ENTER;
	SM_DEBUGF(15,("ARGB of Image \"%s\"\n",node->name));

	if (node->argb)
		SM_RETURN(node->argb, "0x%08lx");

	w = dt_width(node);
	h = dt_height(node);

	if (!(node->argb = AllocVec(4*w*h,MEMF_PUBLIC|MEMF_CLEAR)))
		SM_RETURN(NULL, "0x%08lx");

	if (!node->o)
	{
		dt_put_rect_on_argb(img, node->x1, node->y1, w, h, node->argb, w, 0, 0);
		SM_RETURN(node->argb, "0x%08lx");
	}

	/* TODO: Check for proper picture.datatype rather than CyberGfx */
	if (CyberGfxBase)
	{
		if (!(DoMethod(node->o,PDTM_READPIXELARRAY,node->argb,PBPAFMT_ARGB,4*w,0,0,w,h)))
		{
			SM_DEBUGF(10,("Could not read ARGB data of \"%s\"\n",node->name));
			FreeVec(node->argb);
			node->argb = NULL;
		}
	} else
	{
		/* TODO: Implement me for (via a temp rp and ReadPixelArray8()) */
	}

	SM_RETURN(node->argb, "0x%08lx");
}

/***************************************************************
 Pastes the dt node into an ARGB buffer which is ensured
 to have enough space.
****************************************************************/
void dt_put_on_argb(struct dt_node *node, void *dest, int dest_width, int x, int y)
{
	dt_argb(node);

	if (node->argb)
	{
		int i,j,w,h;
		ULONG *udest = (ULONG*)dest;
		ULONG *usrc = (ULONG*)node->argb;

		w = dt_width(node);
		h = dt_height(node);

		for (j=0;j<h;j++)
			for (i=0;i<w;i++)
				udest[(y+j)*dest_width+x+i]=usrc[j*w+i];
	}
}

/***************************************************************
 Pastes the dt node into an ARGB buffer.
****************************************************************/
static void dt_put_rect_on_argb(struct dt_node *node, int srcx, int srcy, int src_width, int src_height, void *dest, int dest_width, int x, int y)
{
	dt_argb(node);

	if (node->argb)
	{
		int i,j,w,h;
		ULONG *udest = (ULONG*)dest;
		ULONG *usrc = (ULONG*)node->argb;

		w = dt_width(node);
		h = dt_height(node);

		for (j=0;j<src_height;j++)
			for (i=0;i<src_width;i++)
				udest[(y+j)*dest_width+x+i]=usrc[(srcy+j)*w+srcx+i];
		
/*		for (j=0;j<src_height;j++)
		{
			for (i=0;i<src_width;i++)
			{
				__debug_print("%08lx ",udest[j*dest_width+i]);
			}
			__debug_print("\n");
		}*/
	}
}

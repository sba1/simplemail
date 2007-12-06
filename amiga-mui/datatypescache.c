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
#endif

/**************************************************************/

static Object *img_object;
static struct Screen *img_screen;
static int img_usage_count;

static int mason_available;

static struct list desc_list;
static struct list dt_list;

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

	Object *o;
	void *argb;
	int x1,x2,y1,y2;
	struct Screen *scr;
};

/**************************************************************/

#ifndef __AMIGAOS4__
APTR SetProcWindow(void *newvalue)
{
	struct Process *myproc = (struct Process *)FindTask(NULL);
	APTR oldwindowptr = myproc->pr_WindowPtr;
	myproc->pr_WindowPtr = (APTR)newvalue;
	return oldwindowptr;
}
#endif

Object *LoadPicture(char *filename, struct Screen *scr)
{
	Object *o;
	APTR oldwindowptr;

	/* tell DOS not to bother us with requesters */
	oldwindowptr = SetProcWindow((APTR)-1);

	o = NewDTObject(filename,
			DTA_GroupID          , GID_PICTURE,
			OBP_Precision        , PRECISION_EXACT,
			PDTA_Screen          , scr,
//			PDTA_FreeSourceBitMap, TRUE,
			PDTA_DestMode        , PMODE_V43,
			PDTA_UseFriendBitMap , TRUE,
			TAG_DONE);
	
	SetProcWindow(oldwindowptr);
	
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
		oldwindowptr = SetProcWindow((APTR)-1);
		if ((lock = Lock("TBIMAGES:",ACCESS_READ)))
		{
			mason_available = 1;
			UnLock(lock);
		}
		SetProcWindow(oldwindowptr);
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

	if (img_object) DisposeDTObject(img_object);
	img_object = NULL;
}

/****************************************************************
 Create the dt object directly from filename
****************************************************************/
static struct dt_node *dt_create_from_filename(char *filename, struct Screen *scr)
{
	struct dt_node *node;

	if ((node = (struct dt_node*)malloc(sizeof(struct dt_node))))
	{
		/* Clear the data */
		memset(node,0,sizeof(struct dt_node));

		/* create the datatypes object */
		if ((node->o = LoadPicture(filename,scr)))
		{
			struct BitMapHeader *bmhd = NULL;
			GetDTAttrs(node->o,PDTA_BitMapHeader,&bmhd,TAG_DONE);

			if (bmhd)
			{
				int w = bmhd->bmh_Width;
				int h = bmhd->bmh_Height;
				int d = bmhd->bmh_Depth; 

				node->x2 = w - 1;
				node->y2 = h - 1;

				/* TODO: Do this for the big image as well */
				if (d > 8 && CyberGfxBase)
				{
					if ((node->argb = AllocVec(4*w*h,MEMF_PUBLIC|MEMF_CLEAR)))
					{
						if (!(DoMethod(node->o,PDTM_READPIXELARRAY,node->argb,PBPAFMT_ARGB,4*w,0,0,w,h)))
						{
							FreeVec(node->argb);
							node->argb = NULL;
						}
					}
				}
			}
			node->scr = scr;

			
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
struct dt_node *dt_load_picture(char *filename, struct Screen *scr)
{
	struct icon_desc *icon; 
	struct dt_node *node;

	SM_ENTER;

	node = (struct dt_node*)list_first(&dt_list);
	while (node)
	{
		if (!mystricmp(filename,node->name))
		{
			node->count++;
			return node;
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
				SM_DEBUGF(20,("Trying to load mason icon \"%s\"\n",icon->masonname));
				if ((node = dt_create_from_filename(icon->masonname,scr)))
				{
					node->count = 1;
					node->desc = icon;
					node->name = mystrdup(filename);
					list_insert_tail(&dt_list,&node->node);
					SM_DEBUGF(20,("Mason found\n"));
					SM_RETURN(node,"%p");
				}
				SM_DEBUGF(20,("Mason not found\n"));
			}

			/* No, so we use the big image */
			if (img_object && img_screen != scr)
			{
				/* This has debugging purposes only, it should never happen */
				SM_DEBUGF(1,("The given screen differs from the screen for which the icon image was loaded for!\n"));
				img_object = NULL;
				img_screen = NULL; 
			}
			if (!img_object)
			{
				img_object = LoadPicture("PROGDIR:Images/images",scr);
				img_screen = scr;
			}
			if (!img_object) break;
			img_usage_count++;

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
	if ((node = dt_create_from_filename(filename,scr)))
	{
		node->count = 1;
		node->name = mystrdup(filename);
		list_insert_tail(&dt_list,&node->node);
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
				SM_DEBUGF(20,("Disposing dt object at %p\n",node->o));
				DisposeDTObject(node->o);
				if (node->argb) FreeVec(node->argb);
			} else
			{
				SM_DEBUGF(20,("Disposing dt object icon image at %p\n",img_object));
				img_usage_count--;
				if (!img_usage_count)
				{
					DisposeDTObject(img_object);
					img_object = NULL;
					img_screen = NULL;
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

void dt_put_on_rastport(struct dt_node *node, struct RastPort *rp, int x, int y)
{
	struct BitMap *bitmap = NULL;
	Object *o;

	o = node->o;
	if (!o) o = img_object;
	if (!o) return;

	GetDTAttrs(o,PDTA_DestBitMap,&bitmap,TAG_DONE);
	if (!bitmap) GetDTAttrs(o,PDTA_BitMap,&bitmap,TAG_DONE);
	if (bitmap)
	{
		APTR mask = NULL;

		GetDTAttrs(o,PDTA_MaskPlane,&mask,TAG_DONE);

		if (mask)
		{
			MyBltMaskBitMapRastPort(bitmap,node->x1,node->y1,rp,x,y,dt_width(node),dt_height(node),0xe2,(PLANEPTR)mask);
		} else
		{
			if (node->argb)
			{
				WritePixelArrayAlpha(node->argb,0,0,dt_width(node)*4,rp,x,y,dt_width(node),dt_height(node),0xffffffff);
			} else 
			{
				BltBitMapRastPort(bitmap,node->x1,node->y1,rp,x,y,dt_width(node),dt_height(node),0xc0);
			}
		}
	}
}

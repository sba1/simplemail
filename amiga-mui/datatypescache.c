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

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/datatypes.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "lists.h"
#include "support_indep.h"

#include "amigasupport.h"

static Object *img_object;
static struct list dt_list;

struct dt_node
{
	struct node node;
	char *filename;
	Object *o;
	struct Screen *scr;
	int count;

	int x1,x2,y1,y2;
};

Object *LoadPicture(char *filename, struct Screen *scr)
{
	Object *o;

	/* tell DOS not to bother us with requesters */
	struct Process *myproc = (struct Process *)FindTask(NULL);
	APTR oldwindowptr = myproc->pr_WindowPtr;
	myproc->pr_WindowPtr = (APTR)-1;

	o = NewDTObject(filename,
			DTA_GroupID          , GID_PICTURE,
			OBP_Precision        , PRECISION_EXACT,
			PDTA_Screen          , scr,
			PDTA_FreeSourceBitMap, TRUE,
			PDTA_DestMode        , PMODE_V43,
			PDTA_UseFriendBitMap , TRUE,
			TAG_DONE);
	
	myproc->pr_WindowPtr = oldwindowptr;
	
	/* do all the setup/layout stuff that's necessary to get a bitmap from the dto    */
	/* note that when using V43 datatypes, this might not be a real "struct BitMap *" */
	
	if (o)
	{
		struct FrameInfo fri = {0};
		DoMethod(o,DTM_FRAMEBOX,NULL,&fri,&fri,sizeof(struct FrameInfo),0);
	
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

	list_init(&dt_list);

	if ((file = Open("PROGDIR:Images/images.list",MODE_OLDFILE)))
	{
		char buf[256];
		while((FGets(file,buf,sizeof(buf))))
		{
			char *filename_end;
			char *filename;
			if (*buf==';') continue;
			if ((filename_end = strchr(buf,',')))
			{
				if ((filename = malloc(filename_end-buf+50)))
				{
					struct dt_node *node = malloc(sizeof(struct dt_node));
					if (node)
					{
						char *lastchar;
						memset(node,0,sizeof(struct dt_node));
						node->filename = filename;
						strcpy(filename,"PROGDIR:Images/"); /* 15 chars */
						strncpy(&filename[15],buf,filename_end-buf);
						filename[15+filename_end-buf]=0;
						node->x1 = strtol(filename_end+1,&lastchar,10);
						node->y1 = strtol(lastchar+1,&lastchar,10);
						node->x2 = strtol(lastchar+1,&lastchar,10);
						node->y2 = strtol(lastchar+1,&lastchar,10);
						list_insert_tail(&dt_list,&node->node);
					}
				}
			}
			
		}
		Close(file);
	}
}

void dt_cleanup(void)
{
	struct dt_node *node;
	while ((node = (struct dt_node*)list_remove_tail(&dt_list)))
	{
		if (node->o) DisposeDTObject(node->o);
		if (node->filename) free(node->filename);
		free(node);
	}
	if (img_object) DisposeDTObject(img_object);
	img_object = NULL;
}

struct dt_node *dt_load_picture(char *filename, struct Screen *scr)
{
	struct dt_node *node = (struct dt_node*)list_first(&dt_list);
	while (node)
	{
		if (!mystricmp(filename,node->filename))
		{
			if (!node->o)
			{
				if (!img_object) img_object = LoadPicture("PROGDIR:Images/images",scr);
				if (!img_object) break;
			}
			node->count++;
			return node;
		}
		node = (struct dt_node*)node_next(&node->node);
	}

	if ((node = (struct dt_node*)malloc(sizeof(struct dt_node))))
	{
		/* Clear the data */
		memset(node,0,sizeof(struct dt_node));

		/* create the datatypes object */
		if ((node->o = LoadPicture(filename,scr)))
		{
			struct BitMapHeader *bmhd;
			GetDTAttrs(node->o,PDTA_BitMapHeader,&bmhd,TAG_DONE);

			if (bmhd)
			{
				node->x2 = bmhd->bmh_Width - 1;
				node->y2 = bmhd->bmh_Height - 1;
			}
			node->scr = scr;

			list_insert_tail(&dt_list,&node->node);
			return node;
		}
		free(node);
	}
	return NULL;
}

void dt_dispose_picture(struct dt_node *node)
{
	if (node && node->count)
	{
		node->count--;

/*
		if (node->count == 1)
		{
			node_remove(&node->node);
			DisposeDTObject(node->o);
			free(node);
		}
*/
	}
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
		} else BltBitMapRastPort(bitmap,node->x1,node->y1,rp,x,y,dt_width(node),dt_height(node),0xc0);
	}
}

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
#include <proto/datatypes.h>
#include <proto/intuition.h>

#include "lists.h"
#include "support_indep.h"

static struct list dt_list;

struct dt_node
{
	struct node node;
	char *filename;
	Object *o;
	struct Screen *scr;
	int count;
};

void dt_init(void)
{
	list_init(&dt_list);
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
}

Object *dt_load_picture(char *filename, struct Screen *scr)
{
	struct dt_node *node = (struct dt_node*)list_first(&dt_list);
	while (node)
	{
		if (!mystricmp(filename,node->filename) && scr == node->scr)
		{
			node->count++;
			return node->o;
		}
		node = (struct dt_node*)node_next(&node->node);
	}
	if ((node = (struct dt_node*)malloc(sizeof(struct dt_node))))
	{
		/* tell DOS not to bother us with requesters */
		struct Process *myproc = (struct Process *)FindTask(NULL);
		APTR oldwindowptr = myproc->pr_WindowPtr;
		myproc->pr_WindowPtr = (APTR)-1;

		/* Clear the data */
		memset(node,0,sizeof(struct dt_node));

		/* create the datatypes object */
		node->o = NewDTObject(filename,
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
	
		if (node->o)
		{
			struct FrameInfo fri = {0};
	
			DoMethod(node->o,DTM_FRAMEBOX,NULL,&fri,&fri,sizeof(struct FrameInfo),0);
	
			if (fri.fri_Dimensions.Depth>0)
			{
				if (DoMethod(node->o,DTM_PROCLAYOUT,NULL,1))
				{
					node->filename = mystrdup(filename);
					node->scr = scr;
					node->count = 1;
					list_insert_tail(&dt_list,&node->node);
					return node->o;
				}
			}
			DisposeDTObject(node->o);
		}
	}
	return NULL;
}

void dt_dispose_object(Object *o)
{
	struct dt_node *node = (struct dt_node*)list_first(&dt_list);
	while (node)
	{
		if (o == node->o)
			break;
		node = (struct dt_node*)node_next(&node->node);
	}

	if (node)
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

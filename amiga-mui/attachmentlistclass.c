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

/**
 * @file attachmentlistclass.c
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/mui.h>
#include <mui/NListview_mcc.h>
#include <mui/NListtree_mcc.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "mail.h"
#include "smintl.h"
#include "debug.h"
#include "support_indep.h"

#include "attachmentlistclass.h"
#include "amigasupport.h"
#include "compiler.h"
#include "muistuff.h"

struct AttachmentList_Data
{
	struct Hook construct_hook;
	struct Hook destruct_hook;
	struct Hook display_hook;
	int quick;
};

/**
 * Constructor for attachment entries.
 */
STATIC ASM SAVEDS struct attachment *attachment_construct(REG(a0,struct Hook *h),REG(a2,Object *o),REG(a1,struct MUIP_NListtree_ConstructMessage *msg))
{
	struct attachment *attach = (struct attachment *)msg->UserData;
	struct attachment *new_attach = (struct attachment *)malloc(sizeof(struct attachment));
	if (new_attach)
	{
		*new_attach = *attach;
		new_attach->filename = mystrdup(attach->filename);
		new_attach->temporary_filename = mystrdup(attach->temporary_filename);
		new_attach->description = mystrdup(attach->description);
		new_attach->content_type = mystrdup(attach->content_type);
		new_attach->contents = mystrdup(attach->contents);
	}
	return new_attach;
}

/**
 * Destructor for attachment entries.
 */
STATIC ASM SAVEDS VOID attachment_destruct(REG(a0,struct Hook *h),REG(a2,Object *o),REG(a1,struct MUIP_NListtree_DestructMessage *msg))
{
	struct attachment *attach = (struct attachment *)msg->UserData;
	if (attach)
	{
		if (attach->filename) free(attach->filename);
		if (attach->temporary_filename)
		{
			DeleteFile(attach->temporary_filename);
			free(attach->temporary_filename);
		}
		if (attach->description) free(attach->description);
		if (attach->content_type) free(attach->content_type);
		if (attach->contents) free(attach->contents);
		free(attach);
	}
}

/**
 * Display functions for attachment entries.
 */
STATIC ASM SAVEDS VOID attachment_display(REG(a0,struct Hook *h),REG(a2,Object *obj), REG(a1,struct MUIP_NListtree_DisplayMessage *msg))
{
	struct AttachmentList_Data *data = (struct AttachmentList_Data*)INST_DATA(CL_AttachmentList->mcc_Class,obj);

	if (msg->TreeNode)
	{
		struct attachment *attach = (struct attachment *)msg->TreeNode->tn_User;
		if (data->quick)
		{
			if (attach->filename) *msg->Array = attach->filename;
			else
			{
				if (!mystricmp(attach->content_type,"text/plain") && attach->editable)
					*msg->Array = (char*)_("Editable Mailtext");
				else *msg->Array = attach->content_type;
			}
		} else
		{
			*msg->Array++ = attach->filename?(char*)FilePart(attach->filename):(char *)"";
			*msg->Array++ = NULL;
			*msg->Array++ = attach->content_type;
			*msg->Array++ = attach->description;
		}
	} else
	{
		*msg->Array++ = (char*)_("File Name");
		*msg->Array++ = (char*)_("Size");
		*msg->Array++ = (char*)_("Contents");
		*msg->Array = (char*)_("Description");
	}
}

/**
 * Implementation of OM_NEW
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AttachmentList_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct AttachmentList_Data *data;
	struct TagItem *ti;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct AttachmentList_Data*)INST_DATA(cl,obj);
	init_hook(&data->construct_hook,(HOOKFUNC)attachment_construct);
	init_hook(&data->destruct_hook,(HOOKFUNC)attachment_destruct);
	init_hook(&data->display_hook,(HOOKFUNC)attachment_display);

	if ((ti = FindTagItem(MUIA_AttachmentList_Quick, msg->ops_AttrList)))
		data->quick = !!ti->ti_Data;

	SetAttrs(obj,
						MUIA_NListtree_ConstructHook, &data->construct_hook,
						MUIA_NListtree_DestructHook, &data->destruct_hook,
						MUIA_NListtree_DisplayHook, &data->display_hook,
						MUIA_NListtree_Title, !data->quick,
						data->quick?TAG_IGNORE:MUIA_NListtree_Format, ",,,",
						data->quick?TAG_IGNORE:MUIA_NListtree_TreeColumn, 2,
						MUIA_NListtree_ShowTree, !data->quick,
						TAG_DONE);


	return (ULONG)obj;
}

/**
 * Implementation of MUIM_AskMinMax
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AttachmentList_AskMinMax(struct IClass *cl,Object *obj, struct MUIP_AskMinMax *msg)
{
  DoSuperMethodA(cl, obj, (Msg) msg);
  msg->MinMaxInfo->DefHeight = msg->MinMaxInfo->MinHeight + _font(obj)->tf_YSize;
  return 0;
}


/**
 * Implementation of MUIM_NList_DropType
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AttachmentList_DropType(struct IClass *cl,Object *obj,struct MUIP_NList_DropType *msg)
{
	ULONG rv = DoSuperMethodA(cl,obj,(Msg)msg);
	ULONG active = xget(obj, MUIA_NList_Active);
	struct MUI_NListtree_TreeNode *treenode;

	DoMethod(obj, MUIM_NList_GetEntry, *msg->pos, (ULONG)&treenode);

	if (treenode)
	{
		if (!(treenode->tn_Flags & TNF_LIST) && *msg->type == MUIV_NListtree_DropType_Onto)
		{
			if (*msg->pos > (LONG)active)
			{
				*msg->type = MUIV_NListtree_DropType_Above;
			} else
			{
				*msg->type = MUIV_NListtree_DropType_Below;
			}
		}
	}

	/* no dropping above the first list */
	if (*msg->pos == 0) *msg->type = MUIV_NListtree_DropType_None;
	return rv;
}

/**
 * Implementation of MUIM_AttachmentList_FindUniqueID
 *
 * @param cl the class
 * @param obj the object
 * @param msg the parameter of the method
 * @return
 */
STATIC ULONG AttachmentList_FindUniqueID(struct IClass *cl, Object *obj, struct MUIP_AttachmentList_FindUniqueID *msg)
{
	int i;
	int count = DoMethod(obj,MUIM_NListtree_GetNr, MUIV_NListtree_GetNr_TreeNode_Active,MUIV_NListtree_GetNr_Flag_CountAll);

	for (i=0;i<count;i++)
	{
		struct MUI_NListtree_TreeNode *tn = (struct MUI_NListtree_TreeNode*)
			DoMethod(obj,MUIM_NListtree_GetEntry,MUIV_NListtree_GetEntry_ListNode_Root,i,0);
		if (tn->tn_User && ((((struct attachment*)(tn->tn_User))->unique_id) == msg->unique_id)) return (ULONG)tn;
	}
	return 0;
}

/**
 * The Boopsi Dispatcher for the attachment list class.
 */
STATIC MY_BOOPSI_DISPATCHER(ULONG,AttachmentList_Dispatcher, cl, obj, msg)
{
	switch (msg->MethodID)
	{
		case	OM_NEW:				return AttachmentList_New(cl,obj,(struct opSet*)msg);
		case	MUIM_AskMinMax: return AttachmentList_AskMinMax(cl,obj,(struct MUIP_AskMinMax*)msg);
    case	MUIM_NList_DropType: return AttachmentList_DropType(cl,obj,(struct MUIP_NList_DropType*)msg); 
    case	MUIM_AttachmentList_FindUniqueID: return AttachmentList_FindUniqueID(cl,obj,(struct MUIP_AttachmentList_FindUniqueID*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

/*****************************************************************************/

struct MUI_CustomClass *CL_AttachmentList;

/*****************************************************************************/

int create_attachmentlist_class(void)
{
	SM_ENTER;
	if ((CL_AttachmentList = CreateMCC(MUIC_NListtree,NULL,sizeof(struct AttachmentList_Data),AttachmentList_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_AttachmentList: 0x%lx\n",CL_AttachmentList));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_AttachmentList\n"));
	SM_RETURN(0,"%ld");
}

/*****************************************************************************/

void delete_attachmentlist_class(void)
{
	SM_ENTER;
	if (CL_AttachmentList)
	{
		if (MUI_DeleteCustomClass(CL_AttachmentList))
		{
			SM_DEBUGF(15,("Deleted CL_AttachmentList: 0x%lx\n",CL_AttachmentList));
			CL_AttachmentList = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_AttachmentList: 0x%lx\n",CL_AttachmentList));
		}
	}
	SM_LEAVE;
}

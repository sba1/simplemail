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
#include <dos.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/mui.h>
#include <mui/NListview_MCC.h>
#include <mui/NListtree_Mcc.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>

#include "mail.h"
#include "support.h"

#include "amigasupport.h"
#include "compiler.h"
#include "attachmentlistclass.h"
#include "muistuff.h"

struct AttachmentList_Data
{
	struct Hook construct_hook;
	struct Hook destruct_hook;
	struct Hook display_hook;
};

STATIC ASM struct attachment *attachment_construct(register __a1 struct MUIP_NListtree_ConstructMessage *msg)
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

STATIC ASM VOID attachment_destruct(register __a1 struct MUIP_NListtree_DestructMessage *msg)
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

STATIC ASM VOID attachment_display(register __a1 struct MUIP_NListtree_DisplayMessage *msg)
{
	if (msg->TreeNode)
	{
		struct attachment *attach = (struct attachment *)msg->TreeNode->tn_User;
		*msg->Array++ = attach->filename?(char*)FilePart(attach->filename):"";
		*msg->Array++ = NULL;
		*msg->Array++ = attach->content_type;
		*msg->Array++ = attach->description;
	} else
	{
		*msg->Array++ = "File name";
		*msg->Array++ = "Size";
		*msg->Array++ = "Contents";
		*msg->Array = "Description";
	}
}

STATIC ULONG AttachmentList_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct AttachmentList_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct AttachmentList_Data*)INST_DATA(cl,obj);
	init_hook(&data->construct_hook,(HOOKFUNC)attachment_construct);
	init_hook(&data->destruct_hook,(HOOKFUNC)attachment_destruct);
	init_hook(&data->display_hook,(HOOKFUNC)attachment_display);

	SetAttrs(obj,
						MUIA_NListtree_ConstructHook, &data->construct_hook,
						MUIA_NListtree_DestructHook, &data->destruct_hook,
						MUIA_NListtree_DisplayHook, &data->display_hook,
						MUIA_NListtree_Title, TRUE,
						MUIA_NListtree_Format, ",,,",
						MUIA_NListtree_TreeColumn, 2,
						TAG_DONE);

	return (ULONG)obj;
}

STATIC ULONG AttachmentList_AskMinMax(struct IClass *cl,Object *obj, struct MUIP_AskMinMax *msg)
{
  DoSuperMethodA(cl, obj, (Msg) msg);
  msg->MinMaxInfo->DefHeight = msg->MinMaxInfo->MinHeight + _font(obj)->tf_YSize;
  return 0;
}


STATIC ULONG AttachmentList_DropType(struct IClass *cl,Object *obj,struct MUIP_NList_DropType *msg)
{
	ULONG rv = DoSuperMethodA(cl,obj,(Msg)msg);
	ULONG active = xget(obj, MUIA_NList_Active);
	struct MUI_NListtree_TreeNode *treenode;

	DoMethod(obj, MUIM_NList_GetEntry, *msg->pos, &treenode);

	if (treenode)
	{
		if (!(treenode->tn_Flags & TNF_LIST) && *msg->type == MUIV_NListtree_DropType_Onto)
		{
			if (*msg->pos > active)
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

STATIC ASM ULONG AttachmentList_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case	OM_NEW:				return AttachmentList_New(cl,obj,(struct opSet*)msg);
		case	MUIM_AskMinMax: return AttachmentList_AskMinMax(cl,obj,(struct MUIP_AskMinMax*)msg);
    case	MUIM_NList_DropType: return AttachmentList_DropType(cl,obj,(struct MUIP_NList_DropType*)msg); 
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_AttachmentList;

int create_attachmentlist_class(void)
{
	if ((CL_AttachmentList = MUI_CreateCustomClass(NULL,MUIC_NListtree,NULL,sizeof(struct AttachmentList_Data),AttachmentList_Dispatcher)))
	{
		CL_AttachmentList->mcc_Class->cl_UserData = getreg(REG_A4);
		return 1;
	}
	return 0;
}

void delete_attachmentlist_class(void)
{
	if (CL_AttachmentList) MUI_DeleteCustomClass(CL_AttachmentList);
}

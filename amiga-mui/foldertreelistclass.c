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
** foldertreelistclass.c
*/

#include <string.h>
#include <stdio.h>

#include <dos.h>
#include <libraries/mui.h>
#include <mui/NListview_MCC.h>
#include <mui/NListtree_Mcc.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "folder.h"
#include "simplemail.h"

#include "compiler.h"
#include "foldertreelistclass.h"
#include "muistuff.h"

struct FolderTreelist_Data
{
/*
	struct Hook construct_hook;
	struct Hook destruct_hook;
*/
  /* Constructor and Destructor no longer needed */

	struct Hook display_hook;

	struct folder *folder_maildrop;

	Object *context_menu;
};

/*
STATIC ASM SAVEDS struct folder *folder_construct(register __a1 struct MUIP_NListtree_ConstructMessage *msg)
{
	struct folder *new_folder = (struct folder*)msg->UserData;
	struct folder *folder = (struct folder*)AllocVec(sizeof(struct folder),0);

	if (folder)
	{
		folder->name = StrCopy(new_folder->name);
		folder->path = StrCopy(new_folder->path);
		folder->num_mails = new_folder->num_mails;
	}
	return folder;
}

STATIC ASM SAVEDS VOID folder_destruct(register __a1 struct MUIP_NListtree_DestructMessage *msg)
{
	struct folder *folder = (struct folder*)msg->UserData;
	if (folder->name) FreeVec(folder->name);
	if (folder->path) FreeVec(folder->path);
	FreeVec(folder);
}
*/

STATIC ASM VOID folder_display(register __a1 struct MUIP_NListtree_DisplayMessage *msg)
{
	if (msg->TreeNode)
	{
		struct folder *folder = (struct folder*)msg->TreeNode->tn_User;
		static char mails_buf[32];

		sprintf(mails_buf,"%ld",folder->num_mails);

		*msg->Array++ = folder->name;
		*msg->Array = mails_buf;
	} else
	{
		*msg->Array++ = "Name";
		*msg->Array = "Mails";
	}
}


#define MENU_FOLDER_NEW			1
#define MENU_FOLDER_REM			2
#define MENU_FOLDER_SETTINGS	3
#define MENU_FOLDER_GROUP		4

STATIC ULONG FolderTreelist_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct FolderTreelist_Data *data;

	if (!(obj=(Object *)DoSuperMethodA(cl,obj,(Msg)msg)))
		return 0;

	data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);

	init_hook(&data->display_hook,(HOOKFUNC)folder_display);

	data->context_menu = MenustripObject,
		Child, MenuObjectT("Folders"),
			Child, MenuitemObject, MUIA_Menuitem_Title, "New Folder...", MUIA_UserData, MENU_FOLDER_NEW, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, "New Group...", MUIA_UserData, MENU_FOLDER_GROUP, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, "Remove Folder...", MUIA_UserData, MENU_FOLDER_REM, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, (STRPTR)-1, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, "Settings...", MUIA_UserData, MENU_FOLDER_SETTINGS, End,
			End,
		End;

	SetAttrs(obj,
						MUIA_NListtree_DisplayHook, &data->display_hook,
						MUIA_NListtree_Format, "BAR,",
						MUIA_NListtree_Title, TRUE,
						MUIA_NListtree_DragDropSort, FALSE, /* tempoarary disabled */
						MUIA_ContextMenu, data->context_menu,
						TAG_DONE);

	return (ULONG)obj;
}

STATIC ULONG FolderTreelist_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	if (data->context_menu)
	{
		set(obj,MUIA_ContextMenu, NULL);
		MUI_DisposeObject(data->context_menu);
	}
	return DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG FolderTreelist_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	struct TagItem *ti;
	if ((ti = FindTagItem(MUIA_FolderTreelist_MailDrop,msg->ops_AttrList)))
	{
		data->folder_maildrop = (struct folder*)ti->ti_Data;
	}
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG FolderTreelist_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	switch (msg->opg_AttrID)
	{
		case	MUIA_FolderTreelist_MailDrop:
					*msg->opg_Storage = (LONG)data->folder_maildrop;
					return 1;

		default:
					return DoSuperMethodA(cl,obj,(Msg)msg);
	}
}

STATIC ULONG FolderTreelist_DragQuery(struct IClass *cl, Object *obj, struct MUIP_DragQuery *msg)
{
  if (msg->obj == (Object *)muiUserData(obj))
  {
  	return MUIV_DragQuery_Accept;
  }
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG FolderTreelist_DragDrop(struct IClass *cl,Object *obj,struct MUIP_DragDrop *msg)
{
	if (msg->obj == obj) return DoSuperMethodA(cl,obj,(Msg)msg);

  if (msg->obj == (Object *)muiUserData(obj))
  {
  	LONG drop_type;

/*		DoSuperMethodA(cl,obj,(Msg)msg); */

		drop_type = xget(obj,MUIA_NListtree_DropType);

  	if (drop_type == MUIV_NListtree_DropType_Onto)
  	{
			struct MUI_NListtree_TreeNode *node = (struct MUI_NListtree_TreeNode *)xget(obj,MUIA_NListtree_DropTarget);
			struct folder *folder;

			if (node)
			{
				folder = (struct folder*)node->tn_User;
  			set(obj,MUIA_FolderTreelist_MailDrop,folder);
  		}
  	}
		
		return 0;
  }

  return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG FolderTreelist_DropType(struct IClass *cl, Object *obj,struct MUIP_NList_DropType *msg)
{
	DoSuperMethodA(cl,obj,(Msg)msg);

//	kprintf("before: %ld\n",*msg->type);

	if (*msg->pos == xget(obj,MUIA_NList_Active))
		*msg->type = MUIV_NListtree_DropType_None;
	else *msg->type = MUIV_NListtree_DropType_Onto;

//	kprintf("after: %ld\n",*msg->type);

	return 0;
}

STATIC ULONG FolderTreelist_ContextMenuChoice(struct IClass *cl, Object *obj,struct MUIP_ContextMenuChoice *msg)
{
	switch(xget(msg->item,MUIA_UserData))
	{
		case	MENU_FOLDER_NEW: callback_new_folder(); break;
		case	MENU_FOLDER_GROUP: callback_new_group(); break;
		case	MENU_FOLDER_REM: callback_remove_folder(); break;
		case	MENU_FOLDER_SETTINGS: callback_edit_folder(); break;
	}
	return 0;
}

STATIC ASM ULONG FolderTreelist_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case	OM_NEW:				return FolderTreelist_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE:		return FolderTreelist_Dispose(cl,obj,msg);
		case	OM_SET:				return FolderTreelist_Set(cl,obj,(struct opSet*)msg);
		case	OM_GET:				return FolderTreelist_Get(cl,obj,(struct opGet*)msg);
    case  MUIM_DragQuery: return FolderTreelist_DragQuery(cl,obj,(struct MUIP_DragQuery *)msg);
    case  MUIM_DragDrop:  return FolderTreelist_DragDrop (cl,obj,(struct MUIP_DragDrop *)msg);
    case	MUIM_NList_DropType: return FolderTreelist_DropType(cl,obj,(struct MUIP_NList_DropType*)msg);
    case	MUIM_ContextMenuChoice: return FolderTreelist_ContextMenuChoice(cl,obj,(struct MUIP_ContextMenuChoice*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_FolderTreelist;

int create_foldertreelist_class(void)
{
	if ((CL_FolderTreelist = MUI_CreateCustomClass(NULL,MUIC_NListtree,NULL,sizeof(struct FolderTreelist_Data),FolderTreelist_Dispatcher)))
	{
		CL_FolderTreelist->mcc_Class->cl_UserData = getreg(REG_A4);
		return 1;
	}
	return 0;
}

void delete_foldertreelist_class(void)
{
	if (CL_FolderTreelist) MUI_DeleteCustomClass(CL_FolderTreelist);
}

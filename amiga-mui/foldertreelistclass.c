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
#include "smintl.h"

#include "compiler.h"
#include "foldertreelistclass.h"
#include "muistuff.h"
#include "picturebuttonclass.h"

struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata);

struct FolderTreelist_Data
{
	struct Hook close_hook;
	struct Hook display_hook;
	struct Hook open_hook;

	struct folder *folder_maildrop;

	Object *context_menu;

	int mails_drag;

	APTR image_incoming;
	APTR image_outgoing;
	APTR image_sent;
	APTR image_deleted;
	APTR image_other;
	APTR image_group;

	char name_buf[300];
};

STATIC ASM void folder_close(register __a1 struct MUIP_NListtree_CloseMessage *msg)
{
	struct folder *folder = (struct folder*)msg->TreeNode->tn_User;
	if (folder)
		folder->closed = 1;
}

STATIC ASM VOID folder_display(register __a1 struct MUIP_NListtree_DisplayMessage *msg, register __a2 Object *obj)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(CL_FolderTreelist->mcc_Class,obj);
	if (msg->TreeNode)
	{
		struct folder *folder = (struct folder*)msg->TreeNode->tn_User;
		static char mails_buf[32];
		int num = folder_number_of_mails(folder);
		int unread = folder_number_of_unread_mails(folder);
		APTR image;

		switch (folder->special)
		{
			case	FOLDER_SPECIAL_INCOMING: image = data->image_incoming; break;
			case	FOLDER_SPECIAL_OUTGOING: image = data->image_outgoing; break;
			case	FOLDER_SPECIAL_SENT: image = data->image_sent; break;
			case	FOLDER_SPECIAL_DELETED: image = data->image_deleted; break;
			case	FOLDER_SPECIAL_GROUP: image = data->image_group; break;
			default: image = data->image_other; break;
		}

		if (num != -1)
		{
			if(unread > 0)
			{
				sprintf(mails_buf,folder_number_of_new_mails(folder)?(MUIX_PH "\33b%ld"):(MUIX_PH "%ld"),num);
			}
			else
			{
				sprintf(mails_buf,"%ld",num);
			}
		}	
		else mails_buf[0] = 0;

		sprintf(data->name_buf,"\33O[%08lx]",image);
		if (folder->name) strcat(data->name_buf,folder->name);

		*msg->Array++ = data->name_buf;
		*msg->Array = mails_buf;
	} else
	{
		*msg->Array++ = _("Name");
		*msg->Array = _("Mails");
	}
}

STATIC ASM void folder_open(register __a1 struct MUIP_NListtree_OpenMessage *msg)
{
	struct folder *folder = (struct folder*)msg->TreeNode->tn_User;
	if (folder)
		folder->closed = 0;
}

#define MENU_FOLDER_NEW			1
#define MENU_FOLDER_REM			2
#define MENU_FOLDER_SETTINGS	3
#define MENU_FOLDER_GROUP		4
#define MENU_FOLDER_SAVE			5
#define MENU_FOLDER_RESET		6

STATIC ULONG FolderTreelist_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct FolderTreelist_Data *data;

	if (!(obj=(Object *)DoSuperMethodA(cl,obj,(Msg)msg)))
		return 0;

	data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);

	init_hook(&data->close_hook, (HOOKFUNC)folder_close);
	init_hook(&data->display_hook,(HOOKFUNC)folder_display);
	init_hook(&data->open_hook, (HOOKFUNC)folder_open);

	data->context_menu = MenustripObject,
		Child, MenuObjectT(_("Folders")),
			Child, MenuitemObject, MUIA_Menuitem_Title, _("New Folder..."), MUIA_UserData, MENU_FOLDER_NEW, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("New Group..."), MUIA_UserData, MENU_FOLDER_GROUP, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Remove..."), MUIA_UserData, MENU_FOLDER_REM, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, (STRPTR)-1, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Settings..."), MUIA_UserData, MENU_FOLDER_SETTINGS, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, (STRPTR)-1, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Order"),
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Save"), MUIA_UserData, MENU_FOLDER_SAVE, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Reset"), MUIA_UserData, MENU_FOLDER_RESET, End,		
				End,
			End,
		End;

	SetAttrs(obj,
						MUIA_NListtree_CloseHook, &data->close_hook,
						MUIA_NListtree_DisplayHook, &data->display_hook,
						MUIA_NListtree_OpenHook, &data->open_hook,
						MUIA_NListtree_Format, "BAR,",
						MUIA_NListtree_Title, TRUE,
						MUIA_NListtree_DragDropSort, FALSE, /* tempoarary disabled */
						MUIA_ContextMenu, MUIV_NList_ContextMenu_Always,
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

STATIC ULONG FolderTreelist_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	if (!DoSuperMethodA(cl,obj,(Msg)msg)) return 0;

	data->image_incoming = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/folder_incoming", End, 0);
	data->image_outgoing = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/folder_outgoing", End, 0);
	data->image_sent = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/folder_sent", End, 0);
	data->image_deleted = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/folder_deleted", End, 0);
	data->image_other = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/folder_other", End, 0);
	data->image_group = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/folder_group", End, 0);

	return 1;
}

STATIC ULONG FolderTreelist_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	if (data->image_group) DoMethod(obj, MUIM_NList_DeleteImage, data->image_group);
	if (data->image_other) DoMethod(obj, MUIM_NList_DeleteImage, data->image_other);
	if (data->image_deleted) DoMethod(obj, MUIM_NList_DeleteImage, data->image_deleted);
	if (data->image_sent) DoMethod(obj, MUIM_NList_DeleteImage, data->image_sent);
	if (data->image_outgoing) DoMethod(obj, MUIM_NList_DeleteImage, data->image_outgoing);
	if (data->image_incoming) DoMethod(obj, MUIM_NList_DeleteImage, data->image_incoming);
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

		case	MUIA_FolderTreelist_OrderChanged:
					*msg->opg_Storage = (LONG)1;
					return 1;

		default:
					return DoSuperMethodA(cl,obj,(Msg)msg);
	}
}

STATIC ULONG FolderTreelist_DragQuery(struct IClass *cl, Object *obj, struct MUIP_DragQuery *msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
  if (msg->obj == (Object *)muiUserData(obj))
  {
  	data->mails_drag = 1;
  	return MUIV_DragQuery_Accept;
  }
	data->mails_drag = 0;
  if (msg->obj == obj) 
  	return MUIV_DragQuery_Accept;
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG FolderTreelist_DragDrop(struct IClass *cl,Object *obj,struct MUIP_DragDrop *msg)
{
	if (msg->obj == obj)
	{
		ULONG rc = DoSuperMethodA(cl,obj,(Msg)msg);
		set(obj,MUIA_FolderTreelist_OrderChanged,TRUE);
		return rc;
	}

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

STATIC ULONG FolderTreelist_DropType(struct IClass *cl, Object *obj,struct MUIP_NListtree_DropType *msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	struct MUI_NListtree_TreeNode *node;
	struct folder *f;

	node = (struct MUI_NListtree_TreeNode *)
		DoMethod(obj,MUIM_NListtree_GetEntry, MUIV_NListtree_GetEntry_ListNode_Root, *msg->Pos, 0);

	f = (struct folder*)node->tn_User;

  if (data->mails_drag)
  {
		if (*msg->Pos == xget(obj,MUIA_NList_Active) || (f->special == FOLDER_SPECIAL_GROUP))
			*msg->Type = MUIV_NListtree_DropType_None;
		else *msg->Type = MUIV_NListtree_DropType_Onto;
	} else
	{
		if (*msg->Type == MUIV_NListtree_DropType_Onto && !(node->tn_Flags & TNF_LIST))
			*msg->Type = MUIV_NListtree_DropType_Above;
	}

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
		case	MENU_FOLDER_SAVE: folder_save_order(); break;
		case	MENU_FOLDER_RESET: callback_reload_folder_order(); break;

		default: return DoSuperMethodA(cl,obj,(Msg)msg);
	}
	return 0;
}

STATIC ULONG FolderTreelist_NList_ContextMenuBuild(struct IClass *cl, Object * obj, struct MUIP_NList_ContextMenuBuild *msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	if (msg->ontop) return NULL; /* The default NList Menu should be returned */

/*	set(obj,MUIA_NList_Active,msg->pos);*/

  return (ULONG) data->context_menu;
}

STATIC ULONG FolderTreelist_Refresh(struct IClass *cl, Object *obj, Msg msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	struct folder *f;

	set(obj,MUIA_NListtree_Quiet,TRUE);
	DoMethod(obj, MUIM_NListtree_Clear, NULL, 0);

	for (f = folder_first();f;f = folder_next(f))
	{
		APTR treenode = (APTR)MUIV_NListtree_Insert_ListNode_Root;
		if (f->parent_folder) treenode = FindListtreeUserData(obj, f->parent_folder);
		if (f->special == FOLDER_SPECIAL_GROUP)
		{
			int flags;

			if (f->closed) flags = TNF_LIST;
			else flags = TNF_OPEN|TNF_LIST;

			DoMethod(obj,MUIM_NListtree_Insert,"" /*name*/, f, /*udata */
						treenode,MUIV_NListtree_Insert_PrevNode_Tail,flags);
		} else
		{
			DoMethod(obj,MUIM_NListtree_Insert,"" /*name*/, f, /*udata */
						treenode,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);
		}
	}
	set(obj,MUIA_NListtree_Quiet,FALSE);
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
		case	MUIM_Setup:		return FolderTreelist_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup:	return FolderTreelist_Cleanup(cl,obj,msg);
    case  MUIM_DragQuery: return FolderTreelist_DragQuery(cl,obj,(struct MUIP_DragQuery *)msg);
    case  MUIM_DragDrop:  return FolderTreelist_DragDrop (cl,obj,(struct MUIP_DragDrop *)msg);
		case	MUIM_NListtree_DropType: return FolderTreelist_DropType(cl,obj,(struct MUIP_NListtree_DropType*)msg);
    case	MUIM_ContextMenuChoice: return FolderTreelist_ContextMenuChoice(cl,obj,(struct MUIP_ContextMenuChoice*)msg);
		case  MUIM_NList_ContextMenuBuild: return FolderTreelist_NList_ContextMenuBuild(cl,obj,(struct MUIP_NList_ContextMenuBuild *)msg);
		case	MUIM_FolderTreelist_Refresh: return FolderTreelist_Refresh(cl,obj,msg);
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

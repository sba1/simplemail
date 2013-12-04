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

#include <libraries/mui.h>
#include <mui/NListview_mcc.h>
#include <mui/NListtree_mcc.h>

#include <libraries/iffparse.h> /* MAKE_ID */

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "configuration.h"
#include "folder.h"
#include "simplemail.h"
#include "smintl.h"
#include "debug.h"

#include "compiler.h"
#include "foldertreelistclass.h"
#include "gui_main_arch.h"
#include "muistuff.h"
#include "picturebuttonclass.h"
#include "support.h"

struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata);

struct FolderTreelist_Data
{
	struct Hook close_hook;
	struct Hook display_hook;
	struct Hook open_hook;

	struct folder *folder_maildrop;

	Object *context_menu;
	Object *title_menu;

	Object *show_mails_item;
	Object *show_unread_item;
	Object *show_new_item;

	int mails_drag;
	int show_root;

	Object *image_incoming_obj;
	Object *image_outgoing_obj;
	Object *image_sent_obj;
	Object *image_deleted_obj;
	Object *image_other_obj;
	Object *image_group_obj;
	Object *image_spam_obj;

	APTR image_incoming;
	APTR image_outgoing;
	APTR image_sent;
	APTR image_deleted;
	APTR image_other;
	APTR image_group;
	APTR image_spam;

	char name_buf[300];
};

STATIC ASM SAVEDS VOID folder_close(REG(a0,struct Hook*h), REG(a2, Object *obj), REG(a1,struct MUIP_NListtree_CloseMessage *msg))
{
	struct folder *folder = (struct folder*)msg->TreeNode->tn_User;
	if (folder && ((ULONG)folder != MUIV_FolderTreelist_UserData_Root))
		folder->closed = 1;
}

STATIC ASM SAVEDS VOID folder_display(REG(a0,struct Hook*h), REG(a2, Object *obj), REG(a1,struct MUIP_NListtree_DisplayMessage *msg))
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(CL_FolderTreelist->mcc_Class,obj);
	if (msg->TreeNode)
	{
		if ((ULONG)msg->TreeNode->tn_User == MUIV_FolderTreelist_UserData_Root)
		{
			*msg->Array++ = NULL;
			*msg->Array++ = "";
			*msg->Array++ = "";
			*msg->Array = "";
		} else
		{
			struct folder *folder = (struct folder*)msg->TreeNode->tn_User;
			static char mails_buf[32];
			static char unread_buf[32];
			static char new_buf[32];
			int num = folder_number_of_mails(folder);
			int unread = folder_number_of_unread_mails(folder);
			int newm = folder_number_of_new_mails(folder);
			APTR image;

			switch (folder->special)
			{
				case	FOLDER_SPECIAL_INCOMING: image = data->image_incoming; break;
				case	FOLDER_SPECIAL_OUTGOING: image = data->image_outgoing; break;
				case	FOLDER_SPECIAL_SENT: image = data->image_sent; break;
				case	FOLDER_SPECIAL_DELETED: image = data->image_deleted; break;
				case	FOLDER_SPECIAL_GROUP: image = data->image_group; break;
				case	FOLDER_SPECIAL_SPAM: image = data->image_spam; break;
				default: image = data->image_other; break;
			}

			if (num != -1)
			{
				if(unread > 0)
				{
					sprintf(mails_buf,newm?(MUIX_PH "\33b%d"):(MUIX_PH "%d"),num);
				}
				else
				{
					sprintf(mails_buf,"%d",num);
				}
				sprintf(new_buf,"%d",newm);
				sprintf(unread_buf,"%d",unread);
			}
			else
			{
				mails_buf[0] = 0;
				new_buf[0] = 0;
				unread_buf[0] = 0;
			}

			sprintf(data->name_buf,"\33O[%08lx]%s",(ULONG)image,newm?"\33b":"");
			if (folder->name)
			{
				int cur_len = strlen(data->name_buf);
				char *cur_name_buf = data->name_buf + cur_len;

				/* IMAP folders are UTF8 */
				if (folder->is_imap)
				{
					utf8tostr(folder->name, cur_name_buf, sizeof(data->name_buf) - cur_len, user.config.default_codeset);
				} else
				{
					sm_snprintf(cur_name_buf, sizeof(data->name_buf) - cur_len, "%s", folder->name);
				}
			}

			*msg->Array++ = data->name_buf;
			*msg->Array++ = mails_buf;
			*msg->Array++ = new_buf;
			*msg->Array = unread_buf;
		}
	} else
	{
		*msg->Array++ = _("Name");
		*msg->Array++ = _("Mails");
		*msg->Array++ = _("New");
		*msg->Array = _("Unread");
	}
}

STATIC ASM SAVEDS VOID folder_open(REG(a0,struct Hook *h), REG(a2, Object *o), REG(a1,struct MUIP_NListtree_OpenMessage *msg))
{
	struct folder *folder = (struct folder*)msg->TreeNode->tn_User;
	if (folder && ((ULONG)folder != MUIV_FolderTreelist_UserData_Root))
		folder->closed = 0;
}

/* Contextmenu UserData */
#define MENU_FOLDER_NEW			1
#define MENU_FOLDER_REM			2
#define MENU_FOLDER_SETTINGS	3
#define MENU_FOLDER_GROUP		4
#define MENU_FOLDER_SAVE			5
#define MENU_FOLDER_RESET		6

/* Titlemenu UserData */
#define MENU_SHOW_MAILS			7
#define MENU_SHOW_UNREAD		8
#define MENU_SHOW_NEW			9


STATIC VOID FolderTreelist_UpdateFormat(struct IClass *cl,Object *obj)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	char buf[256];

	strcpy(buf,"COL=0 BAR");

	if (xget(data->show_mails_item,MUIA_Menuitem_Checked)) strcat(buf,",COL=1 P=\33r BAR");
	if (xget(data->show_new_item,MUIA_Menuitem_Checked)) strcat(buf,",COL=2 P=\33r BAR");
	if (xget(data->show_unread_item,MUIA_Menuitem_Checked)) strcat(buf,",COL=3 P=\33r BAR");

	set(obj, MUIA_NListtree_Format, buf);
}

STATIC ULONG FolderTreelist_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct FolderTreelist_Data *data;
	int read_only = GetTagData(MUIA_FolderTreelist_ReadOnly,0,msg->ops_AttrList);
	int show_root = GetTagData(MUIA_FolderTreelist_ShowRoot,0,msg->ops_AttrList);

	if (!(obj=(Object *)DoSuperMethodA(cl,obj,(Msg)msg)))
		return 0;

	data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	data->show_root = show_root;

	data->image_incoming_obj = PictureButtonObject, MUIA_PictureButton_Directory, gui_get_images_directory(), MUIA_PictureButton_Filename, "folder_incoming", End;
	data->image_outgoing_obj = PictureButtonObject, MUIA_PictureButton_Directory, gui_get_images_directory(), MUIA_PictureButton_Filename, "folder_outgoing", End;
	data->image_sent_obj = PictureButtonObject, MUIA_PictureButton_Directory, gui_get_images_directory(), MUIA_PictureButton_Filename, "folder_sent", End;
	data->image_deleted_obj = PictureButtonObject, MUIA_PictureButton_Directory, gui_get_images_directory(), MUIA_PictureButton_Filename, "folder_deleted", End;
	data->image_other_obj = PictureButtonObject, MUIA_PictureButton_Directory, gui_get_images_directory(), MUIA_PictureButton_Filename, "folder_other", End;
	data->image_spam_obj = PictureButtonObject, MUIA_PictureButton_Directory, gui_get_images_directory(), MUIA_PictureButton_Filename, "folder_spam", End;
	data->image_group_obj = PictureButtonObject, MUIA_PictureButton_Directory, gui_get_images_directory(), MUIA_PictureButton_Filename, "folder_group", End;

	init_hook(&data->close_hook, (HOOKFUNC)folder_close);
	init_hook(&data->display_hook,(HOOKFUNC)folder_display);
	init_hook(&data->open_hook, (HOOKFUNC)folder_open);

	if (!read_only)
	{
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
	}

	SetAttrs(obj,
						MUIA_NListtree_CloseHook, &data->close_hook,
						MUIA_NListtree_DisplayHook, &data->display_hook,
						MUIA_NListtree_OpenHook, &data->open_hook,
						MUIA_NListtree_Title, TRUE,
						MUIA_NListtree_DragDropSort, FALSE, /* tempoarary disabled */
						MUIA_NListtree_MultiSelect, MUIV_NListtree_MultiSelect_None,
						MUIA_NListtree_DoubleClick, MUIV_NListtree_DoubleClick_Off,
						read_only?TAG_IGNORE:MUIA_ContextMenu, MUIV_NList_ContextMenu_Always,
						TAG_DONE);

	data->title_menu = MenustripObject,
		Child, MenuObjectT(_("Folder Settings")),
			Child, data->show_mails_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('F','S','M','A'),MUIA_Menuitem_Title, _("Show Mail?"), MUIA_UserData, MENU_SHOW_MAILS, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_new_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('F','S','N','W'),MUIA_Menuitem_Title, _("Show New?"), MUIA_UserData, MENU_SHOW_NEW, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_unread_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('F','S','U','R'),MUIA_Menuitem_Title, _("Show Unread?"), MUIA_UserData, MENU_SHOW_UNREAD, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, -1, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Width: this"), MUIA_UserData, MUIV_NList_Menu_DefWidth_This, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Width: all"), MUIA_UserData, MUIV_NList_Menu_DefWidth_All, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Order: this"), MUIA_UserData, MUIV_NList_Menu_DefOrder_This, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Order: all"), MUIA_UserData, MUIV_NList_Menu_DefOrder_All, End,
			End,
		End;

	FolderTreelist_UpdateFormat(cl,obj);

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
	if (data->title_menu) MUI_DisposeObject(data->title_menu);

	if (data->image_incoming_obj) MUI_DisposeObject(data->image_incoming_obj);
	if (data->image_outgoing_obj) MUI_DisposeObject(data->image_outgoing_obj);
	if (data->image_sent_obj) MUI_DisposeObject(data->image_sent_obj);
	if (data->image_deleted_obj) MUI_DisposeObject(data->image_deleted_obj);
	if (data->image_other_obj) MUI_DisposeObject(data->image_other_obj);
	if (data->image_spam_obj) MUI_DisposeObject(data->image_spam_obj);
	if (data->image_group_obj) MUI_DisposeObject(data->image_group_obj);

	return DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG FolderTreelist_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	if (!DoSuperMethodA(cl,obj,(Msg)msg)) return 0;

	data->image_incoming = (APTR)DoMethod(obj, MUIM_NList_CreateImage, (ULONG)data->image_incoming_obj, 0);
	data->image_outgoing = (APTR)DoMethod(obj, MUIM_NList_CreateImage, (ULONG)data->image_outgoing_obj, 0);
	data->image_sent = (APTR)DoMethod(obj, MUIM_NList_CreateImage, (ULONG)data->image_sent_obj, 0);
	data->image_deleted = (APTR)DoMethod(obj, MUIM_NList_CreateImage, (ULONG)data->image_deleted_obj, 0);
	data->image_other = (APTR)DoMethod(obj, MUIM_NList_CreateImage, (ULONG)data->image_other_obj, 0);
	data->image_spam = (APTR)DoMethod(obj, MUIM_NList_CreateImage, (ULONG)data->image_spam_obj, 0);
	data->image_group = (APTR)DoMethod(obj, MUIM_NList_CreateImage, (ULONG)data->image_group_obj, 0);

	return 1;
}

STATIC ULONG FolderTreelist_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	if (data->image_group) DoMethod(obj, MUIM_NList_DeleteImage, (ULONG)data->image_group);
	if (data->image_spam) DoMethod(obj, MUIM_NList_DeleteImage, (ULONG)data->image_spam);
	if (data->image_other) DoMethod(obj, MUIM_NList_DeleteImage, (ULONG)data->image_other);
	if (data->image_deleted) DoMethod(obj, MUIM_NList_DeleteImage, (ULONG)data->image_deleted);
	if (data->image_sent) DoMethod(obj, MUIM_NList_DeleteImage, (ULONG)data->image_sent);
	if (data->image_outgoing) DoMethod(obj, MUIM_NList_DeleteImage, (ULONG)data->image_outgoing);
	if (data->image_incoming) DoMethod(obj, MUIM_NList_DeleteImage, (ULONG)data->image_incoming);
	return DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG FolderTreelist_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;
	ULONG rc;

	tstate = (struct TagItem *)msg->ops_AttrList;

	while ((tag = NextTagItem ((APTR)&tstate)))
	{
		ULONG tidata = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case	MUIA_FolderTreelist_MailDrop:
						data->folder_maildrop = (struct folder*)tidata;
						break;

			case	MUIA_FolderTreelist_Active:
						{
							struct MUI_NListtree_TreeNode *tn;
							tn = FindListtreeUserData(obj, (APTR)tidata);
							set(obj,MUIA_NListtree_Active,tn);
						}
						break;
		}
	}

	rc = DoSuperMethodA(cl,obj,(Msg)msg);
	return rc;
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

		case	MUIA_FolderTreelist_Active:
					{
						struct MUI_NListtree_TreeNode *tree_node;

						tree_node = (struct MUI_NListtree_TreeNode *)xget(obj,MUIA_NListtree_Active);

						if (tree_node && tree_node->tn_User)
							*msg->opg_Storage = (IPTR)tree_node->tn_User;
						else
							*msg->opg_Storage = 0;
					}

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
  {
  	return MUIV_DragQuery_Accept;
  }
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG FolderTreelist_DragDrop(struct IClass *cl,Object *obj,struct MUIP_DragDrop *msg)
{
	if (msg->obj == obj)
	{
		ULONG rc;
		ULONG droptype = xget(obj,MUIA_NListtree_DropType);
		struct MUI_NListtree_TreeNode *src_node = (struct MUI_NListtree_TreeNode *)xget(obj,MUIA_NListtree_Active);
		struct MUI_NListtree_TreeNode *dest_node = (struct MUI_NListtree_TreeNode *)xget(obj,MUIA_NListtree_DropTarget);

		if (src_node && dest_node)
		{
			struct folder *src_folder, *dest_folder, *prev_dest_folder;

			src_folder = (struct folder*)src_node->tn_User;
			dest_folder = (struct folder*)dest_node->tn_User;

			if (src_folder && dest_folder)
			{
				prev_dest_folder = folder_prev(dest_folder);

				if (!(droptype == MUIV_NListtree_DropType_Above && (!prev_dest_folder || !folder_on_same_imap_server(prev_dest_folder,dest_folder))))
				{
					if ((src_folder->is_imap || dest_folder->is_imap) && !folder_on_same_imap_server(src_folder,dest_folder))
						return 0;
				}
			}
		}

		rc = DoSuperMethodA(cl,obj,(Msg)msg);
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

STATIC ULONG FolderTreelist_Export(struct IClass *cl, Object *obj, struct MUIP_Export *msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);

	DoMethodA(data->show_mails_item, (Msg)msg);
	DoMethodA(data->show_unread_item, (Msg)msg);
	DoMethodA(data->show_new_item, (Msg)msg);
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG FolderTreelist_Import(struct IClass *cl, Object *obj, struct MUIP_Import *msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);

	DoMethodA(data->show_mails_item, (Msg)msg);
	DoMethodA(data->show_unread_item, (Msg)msg);
	DoMethodA(data->show_new_item, (Msg)msg);

	FolderTreelist_UpdateFormat(cl,obj);

	return DoSuperMethodA(cl,obj,(Msg)msg);
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
		case	MENU_SHOW_MAILS:
		case	MENU_SHOW_UNREAD:
		case	MENU_SHOW_NEW:
				  FolderTreelist_UpdateFormat(cl,obj);
				  break;

		default: return DoSuperMethodA(cl,obj,(Msg)msg);
	}
	return 0;
}

STATIC ULONG FolderTreelist_NList_ContextMenuBuild(struct IClass *cl, Object * obj, struct MUIP_NList_ContextMenuBuild *msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	if (msg->ontop) return (ULONG)data->title_menu;

/*	set(obj,MUIA_NList_Active,msg->pos);*/

	return (ULONG) data->context_menu;
}

/* TODO: We need to make sure that we are always notifed if something changes in the folder model */
STATIC ULONG FolderTreelist_Refresh(struct IClass *cl, Object *obj, struct MUIP_FolderTreelist_Refresh *msg)
{
	struct FolderTreelist_Data *data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);
	struct folder *f;
	APTR root;

	set(obj,MUIA_NListtree_Quiet,TRUE);
	DoMethod(obj, MUIM_NListtree_Clear, NULL, 0);

	if (data->show_root)
	{
		root = (APTR)DoMethod(obj,MUIM_NListtree_Insert, (ULONG)_("All folders") /*name*/, MUIV_FolderTreelist_UserData_Root, /*udata */
					MUIV_NListtree_Insert_ListNode_Root, MUIV_NListtree_Insert_PrevNode_Tail, TNF_OPEN|TNF_LIST/*flags*/);
	} else root = (APTR)MUIV_NListtree_Insert_ListNode_Root;

	for (f = folder_first();f;f = folder_next(f))
	{
		APTR treenode = root;

		/* groups cannot be excluded at the moment */
		if (msg->exclude == f && msg->exclude->special != FOLDER_SPECIAL_GROUP) continue;

		if (f->parent_folder) treenode = FindListtreeUserData(obj, f->parent_folder);
		if (f->special == FOLDER_SPECIAL_GROUP)
		{
			int flags;

			if (f->closed) flags = TNF_LIST;
			else flags = TNF_OPEN|TNF_LIST;

			DoMethod(obj, MUIM_NListtree_Insert, (ULONG)"" /*name*/, (ULONG)f, /*udata */
						(ULONG)treenode, MUIV_NListtree_Insert_PrevNode_Tail, flags);
		} else
		{
			DoMethod(obj, MUIM_NListtree_Insert, (ULONG)"" /*name*/, (ULONG)f, /*udata */
						(ULONG)treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0/*flags*/);
		}
	}
	set(obj,MUIA_NListtree_Quiet,FALSE);
	return 0;
}

STATIC MY_BOOPSI_DISPATCHER(ULONG, FolderTreelist_Dispatcher, cl, obj, msg)
{
	switch (msg->MethodID)
	{
		case	OM_NEW:				return FolderTreelist_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE:		return FolderTreelist_Dispose(cl,obj,msg);
		case	OM_SET:				return FolderTreelist_Set(cl,obj,(struct opSet*)msg);
		case	OM_GET:				return FolderTreelist_Get(cl,obj,(struct opGet*)msg);
		case	MUIM_Setup:		return FolderTreelist_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup:	return FolderTreelist_Cleanup(cl,obj,msg);
		case	MUIM_DragQuery: return FolderTreelist_DragQuery(cl,obj,(struct MUIP_DragQuery *)msg);
		case	MUIM_DragDrop:  return FolderTreelist_DragDrop (cl,obj,(struct MUIP_DragDrop *)msg);
		case	MUIM_NListtree_DropType: return FolderTreelist_DropType(cl,obj,(struct MUIP_NListtree_DropType*)msg);
		case	MUIM_Export:		return FolderTreelist_Export(cl,obj,(struct MUIP_Export *)msg);
		case	MUIM_Import:		return FolderTreelist_Import(cl,obj,(struct MUIP_Import *)msg);
		case	MUIM_ContextMenuChoice: return FolderTreelist_ContextMenuChoice(cl,obj,(struct MUIP_ContextMenuChoice*)msg);
		case	MUIM_NList_ContextMenuBuild: return FolderTreelist_NList_ContextMenuBuild(cl,obj,(struct MUIP_NList_ContextMenuBuild *)msg);
		case	MUIM_FolderTreelist_Refresh: return FolderTreelist_Refresh(cl,obj,(struct MUIP_FolderTreelist_Refresh*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_FolderTreelist;

int create_foldertreelist_class(void)
{
	SM_ENTER;
	if ((CL_FolderTreelist = CreateMCC(MUIC_NListtree,NULL,sizeof(struct FolderTreelist_Data),FolderTreelist_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_FolterTreelist: 0x%lx\n",CL_FolderTreelist));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_FolterTreelist\n"));
	SM_RETURN(0,"%ld");
}

void delete_foldertreelist_class(void)
{
	SM_ENTER;
	if (CL_FolderTreelist)
	{
		if (MUI_DeleteCustomClass(CL_FolderTreelist))
		{
			SM_DEBUGF(15,("Deleted CL_FolterTreelist: 0x%lx\n",CL_FolderTreelist));
			CL_FolderTreelist = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_FolterTreelist: 0x%lx\n",CL_FolderTreelist));
		}
	}
	SM_LEAVE;
}

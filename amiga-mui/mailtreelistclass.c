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
** mailtreelistclass.c
*/

#include <dos.h>
#include <string.h>
#include <stdio.h>

#include <libraries/iffparse.h>

#include <libraries/mui.h>
#include <mui/NListview_MCC.h>
#include <mui/NListtree_Mcc.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "mail.h"
#include "folder.h"
#include "simplemail.h"
#include "smintl.h"

#include "amigasupport.h"
#include "compiler.h"
#include "mailtreelistclass.h"
#include "muistuff.h"
#include "picturebuttonclass.h"
#include "support_indep.h"

struct MailTreelist_Data
{
	struct Hook display_hook;
	int folder_type;

	APTR status_unread;
	APTR status_read;
	APTR status_waitsend;
	APTR status_sent;
	APTR status_mark;
	APTR status_hold;
	APTR status_reply;
	APTR status_forward;

	APTR status_important;
	APTR status_attach;
	APTR status_group;
	APTR status_new;
	APTR status_crypt;

	Object *context_menu;
	Object *title_menu;

	Object *show_from_item;
	Object *show_subject_item;
	Object *show_reply_item;
	Object *show_date_item;
	Object *show_size_item;
	Object *show_filename_item;

	/* translated strings (faster to hold the translation) */
	char *status_text;
	char *from_text;
	char *to_text;
	char *subject_text;
	char *reply_text;
	char *date_text;
	char *size_text;
	char *filename_text;
};

STATIC ASM VOID mails_display(register __a1 struct MUIP_NListtree_DisplayMessage *msg, register __a2 Object *obj)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(CL_MailTreelist->mcc_Class,obj);

	if (msg->TreeNode)
	{
		struct mail *mail = (struct mail*)msg->TreeNode->tn_User;
		if (mail == (struct mail*)MUIV_MailTreelist_UserData_Name)
		{
			/* only one string should be displayed */
			*msg->Array++ = NULL; /* status */
			*msg->Array++ = NULL; /* status */
			*msg->Array++ = NULL; /* status */
			*msg->Array++ = NULL; /* status */
			*msg->Array++ = NULL; /* status */
			*msg->Array++ = NULL; /* status */
			*msg->Array = NULL; /* status */
		} else
		{
			/* is a mail */
			APTR status;
			static char size_buf[32];
			static char date_buf[64];
			static char status_buf[128];
			static char field_buf[256];

			if (mail->flags & MAIL_FLAGS_NEW)
			{
				sprintf(status_buf,"\33O[%08lx]",data->status_new);
				*msg->Preparse++ = "\33b";
				*msg->Preparse++ = "\33b";
				*msg->Preparse++ = "\33b";
				*msg->Preparse++ = "\33b";
				*msg->Preparse++ = "\33b";
				*msg->Preparse++ = "\33b";
				*msg->Preparse = "\33b";

			} else
			{
				switch(mail_get_status_type(mail))
				{
					case	MAIL_STATUS_UNREAD:status = data->status_unread;break;
					case	MAIL_STATUS_READ:status = data->status_read;break;
					case	MAIL_STATUS_WAITSEND:status = data->status_waitsend;break;
					case	MAIL_STATUS_SENT:status = data->status_sent;break;
					case	MAIL_STATUS_HOLD:status = data->status_hold;break;
					case	MAIL_STATUS_REPLIED:status = data->status_reply;break;
					case	MAIL_STATUS_FORWARD:status = data->status_forward;break;
					default: status = NULL;
				}
				sprintf(status_buf,"\33O[%08lx]",status);
			}

			if (mail->status & MAIL_STATUS_FLAG_MARKED) sprintf(status_buf+strlen(status_buf),"\33O[%08lx]",data->status_mark);
			if (mail->flags & MAIL_FLAGS_IMPORTANT) sprintf(status_buf+strlen(status_buf),"\33O[%08lx]",data->status_important);
			if (mail->flags & MAIL_FLAGS_CRYPT) sprintf(status_buf+strlen(status_buf),"\33O[%08lx]",data->status_crypt);
			else if (mail->flags & MAIL_FLAGS_ATTACH) sprintf(status_buf+strlen(status_buf),"\33O[%08lx]",data->status_attach);

			sprintf(size_buf,"%ld",mail->size);
			SecondsToString(date_buf,mail->seconds);

			*msg->Array++ = status_buf; /* status */

			{
				char *field;

				if (data->folder_type == FOLDER_TYPE_SEND) field = mail->to;
				else field = mail->from;

				if (mail->flags & MAIL_FLAGS_GROUP)
				{
					int field_len;
					sprintf(field_buf,"\33O[%08lx]",data->status_group);
					if (field)
					{
						field_len = strlen(field_buf);
						mystrlcpy(field_buf+field_len,field,sizeof(field_buf)-field_len);
					}
					*msg->Array++ = field_buf;
				} else *msg->Array++ = field;
			}

			*msg->Array++ = mail->subject;
			*msg->Array++ = mail->reply;
			*msg->Array++ = date_buf;
			*msg->Array++ = size_buf;
			*msg->Array = mail->filename;
		}
	} else
	{
		*msg->Array++ = data->status_text;

		if (data->folder_type != FOLDER_TYPE_SEND)
			*msg->Array++ = data->from_text;
		else *msg->Array++ = data->to_text;

		*msg->Array++ = data->subject_text;
		*msg->Array++ = data->reply_text;
		*msg->Array++ = data->date_text;
		*msg->Array++ = data->size_text;
		*msg->Array = data->filename_text;
	}
}

STATIC VOID MailTreelist_UpdateFormat(struct IClass *cl,Object *obj)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	char buf[256];

	strcpy(buf,"COL=0");

	if (xget(data->show_from_item,MUIA_Menuitem_Checked)) strcat(buf," BAR,COL=1");
	if (xget(data->show_subject_item,MUIA_Menuitem_Checked)) strcat(buf," BAR,COL=2");
	if (xget(data->show_reply_item,MUIA_Menuitem_Checked)) strcat(buf," BAR,COL=3");
	if (xget(data->show_date_item,MUIA_Menuitem_Checked)) strcat(buf," BAR,COL=4");
	if (xget(data->show_size_item,MUIA_Menuitem_Checked)) strcat(buf," BAR,COL=5");
	if (xget(data->show_filename_item,MUIA_Menuitem_Checked)) strcat(buf," BAR,COL=6");

	set(obj, MUIA_NListtree_Format, buf);
}

STATIC ULONG MailTreelist_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct MailTreelist_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_NListtree_MultiSelect,MUIV_NListtree_MultiSelect_Default/*|MUIV_NListtree_MultiSelect_Flag_AutoSelectChilds*/,
					MUIA_ContextMenu, MUIV_NList_ContextMenu_Always,
					MUIA_NListtree_DupNodeName, FALSE,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	data->status_text = _("Status");
	data->from_text = _("From");
	data->to_text = _("To");
	data->subject_text = _("Subject");
	data->reply_text = _("Reply");
	data->date_text = _("Date");
	data->size_text = _("Size");
	data->filename_text = ("Filename");

	init_hook(&data->display_hook,(HOOKFUNC)mails_display);

	SetAttrs(obj,
						MUIA_NListtree_DisplayHook, &data->display_hook,
						MUIA_NListtree_Title, TRUE,
						TAG_DONE);

	data->title_menu = MenustripObject,
		Child, MenuObjectT(_("Mail Settings")),
			Child, data->show_from_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','F','T'),MUIA_Menuitem_Title, _("Show From/To?"), MUIA_UserData, 1, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_subject_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','S','B'),MUIA_Menuitem_Title, _("Show Subject?"), MUIA_UserData, 2, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_reply_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','R','T'),MUIA_Menuitem_Title, _("Show Reply-To?"), MUIA_UserData, 3, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_date_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','D','T'),MUIA_Menuitem_Title, _("Show Date?"), MUIA_UserData, 4, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_size_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','S','Z'),MUIA_Menuitem_Title, _("Show Size?"), MUIA_UserData, 5, MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, data->show_filename_item = MenuitemObject, MUIA_ObjectID, MAKE_ID('M','S','F','N'), MUIA_Menuitem_Title, _("Show Filename?"), MUIA_UserData, 6,  MUIA_Menuitem_Checked, TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, -1, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Width: this"), MUIA_UserData, MUIV_NList_Menu_DefWidth_This, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Width: all"), MUIA_UserData, MUIV_NList_Menu_DefWidth_All, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Order: this"), MUIA_UserData, MUIV_NList_Menu_DefOrder_This, End,
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Default Order: all"), MUIA_UserData, MUIV_NList_Menu_DefOrder_All, End,
			End,
		End;

	MailTreelist_UpdateFormat(cl,obj);

	return (ULONG)obj;
}

STATIC ULONG MailTreelist_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	if (data->context_menu) MUI_DisposeObject(data->context_menu);
	if (data->title_menu) MUI_DisposeObject(data->title_menu);
	return DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG MailTreelist_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;

	tstate = (struct TagItem *)msg->ops_AttrList;

	while (tag = NextTagItem (&tstate))
	{
		ULONG tidata = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case	MUIA_MailTreelist_FolderType:
						if (data->folder_type != tag->ti_Data)
						{
							data->folder_type = tag->ti_Data;
						}
						break;
		}
	}

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG MailTreelist_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	if (!DoSuperMethodA(cl,obj,(Msg)msg)) return 0;

	data->status_unread = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_unread", End, 0);
	data->status_read = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_old", End, 0);
	data->status_waitsend = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_waitsend", End, 0);
	data->status_sent = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_sent", End, 0);
	data->status_mark = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_mark", End, 0);
	data->status_hold = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_hold", End, 0);
	data->status_reply = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_reply", End, 0);
	data->status_forward = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_forward", End, 0);

	data->status_important = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_urgent", End, 0);
	data->status_attach = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_attach", End, 0);
	data->status_group = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_group", End, 0);
	data->status_new = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_new", End, 0);
	data->status_crypt = (APTR)DoMethod(obj, MUIM_NList_CreateImage, PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_crypt", End, 0);
	
	return 1;
}

STATIC ULONG MailTreelist_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	if (data->status_crypt) DoMethod(obj, MUIM_NList_DeleteImage, data->status_crypt);
	if (data->status_new) DoMethod(obj, MUIM_NList_DeleteImage, data->status_new);
	if (data->status_group) DoMethod(obj, MUIM_NList_DeleteImage, data->status_group);
	if (data->status_attach) DoMethod(obj, MUIM_NList_DeleteImage, data->status_attach);
	if (data->status_important) DoMethod(obj, MUIM_NList_DeleteImage, data->status_important);
	if (data->status_hold) DoMethod(obj, MUIM_NList_DeleteImage, data->status_hold);
	if (data->status_mark) DoMethod(obj, MUIM_NList_DeleteImage, data->status_mark);
	if (data->status_reply) DoMethod(obj, MUIM_NList_DeleteImage, data->status_reply);
	if (data->status_forward) DoMethod(obj, MUIM_NList_DeleteImage, data->status_forward);
	if (data->status_unread) DoMethod(obj, MUIM_NList_DeleteImage, data->status_unread);
	if (data->status_read) DoMethod(obj, MUIM_NList_DeleteImage, data->status_read);
	if (data->status_waitsend) DoMethod(obj, MUIM_NList_DeleteImage, data->status_waitsend);
	if (data->status_sent) DoMethod(obj, MUIM_NList_DeleteImage, data->status_sent);
	return DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG MailTreelist_DragQuery(struct IClass *cl, Object *obj, struct MUIP_DragDrop *msg)
{
  if (msg->obj==obj) return MUIV_DragQuery_Refuse; /* mails should not be resorted by the user */
  return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG MailTreelist_MultiTest(struct IClass *cl, Object *obj, struct MUIP_NListtree_MultiTest *msg)
{
	if (msg->TreeNode->tn_User == (APTR)MUIV_MailTreelist_UserData_Name) return FALSE;
	return TRUE;
}

STATIC ULONG MailTreelist_Export(struct IClass *cl, Object *obj, struct MUIP_Export *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	DoMethodA(data->show_from_item, (Msg)msg);
	DoMethodA(data->show_subject_item, (Msg)msg);
	DoMethodA(data->show_reply_item, (Msg)msg);
	DoMethodA(data->show_date_item, (Msg)msg);
	DoMethodA(data->show_size_item, (Msg)msg);
	DoMethodA(data->show_filename_item, (Msg)msg);
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG MailTreelist_Import(struct IClass *cl, Object *obj, struct MUIP_Import *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	DoMethodA(data->show_from_item, (Msg)msg);
	DoMethodA(data->show_subject_item, (Msg)msg);
	DoMethodA(data->show_reply_item, (Msg)msg);
	DoMethodA(data->show_date_item, (Msg)msg);
	DoMethodA(data->show_size_item, (Msg)msg);
	DoMethodA(data->show_filename_item, (Msg)msg);	

	MailTreelist_UpdateFormat(cl,obj);

	return DoSuperMethodA(cl,obj,(Msg)msg);
}


#define MENU_SETSTATUS_MARK   7
#define MENU_SETSTATUS_UNMARK 8
#define MENU_SETSTATUS_READ   9
#define MENU_SETSTATUS_UNREAD 10
#define MENU_SETSTATUS_HOLD	11
#define MENU_SETSTATUS_WAITSEND  12

STATIC ULONG MailTreelist_NList_ContextMenuBuild(struct IClass *cl, Object * obj, struct MUIP_NList_ContextMenuBuild *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
  Object *context_menu;

  if (data->context_menu)
  {
  	MUI_DisposeObject(data->context_menu);
  	data->context_menu = NULL;
  }

	if (msg->ontop) return (ULONG)data->title_menu; /* The default NList Menu should be returned */

	context_menu = MenustripObject,
		Child, MenuObjectT(_("Mail")),
			Child, MenuitemObject, MUIA_Menuitem_Title, _("Set status"),
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Mark"), MUIA_UserData, MENU_SETSTATUS_MARK, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Unmark"), MUIA_UserData, MENU_SETSTATUS_UNMARK, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Hold"), MUIA_UserData, MENU_SETSTATUS_HOLD, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Pending"), MUIA_UserData, MENU_SETSTATUS_WAITSEND, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Read"), MUIA_UserData, MENU_SETSTATUS_READ, End,
				Child, MenuitemObject, MUIA_Menuitem_Title, _("Unread"), MUIA_UserData, MENU_SETSTATUS_UNREAD, End,
				End,
			End,
		End;

  data->context_menu = context_menu;
  return (ULONG) context_menu;
}

STATIC ULONG MailTreelist_ContextMenuChoice(struct IClass *cl, Object *obj, struct MUIP_ContextMenuChoice *msg)
{
	switch (xget(msg->item,MUIA_UserData))
	{
		case	1:
		case  2:
		case  3:
		case  4:
		case  5:
		case  6:
				  MailTreelist_UpdateFormat(cl,obj);
				  break;
		case	MENU_SETSTATUS_MARK: callback_mails_mark(1); break;
		case	MENU_SETSTATUS_UNMARK: callback_mails_mark(0); break;
		case	MENU_SETSTATUS_READ: callback_mails_set_status(MAIL_STATUS_READ); break;
		case	MENU_SETSTATUS_UNREAD: callback_mails_set_status(MAIL_STATUS_UNREAD); break;
		case	MENU_SETSTATUS_HOLD: callback_mails_set_status(MAIL_STATUS_HOLD); break;
		case	MENU_SETSTATUS_WAITSEND: callback_mails_set_status(MAIL_STATUS_WAITSEND); break;
		default: 
		{
			return DoSuperMethodA(cl,obj,(Msg)msg);
		}
	}
  return 0;
}

STATIC ASM ULONG MailTreelist_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case	OM_NEW:				return MailTreelist_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE:		return MailTreelist_Dispose(cl,obj,msg);
		case	OM_SET:				return MailTreelist_Set(cl,obj,(struct opSet*)msg);
		case	MUIM_Setup:		return MailTreelist_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup:	return MailTreelist_Cleanup(cl,obj,msg);
		case  MUIM_DragQuery: return MailTreelist_DragQuery(cl,obj,(struct MUIP_DragDrop *)msg);
		case	MUIM_Export:		return MailTreelist_Export(cl,obj,(struct MUIP_Export *)msg);
		case	MUIM_Import:		return MailTreelist_Import(cl,obj,(struct MUIP_Import *)msg);
		case	MUIM_NListtree_MultiTest: return MailTreelist_MultiTest(cl,obj,(struct MUIP_NListtree_MultiTest*)msg);
		case	MUIM_ContextMenuChoice: return MailTreelist_ContextMenuChoice(cl, obj, (struct MUIP_ContextMenuChoice *)msg);
		case  MUIM_NList_ContextMenuBuild: return MailTreelist_NList_ContextMenuBuild(cl,obj,(struct MUIP_NList_ContextMenuBuild *)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_MailTreelist;

int create_mailtreelist_class(void)
{
	if ((CL_MailTreelist = MUI_CreateCustomClass(NULL,MUIC_NListtree,NULL,sizeof(struct MailTreelist_Data),MailTreelist_Dispatcher)))
	{
		CL_MailTreelist->mcc_Class->cl_UserData = getreg(REG_A4);
		return 1;
	}
	return 0;
}

void delete_mailtreelist_class(void)
{
	if (CL_MailTreelist) MUI_DeleteCustomClass(CL_MailTreelist);
}

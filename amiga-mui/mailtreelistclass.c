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

#include "amigasupport.h"
#include "compiler.h"
#include "mailtreelistclass.h"
#include "muistuff.h"

struct MailTreelist_Data
{
	struct Hook display_hook;
	int folder_type;

	APTR status_unread;
	APTR status_read;
	APTR status_waitsend;
	APTR status_sent;
	APTR status_new;
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
			*msg->Array++ = NULL; /* status */
			*msg->Array = NULL; /* status */
		} else
		{
			/* is a mail */
			APTR status;
			static char size_buf[32];
			static char date_buf[64];
			static char status_buf[128];

			switch(mail->status)
			{
				case	MAIL_STATUS_UNREAD:status = data->status_unread;break;
				case	MAIL_STATUS_READ:status = data->status_read;break;
				case	MAIL_STATUS_WAITSEND:status = data->status_waitsend;break;
				case	MAIL_STATUS_SENT:status = data->status_sent;break;
				default: status = NULL;
			}

			sprintf(status_buf,"\33O[%08lx]",status);
			if (mail->flags & MAIL_FLAGS_NEW) sprintf(status_buf+strlen(status_buf),"\33O[%08lx]",data->status_new);

			sprintf(size_buf,"%ld",mail->size);
			SecondsToString(date_buf,mail->seconds);

			*msg->Array++ = status_buf; /* status */
			*msg->Array++ = mail->from;
			*msg->Array++ = mail->to;
			*msg->Array++ = mail->subject;
			*msg->Array++ = mail->reply;
			*msg->Array++ = date_buf;
			*msg->Array++ = size_buf;
			*msg->Array = mail->filename;
		}
	} else
	{
		*msg->Array++ = "Status";
		*msg->Array++ = "From";
		*msg->Array++ = "To";
		*msg->Array++ = "Subject";
		*msg->Array++ = "Reply";
		*msg->Array++ = "Date";
		*msg->Array++ = "Size";
		*msg->Array = "Filename";
	}
}


STATIC ULONG MailTreelist_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct MailTreelist_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_NListtree_MultiSelect,MUIV_NListtree_MultiSelect_Default/*|MUIV_NListtree_MultiSelect_Flag_AutoSelectChilds*/,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	init_hook(&data->display_hook,(HOOKFUNC)mails_display);

	SetAttrs(obj,
						MUIA_NListtree_DisplayHook, &data->display_hook,
						MUIA_NListtree_Title, TRUE,
						MUIA_NListtree_Format, "BAR,BAR,BAR COL=3,BAR COL=4,BAR COL=5,BAR COL=6,COL=7",
						TAG_DONE);

	return (ULONG)obj;
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
							if (data->folder_type == FOLDER_TYPE_SEND)
							{
								set(obj,MUIA_NListtree_Format,"BAR,BAR COL=2,BAR COL=3,BAR COL=4,BAR COL=5,BAR COL=6,COL=7");
							} else
							{
								set(obj,MUIA_NListtree_Format,"BAR,BAR,BAR COL=3,BAR COL=4,BAR COL=5,BAR COL=6,COL=7");
							}
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

	data->status_unread = (APTR)DoMethod(obj, MUIM_NList_CreateImage, DtpicObject, MUIA_Dtpic_Name, "PROGDIR:Images/status_unread", End, 0);
	data->status_read = (APTR)DoMethod(obj, MUIM_NList_CreateImage, DtpicObject, MUIA_Dtpic_Name, "PROGDIR:Images/status_old", End, 0);
	data->status_waitsend = (APTR)DoMethod(obj, MUIM_NList_CreateImage, DtpicObject, MUIA_Dtpic_Name, "PROGDIR:Images/status_waitsend", End, 0);
	data->status_sent = (APTR)DoMethod(obj, MUIM_NList_CreateImage, DtpicObject, MUIA_Dtpic_Name, "PROGDIR:Images/status_sent", End, 0);

	data->status_new = (APTR)DoMethod(obj, MUIM_NList_CreateImage, DtpicObject, MUIA_Dtpic_Name, "PROGDIR:Images/status_new", End, 0);
	
	return 1;
}

STATIC ULONG MailTreelist_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	if (data->status_new) DoMethod(obj, MUIM_NList_DeleteImage, data->status_new);
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

STATIC ASM ULONG MailTreelist_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case	OM_NEW:				return MailTreelist_New(cl,obj,(struct opSet*)msg);
		case	OM_SET:				return MailTreelist_Set(cl,obj,(struct opSet*)msg);
		case	MUIM_Setup:		return MailTreelist_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup:	return MailTreelist_Cleanup(cl,obj,msg);
    case  MUIM_DragQuery: return MailTreelist_DragQuery(cl,obj,(struct MUIP_DragDrop *)msg);
    case	MUIM_NListtree_MultiTest: return MailTreelist_MultiTest(cl,obj,(struct MUIP_NListtree_MultiTest*)msg);
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

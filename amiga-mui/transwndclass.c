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
** transwndclass.c
*/

#include <dos.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libraries/mui.h>
#include <mui/NListview_MCC.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <libraries/iffparse.h> /* MAKE_ID */

#include "parse.h"
#include "configuration.h"
#include "smintl.h"
#include "support_indep.h"
#include "debug.h"

#include "compiler.h"
#include "muistuff.h"
#include "picturebuttonclass.h"
#include "support.h"
#include "transwndclass.h"

int extract_name_from_address(char *addr, char **dest_phrase, char **dest_addr, int *more_ptr);

struct transwnd_Data
{
	Object *gauge1, /* *gauge2 ,*/ *status, *abort, *skip, *head;
	Object *mail_listview, *mail_list, *mail_group;
	Object *start;
	Object *ignore_check;
	Object *status_download;
	Object *status_trashcan;

	struct Hook construct_hook;
	struct Hook destruct_hook;
	struct Hook display_hook;

	char nobuf[32];
	char sizebuf[32];

	int mail_group_shown;

	int start_pressed;
};

#define MAILF_DELETE   (1<<0) /* mail should be deleted */
#define MAILF_DOWNLOAD (1<<1) /* mail should be downloaded */

struct mail_entry
{
	int flags;
	int no;
	int size;
	unsigned int seconds;
	char *subject;
	char *from;
};

STATIC ASM APTR mail_construct(REG(a0, struct Hook *hook), REG(a2, APTR pool), REG(a1, struct mail_entry *ent))
{
	struct mail_entry *new_ent = (struct mail_entry*)malloc(sizeof(*new_ent));
	if (new_ent)
	{
		new_ent->flags = ent->flags;
		new_ent->no = ent->no;
		new_ent->size = ent->size;
		new_ent->seconds = ent->seconds;
		new_ent->subject = mystrdup(ent->subject);
		new_ent->from = mystrdup(ent->from);
	}
	return new_ent;
}

STATIC ASM VOID mail_destruct(REG(a0, struct Hook *hook), REG(a2, APTR pool), REG(a1, struct mail_entry *ent))
{
	if (ent)
	{
		if (ent->subject) free(ent->subject);
		if (ent->from) free(ent->from);
		free(ent);
	}
}

STATIC ASM VOID mail_display(REG(a0, struct Hook *h), REG(a2, char **array), REG(a1, struct mail_entry *ent))
{
	if (ent)
	{
		struct transwnd_Data *data = (struct transwnd_Data*)h->h_Data;

		sprintf(data->nobuf,"%ld%s%s",ent->no,
						((ent->flags & MAILF_DOWNLOAD)?"\033o[1]":""),
						((ent->flags & MAILF_DELETE)?"\033o[2]":""));
		sprintf(data->sizebuf, "%ld",ent->size);
		*array++ = data->nobuf;
		*array++ = data->sizebuf;
		*array++ = ent->from;
		*array++ = ent->subject;
	} else
	{
		*array++ = _("Mail No");
		*array++ = _("Size");
		*array++ = _("From");
		*array++ = _("Subject");
	}
}

STATIC void transwnd_set_mail_flags(void **args)
{
	struct transwnd_Data *data = (struct transwnd_Data *)args[0];
	int flags = (int)args[1];
	LONG pos = MUIV_NList_NextSelected_Start;

	for (;;)
	{
		struct mail_entry *entry;
		DoMethod(data->mail_list, MUIM_NList_NextSelected, (ULONG)&pos);
		if (pos == MUIV_NList_NextSelected_End) break;
		DoMethod(data->mail_list, MUIM_NList_GetEntry, pos, (ULONG)&entry);
		entry->flags = flags;
		DoMethod(data->mail_list,MUIM_NList_Redraw,pos);
	}
}

STATIC ULONG transwnd_New(struct IClass *cl, Object *obj, struct opSet *msg)
{
	Object *gauge1,/* *gauge2,*/*status,*abort,*mail_listview, *mail_list, *mail_group, *start, *ignore, *down, *del, *downdel, *ignore_check,*all,*none, *skip;
	Object *head;

	obj = (Object *) DoSuperNew(cl, obj,
				WindowContents, VGroup,
					Child, head = TextObject,End,
					Child, RectangleObject, MUIA_Weight, 1, End,
					Child, mail_group = VGroup,
						MUIA_ShowMe, FALSE,
						Child, mail_listview = NListviewObject,
							MUIA_NListview_NList, mail_list = NListObject,
								MUIA_NList_Title, TRUE,
								MUIA_NList_Format, "P=\033r,,,",
								End,
							End,
						Child, HGroup,
							Child, ignore = MakeButton(_("Ignore")),
							Child, down = MakeButton(_("Download")),
							Child, del = MakeButton(_("Delete")),
							Child, downdel = MakeButton(_("Download & Delete")),
							Child, start = MakeButton(_("_Start")),
							End,
						Child, HGroup,
							Child, all = MakeButton(_("Select All")),
							Child, none = MakeButton(_("Select None")),
							Child, MakeLabel(_("Ignore not listed mails")),
							Child, ignore_check = MakeCheck(_("Ignore not listed mails"),FALSE),
							Child, HVSpace,
							End,
						End,
					Child, gauge1 = GaugeObject,
						GaugeFrame,
						MUIA_Gauge_InfoText, _("Waiting..."),
						MUIA_Gauge_Horiz,			TRUE,
						End,
/*					Child, gauge2 = GaugeObject,
						GaugeFrame,
						MUIA_Gauge_InfoText, _("Waiting..."),
						MUIA_Gauge_Horiz,			TRUE,
						End,*/
					Child, HGroup,
						Child, status = TextObject, TextFrame, MUIA_Text_Contents, "", MUIA_Background, MUII_TextBack, End,
						Child, skip = MakeButton(_("_Skip")),
						Child, abort = MakeButton(_("_Abort")),
						End,
					End,	
				TAG_MORE, msg->ops_AttrList);

	if (obj != NULL)
	{
		struct transwnd_Data *data = (struct transwnd_Data *) INST_DATA(cl, obj);
		data->gauge1 = gauge1;
/*		data->gauge2 = gauge2; */
		data->status = status;
		data->abort  = abort;
		data->skip = skip;
		data->head = head;
		data->mail_listview = mail_listview;
		data->mail_list = mail_list;
		data->mail_group = mail_group;
		data->start = start;
		data->ignore_check = ignore_check;
		data->status_download = PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_download", End;
		data->status_trashcan = PictureButtonObject, MUIA_PictureButton_Filename, "PROGDIR:Images/status_trashcan", End;

		init_hook_with_data(&data->construct_hook, (HOOKFUNC)mail_construct, data);
		init_hook_with_data(&data->destruct_hook, (HOOKFUNC)mail_destruct, data);
		init_hook_with_data(&data->display_hook, (HOOKFUNC)mail_display, data);

		SetAttrs(mail_list,
				MUIA_NList_ConstructHook, &data->construct_hook,
				MUIA_NList_DestructHook, &data->destruct_hook,
				MUIA_NList_DisplayHook, &data->display_hook,
				MUIA_NList_MultiSelect, MUIV_NList_MultiSelect_Default,
				TAG_DONE);

		set(skip, MUIA_Weight, 0);
		set(abort, MUIA_Weight, 0);

		DoMethod(abort, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)obj, 3, MUIM_Set, MUIA_transwnd_Aborted, TRUE);
		DoMethod(skip, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)obj, 3, MUIM_Set, MUIA_transwnd_Skipped, TRUE);
		DoMethod(start, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_WriteLong, (1<<0), (ULONG)&data->start_pressed);
		DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)obj, 3, MUIM_Set, MUIA_transwnd_Aborted, TRUE);
		DoMethod(ignore, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 5, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)transwnd_set_mail_flags, (ULONG)data, 0);
		DoMethod(down, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 5, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)transwnd_set_mail_flags, (ULONG)data, MAILF_DOWNLOAD);
		DoMethod(del, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 5, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)transwnd_set_mail_flags, (ULONG)data, MAILF_DELETE);
		DoMethod(downdel, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 5, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)transwnd_set_mail_flags, (ULONG)data, MAILF_DOWNLOAD|MAILF_DELETE);
		DoMethod(all,MUIM_Notify,MUIA_Pressed, FALSE, (ULONG)mail_list, 4, MUIM_NList_Select, MUIV_NList_Select_All, MUIV_NList_Select_On, NULL);
		DoMethod(none,MUIM_Notify,MUIA_Pressed, FALSE, (ULONG)mail_list, 4, MUIM_NList_Select, MUIV_NList_Select_All, MUIV_NList_Select_Off, NULL);
	}

	return((ULONG) obj);
}

STATIC VOID transwnd_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct transwnd_Data *data = (struct transwnd_Data *) INST_DATA(cl, obj);

	DoMethod(data->mail_list, MUIM_NList_UseImage, NULL, MUIV_NList_UseImage_All, 0);
	if (data->status_download) MUI_DisposeObject(data->status_download);
	if (data->status_trashcan) MUI_DisposeObject(data->status_trashcan);

	DoSuperMethodA(cl, obj, msg);
}

STATIC ULONG transwnd_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	int close = 0;
	ULONG rc;
	struct transwnd_Data *data;
	struct TagItem *tags, *tag;

	char *gauge1_str = NULL;
	int gauge1_max = -1;
	int gauge1_val = -1;
	int gauge1_div = 1;

	data = (struct transwnd_Data *) INST_DATA(cl, obj);
		
	for (tags = msg->ops_AttrList; tag = NextTagItem(&tags);)
	{
		switch (tag->ti_Tag)
		{
			case MUIA_transwnd_Status:	
				set(data->status, MUIA_Text_Contents, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge1_Str:	
				gauge1_str = (char*)tag->ti_Data;
				break;
				
			case MUIA_transwnd_Gauge1_Max:
				gauge1_max = tag->ti_Data;
				/* 16bit only */
				while (gauge1_max > 65535)
				{
					gauge1_max >>= 1;
					gauge1_div <<= 1;
				}
				SM_DEBUGF(20,("Gauge_Max: real=%ld mui=%ld div=%ld\n",tag->ti_Data, gauge1_max, gauge1_div));
				break;
				
			case MUIA_transwnd_Gauge1_Val:	
				gauge1_val = tag->ti_Data;
				break;

			case MUIA_transwnd_Head:
				set(data->head,MUIA_Text_Contents, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge2_Str:	
/*				set(data->gauge2, MUIA_Gauge_InfoText, tag->ti_Data); */
				break;
				
			case MUIA_transwnd_Gauge2_Max:
/*				set(data->gauge2, MUIA_Gauge_Max, tag->ti_Data); */
				break;
				
			case MUIA_transwnd_Gauge2_Val:	
/*				set(data->gauge2, MUIA_Gauge_Current, tag->ti_Data); */
				break;

			case MUIA_transwnd_QuietList:
				set(data->mail_list, MUIA_NList_Quiet, tag->ti_Data);
				break;

			case MUIA_Window_Open:
				if (!tag->ti_Data) close = 1;
				break;
		}
	}

	if (gauge1_str || gauge1_max != -1 || gauge1_val != -1)
	{
		SetAttrs(data->gauge1,
					gauge1_str==NULL?TAG_IGNORE:MUIA_Gauge_InfoText, gauge1_str,
					gauge1_max==-1?TAG_IGNORE:MUIA_Gauge_Max, gauge1_max,
					gauge1_max==-1?TAG_IGNORE:MUIA_Gauge_Divide, gauge1_div==1?0:gauge1_div,
					gauge1_val==-1?TAG_IGNORE:MUIA_Gauge_Current, gauge1_val,
					TAG_DONE);
	}

	rc = DoSuperMethodA(cl, obj, (Msg)msg);
	if (close)
	{
		if (data->mail_group_shown)
		{
			set(data->mail_group, MUIA_ShowMe, FALSE);
			data->mail_group_shown = 0;
		}
		
		DoMethod(data->mail_list, MUIM_NList_Clear);
		SetAttrs(obj,
/*			MUIA_Window_Title,"SimpleMail", */
			MUIA_transwnd_Status, _("Waiting..."),
			MUIA_transwnd_Gauge1_Str, _("Waiting..."),
			MUIA_transwnd_Gauge1_Max, 1,
			MUIA_transwnd_Gauge1_Val, 0,
			MUIA_transwnd_Gauge2_Str, _("Waiting..."),
			MUIA_transwnd_Gauge2_Max, 1,
			MUIA_transwnd_Gauge2_Val, 0,
			TAG_DONE);
	}
	return rc;
}

STATIC ULONG transwnd_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct transwnd_Data *data = (struct transwnd_Data *)INST_DATA(cl, obj);
	ULONG *store = ((struct opGet *)msg)->opg_Storage;

	switch (msg->opg_AttrID)
	{
/*
		case MUIA_transwnd_Aborted:
			*store = (ULONG) (DoMethod(App, MUIM_Application_NewInput, 0) == MUIV_transwnd_Aborted);
			return(TRUE);
		break;*/
		case	MUIA_transwnd_StartPressed:
					*store = data->start_pressed;
					data->start_pressed = 0;
					break;
	}

	return DoSuperMethodA(cl, obj, (Msg)msg);
}

STATIC ULONG transwnd_InsertMailSize (struct IClass *cl, Object *obj, struct MUIP_transwnd_InsertMailSize *msg)
{
	struct transwnd_Data *data = (struct transwnd_Data *) INST_DATA(cl, obj);
	struct mail_entry ent;
	ent.no = msg->Num;
	ent.flags = msg->Flags;
	ent.size = msg->Size;
	ent.subject = NULL;
	ent.from = NULL;

	if (!data->mail_group_shown)
	{
		DoMethod(data->mail_list, MUIM_NList_UseImage, NULL, MUIV_NList_UseImage_All, 0);
		DoMethod(data->mail_list, MUIM_NList_UseImage, (ULONG)data->status_download, 1, 0);
		DoMethod(data->mail_list, MUIM_NList_UseImage, (ULONG)data->status_trashcan, 2, 0);
		set(data->mail_group, MUIA_ShowMe, TRUE);
		data->mail_group_shown = 1;
	}
	DoMethod(data->mail_list, MUIM_NList_InsertSingle, (ULONG)&ent,  MUIV_NList_Insert_Bottom);

	return 0;
}

STATIC ULONG transwnd_InsertMailInfo (struct IClass *cl, Object *obj, struct MUIP_transwnd_InsertMailInfo *msg)
{
	struct transwnd_Data *data = (struct transwnd_Data *) INST_DATA(cl, obj);
	int i;
	for (i=0;i<xget(data->mail_list, MUIA_NList_Entries);i++)
	{
		struct mail_entry *entry;
		DoMethod(data->mail_list, MUIM_NList_GetEntry, i, (ULONG)&entry);
		if (entry->no == msg->Num)
		{
			char *utf8phrase, *utf8subject, *phrase, *addr;
			int day,month,year,hour,min,sec,gmt;
			int more;

			extract_name_from_address(msg->From, &utf8phrase, &addr, &more);
			phrase = utf8tostrcreate(utf8phrase,user.config.default_codeset);
			if (phrase)
			{
				entry->from = phrase;
				free(addr);
			} else entry->from = addr;
			free(utf8phrase);

			parse_date(msg->Date,&day,&month,&year,&hour,&min,&sec,&gmt);
			entry->seconds = sm_get_seconds(day,month,year) + (hour*60+min)*60 + sec - (gmt - sm_get_gmt_offset())*60;

			if (msg->Subject)
			{
				parse_text_string(msg->Subject, (utf8 **)&utf8subject);
				entry->subject = utf8tostrcreate(utf8subject,user.config.default_codeset);
				free(utf8subject);
			}

			DoMethod(data->mail_list,MUIM_NList_Redraw,i);
			return NULL;
		}
	}

	return 0;
}

STATIC ULONG transwnd_GetMailFlags (struct IClass *cl, Object *obj, struct MUIP_transwnd_GetMailFlags *msg)
{
	struct transwnd_Data *data = (struct transwnd_Data *) INST_DATA(cl, obj);
	int i;
	for (i=0;i<xget(data->mail_list, MUIA_NList_Entries);i++)
	{
		struct mail_entry *entry;
		DoMethod(data->mail_list, MUIM_NList_GetEntry, i, (ULONG)&entry);
		if (entry->no == msg->Num) return (ULONG)entry->flags;
	}
	return (ULONG)-1;
}

STATIC ULONG transwnd_SetMailFlags (struct IClass *cl, Object *obj, struct MUIP_transwnd_SetMailFlags *msg)
{
	struct transwnd_Data *data = (struct transwnd_Data *) INST_DATA(cl, obj);
	int i;
	for (i=0;i<xget(data->mail_list, MUIA_NList_Entries);i++)
	{
		struct mail_entry *entry;
		DoMethod(data->mail_list, MUIM_NList_GetEntry, i, (ULONG)&entry);
		if (entry->no == msg->Num)
		{
			entry->flags = msg->Flags;
			DoMethod(data->mail_list,MUIM_NList_Redraw,i);
			break;
		}
	}
	return 0;
}

STATIC ULONG transwnd_Clear (struct IClass *cl, Object *obj, Msg msg)
{
	struct transwnd_Data *data = (struct transwnd_Data *) INST_DATA(cl, obj);
	DoMethod(data->mail_list, MUIM_NList_Clear);
	return 0;
}

STATIC ULONG transwnd_Wait (struct IClass *cl, Object *obj, Msg msg)
{
	extern void loop(void);
	struct transwnd_Data *data = (struct transwnd_Data *) INST_DATA(cl, obj);
	ULONG start = 0;

	/* Kill the orginal notifies */
	DoMethod(data->abort, MUIM_KillNotify, MUIA_Pressed);
	DoMethod(obj, MUIM_KillNotify, MUIA_Window_CloseRequest);

	/* Set the new notifies */
	DoMethod(data->start, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
	DoMethod(data->start, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_WriteLong, (1<<0), (ULONG)&start);
	DoMethod(data->abort, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
	DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

	loop();

	/* Kill all new notifies */
	DoMethod(obj, MUIM_KillNotify, MUIA_Window_CloseRequest);
	DoMethod(data->abort, MUIM_KillNotify, MUIA_Pressed);
	DoMethod(data->start, MUIM_KillNotify, MUIA_Pressed);
	DoMethod(data->start, MUIM_KillNotify, MUIA_Pressed);

	/* Restore the original notifies */
	DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)obj, 3, MUIM_Set, MUIA_transwnd_Aborted, TRUE);
	DoMethod(data->abort, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)obj, 3, MUIM_Set, MUIA_transwnd_Aborted, TRUE);

	if (start && xget(data->ignore_check,MUIA_Selected))
		start |= (1<<1);

	data->start_pressed = 0;

	return start;
}

STATIC BOOPSI_DISPATCHER(ULONG, transwnd_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case OM_NEW: return(transwnd_New		(cl, obj, (struct opSet*) msg));
		case OM_DISPOSE: transwnd_Dispose	(cl, obj,  msg);return 0;
		case OM_SET: return(transwnd_Set		(cl, obj, (struct opSet*) msg));
		case OM_GET: return(transwnd_Get		(cl, obj, (struct opGet*) msg));
		case MUIM_transwnd_InsertMailSize: return transwnd_InsertMailSize (cl, obj, (struct MUIP_transwnd_InsertMailSize*)msg);
		case MUIM_transwnd_InsertMailInfo: return transwnd_InsertMailInfo (cl, obj, (struct MUIP_transwnd_InsertMailInfo*)msg);
		case MUIM_transwnd_GetMailFlags: return transwnd_GetMailFlags (cl, obj, (struct MUIP_transwnd_GetMailFlags*)msg);
		case MUIM_transwnd_SetMailFlags: return transwnd_SetMailFlags (cl, obj, (struct MUIP_transwnd_SetMailFlags*)msg);
		case MUIM_transwnd_Clear: return transwnd_Clear (cl, obj, msg);
		case MUIM_transwnd_Wait: return transwnd_Wait (cl, obj, msg);
	}
	
	return(DoSuperMethodA(cl, obj, msg));
}

struct MUI_CustomClass *CL_transwnd;

int create_transwnd_class(VOID)
{
	SM_ENTER;
	if ((CL_transwnd = CreateMCC(MUIC_Window, NULL, sizeof(struct transwnd_Data), transwnd_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_transwnd: 0x%lx\n",CL_transwnd));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_transwnd\n",CL_transwnd));
	SM_RETURN(0,"%ld");
}

VOID delete_transwnd_class(VOID)
{
	SM_ENTER;
	if (CL_transwnd)
	{
		if (MUI_DeleteCustomClass(CL_transwnd))
		{
			SM_DEBUGF(15,("Deleted CL_transwnd: 0x%lx\n",CL_transwnd));
			CL_transwnd = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_transwnd: 0x%lx\n",CL_transwnd));
		}
	}
	SM_LEAVE;
}

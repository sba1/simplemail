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

#include "support.h"

#include "compiler.h"
#include "muistuff.h"
#include "transwndclass.h"

struct transwnd_Data
{
	Object *gauge1, *gauge2, *status, *abort;
	Object *mail_listview, *mail_list, *mail_group;
	struct MyHook construct_hook;
	struct MyHook destruct_hook;
	struct MyHook display_hook;

	char nobuf[32];
	char sizebuf[32];
};

struct mail_entry
{
	int no;
	int size;
	char *subject;
	char *from;
};

STATIC ASM APTR mail_construct(register __a2 APTR pool, register __a1 struct mail_entry *ent)
{
	struct mail_entry *new_ent = (struct mail_entry*)malloc(sizeof(*new_ent));
	if (new_ent)
	{
		new_ent->no = ent->no;
		new_ent->size = ent->size;
		new_ent->subject = mystrdup(ent->subject);
		new_ent->from = mystrdup(ent->from);
	}
	return new_ent;
}

STATIC ASM VOID mail_destruct( register __a2 APTR pool, register __a1 struct mail_entry *ent)
{
	if (ent)
	{
		if (ent->subject) free(ent->subject);
		if (ent->from) free(ent->from);
		free(ent);
	}
}

STATIC ASM VOID mail_display(register __a0 struct Hook *h, register __a2 char **array, register __a1 struct mail_entry *ent)
{
	if (ent)
	{
		struct transwnd_Data *data = (struct transwnd_Data*)h->h_Data;
		sprintf(data->nobuf,"%ld",ent->no);
		sprintf(data->sizebuf, "%ld",ent->size);
		*array++ = data->nobuf;
		*array++ = ent->from;
		*array++ = ent->subject;
		*array++ = data->sizebuf;
	} else
	{
		*array++ = "Mail No";
		*array++ = "Author";
		*array++ = "Subject";
		*array++ = "Size";
	}
}

STATIC ULONG transwnd_New(struct IClass *cl, Object *obj, struct opSet *msg)
{
	Object *gauge1,*gauge2,*status,*abort,*mail_listview, *mail_list, *mail_group;
	
	obj = (Object *) DoSuperNew(cl, obj,
				WindowContents, VGroup,
					Child, mail_group = VGroup,
						Child, mail_listview = NListviewObject,
							MUIA_NListview_NList, mail_list = NListObject,
								MUIA_NList_Title, TRUE,
								MUIA_NList_Format, ",,,",
								End,
							End,
						End,
					Child, gauge1 = GaugeObject,
						GaugeFrame,
						MUIA_Gauge_InfoText,		"Mail 0/0",
						MUIA_Gauge_Horiz,			TRUE,
						End,
					Child, gauge2 = GaugeObject,
						GaugeFrame,
						MUIA_Gauge_InfoText,		"0/0 bytes",
						MUIA_Gauge_Horiz,			TRUE,
						End,
					Child, HGroup,
						Child, status = TextObject, TextFrame,End,
						Child, abort = MakeButton("_Abort"),
						End,
					End,	
				TAG_MORE, msg->ops_AttrList);

	if (obj != NULL)
	{
		struct transwnd_Data *data = (struct transwnd_Data *) INST_DATA(cl, obj);
		data->gauge1 = gauge1;
		data->gauge2 = gauge2;
		data->status = status;
		data->abort  = abort;
		data->mail_listview = mail_listview;
		data->mail_list = mail_list;
		data->mail_group = mail_group;

		init_myhook(&data->construct_hook, (HOOKFUNC)mail_construct, data);
		init_myhook(&data->destruct_hook, (HOOKFUNC)mail_destruct, data);
		init_myhook(&data->display_hook, (HOOKFUNC)mail_display, data);

		set(mail_group, MUIA_ShowMe, FALSE);

		SetAttrs(mail_list,
				MUIA_NList_ConstructHook, &data->construct_hook,
				MUIA_NList_DestructHook, &data->destruct_hook,
				MUIA_NList_DisplayHook, &data->display_hook,
				TAG_DONE);

		set(abort, MUIA_Weight, 0);

		DoMethod(abort, MUIM_Notify, MUIA_Pressed, FALSE, obj, 3, MUIM_Set, MUIA_transwnd_Aborted, TRUE);
	}

	return((ULONG) obj);
}

STATIC VOID transwnd_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	DoSuperMethodA(cl, obj, msg);
}

STATIC ULONG transwnd_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	int close = 0;
	ULONG rc;
	struct transwnd_Data *data;
	struct TagItem *tags, *tag;
	
	data = (struct transwnd_Data *) INST_DATA(cl, obj);
		
	for (tags = msg->ops_AttrList; tag = NextTagItem(&tags);)
	{
		switch (tag->ti_Tag)
		{
			case MUIA_transwnd_Status:	
				set(data->status, MUIA_Text_Contents, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge1_Str:	
				set(data->gauge1, MUIA_Gauge_InfoText, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge1_Max:
				set(data->gauge1, MUIA_Gauge_Max, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge1_Val:	
				set(data->gauge1, MUIA_Gauge_Current, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge2_Str:	
				set(data->gauge2, MUIA_Gauge_InfoText, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge2_Max:
				set(data->gauge2, MUIA_Gauge_Max, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge2_Val:	
				set(data->gauge2, MUIA_Gauge_Current, tag->ti_Data);
				break;

			case MUIA_Window_Open:
				if (!tag->ti_Data) close = 1;
				break;
		}
	}
	
	rc = DoSuperMethodA(cl, obj, (Msg)msg);
	if (close) DoMethod(data->mail_list, MUIM_NList_Clear);
	return rc;
}

STATIC ULONG transwnd_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	ULONG *store = ((struct opGet *)msg)->opg_Storage;
	
/*
	switch (msg->opg_AttrID)
	{
		case MUIA_transwnd_Aborted:
			*store = (ULONG) (DoMethod(App, MUIM_Application_NewInput, 0) == MUIV_transwnd_Aborted);
			return(TRUE);
		break;
	}
*/
	return(DoSuperMethodA(cl, obj, (Msg)msg));
}

STATIC ULONG transwnd_InsertMailSize (struct IClass *cl, Object *obj, struct MUIP_transwnd_InsertMailSize *msg)
{
	struct transwnd_Data *data = (struct transwnd_Data *) INST_DATA(cl, obj);
	struct mail_entry ent;
	ent.no = msg->Num;
	ent.size = msg->Size;
	ent.subject = NULL;
	ent.from = NULL;
	DoMethod(data->mail_list, MUIM_NList_InsertSingle, &ent,  MUIV_NList_Insert_Bottom);
	set(data->mail_group, MUIA_ShowMe, TRUE);

	return NULL;
}

STATIC ULONG transwnd_InsertMailInfo (struct IClass *cl, Object *obj, struct MUIP_transwnd_InsertMailInfo *msg)
{
	return NULL;
}


STATIC ASM ULONG transwnd_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case OM_NEW: return(transwnd_New		(cl, obj, (struct opSet*) msg));
		case OM_DISPOSE: transwnd_Dispose	(cl, obj,  msg);return 0;
		case OM_SET: return(transwnd_Set		(cl, obj, (struct opSet*) msg));
		case OM_GET: return(transwnd_Get		(cl, obj, (struct opGet*) msg));
		case MUIM_transwnd_InsertMailSize: return transwnd_InsertMailSize (cl, obj, (struct MUIP_transwnd_InsertMailSize*)msg);
		case MUIM_transwnd_InsertMailInfo: return transwnd_InsertMailInfo (cl, obj, (struct MUIP_transwnd_InsertMailInfo*)msg);
	}
	
	return(DoSuperMethodA(cl, obj, msg));
}

struct MUI_CustomClass *CL_transwnd;

int create_transwnd_class(VOID)
{
	int rc;
	
	rc = FALSE;
	
	CL_transwnd = MUI_CreateCustomClass(NULL, MUIC_Window, NULL, sizeof(struct transwnd_Data), transwnd_Dispatcher);
	if(CL_transwnd != NULL)
	{
		CL_transwnd->mcc_Class->cl_UserData = getreg(REG_A4);
		rc = TRUE;
	}
	
	return(rc);
}

VOID delete_transwnd_class(VOID)
{
	if(CL_transwnd != NULL)
	{
		MUI_DeleteCustomClass(CL_transwnd);
	}	
}
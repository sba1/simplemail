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
** addressstringclass.c
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libraries/mui.h>
#include <mui/Betterstring_MCC.h>
#include <mui/NListview_MCC.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "addressbook.h"
#include "codesets.h"
#include "debug.h"

#include "addressstringclass.h"
#include "addressentrylistclass.h"
#include "amigasupport.h"
#include "compiler.h"
#include "muistuff.h"
#include "utf8stringclass.h"

/******************************************************************
 Returns a malloced() sting for the address start (this what should
 be completed)
*******************************************************************/
static char *get_address_start(char *contents, int pos)
{
	char *buf;
	int start_pos = pos;

	if (start_pos && contents[start_pos] == ',')
		start_pos--;

	while (start_pos)
	{
		if (contents[start_pos] == ',')
		{
			start_pos++;
			break;
		}
		start_pos--;
	}

	while (start_pos < pos && contents[start_pos]==' ')
		start_pos++;

	buf = malloc(pos - start_pos + 1);
	if (!buf) return NULL;
	strncpy(buf,&contents[start_pos],pos - start_pos);
	buf[pos-start_pos]=0;

	return buf;
}

/* --------------------------------- */

struct MatchWindow_Data
{
	int dummy;
	Object *str;
	Object *list;
};

struct MUI_CustomClass *CL_MatchWindow;

STATIC VOID MatchWindow_NewActive(void **msg)
{
	struct MatchWindow_Data *data = (struct MatchWindow_Data *)msg[0];
	struct addressbook_entry_new *entry;

	DoMethod(data->list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &entry);

	if (entry)
	{
		char *contents = (char*)xget(data->str,MUIA_UTF8String_Contents);
		int buf_pos =  utf8realpos(contents,xget(data->str,MUIA_String_BufferPos));
		char *addr_start;
		char *complete;

		if ((addr_start = get_address_start(contents, buf_pos)))
		{
			if ((complete = addressbook_get_entry_completing_part(entry, addr_start, NULL)))
			{
				DoMethod(data->str, MUIM_AddressString_Complete, complete);
				/* complete must be not freed */
			}
			free(addr_start);
		}
	}
}

STATIC ULONG MatchWindow_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	Object *list,*str;
	struct MatchWindow_Data *data;

	if (!(str = (Object*)GetTagData(MUIA_MatchWindow_String, (ULONG)NULL, msg->ops_AttrList)))
		return 0;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_Window_Borderless, TRUE,
					MUIA_Window_CloseGadget, FALSE,
					MUIA_Window_DepthGadget, FALSE,
					MUIA_Window_SizeGadget, FALSE,
					MUIA_Window_Activate, FALSE,
					MUIA_Window_DragBar, FALSE,
					WindowContents, VGroup,
						InnerSpacing(0,0),
						Child, NListviewObject,
							MUIA_NListview_NList, list = AddressEntryListObject,
								MUIA_AddressEntryList_Type, MUIV_AddressEntryList_Type_Match,
								End,
							End,
						End,
					TAG_MORE, msg->ops_AttrList)))
		return 0;

	data = (struct MatchWindow_Data*)INST_DATA(cl,obj);
	data->str = str;
	data->list = list;

	DoMethod(data->list, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, App, 4, MUIM_CallHook, &hook_standard, MatchWindow_NewActive, data);
	return (ULONG)obj;
}

STATIC ULONG MatchWindow_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct MatchWindow_Data *data = (struct MatchWindow_Data*)INST_DATA(cl,obj);
	if (msg->opg_AttrID == MUIA_MatchWindow_Entries)
	{
		*msg->opg_Storage = xget(data->list, MUIA_NList_Entries);
		return 1;
	}
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG MatchWindow_Refresh(struct IClass *cl, Object *obj, struct MUIP_AddressEntryList_Refresh *msg)
{
	struct MatchWindow_Data *data = (struct MatchWindow_Data*)INST_DATA(cl,obj);
	return DoMethod(data->list, MUIM_AddressEntryList_Refresh, msg->pattern);
}

STATIC ULONG MatchWindow_Up(struct IClass *cl, Object *obj, Msg msg)
{
	struct MatchWindow_Data *data = (struct MatchWindow_Data*)INST_DATA(cl,obj);
	set(data->list,MUIA_NList_Active, MUIV_NList_Active_Up);
	return 0;
}

STATIC ULONG MatchWindow_Down(struct IClass *cl, Object *obj, Msg msg)
{
	struct MatchWindow_Data *data = (struct MatchWindow_Data*)INST_DATA(cl,obj);
	set(data->list,MUIA_NList_Active, MUIV_NList_Active_Down);
	return 0;
}

STATIC BOOPSI_DISPATCHER(ULONG, MatchWindow_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return MatchWindow_New(cl,obj,(struct opSet*)msg);
		case	OM_GET: return MatchWindow_Get(cl,obj,(struct opGet*)msg);
		case  MUIM_AddressEntryList_Refresh: return MatchWindow_Refresh(cl,obj,(struct MUIP_AddressEntryList_Refresh*)msg);
		case  MUIM_MatchWindow_Up:   return MatchWindow_Up(cl,obj,msg);
		case	MUIM_MatchWindow_Down: return MatchWindow_Down(cl,obj,msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}


/*-------------------------------------*/

struct AddressString_Data
{
   struct MUI_EventHandlerNode ehnode;
   Object *match_wnd;
};

STATIC VOID AddressString_OpenList(struct IClass *cl, Object *obj);
STATIC VOID AddressString_CloseList(struct IClass *cl, Object *obj);
STATIC ULONG AddressString_UpdateList(struct IClass *cl, Object *obj);

STATIC ULONG AddressString_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct AddressString_Data *data;

	if (!(obj=(Object *)DoSuperMethodA(cl,obj,(Msg)msg)))
		return 0;

	data = (struct AddressString_Data*)INST_DATA(cl,obj);
	return (ULONG)obj;
}

STATIC ULONG AddressString_Setup(struct IClass *cl, Object *obj,struct MUIP_Setup *msg)
{
	struct AddressString_Data *data = (struct AddressString_Data*)INST_DATA(cl,obj);
	if (!DoSuperMethodA(cl, obj, (Msg)msg)) return FALSE;
	data->ehnode.ehn_Priority = 1;
	data->ehnode.ehn_Flags    = 0;
	data->ehnode.ehn_Object   = obj;
	data->ehnode.ehn_Class    = cl;
	data->ehnode.ehn_Events   = IDCMP_RAWKEY|IDCMP_CHANGEWINDOW;
	return 1;
}

STATIC ULONG AddressString_Cleanup(struct IClass *cl, Object *obj, struct MUIP_Cleanup *msg)
{
	return DoSuperMethodA(cl, obj, (Msg)msg);
}

STATIC ULONG AddressString_GoActive(struct IClass *cl, Object *obj,Msg msg)
{
	struct AddressString_Data *data = (struct AddressString_Data*)INST_DATA(cl,obj);
	DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->ehnode);
	AddressString_OpenList(cl,obj);
	DoMethod(obj, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, obj, 1, MUIM_AddressString_UpdateList);
	return DoSuperMethodA(cl, obj, msg);
}

STATIC ULONG AddressString_GoInactive(struct IClass *cl, Object *obj,Msg msg)
{
	struct AddressString_Data *data = (struct AddressString_Data*)INST_DATA(cl,obj);
	DoMethod(obj, MUIM_KillNotify, MUIA_String_Contents);
	AddressString_CloseList(cl,obj);
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehnode);
	set(obj, MUIA_BetterString_SelectSize, 0);
	return DoSuperMethodA(cl, obj, msg);
}

STATIC ULONG AddressString_HandleEvent(struct IClass *cl, Object *obj, struct MUIP_HandleEvent *msg)
{
	struct AddressString_Data *data = (struct AddressString_Data*)INST_DATA(cl,obj);
	if (msg->imsg && msg->imsg->Class == IDCMP_RAWKEY)
	{
		UWORD code;
		if (msg->imsg->Code == CURSORDOWN && data->match_wnd)
		{
			DoMethod(data->match_wnd,MUIM_MatchWindow_Down);
			return MUI_EventHandlerRC_Eat;
		} else
		if (msg->imsg->Code == CURSORUP && data->match_wnd)
		{
			DoMethod(data->match_wnd,MUIM_MatchWindow_Up);
			return MUI_EventHandlerRC_Eat;
		}

		code = ConvertKey(msg->imsg);

    if (code == ',')
    {
			int selectsize = xget(obj,MUIA_BetterString_SelectSize);
			int buf_pos = xget(obj,MUIA_String_BufferPos);

			if (selectsize)
			{
				SetAttrs(obj,MUIA_String_BufferPos, buf_pos + selectsize,
										 MUIA_BetterString_SelectSize, 0,
										 TAG_DONE);
			}
    }

		if (((code >= 32 && code <= 126) || code >= 160) && !(msg->imsg->Qualifier & IEQUALIFIER_RCOMMAND))
		{
			char *contents;

			DoSuperMethodA(cl, obj, (Msg)msg);

			if (code != ',')
			{
				contents = (char*)xget(obj, MUIA_UTF8String_Contents);

				if (strlen(contents))
				{
					int buf_pos = utf8realpos(contents,xget(obj,MUIA_String_BufferPos));
					char *addr_start;

					if ((addr_start = get_address_start(contents,buf_pos)))
					{
						char *completed;

						if ((completed = addressbook_complete_address(addr_start)))
						{
							DoMethod(obj, MUIM_AddressString_Complete, completed);
						}
						free(addr_start);
					}
				}
			}
			return MUI_EventHandlerRC_Eat;
		}
	} else if (msg->imsg && msg->imsg->Class == IDCMP_CHANGEWINDOW)
	{
		if (data->match_wnd)
		{
			if (xget(data->match_wnd, MUIA_Window_Open))
			{
				struct Window *match_wnd = (struct Window*)xget(data->match_wnd, MUIA_Window_Window);
				struct Window *wnd = (struct Window*)xget(_win(obj),MUIA_Window_Window);
				ChangeWindowBox(match_wnd, wnd->LeftEdge + _left(obj), wnd->TopEdge + _bottom(obj) + 1, _width(obj), match_wnd->Height);
			}
		}
	}
	return 0;
}

/******************************************************************
 MUIM_DragQuery. Accept dragged objects from Address Entry List
*******************************************************************/
STATIC ULONG AddressString_DragQuery(struct IClass *cl, Object *obj, struct MUIP_DragQuery *msg)
{
	if (OCLASS(msg->obj) == CL_AddressEntryList->mcc_Class) return MUIV_DragQuery_Accept;
	return MUIV_DragQuery_Refuse;
}

STATIC ULONG AddressString_DragDrop(struct IClass *cl, Object *obj, struct MUIP_DragDrop *msg)
{
	struct addressbook_entry_new *entry;

	DoMethod(msg->obj, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &entry);

	if (entry)
	{
		char *append;
		append = entry->alias;
		if (!append || !*append)
		{
			append = entry->realname;
			if (!append || !*append)
			{
				if (entry->email_array)
				{
						append = entry->email_array[0];
				} else append = NULL;
			}
		}

		if (append)
		{
			char *text = (char*)xget(obj,MUIA_String_Contents);
			char *newtext;
			int len;

			if (!text) text = "";
			len = strlen(text);

			if ((newtext = (char*)malloc(strlen(text)+strlen(append)+8)))
			{
				strcpy(newtext,text);
				if (len && text[len-1] != ',')
				{
					newtext[len++]=',';
					newtext[len]=0;
				}
				strcat(newtext,append);
				set(obj,MUIA_UTF8String_Contents,newtext);
				free(newtext);
			}
		}
	}
	return 0;
}

STATIC ULONG AddressString_Complete(struct IClass *cl, Object *obj, struct MUIP_AddressString_Complete *msg)
{
	struct AddressString_Data *data = (struct AddressString_Data*)INST_DATA(cl,obj);
	char *contents, *newcontents;
	int buf_pos_unreal, buf_pos, len;
	char *completed = msg->text;

	/* First clear the old completed string */
	DoMethod(obj, MUIM_BetterString_ClearSelected);

	contents = (char*)xget(obj, MUIA_UTF8String_Contents);
	buf_pos_unreal = xget(obj, MUIA_String_BufferPos);
	buf_pos = utf8realpos(contents,buf_pos_unreal);

	len = strlen(contents) + strlen(completed) + 1;

	if ((newcontents = (char*)malloc(len+100)))
	{
		strncpy(newcontents, contents, buf_pos);
		strcpy(&newcontents[buf_pos], completed);
		strcat(newcontents,&contents[buf_pos]);

		SetAttrs(obj,
					MUIA_UTF8String_Contents, newcontents,
					MUIA_String_BufferPos, buf_pos_unreal,
					MUIA_BetterString_SelectSize, utf8len(completed),
					MUIA_NoNotify, TRUE,
					TAG_DONE);
		free(newcontents);

		/* Open the window if not open and if the list contains some entries */
		if (data->match_wnd)
		{
			int entries = xget(data->match_wnd, MUIA_MatchWindow_Entries);
			if (entries > 1 && !xget(data->match_wnd,MUIA_Window_Open))
				set(data->match_wnd,MUIA_Window_Open,TRUE);
		}
	}
	return 0;
}

STATIC VOID AddressString_CloseList(struct IClass *cl, Object *obj)
{
	struct AddressString_Data *data = (struct AddressString_Data*)INST_DATA(cl,obj);
	if (data->match_wnd)
	{
		set(data->match_wnd, MUIA_Window_Open, FALSE);
		DoMethod(_app(obj),OM_REMMEMBER, data->match_wnd);
		MUI_DisposeObject(data->match_wnd);
		data->match_wnd = NULL;
	}
}

STATIC VOID AddressString_OpenList(struct IClass *cl, Object *obj)
{
	struct AddressString_Data *data = (struct AddressString_Data*)INST_DATA(cl,obj);
	struct Window *wnd = (struct Window *)xget(_win(obj),MUIA_Window_Window);

	if (data->match_wnd) AddressString_CloseList(cl,obj);

	if ((data->match_wnd = NewObject(CL_MatchWindow->mcc_Class, NULL,
				MUIA_MatchWindow_String, obj,
				MUIA_Window_LeftEdge, wnd->LeftEdge + _left(obj),
				MUIA_Window_TopEdge, wnd->TopEdge + _top(obj) + _height(obj),
				MUIA_Window_Width, _width(obj),
				TAG_DONE)))
	{
		DoMethod(_app(obj),OM_ADDMEMBER, data->match_wnd);
		AddressString_UpdateList(cl,obj);
	}
}

STATIC ULONG AddressString_UpdateList(struct IClass *cl, Object *obj)
{
	struct AddressString_Data *data = (struct AddressString_Data*)INST_DATA(cl,obj);
	char *contents = (char*)xget(obj,MUIA_UTF8String_Contents);
	int buf_pos =  utf8realpos(contents,xget(obj,MUIA_String_BufferPos));
	char *addr_start;

	addr_start = get_address_start(contents, buf_pos);

	if (data->match_wnd)
	{
		int entries;
		DoMethod(data->match_wnd, MUIM_AddressEntryList_Refresh, addr_start);
		entries = xget(data->match_wnd, MUIA_MatchWindow_Entries);

		if (entries > 1)
		{
			if (!xget(data->match_wnd, MUIA_Window_Open))
			{
				if (strlen(addr_start))
					set(data->match_wnd, MUIA_Window_Open, TRUE);
			}
		} else set(data->match_wnd, MUIA_Window_Open, FALSE);
	}
	free(addr_start);
	return 0;
}


STATIC BOOPSI_DISPATCHER(ULONG, AddressString_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return AddressString_New(cl,obj,(struct opSet*)msg);
		case  MUIM_Setup: return AddressString_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case  MUIM_Cleanup: return AddressString_Cleanup(cl,obj,(struct MUIP_Cleanup*)msg);
		case	MUIM_GoActive: return AddressString_GoActive(cl,obj,msg);
		case	MUIM_GoInactive: return AddressString_GoInactive(cl,obj,msg);
		case	MUIM_HandleEvent: return AddressString_HandleEvent(cl,obj,(struct MUIP_HandleEvent*)msg);
		case	MUIM_DragQuery: return AddressString_DragQuery(cl,obj,(struct MUIP_DragQuery *)msg);
		case	MUIM_DragDrop: return AddressString_DragDrop(cl,obj,(struct MUIP_DragDrop*)msg);
		case  MUIM_AddressString_Complete: return AddressString_Complete(cl,obj,(struct MUIP_AddressString_Complete*)msg);
		case  MUIM_AddressString_UpdateList: return AddressString_UpdateList(cl,obj);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_AddressString;

int create_addressstring_class(void)
{
	SM_ENTER;
	if ((CL_MatchWindow = CreateMCC(MUIC_Window,NULL,sizeof(struct MatchWindow_Data),MatchWindow_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_MatchWindow: 0x%lx\n",CL_MatchWindow));
		if ((CL_AddressString = CreateMCC(NULL,CL_UTF8String,sizeof(struct AddressString_Data),AddressString_Dispatcher)))
		{
			SM_DEBUGF(15,("Create CL_AddressString: 0x%lx\n",CL_AddressString));
			SM_RETURN(1,"%ld");
		}
		SM_DEBUGF(5,("FAILED! Create CL_AddressString\n"));
		SM_RETURN(0,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_MatchWindow\n"));
	SM_RETURN(0,"%ld");
}

void delete_addressstring_class(void)
{
	SM_ENTER;
	if (CL_MatchWindow)
	{
		if (MUI_DeleteCustomClass(CL_MatchWindow))
		{
			SM_DEBUGF(15,("Deleted CL_MatchWindow: 0x%lx\n",CL_MatchWindow));
			CL_MatchWindow = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_MatchWindow: 0x%lx\n",CL_MatchWindow));
		}
	}

	if (CL_AddressString)
	{
		if (MUI_DeleteCustomClass(CL_AddressString))
		{
			SM_DEBUGF(15,("Deleted CL_AddressString: 0x%lx\n",CL_AddressString));
			CL_AddressString = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_AddressString: 0x%lx\n",CL_AddressString));
		}
	}
	SM_LEAVE;
}

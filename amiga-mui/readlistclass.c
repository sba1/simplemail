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
** $Id$
*/

#include <dos.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/mui.h>
#include <mui/NListview_MCC.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include "mail.h"
#include "support.h"

#include "amigasupport.h"
#include "compiler.h"
#include "muistuff.h"
#include "readlistclass.h"

struct ReadList_Data
{
	LONG color1_pen;
	LONG color2_pen;
	struct Hook display_hook;
	char preparse[16];
};

STATIC ASM VOID ReadList_Display(register __a2 Object *obj, register __a1 struct NList_DisplayMessage *msg)
{
	struct ReadList_Data *data = (struct ReadList_Data*)INST_DATA(CL_ReadList->mcc_Class,obj);
	char **array = msg->strings;
	char *str = (char*)msg->entry;

	if (str)
	{
		if (!strncmp(str,"\033mode[",6))
		{
			LONG pen;
			int mode = atoi(str+6);

			if (mode == 0) pen = data->color1_pen;
			else pen = data->color2_pen;

			*array = strchr(str,']')+1;
			sprintf(data->preparse,"\033P[%ld]",pen);
			msg->preparses[0] = data->preparse;
		}
		else *array = str;
	}
}

STATIC ULONG ReadList_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct ReadList_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_Font, MUIV_Font_Fixed,
					MUIA_NList_TypeSelect, MUIV_NList_TypeSelect_Char,
					MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
					MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
					MUIA_NList_KeepActive, TRUE,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct ReadList_Data*)INST_DATA(cl,obj);
	data->color1_pen = data->color2_pen = -1;

	init_hook(&data->display_hook, (HOOKFUNC)ReadList_Display);
	set(obj, MUIA_NList_DisplayHook2, &data->display_hook);

	return (ULONG)obj;
}

STATIC ULONG ReadList_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct ReadList_Data *data = (struct ReadList_Data*)INST_DATA(cl,obj);
	ULONG retval = DoSuperMethodA(cl,obj,(Msg)msg);

	if (retval)
	{
		data->color1_pen = ObtainBestPenA(_screen(obj)->ViewPort.ColorMap,0xffffffff,0xffffffff,0xffffffff,NULL);
		data->color2_pen = ObtainBestPenA(_screen(obj)->ViewPort.ColorMap,0xffffffff,0xffffffff,0x80000000,NULL);
	}
	return retval;
}

STATIC ULONG ReadList_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct ReadList_Data *data = (struct ReadList_Data*)INST_DATA(cl,obj);

	if (data->color1_pen != -1)
		ReleasePen(_screen(obj)->ViewPort.ColorMap,data->color1_pen);
	if (data->color2_pen != -1)
		ReleasePen(_screen(obj)->ViewPort.ColorMap,data->color2_pen);

	data->color1_pen = -1;
	data->color2_pen = -1;

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG ReadList_InsertSingle(struct IClass *cl, Object *obj, struct MUIP_NList_InsertSingle *msg)
{
	struct ReadList_Data *data = (struct ReadList_Data*)INST_DATA(cl,obj);

	char *text = (char*)msg->entry;
	char *buf;

	char *text_end = strchr(text,'\n');
	int len;

	if (text_end) len = text_end - text;
	else len = strlen(text);

	if (!len) return DoSuperMethodA(cl,obj,(Msg)msg);

	if ((buf = malloc(len+16)))
	{
		ULONG retval;
		char *src = text;
		int i = 0,mode = 0;

		while (i<len)
		{
			int c = *src++;

			if (c == '>')
			{
				if (mode == 2) mode = 3;
				else mode = 2;
			} else
			{
				if ((mode == 2 || mode == 3) && c != ' ') break;
				if (c==' ' && mode == 0) break;
			}
			i++;
		}

		if (mode == 2 || mode == 3)
		{
			int buf_len;
			sprintf(buf,"\033mode[%ld]",mode-2);
			buf_len = strlen(buf);
			strncpy(buf+buf_len,text,len);
			buf[len+buf_len]=0;
		} else
		{
			strncpy(buf,text,len);
			buf[len]=0;
		}

		retval = DoSuperMethod(cl,obj, MUIM_NList_InsertSingle, buf, msg->pos);

		free(buf);
		return retval;
	} else return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ASM ULONG ReadList_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case	OM_NEW: return ReadList_New(cl,obj,(struct opSet*)msg);
		case	MUIM_Setup: return ReadList_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup: return ReadList_Cleanup(cl,obj,msg);
		case	MUIM_NList_InsertSingle: return ReadList_InsertSingle(cl,obj,(struct MUIP_NList_InsertSingle*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_ReadList;

int create_readlist_class(void)
{
	if ((CL_ReadList = MUI_CreateCustomClass(NULL,MUIC_NList,NULL,sizeof(struct ReadList_Data),ReadList_Dispatcher)))
	{
		CL_ReadList->mcc_Class->cl_UserData = getreg(REG_A4);
		return 1;
	}
	return 0;
}

void delete_readlist_class(void)
{
	if (CL_ReadList) MUI_DeleteCustomClass(CL_ReadList);
}

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
#include <stdio.h>
#include <stdlib.h>

#include <libraries/mui.h>
#include <mui/Betterstring_MCC.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "addressbook.h"

#include "addressstringclass.h"
#include "amigasupport.h"
#include "compiler.h"
#include "muistuff.h"

struct AddressString_Data
{
   struct MUI_EventHandlerNode ehnode;
};


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
	data->ehnode.ehn_Events   = IDCMP_RAWKEY;
}

STATIC ULONG AddressString_GoActive(struct IClass *cl, Object *obj,Msg msg)
{
	struct AddressString_Data *data = (struct AddressString_Data*)INST_DATA(cl,obj);
	DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->ehnode);
	return DoSuperMethodA(cl, obj, msg);
}

STATIC ULONG AddressString_GoInactive(struct IClass *cl, Object *obj,Msg msg)
{
	struct AddressString_Data *data = (struct AddressString_Data*)INST_DATA(cl,obj);
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehnode);
	set(obj, MUIA_BetterString_SelectSize, 0);
	return DoSuperMethodA(cl, obj, msg);
}

STATIC ULONG AddressString_HandleEvent(struct IClass *cl, Object *obj, struct MUIP_HandleEvent *msg)
{
	if (msg->imsg && msg->imsg->Class == IDCMP_RAWKEY)
	{
    UWORD code = ConvertKey(msg->imsg);
		if (((code >= 32 && code <= 126) || code >= 160) && !(msg->imsg->Qualifier & IEQUALIFIER_RCOMMAND))
		{
			char *contents;
			LONG bufferpos;
			DoSuperMethodA(cl, obj, (Msg)msg);

			contents = (char*)xget(obj, MUIA_String_Contents);
			bufferpos = xget(obj, MUIA_String_BufferPos);

			if (strlen(contents))
			{
				char *comma = strrchr(contents,',');
				char *completed = NULL;

				if (comma)
				{
					while (*++comma == ' ');
					if (strlen(comma)) completed = addressbook_complete_address(contents);
				} else completed = addressbook_complete_address(contents);

				if (completed)
				{
					char *newcontents = (char*)malloc(strlen(contents)+strlen(completed)+1);
					strcpy(newcontents, contents);
					strcpy(&newcontents[bufferpos], completed);
					SetAttrs(obj,
							MUIA_String_Contents,newcontents,
							MUIA_String_BufferPos,bufferpos,
							MUIA_BetterString_SelectSize, strlen(newcontents) - bufferpos,
							TAG_DONE);
					free(newcontents);
				}
			}

			return MUI_EventHandlerRC_Eat;
		}
	}
	return 0;
}

STATIC ASM ULONG AddressString_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case	OM_NEW: return AddressString_New(cl,obj,(struct opSet*)msg);
		case  MUIM_Setup: return AddressString_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_GoActive: return AddressString_GoActive(cl,obj,msg);
		case	MUIM_GoInactive: return AddressString_GoInactive(cl,obj,msg);
		case	MUIM_HandleEvent: return AddressString_HandleEvent(cl,obj,(struct MUIP_HandleEvent*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_AddressString;

int create_addressstring_class(void)
{
	if ((CL_AddressString = MUI_CreateCustomClass(NULL,MUIC_BetterString,NULL,sizeof(struct AddressString_Data),AddressString_Dispatcher)))
	{
		CL_AddressString->mcc_Class->cl_UserData = getreg(REG_A4);
		return 1;
	}
	return 0;
}

void delete_addressstring_class(void)
{
	if (CL_AddressString) MUI_DeleteCustomClass(CL_AddressString);
}

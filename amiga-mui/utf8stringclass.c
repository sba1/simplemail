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
** utf8stringclass.c
*/

#include <dos.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libraries/mui.h>
#include <mui/Betterstring_MCC.h>
#include <mui/NListview_MCC.h>
#include <mui/NListtree_MCC.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "codesets.h"
#include "support_indep.h"

#include "utf8stringclass.h"
#include "amigasupport.h"
#include "compiler.h"
#include "muistuff.h"

struct UTF8String_Data
{
	struct codeset *codeset;
};

STATIC ULONG UTF8String_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct UTF8String_Data *data;

	if (!(obj=(Object *)DoSuperMethodA(cl,obj,(Msg)msg)))
		return 0;

	

	data = (struct UTF8String_Data*)INST_DATA(cl,obj);
	return (ULONG)obj;
}

STATIC ULONG UTF8String_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct UTF8String_Data *data = (struct UTF8String_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;

	char *new_contents = NULL;
	int contents_setted = 0;

	tstate = (struct TagItem *)msg->ops_AttrList;

	while (tag = NextTagItem (&tstate))
	{
		switch (tag->ti_Tag)
		{
			case	MUIA_UTF8String_Contents:
						new_contents = (char*)tag->ti_Data;
						contents_setted = 1;
						break;

			case	MUIA_UTF8String_Charset:
						data->codeset = codesets_find((char*)tag->ti_Data);
						break;
		}
	}

	if (contents_setted)
	{
		int len = mystrlen(new_contents);
		if (len)
		{
			char *new_cont = (char*)malloc(len+1);
			if (new_cont) utf8tostr(new_contents,new_cont,len+1,data->codeset);
			set(obj,MUIA_String_Contents,new_cont);
			free(new_cont);
		} else set(obj,MUIA_String_Contents,NULL);
	}

	if (msg->MethodID != OM_NEW)
	{
		return DoSuperMethodA(cl,obj,(Msg)msg);
	}
	return 1;
}

STATIC ULONG UTF8String_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct UTF8String_Data *data = (struct UTF8String_Data*)INST_DATA(cl,obj);
	switch (msg->opg_AttrID)
	{
		case	MUIA_UTF8String_Contents:
					break;
	}
	return DoSuperMethodA(cl,obj,(Msg)msg);
}


STATIC ASM ULONG UTF8String_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case	OM_NEW: return UTF8String_New(cl,obj,(struct opSet*)msg);
		case	OM_SET: return UTF8String_Set(cl,obj,(struct opSet*)msg);
		case  OM_GET: return UTF8String_Get(cl,obj,(struct opGet*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_UTF8String;

int create_utf8string_class(void)
{
	if ((CL_UTF8String = MUI_CreateCustomClass(NULL,MUIC_BetterString,NULL,sizeof(struct UTF8String_Data),UTF8String_Dispatcher)))
	{
		CL_UTF8String->mcc_Class->cl_UserData = getreg(REG_A4);
		return 1;
	}
	return 0;
}

void delete_utf8string_class(void)
{
	if (CL_UTF8String) MUI_DeleteCustomClass(CL_UTF8String);
}

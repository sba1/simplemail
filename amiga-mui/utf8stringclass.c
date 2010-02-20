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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libraries/mui.h>
#include <mui/BetterString_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/NListtree_mcc.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "codesets.h"
#include "configuration.h"
#include "debug.h"
#include "support_indep.h"

#include "amigasupport.h"
#include "compiler.h"
#include "muistuff.h"
#include "utf8stringclass.h"

struct UTF8String_Data
{
	struct codeset *codeset;
	char *utf8_string;
};


STATIC ULONG UTF8String_Set(struct IClass *cl, Object *obj, struct opSet *msg);

STATIC ULONG UTF8String_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
/*	struct UTF8String_Data *data;*/

	if (!(obj=(Object *)DoSuperMethodA(cl,obj,(Msg)msg)))
		return 0;

/*	data = (struct UTF8String_Data*)INST_DATA(cl,obj);*/
	UTF8String_Set(cl,obj,msg);
	return (ULONG)obj;
}

STATIC ULONG UTF8String_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct UTF8String_Data *data = (struct UTF8String_Data*)INST_DATA(cl,obj);
	free(data->utf8_string);
	return DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG UTF8String_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct UTF8String_Data *data = (struct UTF8String_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;

	char *new_contents = NULL;
	int contents_setted = 0;

	tstate = (struct TagItem *)msg->ops_AttrList;

	while ((tag = NextTagItem ((APTR)&tstate)))
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
		struct TagItem *newtags = CloneTagItems(msg->ops_AttrList);
		int len = mystrlen(new_contents);
		char *newcont = malloc(len+1);
		ULONG rc;

		if (newcont)
		{
			utf8tostr(new_contents,newcont,len+1,data->codeset?data->codeset:user.config.default_codeset);
		}

		if ((tag = FindTagItem(MUIA_UTF8String_Contents,newtags)))
		{
			tag->ti_Tag = MUIA_String_Contents;
			tag->ti_Data = (ULONG)newcont;
		}

		if (msg->MethodID != OM_NEW)
			rc = DoSuperMethod(cl,obj,msg->MethodID,newtags,NULL);
		else
		{
			set(obj,MUIA_String_Contents,newcont);
			rc = 0;
		}

		free(newcont);
		FreeTagItems(newtags);

		return rc;
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
					{
						char *contents = (char*)xget(obj,MUIA_String_Contents);
						free(data->utf8_string);
						data->utf8_string = utf8create(contents, user.config.default_codeset?user.config.default_codeset->name:NULL);
						*msg->opg_Storage = (ULONG)data->utf8_string;
					}
					break;
	}
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG UTF8String_Insert(struct IClass *cl, Object *obj, struct MUIP_BetterString_Insert *msg)
{
	struct UTF8String_Data *data = (struct UTF8String_Data*)INST_DATA(cl,obj);
	char *text;
	ULONG rc;
	LONG pos;

	if (msg->pos != MUIV_BetterString_Insert_StartOfString && msg->pos != MUIV_BetterString_Insert_EndOfString && msg->pos != MUIV_BetterString_Insert_BufferPos)
	{
		char *utf8text = (char*)xget(obj,MUIA_UTF8String_Contents);
		if (utf8text)
		{
			pos = utf8charpos(utf8text,msg->pos);
		} else pos = 0;
	} else pos = msg->pos;

	text = utf8tostrcreate(msg->text, data->codeset);
	rc = DoSuperMethod(cl, obj, MUIM_BetterString_Insert, text, pos);
	free(text);
	return rc;
}

STATIC MY_BOOPSI_DISPATCHER(ULONG, UTF8String_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return UTF8String_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE: return UTF8String_Dispose(cl, obj, msg);
		case	OM_SET: return UTF8String_Set(cl,obj,(struct opSet*)msg);
		case 	OM_GET: return UTF8String_Get(cl,obj,(struct opGet*)msg);
		case	MUIM_UTF8String_Insert: return UTF8String_Insert(cl,obj,(struct MUIP_BetterString_Insert*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_UTF8String;

int create_utf8string_class(void)
{
	SM_ENTER;
	if ((CL_UTF8String = CreateMCC(MUIC_BetterString,NULL,sizeof(struct UTF8String_Data),UTF8String_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_UTF8String: 0x%lx\n",CL_UTF8String));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_UTF8String\n"));
	SM_RETURN(0,"%ld");
}

void delete_utf8string_class(void)
{
	SM_ENTER;
	if (CL_UTF8String)
	{
		if (MUI_DeleteCustomClass(CL_UTF8String))
		{
			SM_DEBUGF(15,("Deleted CL_UTF8String: 0x%lx\n",CL_UTF8String));
			CL_UTF8String = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_UTF8String: 0x%lx\n",CL_UTF8String));
		}
	}
	SM_LEAVE;
}

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
** iconclass.c
*/

#include <dos.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/mui.h>
#include <workbench/icon.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/icon.h>
#include <proto/intuition.h>

#include "support_indep.h"

#include "amigasupport.h"
#include "compiler.h"
#include "datatypesclass.h"
#include "iconclass.h"
#include "muistuff.h"

struct Icon_Data
{
	char *type;
	char *subtype;

	void *buffer;
	int buffer_len;

	struct DiskObject *obj;

	struct MUI_EventHandlerNode ehnode;
	ULONG select_secs,select_mics;
};

STATIC ULONG Icon_Set(struct IClass *cl,Object *obj,struct opSet *msg);

STATIC ULONG Icon_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct Icon_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct Icon_Data*)INST_DATA(cl,obj);

	Icon_Set(cl,obj,msg);
	return (ULONG)obj;
}

STATIC VOID Icon_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
	if (data->type) FreeVec(data->type);
	if (data->subtype) FreeVec(data->subtype);

	DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG Icon_Set(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;
	void *new_buffer = NULL;
	int new_buffer_len = 0;

	tstate = (struct TagItem *)msg->ops_AttrList;

	while (tag = NextTagItem (&tstate))
	{
		ULONG tidata = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case	MUIA_Icon_MimeType:
						if (data->type) FreeVec(data->type);
						data->type = StrCopy((STRPTR)tidata);
						break;

			case	MUIA_Icon_MimeSubType:
						if (data->subtype) FreeVec(data->subtype);
						data->subtype = StrCopy((STRPTR)tidata);
						break;

			case	MUIA_Icon_Buffer:
						if (data->buffer) FreeVec(data->buffer);
						data->buffer = NULL;
						new_buffer = (void*)tidata;
						break;

			case	MUIA_Icon_BufferLen:
						new_buffer_len = tidata;
						break;

		}
	}
	if (msg->MethodID == OM_SET) return DoSuperMethodA(cl,obj,(Msg)msg);

	if (new_buffer_len && new_buffer)
	{
		if ((data->buffer = AllocVec(new_buffer_len,0x0)))
		{
			data->buffer_len = new_buffer_len;
			CopyMem(new_buffer,data->buffer,new_buffer_len);
		}
	}

	return 1;
}

STATIC ULONG Icon_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	if (msg->opg_AttrID == MUIA_Icon_DoubleClick)
	{
		*msg->opg_Storage = 1;
		return 1;
	}
	return DoSuperMethodA(cl,obj,(Msg)msg);
}


STATIC ULONG Icon_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
	char *def = NULL;
	char result[40];

	if (!DoSuperMethodA(cl,obj,(Msg)msg)) return 0;

	if (IconBase->lib_Version >= 44 && data->buffer && FindPort("DEFICONS"))
	{
		BPTR fh;
		char command[40];
		sprintf(command,"IDENTIFY T:SM_%x",FindTask(NULL));
		if ((fh = Open(command + 9, MODE_NEWFILE)))
		{
			Write(fh,data->buffer,data->buffer_len);
			Close(fh);

/* TODO: Enable this if we support icon caching */
#if 0
			if (SendRexxCommand("DEFICONS",command,result,sizeof(result)))
			{
				def = result;
			}
#endif

			data->obj = GetIconTags(command + 9,
				ICONGETA_FailIfUnavailable,FALSE,
				ICONGETA_Screen, _screen(obj),
				TAG_DONE);

			DeleteFile(command+9);
		}
	}

	if (!def)
	{
		if (!mystricmp(data->type, "image")) def = "picture";
		else if (!mystricmp(data->type, "audio")) def = "audio";
		else
		{
			if (!mystricmp(data->type, "text"))
			{
				if (!mystricmp(data->subtype, "html")) def = "html";
				else if (!mystricmp(data->subtype, "plain")) def = "ascii";
				else def = "text";
			} else def = "attach";
		}
	}

	if (!data->obj)
	{
		if (IconBase->lib_Version >= 44)
		{
			data->obj = GetIconTags(NULL,
					ICONGETA_GetDefaultName, def,
					ICONGETA_Screen, _screen(obj),
					TAG_DONE);

			if (!data->obj)
			{
				data->obj = GetIconTags(NULL,
					ICONGETA_GetDefaultType, WBPROJECT,
					ICONGETA_Screen, _screen(obj),
					TAG_DONE);
			}
		} else
		{
			data->obj = GetDefDiskObject(WBPROJECT);
		}
	}

	data->ehnode.ehn_Priority = -1;
	data->ehnode.ehn_Flags    = 0;
	data->ehnode.ehn_Object   = obj;
	data->ehnode.ehn_Class    = cl;
	data->ehnode.ehn_Events   = IDCMP_MOUSEBUTTONS;

	DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->ehnode);

	return 1;
}

STATIC ULONG Icon_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehnode);
	if (data->obj) FreeDiskObject(data->obj);
	DoSuperMethodA(cl,obj,msg);
	return 0;
}

STATIC ULONG Icon_AskMinMax(struct IClass *cl,Object *obj, struct MUIP_AskMinMax *msg)
{
	int w,h;
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
  DoSuperMethodA(cl, obj, (Msg) msg);

	if (data->obj)
	{
		if (IconBase->lib_Version >= 44)
		{
			struct Rectangle rect;
			GetIconRectangle(NULL, data->obj, NULL, &rect,
						ICONDRAWA_Borderless, TRUE,
						TAG_DONE);

			w = rect.MaxX - rect.MinX + 1;
			h = rect.MaxY - rect.MinY + 1;
		} else
		{
			w = ((struct Image*)data->obj->do_Gadget.GadgetRender)->Width;
			h = ((struct Image*)data->obj->do_Gadget.GadgetRender)->Height;
		}
	} else
	{
		w = 30;
		h = 30;
	}

  msg->MinMaxInfo->MinWidth += w;
  msg->MinMaxInfo->DefWidth += w;
  msg->MinMaxInfo->MaxWidth += w;

  msg->MinMaxInfo->MinHeight += h;
  msg->MinMaxInfo->DefHeight += h;
  msg->MinMaxInfo->MaxHeight += h;
  return 0;
}

STATIC ULONG Icon_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
	DoSuperMethodA(cl,obj,(Msg)msg);

	if (data->obj)
	{
		int sel = xget(obj,MUIA_Selected);
		if (IconBase->lib_Version >= 44)
		{
			DrawIconStateA(_rp(obj),data->obj, NULL, _mleft(obj), _mtop(obj), sel?IDS_SELECTED:IDS_NORMAL, NULL);
		} else
		{
			if (sel && data->obj->do_Gadget.Flags & GFLG_GADGHIMAGE)
				DrawImage(_rp(obj),((struct Image*)data->obj->do_Gadget.SelectRender),_mleft(obj),_mtop(obj));
			else
				DrawImage(_rp(obj),((struct Image*)data->obj->do_Gadget.GadgetRender),_mleft(obj),_mtop(obj));
		}
	}

	return 1;
}

STATIC ULONG Icon_HandleEvent(struct IClass *cl, Object *obj, struct MUIP_HandleEvent *msg)
{
	struct Icon_Data *data = (struct Icon_Data*)INST_DATA(cl,obj);
	if (msg->imsg && msg->imsg->Class == IDCMP_MOUSEBUTTONS)
	{
		if (msg->imsg->Code == SELECTUP && _isinobject(obj,msg->imsg->MouseX,msg->imsg->MouseY))
		{
			ULONG secs, mics;
			CurrentTime(&secs,&mics);

			if (DoubleClick(data->select_secs,data->select_mics, secs, mics))
			{
				set(obj,MUIA_Icon_DoubleClick, TRUE);
			}
			data->select_secs = secs;
			data->select_mics = mics;	
		}
	}
	return 0;
}

STATIC ASM ULONG Icon_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case	OM_NEW:				return Icon_New(cl,obj,(struct opSet*)msg);
		case  OM_DISPOSE:		Icon_Dispose(cl,obj,msg); return 0;
		case  OM_SET:				return Icon_Set(cl,obj,(struct opSet*)msg);
//		case	OM_GET:				return Icon_Get(cl,obj,(struct opGet*)msg);
		case  MUIM_AskMinMax: return Icon_AskMinMax(cl,obj,(struct MUIP_AskMinMax *)msg);
		case	MUIM_Setup:		return Icon_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup:	return Icon_Cleanup(cl,obj,msg);
		case	MUIM_Draw:			return Icon_Draw(cl,obj,(struct MUIP_Draw*)msg);
		case	MUIM_HandleEvent: return Icon_HandleEvent(cl,obj,(struct MUIP_HandleEvent*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_Icon;

int create_icon_class(void)
{
	if ((CL_Icon = MUI_CreateCustomClass(NULL,MUIC_Area,NULL,sizeof(struct Icon_Data),Icon_Dispatcher)))
	{
		CL_Icon->mcc_Class->cl_UserData = getreg(REG_A4);
		return 1;
	}
	return 0;
}

void delete_icon_class(void)
{
	if (CL_Icon) MUI_DeleteCustomClass(CL_Icon);
}

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
#include <mui/texteditor_mcc.h>

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
#include "composeeditorclass.h"
#include "compiler.h"
#include "muistuff.h"

struct ComposeEditor_Data
{
	LONG cmap[8];
};

STATIC ULONG ComposeEditor_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct ComposeEditor_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct ComposeEditor_Data*)INST_DATA(cl,obj);

	set(obj,MUIA_TextEditor_ColorMap, data->cmap);

	return (ULONG)obj;
}

STATIC ULONG ComposeEditor_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct ComposeEditor_Data *data = (struct ComposeEditor_Data*)INST_DATA(cl,obj);
	ULONG retval = DoSuperMethodA(cl,obj,(Msg)msg);

	if (retval)
	{
/*		data->cmap[0] = ObtainBestPenA(_screen(obj)->ViewPort.ColorMap,0xffffffff,0xffffffff,0xffffffff,NULL);*/
		data->cmap[7] = ObtainBestPenA(_screen(obj)->ViewPort.ColorMap,0xffffffff,0xffffffff,0x80000000,NULL);
	} else
	{
		DoSuperMethod(cl,obj,MUIM_Cleanup);
		return 0;
	}
	return retval;
}

STATIC ULONG ComposeEditor_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct ComposeEditor_Data *data = (struct ComposeEditor_Data*)INST_DATA(cl,obj);

/*	ReleasePen(_screen(obj)->ViewPort.ColorMap,data->cmap[0]);*/
	ReleasePen(_screen(obj)->ViewPort.ColorMap,data->cmap[7]);

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG ComposeEditor_InsertText(struct IClass *cl, Object *obj, struct MUIP_TextEditor_InsertText *msg)
{
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ASM ULONG ComposeEditor_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case	OM_NEW: return ComposeEditor_New(cl,obj,(struct opSet*)msg);
		case	MUIM_Setup: return ComposeEditor_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup: return ComposeEditor_Cleanup(cl,obj,msg);
		case	MUIM_TextEditor_InsertText: return ComposeEditor_InsertText(cl,obj,(struct MUIP_TextEditor_InsertText*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_ComposeEditor;

int create_composeeditor_class(void)
{
	if ((CL_ComposeEditor = MUI_CreateCustomClass(NULL,MUIC_TextEditor,NULL,sizeof(struct ComposeEditor_Data),ComposeEditor_Dispatcher)))
	{
		CL_ComposeEditor->mcc_Class->cl_UserData = getreg(REG_A4);
		return 1;
	}
	return 0;
}

void delete_composeeditor_class(void)
{
	if (CL_ComposeEditor) MUI_DeleteCustomClass(CL_ComposeEditor);
}

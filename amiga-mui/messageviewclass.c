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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <intuition/intuitionbase.h>
#include <libraries/mui.h>
#include <mui/betterstring_mcc.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "filter.h"
#include "smintl.h"
#include "debug.h"

#include "compiler.h"
#include "muistuff.h"
#include "multistringclass.h"
#include "messageviewclass.h"
#include "simplehtml_mcc.h"

struct MessageView_Data
{
	Object *simplehtml;
	Object *horiz;
	Object *vert;
};

STATIC ULONG MessageView_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct MessageView_Data *data;
	struct TagItem *ti;
	Object *simplehtml, *horiz, *vert;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
		MUIA_Group_Spacing, 0,
		MUIA_Group_Horiz, FALSE,
		Child, HGroup,
			MUIA_Group_Spacing, 0,
			Child, simplehtml = SimpleHTMLObject,TextFrame,End,
			Child, vert = ScrollbarObject, End,
			End,
		Child, horiz = ScrollbarObject, MUIA_Group_Horiz, TRUE, End,
		TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MessageView_Data*)INST_DATA(cl,obj);

	data->simplehtml = simplehtml;
	data->horiz = horiz;
	data->vert = vert;

	return (ULONG)obj;
}

STATIC ULONG MessageView_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	return DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG MessageView_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG MessageView_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct MessageView_Data *data = (struct MessageView_Data*)INST_DATA(cl,obj);
	switch (msg->opg_AttrID)
	{
		default:
					return DoSuperMethodA(cl,obj,(Msg)msg);
	}
}

STATIC BOOPSI_DISPATCHER(ULONG, MessageView_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW:				return MessageView_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE:		return MessageView_Dispose(cl,obj,msg);
		case	OM_SET:				return MessageView_Set(cl,obj,(struct opSet*)msg);
		case	OM_GET:				return MessageView_Get(cl,obj,(struct opGet*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_MessageView;

int create_messageview_class(void)
{
	SM_ENTER;
	if ((CL_MessageView = CreateMCC(MUIC_Group,NULL,sizeof(struct MessageView_Data),MessageView_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_MessageView: 0x%lx\n",CL_MessageView));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_MessageView\n"));
	SM_RETURN(0,"%ld");
}

void delete_messageview_class(void)
{
	SM_ENTER;
	if (CL_MessageView)
	{
		if (MUI_DeleteCustomClass(CL_MessageView))
		{
			SM_DEBUGF(15,("Deleted CL_MessageView: 0x%lx\n",CL_MessageView));
			CL_MessageView = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_MessageView: 0x%lx\n",CL_MessageView));
		}
	}
	SM_LEAVE;
}

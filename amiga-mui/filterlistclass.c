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
** filterlistclass.c
*/

#include <string.h>
#include <stdio.h>

#include <libraries/mui.h>
#include <mui/NListview_MCC.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "compiler.h"
#include "muistuff.h"

struct FilterList_Data
{
	int dummy;
};

STATIC ULONG FilterList_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
/*	struct FilterList_Data *data; */

	if (!(obj=(Object *)DoSuperMethodA(cl,obj,(Msg)msg)))
		return 0;

/*	data = (struct FolderTreelist_Data*)INST_DATA(cl,obj);*/

	return (ULONG)obj;
}

STATIC ULONG FilterList_DragQuery(struct IClass *cl, Object *obj, struct MUIP_DragQuery *msg)
{
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG FilterList_DragDrop(struct IClass *cl,Object *obj,struct MUIP_DragDrop *msg)
{
  return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC BOOPSI_DISPATCHER(ULONG, FilterList_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW:				return FilterList_New(cl,obj,(struct opSet*)msg);
    case  MUIM_DragQuery: return FilterList_DragQuery(cl,obj,(struct MUIP_DragQuery *)msg);
    case  MUIM_DragDrop:  return FilterList_DragDrop (cl,obj,(struct MUIP_DragDrop *)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_FilterList;

int create_filterlist_class(void)
{
	if ((CL_FilterList = CreateMCC(MUIC_NList,NULL,sizeof(struct FilterList_Data),FilterList_Dispatcher)))
		return 1;
	return 0;
}

void delete_filterlist_class(void)
{
	if (CL_FilterList) MUI_DeleteCustomClass(CL_FilterList);
}

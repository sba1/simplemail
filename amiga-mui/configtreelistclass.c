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
** configtreelistclass.c
*/

#include <string.h>
#include <stdio.h>

#include <dos.h>
#include <libraries/mui.h>
#include <mui/NListview_MCC.h>
#include <mui/NListtree_Mcc.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "folder.h"
#include "simplemail.h"

#include "compiler.h"
#include "configtreelistclass.h"
#include "muistuff.h"

struct ConfigTreelist_Data
{
	int dummy;
};

STATIC ULONG ConfigTreelist_DragQuery(struct IClass *cl, Object *obj, struct MUIP_DragQuery *msg)
{
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG ConfigTreelist_DragDrop(struct IClass *cl,Object *obj,struct MUIP_DragDrop *msg)
{
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG ConfigTreelist_DropType(struct IClass *cl, Object *obj,struct MUIP_NList_DropType *msg)
{
	ULONG rv = DoSuperMethodA(cl,obj,(Msg)msg);

	if (*msg->pos == xget(obj,MUIA_NList_Active))
		*msg->type = MUIV_NListtree_DropType_None;
	else *msg->type = MUIV_NListtree_DropType_Onto;

	return rv;
}

STATIC ASM ULONG ConfigTreelist_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
    case  MUIM_DragQuery: return ConfigTreelist_DragQuery(cl,obj,(struct MUIP_DragQuery *)msg);
    case  MUIM_DragDrop:  return ConfigTreelist_DragDrop (cl,obj,(struct MUIP_DragDrop *)msg);
    case	MUIM_NList_DropType: return ConfigTreelist_DropType(cl,obj,(struct MUIP_NList_DropType*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_ConfigTreelist;

int create_configtreelist_class(void)
{
	if ((CL_ConfigTreelist = MUI_CreateCustomClass(NULL,MUIC_NListtree,NULL,sizeof(struct ConfigTreelist_Data),ConfigTreelist_Dispatcher)))
	{
		CL_ConfigTreelist->mcc_Class->cl_UserData = getreg(REG_A4);
		return 1;
	}
	return 0;
}

void delete_configtreelist_class(void)
{
	if (CL_ConfigTreelist) MUI_DeleteCustomClass(CL_ConfigTreelist);
}

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
** pgplistclass.c
*/

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

#include "mail.h"
#include "pgp.h"
#include "smintl.h"
#include "support_indep.h"
#include "debug.h"

#include "amigasupport.h"
#include "compiler.h"
#include "muistuff.h"
#include "pgplistclass.h"

struct PGPList_Data
{
	struct Hook construct_hook;
	struct Hook destruct_hook;
	struct Hook display_hook;
	char buf[100];
};

STATIC ASM SAVEDS struct pgp_key *pgp_construct(REG(a0,struct Hook *h), REG(a2,Object *obj), REG(a1,struct NList_ConstructMessage *msg))
{
	struct pgp_key *key = (struct pgp_key*)msg->entry;
	return pgp_duplicate(key);
}

STATIC ASM SAVEDS VOID pgp_destruct(REG(a0,struct Hook *h), REG(a2,Object *obj), REG(a1,struct NList_DestructMessage *msg))
{
	struct pgp_key *key = (struct pgp_key*)msg->entry;
	if (key) pgp_dispose(key);
}

STATIC ASM SAVEDS VOID pgp_display(REG(a0,struct Hook *h),REG(a2,Object *obj),REG(a1,struct NList_DisplayMessage *msg))
{
	struct PGPList_Data *data = (struct PGPList_Data*)INST_DATA(CL_PGPList->mcc_Class,obj);
	if (msg->entry)
	{
		struct pgp_key *key = (struct pgp_key *)msg->entry;
		sprintf(data->buf,"0x%08lX",key->keyid);
		msg->strings[0] = data->buf;
		msg->strings[1] = key->userids?key->userids[0]:NULL;
	} else
	{
		msg->strings[0] = _("PGP Key ID");
		msg->strings[1] = _("User ID");
	}
}

STATIC ULONG PGPList_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct PGPList_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct PGPList_Data*)INST_DATA(cl,obj);
	init_hook(&data->construct_hook,(HOOKFUNC)pgp_construct);
	init_hook(&data->destruct_hook,(HOOKFUNC)pgp_destruct);
	init_hook(&data->display_hook,(HOOKFUNC)pgp_display);

	SetAttrs(obj,
						MUIA_NList_ConstructHook2, &data->construct_hook,
						MUIA_NList_DestructHook2, &data->destruct_hook,
						MUIA_NList_DisplayHook2, &data->display_hook,
						MUIA_NList_Format, ",",
						TAG_DONE);

	return (ULONG)obj;
}

STATIC ULONG PGPList_Refresh(struct IClass *cl,Object *obj, Msg msg)
{
	struct pgp_key *key;

	pgp_update_key_list();

	set(obj, MUIA_NList_Quiet, TRUE);
	DoMethod(obj, MUIM_NList_Clear);

	key = pgp_first();
	while (key)
	{
		DoMethod(obj, MUIM_NList_InsertSingle, (ULONG)key, MUIV_NList_Insert_Bottom);
		key = pgp_next(key);
	}

	set(obj, MUIA_NList_Quiet, FALSE);
	return 0;
}

STATIC BOOPSI_DISPATCHER(ULONG, PGPList_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return PGPList_New(cl,obj,(struct opSet*)msg);
		case	MUIM_PGPList_Refresh: return PGPList_Refresh(cl,obj,msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_PGPList;

int create_pgplist_class(void)
{
	SM_ENTER;
	if ((CL_PGPList = CreateMCC(MUIC_NList,NULL,sizeof(struct PGPList_Data),PGPList_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_PGPList: 0x%lx\n",CL_PGPList));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_PGPList\n"));
	SM_RETURN(0,"%ld");
}

void delete_pgplist_class(void)
{
	SM_ENTER;
	if (CL_PGPList)
	{
		if (MUI_DeleteCustomClass(CL_PGPList))
		{
			SM_DEBUGF(15,("Deleted CL_PGPList: 0x%lx\n",CL_PGPList));
			CL_PGPList = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_PGPList: 0x%lx\n",CL_PGPList));
		}
	}
	SM_LEAVE;
}

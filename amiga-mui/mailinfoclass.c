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
** mailinfoclass.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/mui.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "debug.h"
#include "mail.h"
#include "parse.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

#include "amigasupport.h"
#include "compiler.h"
#include "mailinfoclass.h"
#include "muistuff.h"

struct MailInfo_Data
{
	struct mail_info *mail;
};

STATIC ULONG MailInfo_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct MailInfo_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MailInfo_Data*)INST_DATA(cl,obj);

	return (ULONG)obj;
}

STATIC ULONG MailInfo_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct MailInfo_Data *data = (struct MailInfo_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;
	tstate = (struct TagItem *)msg->ops_AttrList;

	while ((tag = NextTagItem (&tstate)))
	{
		ULONG tidata = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case	MUIA_MailInfo_MailInfo:
						break;
		}
	}

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC LONG MailInfo_AskMinMax(struct IClass *cl, Object *obj, struct MUIP_AskMinMax *msg)
{
  DoSuperMethodA(cl, obj, (Msg) msg);
  msg->MinMaxInfo->MinWidth = 10;
  msg->MinMaxInfo->DefWidth = 20;
  msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
  return 0;
}

STATIC BOOPSI_DISPATCHER(ULONG, MailInfo_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return MailInfo_New(cl,obj,(struct opSet*)msg);
		case	OM_SET: return MailInfo_Set(cl,obj,(struct opSet*)msg);
		case	MUIM_AskMinMax: return MailInfo_AskMinMax(cl,obj,(struct MUIP_AskMinMax*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_MailInfo;

int create_mailinfo_class(void)
{
	SM_ENTER;
	if ((CL_MailInfo = CreateMCC(MUIC_Area, NULL, sizeof(struct MailInfo_Data), MailInfo_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_MailInfo: 0x%lx\n",CL_MailInfo));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_MailInfo\n"));
	SM_RETURN(0,"%ld");
}

void delete_mailinfo_class(void)
{
	SM_ENTER;
	if (CL_MailInfo)
	{
		if (MUI_DeleteCustomClass(CL_MailInfo))
		{
			SM_DEBUGF(15,("Deleted CL_MailInfo: 0x%lx\n",CL_MailInfo));
			CL_MailInfo = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_MailInfo: 0x%lx\n",CL_MailInfo));
		}
	}
	SM_LEAVE;
}

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
** mailtreelistclass-new.c
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "compiler.h"
#include "datatypescache.h"
#include "debug.h"
#include "mailtreelistclass.h"
#include "muistuff.h"

/**************************************************************************/

enum
{
	IMAGE_UNREAD = 0,
	IMAGE_UNREAD_PARTIAL,
	IMAGE_READ_PARTIAL,
	IMAGE_REPLY_PARTIAL,
	IMAGE_READ,
	IMAGE_WAITSEND,
	IMAGE_SENT,
	IMAGE_MARK,
	IMAGE_HOLD,
	IMAGE_REPLY,
	IMAGE_FORWARD,
	IMAGE_NORCPT,
	IMAGE_NEW_PARTIAL,
	IMAGE_NEW_SPAM,
	IMAGE_UNREAD_SPAM,
	IMAGE_ERROR,
	IMAGE_IMPORTANT,
	IMAGE_ATTACH,
	IMAGE_GROUP,
	IMAGE_NEW,
	IMAGE_CRYPT,
	IMAGE_SIGNED,
	IMAGE_TRASHCAN,
	IMAGE_MAX
};

/* Must match those above (old = read)*/
static const char *image_names[] =
{
	"status_unread",
	"status_unread_partial",
	"status_old_partial",
	"status_reply_partial",
	"status_old",
  "status_waitsend",
	"status_sent",
	"status_mark",
	"status_hold",
	"status_reply",
	"status_forward", 
	"status_norcpt",
	"status_new_partial",
	"status_new_spam",
	"status_unread_spam",
	"status_error",
	"status_urgent",
	"status_attach",
	"status_group",
	"status_new",
	"status_crypt",
	"status_signed",
	"status_trashcan",
	NULL
};

struct MailTreelist_Data
{
	struct dt_node *images[IMAGE_MAX];
};

/**************************************************************************/

/*************************************************************************
 OM_NEW
*************************************************************************/
STATIC ULONG MailTreelist_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct MailTreelist_Data *data;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
		MUIA_ShortHelp, TRUE,
		TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	return (ULONG)obj;
}

/*************************************************************************
 OM_DISPOSE
*************************************************************************/
STATIC ULONG MailTreelist_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	return DoSuperMethodA(cl,obj,msg);
}

/*************************************************************************
 OM_SET
*************************************************************************/
STATIC ULONG MailTreelist_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;

	tstate = (struct TagItem *)msg->ops_AttrList;

	while ((tag = NextTagItem (&tstate)))
	{
/*		ULONG tidata = tag->ti_Data;*/

		switch (tag->ti_Tag)
		{
						break;
		}
	}

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/*************************************************************************
 OM_GET
*************************************************************************/
STATIC ULONG MailTreelist_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	if (msg->opg_AttrID == MUIA_MailTreelist_Active)
	{
		*msg->opg_Storage = 0;
		return 1;
	}

	if (msg->opg_AttrID == MUIA_MailTreelist_DoubleClick)
	{
		*msg->opg_Storage = 0;
		return 1;
	}

	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/*************************************************************************
 MUIM_Setup
*************************************************************************/
STATIC ULONG MailTreelist_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);
	char filename[256];
	int i;

	if (!DoSuperMethodA(cl,obj,(Msg)msg))
		return 0;

	for (i=0;i<IMAGE_MAX;i++)
	{
		strcpy(filename,"PROGDIR:Images/");
		strcat(filename,image_names[i]);
		data->images[i] = dt_load_picture(filename, _screen(obj));
		/* It doesn't matter if this fails */
	}

	return 1;
}

/*************************************************************************
 MUIM_Cleanup
*************************************************************************/
STATIC ULONG MailTreelist_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	int i;
	struct MailTreelist_Data *data = (struct MailTreelist_Data*)INST_DATA(cl,obj);

	for (i=0;i<IMAGE_MAX;i++)
	{
		if (data->images[i])
		{
			dt_dispose_picture(data->images[i]);
			data->images[i] = NULL;
		}
	}

	return DoSuperMethodA(cl,obj,msg);
}

/*************************************************************************
 MUIM_Draw
*************************************************************************/
STATIC ULONG MailTreelist_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
	DoSuperMethodA(cl,obj,(Msg)msg);

	return 0;
}


/**************************************************************************/

STATIC BOOPSI_DISPATCHER(ULONG, MailTreelist_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW:				return MailTreelist_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE:		return MailTreelist_Dispose(cl,obj,msg);
		case	OM_SET:				return MailTreelist_Set(cl,obj,(struct opSet*)msg);
		case	OM_GET:				return MailTreelist_Get(cl,obj,(struct opGet*)msg);
		case	MUIM_Setup:		return MailTreelist_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup:	return MailTreelist_Cleanup(cl,obj,msg);
		case	MUIM_Draw:		return MailTreelist_Draw(cl,obj,(struct MUIP_Draw*)msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

/**************************************************************************/

struct MUI_CustomClass *CL_MailTreelist;

int create_mailtreelist_class(void)
{
	SM_ENTER;
	if ((CL_MailTreelist = CreateMCC(MUIC_Area ,NULL,sizeof(struct MailTreelist_Data),MailTreelist_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_MailTreelist: 0x%lx\n",CL_MailTreelist));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_MailTreelist\n"));
	SM_RETURN(0,"%ld");
}

void delete_mailtreelist_class(void)
{
	SM_ENTER;
	if (CL_MailTreelist)
	{
		if (MUI_DeleteCustomClass(CL_MailTreelist))
		{
			SM_DEBUGF(15,("Deleted CL_MailTreelist: 0x%lx\n",CL_MailTreelist));
			CL_MailTreelist = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_MailTreelist: 0x%lx\n",CL_MailTreelist));
		}
	}
	SM_LEAVE;
}

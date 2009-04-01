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
** accountpopclass.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/mui.h>
#include <mui/NListview_mcc.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "account.h"
#include "configuration.h"
#include "debug.h"
#include "parse.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

#include "amigasupport.h"
#include "compiler.h"
#include "muistuff.h"
#include "accountpopclass.h"

struct AccountPop_Data
{
	int has_default_entry;
	Object *obj;
	Object *list;
	Object *string;
	struct Hook objstr_hook;
	struct Hook strobj_hook;
	struct Hook construct_hook;
	struct Hook destruct_hook;
	struct Hook display_hook;

	struct account *selected_account; /* is always a copy */

	char name_buf[64];
	char email_buf[64];
};

STATIC ASM SAVEDS struct account *account_construct(REG(a0,struct Hook *h),REG(a2,Object *obj),REG(a1,struct NList_ConstructMessage *msg))
{
	if ((LONG)msg->entry == -1) return (struct account*)-1;
	return account_duplicate((struct account*)msg->entry);
}

STATIC ASM SAVEDS VOID account_destruct(REG(a0,struct Hook *h),REG(a2,Object *obj),REG(a1,struct NList_DestructMessage *msg))
{
	if ((LONG)msg->entry == -1) return;
	if (msg->entry) account_free((struct account*)msg->entry);
}

STATIC ASM SAVEDS VOID account_display(REG(a0,struct Hook *h),REG(a2,Object *obj),REG(a1,struct NList_DisplayMessage *msg))
{
	struct AccountPop_Data *data = h->h_Data;

	if (msg->entry)
	{
		if ((LONG)msg->entry == -1)
		{
			msg->strings[0] = _("<Default>");
			msg->strings[1] = "";
			msg->strings[2] = "";
		} else
		{
			struct account *account = (struct account *)msg->entry;

			utf8tostr(account->email,data->email_buf,sizeof(data->email_buf),user.config.default_codeset);
			utf8tostr(account->account_name,data->name_buf,sizeof(data->name_buf),user.config.default_codeset);

			msg->strings[0] = data->email_buf;
			msg->strings[1] = data->name_buf;
			msg->strings[2] = account->smtp->name;
		}
	}
}

STATIC ASM SAVEDS VOID account_objstr(REG(a0,struct Hook *h), REG(a2,Object *list), REG(a1,Object *str))
{
	struct AccountPop_Data *data = (struct AccountPop_Data*)h->h_Data;
	struct account *ac;
	DoMethod(list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, (ULONG)&ac);
	if ((LONG)ac == -1) ac = NULL;
	set(data->obj,MUIA_AccountPop_Account,ac);
}

STATIC ASM SAVEDS LONG account_strobj(REG(a0,struct Hook *h), REG(a2,Object *list), REG(a1,Object *str))
{
	struct AccountPop_Data *data = (struct AccountPop_Data*)h->h_Data;
	DoMethod(data->obj, MUIM_AccountPop_Refresh);
	return 1;
}

STATIC ULONG AccountPop_Set(struct IClass *cl, Object *obj, struct opSet *msg, int nodosuper)
{
	struct AccountPop_Data *data = (struct AccountPop_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;
	tstate = (struct TagItem *)msg->ops_AttrList;

	while ((tag = NextTagItem (&tstate)))
	{
		ULONG tidata = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case	MUIA_AccountPop_HasDefaultEntry:
						data->has_default_entry = !!tidata;
						break;

			case	MUIA_AccountPop_Account:
						{
							if (data->selected_account) account_free(data->selected_account);

							if (!tidata && !data->has_default_entry)
							{
								/* Choose the first account if no account is specified and we have not
								 * the posibility to use a default account */
								tidata = (ULONG)list_first(&user.config.account_list);
							}

							if (!tidata)
							{
								if (data->selected_account) account_free(data->selected_account);
								data->selected_account = NULL;
								set(data->string, MUIA_Text_Contents,_("<Default>"));
							} else
							{
								struct account *ac;
								ac = data->selected_account = account_duplicate((struct account*)tidata);
								if (ac && ac->email)
								{
									char iso_buf[256];
									char buf[256];
									char smtp_buf[128];

									buf[0] = 0;

									if (ac->name)
									{
										if (needs_quotation(ac->name))
											sm_snprintf(buf,sizeof(buf),"\"%s\"",ac->name);
										else mystrlcpy(buf,ac->name,sizeof(buf));
									}

									if (ac->account_name && *ac->account_name)
									{
										sm_snprintf(smtp_buf,sizeof(smtp_buf),"%s: %s",ac->account_name,ac->smtp->name);
									} else
									{
										mystrlcpy(smtp_buf,ac->smtp->name?ac->smtp->name:_("Unconfigured"),sizeof(smtp_buf));
									}

									sm_snprintf(buf+strlen(buf),sizeof(buf)-strlen(buf)," <%s> (%s)",ac->email, smtp_buf);

									utf8tostr(buf, iso_buf, sizeof(iso_buf), user.config.default_codeset);
									set(data->string,MUIA_Text_Contents,iso_buf);
								} else
								{
									set(data->string, MUIA_Text_Contents,_("Unconfigured"));
								}
							}
						}
						break;
		}
	}

	if (nodosuper) return 0;
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG AccountPop_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct AccountPop_Data *data;
	Object *list, *string;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
					MUIA_Popstring_Button, PopButton(MUII_PopUp),
					MUIA_Popstring_String, string = TextObject,TextFrame,MUIA_Text_SetMin,FALSE,MUIA_InputMode, MUIV_InputMode_RelVerify,MUIA_CycleChain,1,End,
					MUIA_Popobject_Object, NListviewObject,
							MUIA_NListview_NList, list = NListObject,
								End,
							End,

					TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct AccountPop_Data*)INST_DATA(cl,obj);
	data->obj = obj;
	data->list = list;
	data->string = string;

	init_hook(&data->construct_hook,(HOOKFUNC)account_construct);
	init_hook(&data->destruct_hook,(HOOKFUNC)account_destruct);
	init_hook_with_data(&data->display_hook,(HOOKFUNC)account_display,data);
	init_hook_with_data(&data->objstr_hook,(HOOKFUNC)account_objstr,data);
	init_hook_with_data(&data->strobj_hook,(HOOKFUNC)account_strobj,data);

	DoMethod(string, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)obj, 1, MUIM_Popstring_Open);
	DoMethod(list, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, (ULONG)obj, 2, MUIM_Popstring_Close, 1);

	SetAttrs(list,
						MUIA_NList_ConstructHook2, &data->construct_hook,
						MUIA_NList_DestructHook2, &data->destruct_hook,
						MUIA_NList_DisplayHook2, &data->display_hook,
						MUIA_NList_Format, ",,",
						TAG_DONE);

	SetAttrs(obj,
						MUIA_Popobject_ObjStrHook, &data->objstr_hook,
						MUIA_Popobject_StrObjHook, &data->strobj_hook,
						TAG_DONE);

	AccountPop_Set(cl,obj,msg,1);

	return (ULONG)obj;
}

STATIC VOID AccountPop_Dispose(struct IClass *cl,Object *obj, Msg msg)
{
	struct AccountPop_Data *data = (struct AccountPop_Data*)INST_DATA(cl,obj);

	if (data->selected_account) account_free(data->selected_account);
	DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG AccountPop_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct AccountPop_Data *data = (struct AccountPop_Data*)INST_DATA(cl,obj);
	switch (msg->opg_AttrID)
	{
		case	MUIA_AccountPop_Account:
					*msg->opg_Storage = (ULONG)data->selected_account;
					return 1;

		default:
					return DoSuperMethodA(cl,obj,(Msg)msg);
	}
}

STATIC ULONG AccountPop_Refresh(struct IClass *cl, Object *obj, Msg msg)
{
	struct AccountPop_Data *data = (struct AccountPop_Data*)INST_DATA(cl,obj);
	struct account *account = (struct account*)list_first(&user.config.account_list);

	DoMethod(data->list, MUIM_NList_Clear);

	if (data->has_default_entry)
		DoMethod(data->list, MUIM_NList_InsertSingle, -1, MUIV_NList_Insert_Bottom); /* special value */

	while (account)
	{
		if (account->smtp->name && *account->smtp->name && account->email)
		{
			DoMethod(data->list, MUIM_NList_InsertSingle, (ULONG)account, MUIV_NList_Insert_Bottom);
		}
		account = (struct account*)node_next(&account->node);
	}

	if (data->selected_account == NULL && !data->has_default_entry)
	{
		set(obj, MUIA_AccountPop_Account, (struct account*)list_first(&user.config.account_list));
	}
	return 1;
}

STATIC MY_BOOPSI_DISPATCHER(ULONG, AccountPop_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW: return AccountPop_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE: AccountPop_Dispose(cl,obj,msg); return 0;
		case	OM_GET: return AccountPop_Get(cl,obj,(struct opGet*)msg);
		case	OM_SET: return AccountPop_Set(cl,obj,(struct opSet*)msg,0);
		case	MUIM_AccountPop_Refresh: return AccountPop_Refresh(cl,obj,msg);
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_AccountPop;

int create_accountpop_class(void)
{
	SM_ENTER;
	if ((CL_AccountPop = CreateMCC(MUIC_Popobject, NULL, sizeof(struct AccountPop_Data), AccountPop_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_AccountPop: 0x%lx\n",CL_AccountPop));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_AccountPop\n"));
	SM_RETURN(0,"%ld");
}

void delete_accountpop_class(void)
{
	SM_ENTER;
	if (CL_AccountPop)
	{
		if (MUI_DeleteCustomClass(CL_AccountPop))
		{
			SM_DEBUGF(15,("Deleted CL_AccountPop: 0x%lx\n",CL_AccountPop));
			CL_AccountPop = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_AccountPop: 0x%lx\n",CL_AccountPop));
		}
	}
	SM_LEAVE;
}

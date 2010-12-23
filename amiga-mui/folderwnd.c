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
** folderwnd.c
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <mui/BetterString_mcc.h>
#include <mui/NListview_mcc.h>
#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/exec.h>

#include "account.h"
#include "configuration.h"
#include "debug.h"
#include "folder.h"
#include "imap.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"

#include "accountpopclass.h"
#include "signaturecycleclass.h"
#include "addressstringclass.h"
#include "amigasupport.h"
#include "compiler.h"
#include "folderwnd.h"
#include "muistuff.h"
#include "support.h"
#include "utf8stringclass.h"

static Object *folder_wnd;
static Object *folder_properties_group;
static Object *name_label;
static Object *name_string;
static Object *path_label;
static Object *path_string;
static Object *type_label;
static Object *type_cycle;
static Object *imap_label;
static Object *imap_string;
static Object *prim_group;
static Object *prim_label;
static Object *prim_cycle;
static Object *prim_reverse_check;
static Object *prim_reverse_label;
static Object *second_group;
static Object *second_label;
static Object *second_cycle;
static Object *second_reverse_check;
static Object *second_reverse_label;

static Object *compose_mail_properties_group;
static Object *from_accountpop;
static Object *replyto_string;
static Object *defto_string;
static Object *defsign_cycle;

static Object *imap_folders_horizline;
static Object *imap_folders_group;
static Object *imap_folders_list;
static Object *imap_folders_listview;
static Object *imap_folders_subscribe;
static Object *imap_folders_unsubscribe;
static Object *imap_folders_check;
static Object *imap_folders_submit;
static int group_mode;
static int imap_mode;

static struct Hook imap_folders_construct_hook;
static struct Hook imap_folders_destruct_hook;
static struct Hook imap_folders_display_hook;

struct imap_folder_entry
{
	char *name;
	int subscribed;
};

STATIC ASM SAVEDS APTR imap_folders_construct(REG(a0,struct Hook *h),REG(a2,APTR pool), REG(a1,struct imap_folder_entry *entry))
{
	struct imap_folder_entry *new_entry = malloc(sizeof(*entry));
	if (new_entry)
	{
		new_entry->name = mystrdup(entry->name);
		new_entry->subscribed = entry->subscribed;
	}
	return new_entry;
}

STATIC ASM SAVEDS VOID imap_folders_destruct(REG(a0,struct Hook *h),REG(a2,APTR pool), REG(a1,struct imap_folder_entry *entry))
{
	if (entry)
	{
		free(entry->name);
		free(entry);
	}
}

STATIC ASM SAVEDS VOID imap_folders_display(REG(a0,struct Hook *h),REG(a2,char **array), REG(a1,struct imap_folder_entry *entry))
{
	if (entry)
	{
		static char buf[300];
		if (entry->subscribed)
		{
			utf8tostr(entry->name,buf,sizeof(buf),user.config.default_codeset);
		} else
		{
			buf[0] = '(';
			utf8tostr(entry->name, &buf[1], sizeof(buf) - 2, user.config.default_codeset);
			strcat(buf,")");
		}
		*array = buf;
	}
}

static struct folder *changed_folder;

static void imap_folders_check_pressed(void)
{
	if (changed_folder)
		callback_imap_get_folders(changed_folder);
}

static void imap_folders_submit_pressed(void)
{
	if (changed_folder)
	{
		struct list list;
		int i;

		list_init(&list);
		for (i=0;i<xget(imap_folders_list,MUIA_NList_Entries);i++)
		{
			struct imap_folder_entry *entry;
			DoMethod(imap_folders_list, MUIM_NList_GetEntry, i, (ULONG)&entry);
			if (entry && entry->subscribed)
				string_list_insert_tail(&list,entry->name);
		}
		callback_imap_submit_folders(changed_folder,&list);
		string_list_clear(&list);
	}
}

static void imap_folders_subscribe_pressed(void)
{
	struct imap_folder_entry *entry;
	DoMethod(imap_folders_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, (ULONG)&entry);
	if (entry)
	{
		entry->subscribed = 1;
		DoMethod(imap_folders_list, MUIM_NList_Redraw, MUIV_NList_Redraw_Active);
	}
}

static void imap_folders_unsubscribe_pressed(void)
{
	struct imap_folder_entry *entry;
	DoMethod(imap_folders_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, (ULONG)&entry);
	if (entry)
	{
		entry->subscribed = 0;
		DoMethod(imap_folders_list, MUIM_NList_Redraw, MUIV_NList_Redraw_Active);
	}
}

static void folder_ok(void)
{
	set(folder_wnd, MUIA_Window_Open, FALSE);
	callback_change_folder_attrs();
	changed_folder = NULL;
}

struct folder *folder_get_changed_folder(void)
{
	return changed_folder;
}

char *folder_get_changed_name(void)
{
	return (char*)xget(name_string,MUIA_String_Contents);
}

char *folder_get_changed_path(void)
{
	return (char*)xget(path_string,MUIA_String_Contents);
}

int folder_get_changed_type(void)
{
	return (int)xget(type_cycle, MUIA_Cycle_Active);
}

char *folder_get_changed_defto(void)
{
	return (char *)xget(defto_string,MUIA_UTF8String_Contents);
}

char *folder_get_changed_deffrom(void)
{
	struct account *ac = (struct account*)xget(from_accountpop, MUIA_AccountPop_Account);
	if (ac) return ac->email;
	return NULL;
}

char *folder_get_changed_defreplyto(void)
{
	return (char*)xget(replyto_string,MUIA_UTF8String_Contents);
}

char *folder_get_changed_defsignature(void)
{
	return (char *)xget(defsign_cycle, MUIA_SignatureCycle_SignatureName);
}

int folder_get_changed_primary_sort(void)
{
	return (int)xget(prim_cycle, MUIA_Cycle_Active) | (xget(prim_reverse_check, MUIA_Selected) ? FOLDER_SORT_REVERSE : 0);
}

int folder_get_changed_secondary_sort(void)
{
	return (int)xget(second_cycle, MUIA_Cycle_Active) | (xget(second_reverse_check, MUIA_Selected) ? FOLDER_SORT_REVERSE : 0);
}

/* Refresh the Signature Cycle if the config has changed */
void folder_refresh_signature_cycle(void)
{
	if (folder_wnd)
	{
		DoMethod(compose_mail_properties_group, MUIM_Group_InitChange);
		DoMethod(defsign_cycle, MUIM_SignatureCycle_Refresh, (ULONG)&user.config.signature_list);
		DoMethod(compose_mail_properties_group, MUIM_Group_ExitChange);
	}
}

static void init_folder(void)
{
	Object *ok_button, *cancel_button;
	static char *type_array[5];
	static char *prim_sort_array[12];
	static char *second_sort_array[12];

	type_array[0] = _("received");
	type_array[1] = _("sent");
	type_array[2] = _("received and sent");
	type_array[3] = _("mailinglist");

	prim_sort_array[FOLDER_SORT_STATUS] = _("Status");
	prim_sort_array[FOLDER_SORT_FROMTO] = _("From/To");
	prim_sort_array[FOLDER_SORT_SUBJECT] = _("Subject");
	prim_sort_array[FOLDER_SORT_REPLY] = _("Reply");
	prim_sort_array[FOLDER_SORT_DATE] = _("Date");
	prim_sort_array[FOLDER_SORT_SIZE] = _("Size");
	prim_sort_array[FOLDER_SORT_FILENAME] = _("Filename");
	prim_sort_array[FOLDER_SORT_POP3] = _("POP3");
	prim_sort_array[FOLDER_SORT_RECV] = _("Received");

	second_sort_array[FOLDER_SORT_STATUS] = _("Status");
	second_sort_array[FOLDER_SORT_FROMTO] = _("From/TO");
	second_sort_array[FOLDER_SORT_SUBJECT] = _("Subject");
	second_sort_array[FOLDER_SORT_REPLY] = _("Reply");
	second_sort_array[FOLDER_SORT_DATE] = _("Date");
	second_sort_array[FOLDER_SORT_SIZE] = _("Size");
	second_sort_array[FOLDER_SORT_FILENAME] = _("Filename");
	second_sort_array[FOLDER_SORT_POP3] = _("POP3");
	second_sort_array[FOLDER_SORT_RECV] = _("Received");

	init_hook(&imap_folders_construct_hook, (HOOKFUNC)imap_folders_construct);
	init_hook(&imap_folders_destruct_hook, (HOOKFUNC)imap_folders_destruct);
	init_hook(&imap_folders_display_hook, (HOOKFUNC)imap_folders_display);

	folder_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('F','O','L','D'),
		WindowContents, VGroup,
			Child, HorizLineTextObject(_("Folder Properties")),
			Child, folder_properties_group = ColGroup(2),
				Child, name_label = MakeLabel(_("N_ame")),
				Child, name_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_ControlChar, 'a',
					MUIA_String_AdvanceOnCR, TRUE,
					End,

				Child, imap_label = MakeLabel(_("IMAP path")),
				Child, imap_string = BetterStringObject,
					TextFrame,
					MUIA_BetterString_NoInput, TRUE,
					End,

				Child, path_label = MakeLabel(_("Local path")),
				Child, path_string = BetterStringObject,
					TextFrame,
					MUIA_BetterString_NoInput, TRUE,
					End,

				Child, type_label = MakeLabel(_("_Type")),
				Child, type_cycle = MakeCycle(_("_Type"), type_array),

				Child, prim_label = MakeLabel(_("_Primary sort")),
				Child, prim_group = HGroup,
					Child, prim_cycle = MakeCycle(_("_Primary sort"), prim_sort_array),
					Child, prim_reverse_check = MakeCheck(_("Reverse"), 0),
					Child, prim_reverse_label = MakeLabel(_("Reverse")),
					End,

				Child, second_label = MakeLabel(_("_Secondary sort")),
				Child, second_group = HGroup,
					Child, second_cycle = MakeCycle(_("_Secondary sort"), second_sort_array),
					Child, second_reverse_check = MakeCheck(_("Reverse"), 0),
					Child, second_reverse_label = MakeLabel(_("Reverse")),
					End,
				End,

			Child, HorizLineTextObject(_("Compose Mail Properties")),
			Child, compose_mail_properties_group = ColGroup(2),
				Child, MakeLabel(_("From")),
				Child, from_accountpop = AccountPopObject, MUIA_AccountPop_HasDefaultEntry, TRUE, End,

				Child, MakeLabel(_("T_o")),
				Child, defto_string = AddressStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_ControlChar, GetControlChar(_("T_o")),
					End,

				Child, MakeLabel(_("_Reply to")),
				Child, replyto_string = AddressStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_ControlChar, GetControlChar(_("_Reply to")),
					End,

				/* This two MUST always be the last objects of this group! */
				/* because the SignatureCycle() gets removed and reinserted for update. */
				Child, MakeLabel(_("Signature")),
				Child, defsign_cycle = SignatureCycleObject, MUIA_SignatureCycle_HasDefaultEntry, TRUE, End,

				End,

			Child, imap_folders_horizline = HorizLineTextObject(_("IMAP Properties")),
			Child, imap_folders_group = VGroup,
				Child, imap_folders_listview = NListviewObject,
					MUIA_NListview_NList, imap_folders_list = NListObject,
						MUIA_NList_ConstructHook, &imap_folders_construct_hook,
						MUIA_NList_DestructHook, &imap_folders_destruct_hook,
						MUIA_NList_DisplayHook, &imap_folders_display_hook,
						End,
					End,
				Child, HGroup,
					Child, imap_folders_subscribe = MakeButton(_("Subscribe")),
					Child, imap_folders_unsubscribe = MakeButton(_("Unsubscribe")),
					Child, imap_folders_check = MakeButton(_("Reload from server")),
					Child, imap_folders_submit = MakeButton(_("Submit to server")),
					End,
				End,

			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton(_("_Ok")),
				Child, cancel_button = MakeButton(_("_Cancel")),
				End,
			End,
		End;

	if (folder_wnd)
	{
		DoMethod(App, OM_ADDMEMBER, (ULONG)folder_wnd);
		DoMethod(folder_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)folder_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)folder_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)folder_ok);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)folder_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		group_mode = 0;

		DoMethod(imap_folders_check, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)folder_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)imap_folders_check_pressed);
		DoMethod(imap_folders_submit, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)folder_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)imap_folders_submit_pressed);
		DoMethod(imap_folders_subscribe, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)folder_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)imap_folders_subscribe_pressed);
		DoMethod(imap_folders_unsubscribe, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)folder_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)imap_folders_unsubscribe_pressed);
		imap_mode = 1;
	}
}

void folder_edit(struct folder *f)
{
	if (!f) return;

	if (!folder_wnd)
	{
		init_folder();
		if (!folder_wnd) return;
	}

	if (f->special == FOLDER_SPECIAL_GROUP)
	{
		if (f->is_imap)
		{
			set(folder_wnd,MUIA_Window_Title,_("SimpleMail - Edit IMAP Server"));

			set(imap_folders_group,MUIA_ShowMe, TRUE);
			set(imap_folders_horizline,MUIA_ShowMe, TRUE);
		} else
		{
			set(folder_wnd,MUIA_Window_Title,_("SimpleMail - Edit folder group"));
			set(imap_folders_group,MUIA_ShowMe, FALSE);
			set(imap_folders_horizline,MUIA_ShowMe, FALSE);
		}


		if (!group_mode)
		{
			DoMethod(folder_properties_group,MUIM_Group_InitChange);

			DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)path_label);
			DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)path_string);
			DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)type_label);
			DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)type_cycle);
			DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)prim_label);
			DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)prim_group);
			DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)second_label);
			DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)second_group);
			if (imap_mode)
			{
				DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)imap_label);
				DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)imap_string);
				imap_mode = 0;
			}
			group_mode = 1;
			DoMethod(folder_properties_group,MUIM_Group_ExitChange);
		}
	} else
	{
		set(folder_wnd,MUIA_Window_Title, _("SimpleMail - Edit folder"));
		set(imap_folders_group,MUIA_ShowMe, FALSE);
		set(imap_folders_horizline,MUIA_ShowMe, FALSE);
		DoMethod(folder_properties_group,MUIM_Group_InitChange);
		if (group_mode)
		{
			DoMethod(folder_properties_group, OM_ADDMEMBER, (ULONG)path_label);
			DoMethod(folder_properties_group, OM_ADDMEMBER, (ULONG)path_string);

			if (f->is_imap && !imap_mode)
			{
				DoMethod(folder_properties_group, OM_ADDMEMBER, (ULONG)imap_label);
				DoMethod(folder_properties_group, OM_ADDMEMBER, (ULONG)imap_string);
				imap_mode = 1;
			} else
			if (!f->is_imap && imap_mode)
			{
				DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)imap_label);
				DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)imap_string);
				imap_mode = 0;
			}

			DoMethod(folder_properties_group, OM_ADDMEMBER, (ULONG)type_label);
			DoMethod(folder_properties_group, OM_ADDMEMBER, (ULONG)type_cycle);
			DoMethod(folder_properties_group, OM_ADDMEMBER, (ULONG)prim_label);
			DoMethod(folder_properties_group, OM_ADDMEMBER, (ULONG)prim_group);
			DoMethod(folder_properties_group, OM_ADDMEMBER, (ULONG)second_label);
			DoMethod(folder_properties_group, OM_ADDMEMBER, (ULONG)second_group);
			group_mode = 0;
		} else
		{
			if (f->is_imap && !imap_mode)
			{
				DoMethod(folder_properties_group, OM_ADDMEMBER, (ULONG)imap_label);
				DoMethod(folder_properties_group, OM_ADDMEMBER, (ULONG)imap_string);
				imap_mode = 1;
			} else
			if (!f->is_imap && imap_mode)
			{
				DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)imap_label);
				DoMethod(folder_properties_group, OM_REMMEMBER, (ULONG)imap_string);
				imap_mode = 0;
			}
		}
		if (imap_mode)
		{
			DoMethod(folder_properties_group, MUIM_Group_Sort,
			         (ULONG)name_label, (ULONG)name_string,
			         (ULONG)path_label, (ULONG)path_string,
			         (ULONG)imap_label, (ULONG)imap_string,
			         (ULONG)type_label, (ULONG)type_cycle,
			         (ULONG)prim_label, (ULONG)prim_group,
			         (ULONG)second_label, (ULONG)second_group,
			         NULL);
		} else
		{
			DoMethod(folder_properties_group, MUIM_Group_Sort,
			         (ULONG)name_label, (ULONG)name_string,
			         (ULONG)path_label, (ULONG)path_string,
			         (ULONG)type_label, (ULONG)type_cycle,
			         (ULONG)prim_label, (ULONG)prim_group,
			         (ULONG)second_label, (ULONG)second_group,
			         NULL);
		}
		DoMethod(folder_properties_group,MUIM_Group_ExitChange);
	}

	set(name_string, MUIA_String_Contents, f->name);
	set(path_string, MUIA_String_Contents, f->path);
	set(type_cycle, MUIA_Cycle_Active, f->type);
	set(defto_string, MUIA_UTF8String_Contents, f->def_to);
	set(from_accountpop, MUIA_AccountPop_Account, account_find_by_from(f->def_from));
	set(replyto_string, MUIA_UTF8String_Contents, f->def_replyto);
	set(defsign_cycle, MUIA_SignatureCycle_SignatureName, f->def_signature);

	set(prim_cycle, MUIA_Cycle_Active, folder_get_primary_sort(f) & FOLDER_SORT_MODEMASK);
	set(prim_reverse_check, MUIA_Selected, folder_get_primary_sort(f) & FOLDER_SORT_REVERSE);
	set(second_cycle, MUIA_Cycle_Active, folder_get_secondary_sort(f) & FOLDER_SORT_MODEMASK);
	set(second_reverse_check, MUIA_Selected, folder_get_secondary_sort(f) & FOLDER_SORT_REVERSE);

	{
		int buf_len = strlen(f->imap_server) + strlen(f->imap_path) + strlen(f->imap_user) + 40;
		char *buf = malloc(buf_len);
		if (buf)
		{
			sm_snprintf(buf,buf_len,"imap://%s@%s:/%s",f->imap_user,f->imap_server,f->imap_path);
			set(imap_string, MUIA_String_Contents, buf);
			free(buf);
		} else
		{
			set(imap_string, MUIA_String_Contents, f->imap_server);
		}
	}
	changed_folder = f;

	if (f->is_imap)
	{
		struct string_node *node;
		DoMethod(imap_folders_list,MUIM_NList_Clear);
		node = (struct string_node*)list_first(&f->imap_all_folder_list);
		while (node)
		{
			struct imap_folder_entry entry;
			entry.name = node->string;
			entry.subscribed = !!string_list_find(&f->imap_sub_folder_list,node->string);

			DoMethod(imap_folders_list, MUIM_NList_InsertSingle, (ULONG)&entry, MUIV_NList_Insert_Bottom);
			node = (struct string_node*)node_next(&node->node);
		}
	}

	set(folder_wnd, MUIA_Window_ActiveObject, name_string);
	set(folder_wnd, MUIA_Window_Open, TRUE);
}

void folder_fill_lists(struct list *list, struct list *sub_folder_list)
{
	if (imap_folders_list)
	{
		struct string_node *node;
		DoMethod(imap_folders_list,MUIM_NList_Clear);
		node = (struct string_node*)list_first(list);
		while (node)
		{
			struct imap_folder_entry entry;
			entry.name = node->string;
			entry.subscribed = !!string_list_find(sub_folder_list,node->string);

			DoMethod(imap_folders_list, MUIM_NList_InsertSingle, (ULONG)&entry, MUIV_NList_Insert_Bottom);
			node = (struct string_node*)node_next(&node->node);
		}
	}
}

/*************************************************************/

static Object *new_folder_wnd;
static Object *new_folder_name_string;
static Object *new_folder_path_string;

void new_folder_create(void)
{
	char *name_str = (char*)xget(new_folder_name_string, MUIA_String_Contents);
	char *path_str = (char*)xget(new_folder_path_string, MUIA_String_Contents);

	set(new_folder_wnd,MUIA_Window_Open, FALSE);
	callback_new_folder_path(path_str,name_str);
}

void init_new_folder(void)
{
	Object *create_button, *cancel_button;

	new_folder_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('F','O','L','N'),
		MUIA_Window_Title, "SimpleMail - New folder",
		WindowContents, VGroup,
			Child, ColGroup(2),
				Child, MakeLabel(_("Name")),
				Child, new_folder_name_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_String_AdvanceOnCR, TRUE,
					End,

				Child, MakeLabel(_("Path")),
				Child, PopaslObject,
					ASLFR_DrawersOnly, TRUE,
					MUIA_Popstring_Button, PopButton(MUII_PopDrawer),
					MUIA_Popstring_String, new_folder_path_string = BetterStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						MUIA_String_AdvanceOnCR, TRUE,
						End,
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, create_button = MakeButton(_("_OK")),
				Child, cancel_button = MakeButton(_("_Cancel")),
				End,
			End,
		End;

	if (new_folder_wnd)
	{
		DoMethod(App, OM_ADDMEMBER, (ULONG)new_folder_wnd);
		DoMethod(new_folder_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)new_folder_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(create_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)new_folder_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)new_folder_create);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)new_folder_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
	}

}

void folder_edit_new_path(char *init_path)
{
	if (!new_folder_wnd)
	{
		init_new_folder();
		if (!new_folder_wnd) return;
	}

	if (new_folder_wnd)
	{
		if (init_path)
		{
			char *buf = malloc(strlen(FilePart(init_path))+1);
			if (buf)
			{
				strcpy(buf,FilePart(init_path));
				*buf = ToUpper(*buf);
				set(new_folder_name_string, MUIA_String_Contents, buf);
				free(buf);
			}
		}
		set(new_folder_path_string, MUIA_String_Contents, init_path);
		set(new_folder_wnd, MUIA_Window_Open, TRUE);
	}
}


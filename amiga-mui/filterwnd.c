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
** filterwnd.c
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <mui/betterstring_mcc.h>
#include <mui/NListview_mcc.h>
#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "configuration.h"
#include "filter.h"
#include "folder.h"
#include "support_indep.h"

#include "compiler.h"
#include "filterwnd.h"
#include "muistuff.h"

struct rule
{
	char *name; /* Name of the rule */
	Object *page;
	int (*create)(struct rule*);
	void (*set_page)(struct filter_rule*);
	void (*get_page)(struct filter_rule*);
	int type;
};

static Object *filter_wnd;
static Object *filter_name_string;
static Object *filter_listview;
static Object *filter_list;
static Object *filter_new_button;
static Object *filter_remove_button;

static struct Hook filter_construct_hook;
static struct Hook filter_destruct_hook;
static struct Hook filter_display_hook;

static int rules_create_from(struct rule *rule);
static int rules_create_subject(struct rule *rule);
static int rules_create_header(struct rule *rule);

static void rules_set_from(struct filter_rule *rule);
static void rules_set_subject(struct filter_rule *rule);
static void rules_set_header(struct filter_rule *rule);

static void rules_get_from(struct filter_rule *rule);
static void rules_get_subject(struct filter_rule *rule);
static void rules_get_header(struct filter_rule *rule);

struct rule rules[] = {
	{"From match", NULL, rules_create_from, rules_set_from, rules_get_from, RULE_FROM_MATCH},
	{"Subject macth", NULL, rules_create_subject, rules_set_subject, rules_get_subject, RULE_SUBJECT_MATCH},
	{"Header match", NULL, rules_create_header, rules_set_header, rules_get_header, RULE_HEADER_MATCH},
	{"Attachment", NULL, NULL, NULL, NULL, RULE_ATTACHMENT_MATCH},
	{NULL,NULL,NULL,NULL},
};
char *rule_cycle_array[sizeof(rules)/sizeof(struct rule)];

STATIC ASM APTR filter_construct(register __a2 APTR pool, register __a1 struct filter *ent)
{
	return filter_duplicate(ent);
}

STATIC ASM VOID filter_destruct( register __a2 APTR pool, register __a1 struct filter *ent)
{
	if (ent) filter_dispose(ent);
}

STATIC ASM VOID filter_display(register __a0 struct Hook *h, register __a2 char **array, register __a1 struct filter *ent)
{
	if (ent) *array = ent->name;
}

STATIC ASM VOID rules_display(register __a0 struct Hook *h, register __a2 char **array, register __a1 struct filter_rule *rule)
{
	if (rule) *array = filter_get_rule_string(rule);
}

STATIC ASM VOID move_objstr(register __a2 Object *list, register __a1 Object *str)
{
	char *x;
	DoMethod(list,MUIM_NList_GetEntry,MUIV_List_GetEntry_Active,&x);
	set(str,MUIA_Text_Contents,x);
}

STATIC ASM LONG move_strobj(register __a2 Object *list, register __a1 Object *str)
{
	char *x,*s;
	int i = 0;
	get(str,MUIA_Text_Contents,&s);

	while (1)
	{
		DoMethod(list,MUIM_NList_GetEntry,i,&x);
		if (!x)
		{
			set(list,MUIA_NList_Active,MUIV_NList_Active_Off);
			break;
		}
		else if (!mystricmp(x,s))
	  {
			set(list,MUIA_NList_Active,i);
			break;
		}
		i++;
	}
	return 1;
}


static Object *rules_wnd;
static Object *rules_page_listview;
static Object *rules_page_group;
static Object *rules_page_space;
static Object *rules_page_cycle;
static Object *rules_from_string;
static Object *rules_subject_string;
static Object *rules_header_name_string;
static Object *rules_header_contents_string;
static Object *rules_move_check;
static Object *rules_move_text;

static struct filter_rule *rules_active_rule;

static struct filter *rules_filter;

/**************************************************************************
 Create the from match object
**************************************************************************/
static int rules_create_from(struct rule *rule)
{
	rule->page = HGroup,
		MUIA_ShowMe, TRUE,
		MUIA_UserData, 1,
//		MUIA_Disabled, TRUE,
		Child, MakeLabel("Address"),
		Child, rules_from_string = BetterStringObject,
			StringFrame,
			End,
		End;

	return rule->page?1:0;
}

/**************************************************************************
 Create the subject match object
**************************************************************************/
static int rules_create_subject(struct rule *rule)
{
	rule->page = HGroup,
		MUIA_ShowMe, FALSE,
		Child, MakeLabel("Subject"),
		Child, rules_subject_string = BetterStringObject,
			StringFrame,
			End,
		End;

	return rule->page?1:0;
}

/**************************************************************************
 Create the header match object
**************************************************************************/
static int rules_create_header(struct rule *rule)
{
	rule->page = VGroup,
		MUIA_ShowMe, FALSE,
		Child, HGroup,
			Child, MakeLabel("Name"),
			Child, rules_header_name_string = BetterStringObject,
				StringFrame,
				End,
			End,
		Child, HGroup,
			Child, MakeLabel("Contents"),
			Child, rules_header_contents_string = BetterStringObject,
				StringFrame,
				End,
			End,
		End;

	return rule->page?1:0;
}


/**************************************************************************
...
**************************************************************************/
static void rules_set_from(struct filter_rule *rule)
{
	set(rules_from_string, MUIA_String_Contents, rule?rule->u.from.from:"");
}

/**************************************************************************
...
**************************************************************************/
static void rules_set_subject(struct filter_rule *rule)
{
	set(rules_subject_string, MUIA_String_Contents, rule?rule->u.subject.subject:"");
}

/**************************************************************************
...
**************************************************************************/
static void rules_set_header(struct filter_rule *rule)
{
	set(rules_header_name_string, MUIA_String_Contents, rule?rule->u.header.name:"");
	set(rules_header_contents_string, MUIA_String_Contents, rule?rule->u.header.contents:"");
}


/**************************************************************************
...
**************************************************************************/
static void rules_get_from(struct filter_rule *rule)
{
	if (rule->u.from.from) free(rule->u.from.from);
	rule->u.from.from = mystrdup((char*)xget(rules_from_string,MUIA_String_Contents));
}

/**************************************************************************
...
**************************************************************************/
static void rules_get_subject(struct filter_rule *rule)
{
	if (rule->u.subject.subject) free(rule->u.subject.subject);
	rule->u.subject.subject = mystrdup((char*)xget(rules_subject_string,MUIA_String_Contents));
}

/**************************************************************************
...
**************************************************************************/
static void rules_get_header(struct filter_rule *rule)
{
	if (rule->u.header.name) free(rule->u.header.name);
	rule->u.header.name = mystrdup((char*)xget(rules_header_name_string,MUIA_String_Contents));
	if (rule->u.header.contents) free(rule->u.header.contents);
	rule->u.header.contents = mystrdup((char*)xget(rules_header_contents_string,MUIA_String_Contents));
}

static void rules_active(void);

/**************************************************************************
 Ok the rule 
**************************************************************************/
static void rules_ok(void)
{
	set(rules_wnd, MUIA_Window_Open, FALSE);
	if (rules_filter)
	{
		if (rules_filter) free(rules_filter->dest_folder);
		rules_filter->dest_folder = mystrdup((char*)xget(rules_move_text,MUIA_Text_Contents));
		rules_filter->use_dest_folder = xget(rules_move_check,MUIA_Selected);
		rules_active();
	}
}

/**************************************************************************
 Add a new rule
**************************************************************************/
static void rules_new(void)
{
	struct filter *f;
	DoMethod(filter_listview, MUIM_NList_GetEntry, xget(filter_listview, MUIA_NList_Active),&f);
	if (f)
	{
		struct filter_rule *fr;

		fr = filter_create_and_add_rule(f, xget(rules_page_cycle, MUIA_Cycle_Active));
		DoMethod(rules_page_listview,MUIM_NList_InsertSingle, fr, MUIV_NList_Insert_Bottom);
		set(rules_page_listview, MUIA_NList_Active, MUIV_NList_Active_Bottom);
	}
}

/**************************************************************************
 Remove a new rule
**************************************************************************/
static void rules_rem(void)
{
	struct filter_rule *fr;
	rules_active_rule = NULL;
	DoMethod(rules_page_listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &fr);
	DoMethod(rules_page_listview, MUIM_NList_Remove, MUIV_NList_Remove_Active);
	filter_remove_rule(fr);
}


/**************************************************************************
 A new rule is active
**************************************************************************/
static void show_rule_group(int type)
{
	int i,changed = 0;
	struct filter_rule *fr;

	DoMethod(rules_page_listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &fr);

	for (i=0;rules[i].name;i++)
	{
		if (rules[i].type == type)
		{
			if (!xget(rules[i].page,MUIA_UserData))
			{
				if (!changed)
				{
					DoMethod(rules_page_group,MUIM_Group_InitChange);
					changed = 1;
				}
				SetAttrs(rules[i].page,
						MUIA_ShowMe, TRUE,
						MUIA_UserData, 1,
						MUIA_Disabled, FALSE,
						TAG_DONE);
			}
			if (rules[i].set_page) rules[i].set_page(fr);
		} else
		{
			if (xget(rules[i].page,MUIA_UserData))
			{
				if (!changed)
				{
					DoMethod(rules_page_group,MUIM_Group_InitChange);
					changed = 1;
				}
				SetAttrs(rules[i].page,
							MUIA_ShowMe, FALSE,
							MUIA_UserData, 0,
							MUIA_Disabled, FALSE,
							TAG_DONE);
			}
		}
	}

	if (changed)
	{
		DoMethod(rules_page_group,MUIM_Group_ExitChange);
	}
}


/**************************************************************************
 The rule type cycle has been clicked
**************************************************************************/
static void rules_cycle(void)
{
	struct filter_rule *fr;
	DoMethod(rules_page_listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &fr);

	if (!fr)
	{
		int to_show = xget(rules_page_cycle,MUIA_Cycle_Active);
		show_rule_group(to_show);
	}
}

/**************************************************************************
 A new rule is active
**************************************************************************/
static void rules_active(void)
{
	struct filter_rule *fr;
	int i,changed = 0;

	int to_show = 0;

	if (rules_active_rule)
	{
		if (rules[rules_active_rule->type].get_page)
		{
			rules[rules_active_rule->type].get_page(rules_active_rule);
			DoMethod(rules_page_listview, MUIM_NList_RedrawEntry, rules_active_rule);
		}
	}

	DoMethod(rules_page_listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &fr);

	rules_active_rule = fr;

	if (fr)
	{
		for (i=0;rules[i].name;i++)
		{
			if (rules[i].type == fr->type)
			{
				to_show = fr->type;
				break;
			}
		}
	} else to_show = xget(rules_page_cycle, MUIA_Cycle_Active);
	show_rule_group(to_show);
}

/**************************************************************************
 Init rules
**************************************************************************/
static void init_rules(void)
{
	Object *ok_button, *rule_add_button, *folder_list, *move_popobject, *rule_rem_button;
	static struct Hook rules_display_hook;
	static struct Hook move_objstr_hook, move_strobj_hook;
	struct folder *f;
	int i;

	init_hook(&rules_display_hook,(HOOKFUNC)rules_display);
	init_hook(&move_objstr_hook, (HOOKFUNC)move_objstr);
	init_hook(&move_strobj_hook, (HOOKFUNC)move_strobj);

	for (i=0;i<sizeof(rules)/sizeof(struct rule);i++)
	{
		rule_cycle_array[i] = rules[i].name;
		if (rules[i].create) rules[i].create(&rules[i]);
	}

	rules_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('R','U','L','S'),
		MUIA_Window_Title, "SimpleMail - Edit Rule",
		WindowContents, VGroup,
			Child, VGroup,
				Child, HGroup,
					Child, VGroup,
						Child, rules_page_listview = NListviewObject,
							MUIA_NListview_NList, NListObject,
								MUIA_NList_DisplayHook, &rules_display_hook,
								End,
							End,
						Child, HGroup,
							Child, rules_page_cycle = MakeCycle(NULL,rule_cycle_array),
							Child, rule_add_button = MakeButton("Add"),
							Child, rule_rem_button = MakeButton("Remove"),
							End,
						End,
					Child, rules_page_group = VGroup,
						Child, rules_page_space = HVSpace,
						End,
					End,
				Child, VGroup,
					Child, ColGroup(3),
						Child, MakeLabel("Move to Folder"),
						Child, rules_move_check = MakeCheck("Move to Folder",FALSE),
						Child, move_popobject = PopobjectObject,
							MUIA_Disabled, TRUE,
							MUIA_Popstring_Button, PopButton(MUII_PopUp),
							MUIA_Popstring_String, rules_move_text = TextObject, TextFrame, End,
							MUIA_Popobject_ObjStrHook, &move_objstr_hook,
							MUIA_Popobject_StrObjHook, &move_strobj_hook,
							MUIA_Popobject_Object, NListviewObject,
								MUIA_NListview_NList, folder_list = NListObject,
									MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
									MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
									End,
								End,
							End,
						End,
					End,
				End,
			Child, HorizLineObject,
			Child, ok_button = MakeButton("_Ok"),
			End,
		End;

	if (rules_wnd)
	{
		for (i=0;i<sizeof(rules)/sizeof(struct rule);i++)
		{
			if (rules[i].page) DoMethod(rules_page_group, OM_ADDMEMBER, rules[i].page);
		}

		DoMethod(rules_page_group, OM_ADDMEMBER, HVSpace);
		DoMethod(App, OM_ADDMEMBER, rules_wnd);
		DoMethod(rules_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, rules_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, rules_wnd, 3, MUIM_CallHook, &hook_standard, rules_ok);
		DoMethod(rules_page_cycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, rules_wnd, 3, MUIM_CallHook, &hook_standard, rules_cycle);
		DoMethod(rule_add_button, MUIM_Notify, MUIA_Pressed, FALSE, rules_wnd, 3, MUIM_CallHook, &hook_standard, rules_new);
		DoMethod(rule_rem_button, MUIM_Notify, MUIA_Pressed, FALSE, rules_wnd, 3, MUIM_CallHook, &hook_standard, rules_rem);
		DoMethod(rules_page_listview, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, rules_wnd, 3, MUIM_CallHook, &hook_standard, rules_active);
		DoMethod(rules_move_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, move_popobject, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
		DoMethod(folder_list, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, move_popobject, 2, MUIM_Popstring_Close, 1);
	}

	/* Count the number of folders */
	f = folder_first();
	while (f)
	{
		DoMethod(folder_list, MUIM_NList_InsertSingle, f->name, MUIV_NList_Insert_Bottom);
		f = folder_next(f);
	}
}

/**************************************************************************
 Set the rules
**************************************************************************/
static void set_rules(struct filter *f)
{
	int i;
	struct filter_rule *fr;

	rules_filter = f;
	DoMethod(rules_page_listview, MUIM_NList_Clear);

	fr = (struct filter_rule*)list_first(&f->rules_list);

	while (fr)
	{
		DoMethod(rules_page_listview, MUIM_NList_InsertSingle, fr, MUIV_NList_Insert_Bottom);
		fr = (struct filter_rule*)node_next(&fr->node);
	}

	set(rules_move_check, MUIA_Selected, f->use_dest_folder);
	set(rules_move_text, MUIA_Text_Contents, f->dest_folder);
}

/**************************************************************************
 Edit the currenty selected rule
**************************************************************************/
static void filter_edit(void)
{
	if (!rules_wnd) init_rules();
	if (rules_wnd)
	{
		struct filter *f;
		DoMethod(filter_listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &f);
		set_rules(f);
		set(rules_wnd,MUIA_Window_Open,TRUE);
	}
}

/**************************************************************************
 Create a new rule
**************************************************************************/
static void filter_new(void)
{
	struct filter *f = filter_create();
	if (f)
	{
		DoMethod(filter_listview, MUIM_NList_InsertSingle, f, MUIV_NList_Insert_Bottom);
		filter_dispose(f);
		set(filter_listview, MUIA_NList_Active, xget(filter_listview, MUIA_NList_Entries)-1);
		filter_edit();
	}
}

/**************************************************************************
 Ok, callback and close the filterwindow
**************************************************************************/
static void filter_ok(void)
{
	struct filter *f;
	int i;

	set(filter_wnd,MUIA_Window_Open,FALSE);

	filter_list_clear();

	for (i=0;i<xget(filter_listview, MUIA_NList_Entries);i++)
	{
		DoMethod(filter_listview, MUIM_NList_GetEntry, i, &f);
		if (f)
		{
			filter_list_add_duplicate(f);
		}
	}

	DoMethod(filter_listview, MUIM_NList_Clear);
}

/**************************************************************************
 New Entry has been activated
**************************************************************************/
static void filter_active(void)
{
	struct filter *f;
	DoMethod(filter_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &f);
	if (f) nnset(filter_name_string,MUIA_String_Contents, f->name);
}

/**************************************************************************
 A new name
**************************************************************************/
static void filter_name(void)
{
	struct filter *f;
	DoMethod(filter_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &f);
	if (f)
	{
		if (f->name) free(f->name);
		f->name = mystrdup((char*)xget(filter_name_string, MUIA_String_Contents));
		DoMethod(filter_list,MUIM_NList_Redraw,MUIV_NList_Redraw_Active);
	}
}

/**************************************************************************
 Init the filter window
**************************************************************************/
static void init_filter(void)
{
	Object *ok_button, *cancel_button, *save_button;

	init_hook(&filter_construct_hook,(HOOKFUNC)filter_construct);
	init_hook(&filter_destruct_hook,(HOOKFUNC)filter_destruct);
	init_hook(&filter_display_hook,(HOOKFUNC)filter_display);

	filter_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('F','I','L','T'),
		MUIA_Window_Title, "SimpleMail - Edit Filter",
		WindowContents, VGroup,
			Child, VGroup,
				Child, VGroup,
					MUIA_Group_Spacing, 0,
					Child, filter_listview = NListviewObject,
						MUIA_NListview_NList, filter_list = NListObject,
							MUIA_NList_ConstructHook, &filter_construct_hook,
							MUIA_NList_DestructHook, &filter_destruct_hook,
							MUIA_NList_DisplayHook, &filter_display_hook,
							End,
						End,
					Child, HGroup,
						Child, filter_name_string = BetterStringObject,
							StringFrame,
							End,
						End,
					Child, HGroup,
						Child, filter_new_button = MakeButton("_New..."),
						Child, filter_remove_button = MakeButton("_Remove"),
						End,
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, save_button = MakeButton("_Save"),
				Child, ok_button = MakeButton("_Use"),
				Child, cancel_button = MakeButton("_Cancel"),
				End,
			End,
		End;

	if (filter_wnd)
	{
		DoMethod(App, OM_ADDMEMBER, filter_wnd);
		DoMethod(filter_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, filter_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_wnd, 3, MUIM_CallHook, &hook_standard, filter_ok);
		DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_wnd, 3, MUIM_CallHook, &hook_standard, filter_ok);
		DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_wnd, 3, MUIM_CallHook, &hook_standard, save_filter);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(filter_new_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_wnd, 3, MUIM_CallHook, &hook_standard, filter_new);
		DoMethod(filter_remove_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_listview, 2, MUIM_NList_Remove, MUIV_NList_Remove_Active);
		DoMethod(filter_list, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, filter_wnd, 3, MUIM_CallHook, &hook_standard, filter_edit);

		set(filter_name_string,MUIA_String_AttachedList,filter_listview);
		DoMethod(filter_list, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, filter_list, 3, MUIM_CallHook, &hook_standard, filter_active);
		DoMethod(filter_name_string, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, filter_list, 3, MUIM_CallHook, &hook_standard, filter_name);
	}
}

/**************************************************************************
 Opens the filter window
**************************************************************************/
void filter_open(void)
{
	struct filter *f;

	if (!filter_wnd)
	{
		init_filter();
		if (!filter_wnd) return;
	}

	/* Clear the filter listview contents */
	DoMethod(filter_list, MUIM_NList_Clear);

	/* Add the filters to the list */
	f = filter_list_first();
	while (f)
	{
		DoMethod(filter_list, MUIM_NList_InsertSingle, f, MUIV_NList_Insert_Bottom);
		f = filter_list_next(f);
	}

	set(filter_wnd, MUIA_Window_Open, TRUE);
}


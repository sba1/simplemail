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

#include "filter.h"
#include "filterwnd.h"

#if 0

struct rule
{
	char *name; /* Name of the rule */
	Object *page;
	int (*create)(struct rule*);
	int type;
};

static Object *filter_wnd;
static Object *filter_name_string;
static Object *filter_listview;
static Object *filter_new_button;
static Object *filter_remove_button;

static struct Hook filter_construct_hook;
static struct Hook filter_destruct_hook;
static struct Hook filter_display_hook;

static int rules_create_from(struct rule *rule);

struct rule rules[] = {
	{"From match",NULL,rules_create_from,RULE_FROM_MATCH},
	{"Subject macth",NULL,NULL,RULE_SUBJECT_MATCH},
	{"Header match",NULL,NULL,RULE_HEADER_MATCH},
	{NULL,NULL,NULL,NULL},
};
char *rule_cycle_array[sizeof(rules)/sizeof(struct rule)];


STATIC ASM APTR filter_construct(register __a2 APTR pool, register __a1 struct filter *ent)
{
	return filter_duplicate(ent);
}

STATIC ASM VOID filter_destruct( register __a2 APTR pool, register __a1 struct filter *ent)
{
	if (ent)
	{
		filter_dispose(ent);
	}
}

STATIC ASM VOID filter_display(register __a0 struct Hook *h, register __a2 char **array, register __a1 struct filter *ent)
{
	if (ent)
	{
		*array = ent->name;
	}
}

STATIC ASM VOID rules_display(register __a0 struct Hook *h, register __a2 char **array, register __a1 struct filter_rule *rule)
{
	if (rule)
	{
		*array = filter_get_rule_string(rule);
	}
}

static Object *rules_wnd;
static Object *rules_page_listview;
static Object *rules_page_group;
static Object *rules_page_space;
static Object *rules_page_cycle;

#endif

/**************************************************************************
 Create the from match object
**************************************************************************/
#if 0
static int rules_create_from(struct rule *rule)
{
	rule->page = HGroup,
		MUIA_ShowMe, FALSE,
		Child, BetterStringObject,
			End,
		End;

	return rule->page?1:0;
}
#endif

/**************************************************************************
 Ok the rule 
**************************************************************************/
static void rules_ok(void)
{
#if 0
	set(rules_wnd, MUIA_Window_Open, FALSE);
#endif
}

/**************************************************************************
 Add a new rule
**************************************************************************/
static void rules_new(void)
{
#if 0
	struct filter *f;
	DoMethod(filter_listview, MUIM_NList_GetEntry, xget(filter_listview, MUIA_NList_Active),&f);
	if (f)
	{
		struct filter_rule *fr;

		fr = filter_create_and_add_rule(f, xget(rules_page_cycle, MUIA_Cycle_Active));
		DoMethod(rules_page_listview,MUIM_NList_InsertSingle, fr, MUIV_NList_Insert_Bottom);
	}
#endif
}

/**************************************************************************
 Init rules
**************************************************************************/
static void init_rules(void)
{
#if 0
	Object *ok_button, *cancel_button, *rule_add_button;
	static struct Hook rules_display_hook;
	int i;

	init_hook(&rules_display_hook,(HOOKFUNC)rules_display);

	for (i=0;i<sizeof(rules)/sizeof(struct rule);i++)
	{
		rule_cycle_array[i] = rules[i].name;
		if (rules[i].create) rules[i].create(&rules[i]);
	}

	rules_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('R','U','L','S'),
		MUIA_Window_Title, "SimpleMail - Edit Rule",
		WindowContents, VGroup,
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
						Child, MakeButton("Remove"),
						End,
					End,
				Child, rules_page_group = HGroup,
					Child, rules_page_space = HVSpace,
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton("_Ok"),
				Child, cancel_button = MakeButton("_Cancel"),
				End,
			End,
		End;

	if (rules_wnd)
	{
		for (i=0;i<sizeof(rules)/sizeof(struct rule);i++)
			DoMethod(App, OM_ADDMEMBER, rules[i].page);

		DoMethod(App, OM_ADDMEMBER, rules_wnd);
		DoMethod(rules_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, rules_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, rules_wnd, 3, MUIM_CallHook, &hook_standard, rules_ok);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, rules_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(rule_add_button, MUIM_Notify, MUIA_Pressed, FALSE, rules_wnd, 3, MUIM_CallHook, &hook_standard, rules_new);
	}
#endif
}

/**************************************************************************
 Set the rules
**************************************************************************/
static void set_rules(struct filter *f)
{
}

/**************************************************************************
 Edit the currenty selected rule
**************************************************************************/
static void filter_edit(void)
{
#if 0
	if (!rules_wnd) init_rules();
	if (rules_wnd)
	{
		struct filter *f;
		DoMethod(filter_listview, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &f);
		set_rules(f);
		set(rules_wnd,MUIA_Window_Open,TRUE);
	}
#endif
}

/**************************************************************************
 Create a new rule
**************************************************************************/
static void filter_new(void)
{
#if 0
	struct filter *f = filter_create();
	if (f)
	{
		DoMethod(filter_listview, MUIM_NList_InsertSingle, f, MUIV_NList_Insert_Bottom);
		filter_dispose(f);
		set(filter_listview, MUIA_NList_Active, xget(filter_listview, MUIA_NList_Entries)-1);
		filter_edit();
	}
#endif
}

/**************************************************************************
 Ok, callback and close the filterwindow
**************************************************************************/
static void filter_ok(void)
{
#if 0
	set(filter_wnd,MUIA_Window_Open,FALSE);
#endif
}

/**************************************************************************
 Init the filter window
**************************************************************************/
static void init_filter(void)
{
#if 0
	Object *ok_button, *cancel_button;

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
						MUIA_NListview_NList, NListObject,
							MUIA_NList_ConstructHook, &filter_construct_hook,
							MUIA_NList_DestructHook, &filter_destruct_hook,
							MUIA_NList_DisplayHook, &filter_display_hook,
							End,
						End,
					Child, filter_name_string = BetterStringObject,
						StringFrame,
						End,
					End,
				Child, HGroup,
					Child, filter_new_button = MakeButton("_New"),
					Child, filter_remove_button = MakeButton("_Remove"),
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton("_Ok"),
				Child, cancel_button = MakeButton("_Cancel"),
				End,
			End,
		End;

	if (filter_wnd)
	{
		DoMethod(App, OM_ADDMEMBER, filter_wnd);
		DoMethod(filter_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, filter_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_wnd, 3, MUIM_CallHook, &hook_standard, filter_ok);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(filter_new_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_wnd, 3, MUIM_CallHook, &hook_standard, filter_new);
		DoMethod(filter_remove_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_listview, 2, MUIM_NList_Remove, MUIV_NList_Remove_Active);

		set(filter_name_string,MUIA_String_AttachedList,filter_listview);
	}
#endif
}

/**************************************************************************
 Opens the filter window
**************************************************************************/
void filter_open(void)
{
#if 0
	if (!filter_wnd)
	{
		init_filter();
		if (!filter_wnd) return;
	}

	set(filter_wnd, MUIA_Window_Open, TRUE);
#endif
}

/**************************************************************************
...
**************************************************************************/
void filter_update_folder_list(void)
{
}

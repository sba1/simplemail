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

#include "filter.h"

#include "compiler.h"
#include "filterwnd.h"
#include "muistuff.h"

static Object *filter_wnd;
static Object *filter_name_string;
static Object *filter_listview;
static Object *filter_new_button;
static Object *filter_remove_button;

static struct Hook filter_construct_hook;
static struct Hook filter_destruct_hook;
static struct Hook filter_display_hook;

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

static Object *rules_wnd;

/**************************************************************************
 Ok the rule 
**************************************************************************/
static void rules_ok(void)
{
	set(rules_wnd, MUIA_Window_Open, FALSE);
}

/**************************************************************************
 Init rules
**************************************************************************/
static void init_rules(void)
{
	Object *ok_button, *cancel_button;

	rules_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('R','U','L','S'),
		MUIA_Window_Title, "SimpleMail - Edit Rule",
		WindowContents, VGroup,
			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton("_Ok"),
				Child, cancel_button = MakeButton("_Cancel"),
				End,
			End,
		End;

	if (rules_wnd)
	{
		DoMethod(App, OM_ADDMEMBER, rules_wnd);
		DoMethod(rules_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, rules_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, rules_wnd, 3, MUIM_CallHook, &hook_standard, rules_ok);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, rules_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
	}
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
	set(filter_wnd,MUIA_Window_Open,FALSE);
}

/**************************************************************************
 Init the filter window
**************************************************************************/
static void init_filter(void)
{
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
}

/**************************************************************************
 Opens the filter window
**************************************************************************/
void filter_open(void)
{
	if (!filter_wnd)
	{
		init_filter();
		if (!filter_wnd) return;
	}

	set(filter_wnd, MUIA_Window_Open, TRUE);
}


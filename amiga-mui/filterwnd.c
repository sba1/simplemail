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
#include "filterruleclass.h"
#include "filterwnd.h"
#include "muistuff.h"

static void rules_refresh(void);

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
static Object *filter_edit_button;
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
static Object *rules_rule_group;
static Object *rules_move_check;
static Object *rules_move_text;
static Object *rules_folder_list;

static struct filter_rule *rules_active_rule;

static struct filter *rules_filter;

/**************************************************************************
 Accept the rule 
**************************************************************************/
static void rules_ok(void)
{
	set(rules_wnd, MUIA_Window_Open, FALSE);
	if (rules_filter)
	{
		/* Store the DestFolder */
		if (rules_filter) free(rules_filter->dest_folder);
		rules_filter->dest_folder = mystrdup((char*)xget(rules_move_text,MUIA_Text_Contents));
		rules_filter->use_dest_folder = xget(rules_move_check,MUIA_Selected);

		/* Go though all objects of the rule_rule_group and build a new
       rule list from it */
		{
		  struct List *child_list = (struct List*)xget(rules_rule_group,MUIA_Group_ChildList);
		  Object *cstate = (Object *)child_list->lh_Head;
		  Object *child;

		  while ((child = (Object*)NextObject(&cstate)))
		  {
		  	struct filter_rule *fr = (struct filter_rule*)xget(child,MUIA_UserData);
		  	if (fr)
		  	{
		  		struct filter_rule *new_fr = (struct filter_rule*)xget(child,MUIA_FilterRule_Data);

					/* free all strings of the rule */
					switch (fr->type)
					{
						case	RULE_FROM_MATCH: array_free(fr->u.from.from); break;
						case	RULE_SUBJECT_MATCH: array_free(fr->u.subject.subject); break;
						case	RULE_HEADER_MATCH:
									if (fr->u.header.name) free(fr->u.header.name);
									array_free(fr->u.header.contents);
									break;
					}

					/* clear all the memory. the node it self should not be cleared, so it stays
					   on the same position */
					memset(((char*)fr)+sizeof(struct node),0,sizeof(struct filter_rule)-sizeof(struct node));

					/* now copy over the new settings */
					fr->type = new_fr->type;
					switch (new_fr->type)
					{
						case	RULE_FROM_MATCH:
									fr->u.from.from = array_duplicate(new_fr->u.from.from);
									break;
						case	RULE_SUBJECT_MATCH:
									fr->u.subject.subject = array_duplicate(new_fr->u.subject.subject);
									break;
						case	RULE_HEADER_MATCH:
									fr->u.header.name = mystrdup(new_fr->u.header.name);
									fr->u.header.contents = array_duplicate(new_fr->u.header.contents);
									break;
					}
		  	}
		  }
		}		
	}
}

/**************************************************************************
 Add a new rule
**************************************************************************/
static void rules_new(void)
{
	struct filter_rule *fr;
	if (rules_filter)
	{
		fr = filter_create_and_add_rule(rules_filter, RULE_FROM_MATCH);
		rules_refresh();
	}
}

/**************************************************************************
 Removes the rule belonging to the group
**************************************************************************/
static void rules_remove(Object **objs)
{
	struct filter_rule *fr;
	Object *parent;

	/* Get the parent of the objects and remove the objects */
	parent = (Object*)xget(objs[0],MUIA_Parent);
	DoMethod(parent,MUIM_Group_InitChange);
	DoMethod(parent,OM_REMMEMBER,objs[1]);
	DoMethod(parent,OM_REMMEMBER,objs[0]);
	DoMethod(parent,MUIM_Group_ExitChange);
	MUI_DisposeObject(objs[1]);
	MUI_DisposeObject(objs[0]);

	/* The pointer to the filter rule is stored in MUIA_UserData */
	if ((fr = (struct filter_rule*)xget(objs[0],MUIA_UserData)))
		filter_remove_rule(fr);

}

/**************************************************************************
 Refreshes the rules.
**************************************************************************/
static void rules_refresh(void)
{
	struct filter_rule *fr;

	DoMethod(rules_rule_group, MUIM_Group_InitChange);

	DisposeAllChilds(rules_rule_group);

	if (rules_filter)
	{
		fr = (struct filter_rule*)list_first(&rules_filter->rules_list);

		/* Add every filter rule as a pair objects - the filter rule object and the
		   remove button */
		while (fr)
		{
			Object *group, *rem;
			Object *rule = FilterRuleObject,
						 MUIA_UserData,fr,   /* Is used to identify the object as a FilterRuleObject */
						 MUIA_FilterRule_Data, fr,
						 End;

			group = HGroup,
				Child, RectangleObject,MUIA_HorizWeight,0,MUIA_Rectangle_VBar, TRUE,End,
				Child, rem = MakeButton("Remove"),
				End;

			set(rem,MUIA_Weight,0);
			if (rule && group)
			{
				DoMethod(rules_rule_group, OM_ADDMEMBER, rule);
				DoMethod(rules_rule_group, OM_ADDMEMBER, group);

				/* According to autodocs MUIM_Application_PushMethod has an limit of 7
				   args, but 8 seems to work also. */
				DoMethod(rem, MUIM_Notify, MUIA_Pressed, FALSE, App, 8, MUIM_Application_PushMethod,  App, 5, MUIM_CallHook, &hook_standard, rules_remove, rule, group);
			}
			fr = (struct filter_rule*)node_next(&fr->node);
		}
	}

	/* Add two space objects (in case no rule objects have been created */
	DoMethod(rules_rule_group, OM_ADDMEMBER, HVSpace);
	DoMethod(rules_rule_group, OM_ADDMEMBER, HVSpace);
	DoMethod(rules_rule_group, MUIM_Group_ExitChange);
}

/**************************************************************************
 Init rules
**************************************************************************/
static void init_rules(void)
{
	Object *ok_button, *rule_add_button, *move_popobject;
	static struct Hook move_objstr_hook, move_strobj_hook;

	init_hook(&move_objstr_hook, (HOOKFUNC)move_objstr);
	init_hook(&move_strobj_hook, (HOOKFUNC)move_strobj);

	rules_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('R','U','L','S'),
		MUIA_Window_Title, "SimpleMail - Edit Rule",
		WindowContents, VGroup,
			Child, VGroup,
				Child, VGroupV,
					VirtualFrame,
					Child, rules_rule_group = ColGroup(2),
						Child, HVSpace,
						Child, HVSpace,
						End,
					End,
				Child, rule_add_button = MakeButton("Add new rule"),
				Child, HorizLineObject,
				Child, VGroup,
					Child, ColGroup(3),
						Child, MakeLabel("Move to Folder"),
						Child, rules_move_check = MakeCheck("Move to Folder",FALSE),
						Child, move_popobject = PopobjectObject,
							MUIA_Disabled, TRUE,
							MUIA_Popstring_Button, PopButton(MUII_PopUp),
							MUIA_Popstring_String, rules_move_text = TextObject, TextFrame, MUIA_Background, MUII_TextBack, End,
							MUIA_Popobject_ObjStrHook, &move_objstr_hook,
							MUIA_Popobject_StrObjHook, &move_strobj_hook,
							MUIA_Popobject_Object, NListviewObject,
								MUIA_NListview_NList, rules_folder_list = NListObject,
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
		DoMethod(App, OM_ADDMEMBER, rules_wnd);
		DoMethod(rules_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, rules_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, rules_wnd, 3, MUIM_CallHook, &hook_standard, rules_ok);
		DoMethod(rule_add_button, MUIM_Notify, MUIA_Pressed, FALSE, rules_wnd, 3, MUIM_CallHook, &hook_standard, rules_new);
		DoMethod(rules_move_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, move_popobject, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
		DoMethod(rules_folder_list, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, move_popobject, 2, MUIM_Popstring_Close, 1);

		filter_update_folder_list();
	}
}

/**************************************************************************
 This function is called whenever the folder list has changed
**************************************************************************/
void filter_update_folder_list(void)
{
	struct folder *f;
	if (!rules_folder_list) return;	
	set(rules_folder_list,MUIA_NList_Quiet,TRUE);
	DoMethod(rules_folder_list,MUIM_NList_Clear);
	/* Insert the folder names into the folder list for the move action */
	f = folder_first();
	while (f)
	{
		if (f->special != FOLDER_SPECIAL_GROUP)
			DoMethod(rules_folder_list, MUIM_NList_InsertSingle, f->name, MUIV_NList_Insert_Bottom);
		f = folder_next(f);
	}
	set(rules_folder_list,MUIA_NList_Quiet,FALSE);
}

/**************************************************************************
 Set the currently edit rule
**************************************************************************/
static void set_rules(struct filter *f)
{
	/* save the filter. Only one filter can be edited at the same time */
	rules_filter = f;

	rules_refresh();

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
		if (f)
		{
			set_rules(f);
			set(rules_wnd,MUIA_Window_Open,TRUE);
		}
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
				Child, HGroup,
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
						End,
					Child, VGroup,
						MUIA_HorizWeight,0,
						Child, filter_new_button = MakeButton("_New..."),
						Child, filter_edit_button = MakeButton("_Edit..."),
						Child, filter_remove_button = MakeButton("_Remove"),
						Child, HVSpace,
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
		DoMethod(filter_edit_button, MUIM_Notify, MUIA_Pressed, FALSE, filter_wnd, 3, MUIM_CallHook, &hook_standard, filter_edit);
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


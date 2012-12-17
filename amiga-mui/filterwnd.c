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

/**
 * Handle the filter window.
 *
 * @file filterwnd.c
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <mui/BetterString_mcc.h>
#include <mui/NListview_mcc.h>
#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "configuration.h"
#include "debug.h"
#include "filter.h"
#include "folder.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"

#include "audioselectgroupclass.h"
#include "compiler.h"
#include "filterlistclass.h"
#include "filterruleclass.h"
#include "filterwnd.h"
#include "foldertreelistclass.h"
#include "muistuff.h"
#include "utf8stringclass.h"

static void filter_remove_rule_gui(Object **objs);

static Object *filter_wnd;
static Object *filter_name_string;
static Object *filter_list;
static struct filter *filter_last_selected;
static Object *filter_new_button;
static Object *filter_remove_button;
static Object *filter_moveup_button;
static Object *filter_movedown_button;
static Object *filter_remote_check;
static Object *filter_request_check;
static Object *filter_new_check;
static Object *filter_sent_check;
static Object *filter_rule_group;
static Object *filter_move_check;
static Object *filter_move_text;
static Object *filter_arexx_check;
static Object *filter_arexx_popasl;
static Object *filter_arexx_string;
static Object *filter_sound_check;
static Object *filter_sound_string;
static Object *filter_add_rule_button;
static Object *filter_apply_now_button;

static Object *filter_folder_list;

STATIC ASM SAVEDS APTR filter_construct(REG(a0,struct Hook *h),REG(a2,APTR pool), REG(a1,struct filter *ent))
{
	return filter_duplicate(ent);
}

STATIC ASM SAVEDS VOID filter_destruct(REG(a0,struct Hook *h),REG(a2,APTR pool), REG(a1,struct filter *ent))
{
	if (ent) filter_dispose(ent);
}

STATIC ASM SAVEDS VOID filter_display(REG(a0,struct Hook *h), REG(a2,char **array), REG(a1,struct filter *ent))
{
	if (ent)
	{
		static char buf[300];
		utf8tostr(ent->name, buf, sizeof(buf), user.config.default_codeset);

		*array = buf;
	}
}

STATIC ASM SAVEDS VOID move_objstr(REG(a0,struct Hook *h),REG(a2, Object *list), REG(a1,Object *str))
{
	struct folder *f;

	if ((f = (struct folder*)xget(filter_folder_list, MUIA_FolderTreelist_Active)))
		set(str, MUIA_Text_Contents, f->name);
}

STATIC ASM LONG move_strobj(REG(a0,struct Hook *h),REG(a2,Object *list), REG(a1,Object *str))
{
	char *s;
	struct folder *f;
/* TODO: This shouldn't happen over the plain text
 * (different folders could have the same name) */
	get(str,MUIA_Text_Contents,&s);

	for (f=folder_first();f;f=folder_next(f))
	{
		if (!strcmp(f->name,s))
		{
			set(filter_folder_list, MUIA_FolderTreelist_Active, f);
			break;
		}
	}
	return 1;
}

/**
 * Refreshes the folder list. It should be called, whenever the folder list
 * has been changed.
 */
void filter_update_folder_list(void)
{
	DoMethod(filter_folder_list, MUIM_FolderTreelist_Refresh, NULL);
}

/**
 * Accept the current filter settings.
 */
static void filter_accept_rule(void)
{
	if (filter_last_selected)
	{
		/* Store the DestFolder */
		free(filter_last_selected->dest_folder); /* Safe to call with NULL */
		filter_last_selected->dest_folder = mystrdup((char*)xget(filter_move_text,MUIA_Text_Contents));
		filter_last_selected->use_dest_folder = xget(filter_move_check,MUIA_Selected);

		/* Store the ARexxfile */
		free(filter_last_selected->arexx_file);
		filter_last_selected->arexx_file = mystrdup((char*)xget(filter_arexx_string,MUIA_String_Contents));
		filter_last_selected->use_arexx_file = xget(filter_arexx_check,MUIA_Selected);

		/* Store the Soundfile */
		free(filter_last_selected->sound_file);
		filter_last_selected->sound_file = mystrdup((char*)xget(filter_sound_string,MUIA_String_Contents));
		filter_last_selected->use_sound_file = xget(filter_sound_check,MUIA_Selected);

		/* Store the flags */
		filter_last_selected->flags = 0;

		if (xget(filter_request_check, MUIA_Selected)) filter_last_selected->flags |= FILTER_FLAG_REQUEST;
		if (xget(filter_new_check, MUIA_Selected)) filter_last_selected->flags |= FILTER_FLAG_NEW;
		if (xget(filter_sent_check, MUIA_Selected)) filter_last_selected->flags |= FILTER_FLAG_SENT;
		if (xget(filter_remote_check, MUIA_Selected)) filter_last_selected->flags |= FILTER_FLAG_REMOTE;

		/* Go though all objects of the rule_rule_group and build a new
       rule list from it */
		{
		  struct List *child_list = (struct List*)xget(filter_rule_group,MUIA_Group_ChildList);
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
						case	RULE_FROM_MATCH:
						    	array_free(fr->u.from.from);
						    	array_free(fr->u.from.from_pat);
						    	break;
						case	RULE_RCPT_MATCH:
						    	array_free(fr->u.rcpt.rcpt);
						    	array_free(fr->u.rcpt.rcpt_pat);
						    	break;
						case	RULE_SUBJECT_MATCH:
						    	array_free(fr->u.subject.subject);
						    	array_free(fr->u.subject.subject_pat);
						    	break;
						case	RULE_HEADER_MATCH:
						    	if (fr->u.header.name) free(fr->u.header.name);
						    	if (fr->u.header.name_pat) free(fr->u.header.name_pat);
						    	array_free(fr->u.header.contents);
						    	array_free(fr->u.header.contents_pat);
						    	break;
						case	RULE_BODY_MATCH:
						    	if (fr->u.body.body) free(fr->u.body.body);
						    	filter_deinit_rule(&fr->u.body.body_parsed);
						    	break;
					}

					/* clear all the memory. the node it self should not be cleared, so it stays
					   on the same position */
					memset(((char*)fr)+sizeof(struct node),0,sizeof(struct filter_rule)-sizeof(struct node));

					/* now copy over the new settings */
					fr->type = new_fr->type;
					fr->flags = new_fr->flags;
					switch (new_fr->type)
					{
						case	RULE_FROM_MATCH:
									fr->u.from.from = array_duplicate(new_fr->u.from.from);
									break;
						case	RULE_RCPT_MATCH:
									fr->u.rcpt.rcpt = array_duplicate(new_fr->u.rcpt.rcpt);
									break;
						case	RULE_SUBJECT_MATCH:
									fr->u.subject.subject = array_duplicate(new_fr->u.subject.subject);
									break;
						case	RULE_HEADER_MATCH:
									fr->u.header.name = mystrdup(new_fr->u.header.name);
									fr->u.header.contents = array_duplicate(new_fr->u.header.contents);
									break;
						case	RULE_STATUS_MATCH:
									fr->u.status.status = new_fr->u.status.status;
									break;
					}
		  	}
		  }
		}
		filter_parse_filter_rules(filter_last_selected);
	}
}

/**
 * Refreshes the rules of the current filter.
 */
static void filter_refresh_rules(void)
{
	struct filter *filter;

	/* Get the current selected filter */
	DoMethod(filter_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, (ULONG)&filter);

	DoMethod(filter_rule_group, MUIM_Group_InitChange);

	/* Dispose all the objects in the group, they are setted completly new */
	DisposeAllChilds(filter_rule_group);

	if (filter)
	{
		struct filter_rule *fr;
		fr = (struct filter_rule*)list_first(&filter->rules_list);

		/* Add every filter rule as a pair objects - the filter rule object and the
		   remove button */
		while (fr)
		{
			Object *group, *rem;
			Object *rule = FilterRuleObject,
						 MUIA_Dropable, TRUE,
						 MUIA_UserData,fr,   /* Is used to identify the object as a FilterRuleObject */
						 MUIA_FilterRule_Data, fr,
						 End;

			group = HGroup,
				Child, RectangleObject,MUIA_HorizWeight,0,MUIA_Rectangle_VBar, TRUE,End,
				Child, rem = MakeButton(_("Remove")),
				End;

			set(rem,MUIA_Weight,0);
			if (rule && group)
			{
				DoMethod(filter_rule_group, OM_ADDMEMBER, (ULONG)rule);
				DoMethod(filter_rule_group, OM_ADDMEMBER, (ULONG)group);

				/* According to autodocs MUIM_Application_PushMethod has an limit of 7
				   args, but 8 seems to work also. */
				DoMethod(rem, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 8, MUIM_Application_PushMethod, (ULONG)App, 5, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)filter_remove_rule_gui, (ULONG)rule, (ULONG)group);
			}
			fr = (struct filter_rule*)node_next(&fr->node);
		}
	}

	/* Add two space objects (in case no rule objects have been created */
	DoMethod(filter_rule_group, OM_ADDMEMBER, (ULONG)HVSpace);
	DoMethod(filter_rule_group, OM_ADDMEMBER, (ULONG)HVSpace);
	DoMethod(filter_rule_group, MUIM_Group_ExitChange);
}

/**
 * Adds a new rule into the currently selected filter.
 */
static void filter_add_rule_gui(void)
{
	if (filter_last_selected)
	{
		filter_accept_rule();
		filter_create_and_add_rule(filter_last_selected, RULE_FROM_MATCH);
		filter_refresh_rules();
	}
}

/**
 * Remove the rule from the GUI.
 *
 * @param objs the object representing the rule.
 */
static void filter_remove_rule_gui(Object **objs)
{
	struct filter_rule *fr;
	Object *parent;

	/* The pointer to the real filter rule is stored in MUIA_UserData */
	if ((fr = (struct filter_rule*)xget(objs[0],MUIA_UserData)))
		filter_remove_rule(fr);

	/* Get the parent of the objects and remove the objects */
	parent = (Object*)xget(objs[0],MUIA_Parent);
	DoMethod(parent, MUIM_Group_InitChange);
	DoMethod(parent, OM_REMMEMBER, (ULONG)objs[1]);
	DoMethod(parent, OM_REMMEMBER, (ULONG)objs[0]);
	DoMethod(parent, MUIM_Group_ExitChange);
	MUI_DisposeObject(objs[1]);
	MUI_DisposeObject(objs[0]);
}

/**
 * Apply the currently selected filter.
 */
static void filter_apply(void)
{
	if (filter_last_selected)
	{
		filter_accept_rule();
		callback_apply_folder(filter_last_selected);
	}
}

/**
 * Creates a new filter.
 */
static void filter_new(void)
{
	struct filter *f = filter_create();
	if (f)
	{
		DoMethod(filter_list, MUIM_NList_InsertSingle, (ULONG)f, MUIV_NList_Insert_Bottom);
		filter_dispose(f);
		set(filter_list, MUIA_NList_Active, MUIV_NList_Active_Bottom);

		if (filter_last_selected) set(filter_name_string, MUIA_BetterString_SelectSize, -utf8len(filter_last_selected->name));
		set(filter_wnd, MUIA_Window_ActiveObject, filter_name_string);
	}
}

/**
 * Removes the currently selected filter.
 */
static void filter_remove(void)
{
	filter_last_selected = NULL;
	DoMethod(filter_list, MUIM_NList_Remove, MUIV_NList_Remove_Active);
}

/**
 * Accept the filter settings. Also closes the window.
 */
static void filter_ok(void)
{
	struct filter *f;
	int i;

	filter_accept_rule();
	set(filter_wnd,MUIA_Window_Open,FALSE);
	filter_list_clear();

	for (i=0;i<xget(filter_list, MUIA_NList_Entries);i++)
	{
		DoMethod(filter_list, MUIM_NList_GetEntry, i, (ULONG)&f);
		if (f)
			filter_list_add_duplicate(f);
	}

	/* Clear the list to save memory */
	DoMethod(filter_list, MUIM_NList_Clear);

	filter_last_selected = NULL;
	filter_refresh_rules();
}

/**
 * Cancel the filter changes. Also closes the window.
 */
static void filter_cancel(void)
{
	/* Clear the list to save memory */
	DoMethod(filter_list, MUIM_NList_Clear);

	filter_last_selected = NULL;
	set(filter_wnd,MUIA_Window_Open,FALSE);
	filter_refresh_rules();
}

/**
 * A new entry has been activated.
 */
static void filter_active(void)
{
	struct filter *f;
	DoMethod(filter_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, (ULONG)&f);
	if (f)
	{
		filter_accept_rule();
		nnset(filter_name_string,MUIA_UTF8String_Contents, f->name);
		filter_refresh_rules();
		set(filter_move_text, MUIA_Text_Contents, f->dest_folder);
		set(filter_move_check, MUIA_Selected, f->use_dest_folder);
		set(filter_sound_string, MUIA_String_Contents, f->sound_file);
		set(filter_sound_check, MUIA_Selected, f->use_sound_file);
		set(filter_arexx_string, MUIA_String_Contents, f->arexx_file);
		set(filter_arexx_check, MUIA_Selected, f->use_arexx_file);

		set(filter_request_check, MUIA_Selected, !!(f->flags & FILTER_FLAG_REQUEST));
		set(filter_new_check, MUIA_Selected, !!(f->flags & FILTER_FLAG_NEW));
		set(filter_sent_check, MUIA_Selected, !!(f->flags & FILTER_FLAG_SENT));
		set(filter_remote_check, MUIA_Selected, !!(f->flags & FILTER_FLAG_REMOTE));
	} else
	{
		filter_refresh_rules();
	}

	filter_last_selected = f;

	/* Update some disable states */
	set(filter_apply_now_button,MUIA_Disabled,!filter_last_selected);
	set(filter_add_rule_button,MUIA_Disabled,!filter_last_selected);
}

/**
 * A new name has been entered.
 */
static void filter_name(void)
{
	struct filter *f;
	DoMethod(filter_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, (ULONG)&f);
	if (f)
	{
		if (f->name) free(f->name);
		f->name = mystrdup((char*)xget(filter_name_string, MUIA_UTF8String_Contents));
		DoMethod(filter_list,MUIM_NList_Redraw,MUIV_NList_Redraw_Active);
	}
}

/**
 * Initialize the filter window.
 */
static void init_filter(void)
{
	static struct Hook filter_construct_hook;
	static struct Hook filter_destruct_hook;
	static struct Hook filter_display_hook;
	static struct Hook move_objstr_hook, move_strobj_hook;
	Object *ok_button, *cancel_button, *save_button;
	Object *filter_move_popobject, *filter_remote_label;

	init_hook(&filter_construct_hook,(HOOKFUNC)filter_construct);
	init_hook(&filter_destruct_hook,(HOOKFUNC)filter_destruct);
	init_hook(&filter_display_hook,(HOOKFUNC)filter_display);
	init_hook(&move_objstr_hook, (HOOKFUNC)move_objstr);
	init_hook(&move_strobj_hook, (HOOKFUNC)move_strobj);

	filter_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('F','I','L','T'),
		MUIA_Window_Title, _("SimpleMail - Edit Filter"),
		WindowContents, VGroup,
			Child, VGroup,
				Child, HGroup,
					Child, VGroup,
						MUIA_HorizWeight,40,
						Child, VGroup,
							MUIA_Group_Spacing, 0,
							Child, NListviewObject,
								MUIA_NListview_NList, filter_list = FilterListObject,
									MUIA_NList_AutoVisible, 1, /* keep the active filter visible */
									MUIA_NList_ConstructHook, &filter_construct_hook,
									MUIA_NList_DestructHook, &filter_destruct_hook,
									MUIA_NList_DisplayHook, &filter_display_hook,
									End,
								End,
							Child, HGroup,
								Child, filter_name_string = UTF8StringObject,
									StringFrame,
									End,
								End,
							End,
						Child, ColGroup(2),
							Child, filter_new_button = MakeButton(_("_New")),
							Child, filter_remove_button = MakeButton(_("_Remove")),
							Child, filter_moveup_button = MakeButton(_("Move up")),
							Child, filter_movedown_button = MakeButton(_("Move Down")),
							End,
						End,
					Child, BalanceObject, End,
					Child, VGroup,
						Child, HorizLineTextObject(_("Activity")),
						Child, HGroup,
							Child, RectangleObject,MUIA_Weight,25,End,
							Child, ColGroup(5),
								Child, MakeLabel(_("On request")),
								Child, filter_request_check = MakeCheck(NULL,FALSE),
								Child, RectangleObject,MUIA_Weight,200,End,
								Child, MakeLabel(_("On sent mails")),
								Child, filter_sent_check = MakeCheck(NULL,FALSE),

								Child, filter_remote_label = MakeLabel(_("Remotly on POP3 server")),
								Child, filter_remote_check = MakeCheck(NULL,FALSE),
								Child, RectangleObject,MUIA_Weight,200,End,
								Child, MakeLabel(_("On new mails")),
								Child, filter_new_check = MakeCheck(NULL,FALSE),
								End,
							Child, RectangleObject,MUIA_Weight,25,End,
							End,
						Child, HorizLineTextObject(_("Rules")),
						Child, VGroupV,
							Child, filter_rule_group = ColGroup(2),
								Child, HVSpace,
								Child, HVSpace,
								End,
							End,
						Child, HGroup,
							Child, filter_add_rule_button = MakeButton(_("Add new rule")),
							Child, filter_apply_now_button = MakeButton(_("Apply now")),
							End,
						Child, HorizLineTextObject(_("Action")),
						Child, VGroup,
							Child, ColGroup(3),
								Child, MakeLabel(_("Move to Folder")),
								Child, filter_move_check = MakeCheck(_("Move to Folder"),FALSE),
								Child, filter_move_popobject = PopobjectObject,
									MUIA_Disabled, TRUE,
									MUIA_Popstring_Button, PopButton(MUII_PopUp),
									MUIA_Popstring_String, filter_move_text = TextObject, TextFrame, MUIA_Background, MUII_TextBack, End,
									MUIA_Popobject_ObjStrHook, &move_objstr_hook,
									MUIA_Popobject_StrObjHook, &move_strobj_hook,
									MUIA_Popobject_Object, NListviewObject,
										MUIA_NListview_NList, filter_folder_list = FolderTreelistObject,
											End,
										End,
									End,

								Child, MakeLabel(_("Execute ARexx Script")),
								Child, filter_arexx_check = MakeCheck(_("Execute ARexx Script"),FALSE),
								Child, filter_arexx_popasl = PopaslObject,
									MUIA_Disabled, TRUE,
									MUIA_Popstring_Button, PopButton(MUII_PopFile),
									MUIA_Popstring_String, filter_arexx_string = BetterStringObject,
										StringFrame,
										MUIA_CycleChain,1,
										MUIA_String_Acknowledge, TRUE,
										End,
									End,

								Child, MakeLabel(_("Play Sound")),
								Child, filter_sound_check = MakeCheck(_("Play Sound"),FALSE),
								Child, filter_sound_string = AudioSelectGroupObject, MUIA_Disabled, TRUE, End,

								End,
							End,
						End,
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, save_button = MakeButton(Q_("?filter:_Save")),
				Child, ok_button = MakeButton(_("_Use")),
				Child, cancel_button = MakeButton(_("_Cancel")),
				End,
			End,
		End;

	if (filter_wnd)
	{
		char *short_help_txt = _("If activated the filter will be used remotly on POP3 servers\n"
                             "which support the TOP command. Mails which matches the filter\n"
														 "are presented to the user and automatically marked as to be ignored.");

		set(filter_remote_label, MUIA_ShortHelp, short_help_txt);
		set(filter_remote_check, MUIA_ShortHelp, short_help_txt);

		DoMethod(App, OM_ADDMEMBER, (ULONG)filter_wnd);
		DoMethod(filter_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)filter_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)filter_cancel);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)filter_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)filter_ok);
		DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)filter_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)filter_ok);
		DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)filter_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)save_filter);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)filter_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)filter_cancel);

		DoMethod(filter_new_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)filter_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)filter_new);
		DoMethod(filter_remove_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)filter_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)filter_remove);
		DoMethod(filter_moveup_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)filter_list, 3, MUIM_NList_Move, MUIV_NList_Move_Active, MUIV_NList_Move_Previous);
		DoMethod(filter_movedown_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)filter_list, 3, MUIM_NList_Move, MUIV_NList_Move_Active, MUIV_NList_Move_Next);

		set(filter_name_string, MUIA_String_AttachedList, (ULONG)filter_list);
		DoMethod(filter_list, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, (ULONG)filter_list, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)filter_active);
		DoMethod(filter_name_string, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, (ULONG)filter_list, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)filter_name);

		DoMethod(filter_add_rule_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)filter_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)filter_add_rule_gui);
		DoMethod(filter_apply_now_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)filter_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)filter_apply);
		DoMethod(filter_move_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (ULONG)filter_move_popobject, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
		DoMethod(filter_sound_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (ULONG)filter_sound_string, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
		DoMethod(filter_arexx_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (ULONG)filter_arexx_popasl, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
		DoMethod(filter_folder_list, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, (ULONG)filter_move_popobject, 2, MUIM_Popstring_Close, 1);

		filter_update_folder_list();
	}
}

/**
 * Opens the filter window with a new filter.
 *
 * @param nf defines the filter that should be added. The
 * object copied so the argument can be freed after calling
 * the function.
 */
void filter_open_with_new_filter(struct filter *nf)
{
	struct filter *f;
	int new_active;

	if (!filter_wnd)
	{
		init_filter();
		if (!filter_wnd) return;
	}

	filter_last_selected = NULL;

	/* Clear the filter listview contents */
	DoMethod(filter_list, MUIM_NList_Clear);

	/* Add the filters to the list */
	f = filter_list_first();
	while (f)
	{
		DoMethod(filter_list, MUIM_NList_InsertSingle, (ULONG)f, MUIV_NList_Insert_Bottom);
		f = filter_list_next(f);
	}

	if (nf)
	{
		DoMethod(filter_list, MUIM_NList_InsertSingle, (ULONG)nf, MUIV_NList_Insert_Bottom);
		new_active = xget(filter_list,MUIA_NList_Entries)-1;
	} else
	{
		new_active = 0;
	}

	set(filter_list, MUIA_NList_Active, new_active);
	filter_active();
	set(filter_wnd, MUIA_Window_Open, TRUE);
}

/**
 * Opens the filter window.
 */
void filter_open(void)
{
	filter_open_with_new_filter(NULL);
}

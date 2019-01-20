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
 * @file errorwnd.c
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <mui/NListview_mcc.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "debug.h"
#include "lists.h"
#include "logging.h"
#include "smintl.h"
#include "subthreads.h"
#include "support_indep.h"

#include "errorwnd.h"
#include "muistuff.h"
#include "timesupport.h"

struct error_node
{
	struct node node;
	char *text;
	unsigned int date;
	unsigned int id;
	logging_severity_t severity;
};

static struct list error_list;

static Object *error_wnd;
static Object *all_errors_list;
static Object *text_list;
static Object *clear_button;

static logg_listener_t error_logg_listener;

/**
 * Add the logg entry to the error list.
 *
 * @param l
 */
static void error_window_add_logg(logg_t l)
{
	struct error_node *node;

	if (!(node = (struct error_node *)malloc(sizeof(*node))))
		return;

	memset(node, 0, sizeof(*node));
	node->id = logg_id(l);
	node->date = logg_seconds(l);
	node->severity = logg_severity(l);
	if (!(node->text = mystrdup(logg_text(l))))
	{
		free(node);
		return;
	}
	list_insert_tail(&error_list, &node->node);
}

/**
 * Update the error window log with the information of the logg.
 */
static void error_window_update_logg(void)
{
	struct error_node *enode;
	logg_t l;

	set(all_errors_list, MUIA_NList_Quiet, TRUE);
	DoMethod(all_errors_list, MUIM_NList_Clear);

	enode = (struct error_node*)list_first(&error_list);
	l = logg_next(NULL);

	while (enode && l)
	{
		struct error_node *cur = enode;

		enode = (struct error_node*)node_next(&enode->node);

		/* Keep no logg-ones (i.e., added via error_add_message()) */
		if (cur->id == ~0) continue;

		/* Keep id-matching one (they represent the same entry) */
		if (cur->id == logg_id(l))
		{
			l = logg_next(l);
			continue;
		}

		/* If id of the view log list is smaller it is an obsolete one,
		 * remove it.
		 */
		if (cur->id < logg_id(l))
		{
			node_remove(&cur->node);
			free(cur->text);
			free(cur);
			continue;
		}
	}

	if (l)
	{
		/* Add the remaining logg entries */
		do
		{
			error_window_add_logg(l);
		} while((l = logg_next(l)));
	}

	/* Populate the view again */
	enode = (struct error_node*)list_first(&error_list);
	while (enode)
	{
		DoMethod(all_errors_list, MUIM_NList_InsertSingle, enode, MUIV_NList_Insert_Bottom);
		enode = (struct error_node*)node_next(&enode->node);
	}

	set(all_errors_list, MUIA_NList_Quiet, FALSE);
}

/**
 * Callback for selecting the currently displayed error message.
 */
static void error_select(void)
{
	int num = xget(all_errors_list, MUIA_NList_Active);
	struct error_node *node = (struct error_node *)list_find(&error_list,num);

	set(text_list, MUIA_NList_Quiet, TRUE);
	DoMethod(text_list, MUIM_NList_Clear);

	if (node)
	{
		DoMethod(text_list, MUIM_NList_InsertSingleWrap, (ULONG)node->text, MUIV_NList_Insert_Bottom, WRAPCOL0, ALIGN_LEFT);
	}

	set(text_list, MUIA_NList_Quiet, FALSE);
}

/**
 * Called whenever the logg is updated.
 *
 * @param userdata
 */
static void error_window_logg_update_callback(void *userdata)
{
	if (thread_get() != thread_get_main())
	{
		/* We could also use MUI to push the method */
		thread_call_function_async(thread_get_main(), error_window_update_logg, 0);
	} else
	{
		error_window_update_logg();
	}
}

/**
 * Callback that is called when the window open state changes.
 */
static void error_window_open_state(void)
{
	BOOL error_window_open;

	error_window_open = xget(error_wnd, MUIA_Window_Open);

	if (error_window_open && !error_logg_listener)
	{
		error_logg_listener = logg_add_update_listener(error_window_logg_update_callback, NULL);
	} else if (!error_window_open && error_logg_listener)
	{
		logg_remove_update_listener(error_logg_listener);
		error_logg_listener = NULL;
	}
}

/**
 * Clear all messages.
 */
static void error_window_clear(void)
{
	struct error_node *node;

	DoMethod(all_errors_list, MUIM_NList_Clear);
	while ((node = (struct error_node *)list_remove_tail(&error_list)))
	{
		if (node->text) free(node->text);
		free(node);
	}
	logg_clear();
}

STATIC ASM SAVEDS VOID error_display(REG(a0,struct Hook *h),REG(a2,Object *obj),REG(a1,struct NList_DisplayMessage *msg))
{
	struct error_node *error = (struct error_node*)msg->entry;
	if (!error)
	{
		msg->strings[0] = (char*)_("Time");
		msg->strings[1] = (char*)_("Severity");
		msg->strings[2] = (char*)_("Message");
		return;
	}
	msg->strings[0] = sm_get_time_str(error->date);
	msg->strings[1] = error->severity==INFO?(char*)"I":(char*)"E";
	msg->strings[2] = error->text;
}

/**
 * Initialize the error window.
 */
static void init_error(void)
{
	static struct Hook error_display_hook;

	if (!MUIMasterBase) return;

	init_hook(&error_display_hook, (HOOKFUNC)error_display);

	list_init(&error_list);

	error_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('E','R','R','O'),
    MUIA_Window_Title, _("SimpleMail - Logging and Errors"),
    WindowContents, VGroup,
			Child, NListviewObject,
				MUIA_CycleChain, 1,
				MUIA_NListview_NList, all_errors_list = NListObject,
					MUIA_NList_Format, ",PREPARSE=" MUIX_C ",",
					MUIA_NList_DisplayHook2, &error_display_hook,
					MUIA_NList_Title, TRUE,
					End,
				End,
			Child, BalanceObject, End,
			Child, NListviewObject,
				MUIA_Weight, 33,
				MUIA_CycleChain, 1,
				MUIA_NListview_NList, text_list = NListObject,
					MUIA_NList_TypeSelect, MUIV_NList_TypeSelect_Char,
					End,
				End,
			Child, HGroup,
				Child, clear_button = MakeButton(_("_Clear")),
				End,
			End,
		End;

	if (error_wnd)
	{
		DoMethod(App, OM_ADDMEMBER, (ULONG)error_wnd);
		DoMethod(error_wnd, MUIM_Notify, MUIA_Window_Open, MUIV_EveryTime, (ULONG)error_wnd, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)error_window_open_state);
		DoMethod(error_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)error_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(clear_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)clear_button, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)error_window_clear);
		DoMethod(all_errors_list, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, (ULONG)all_errors_list, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)error_select);
	}
}

/*****************************************************************************/

void error_window_open(void)
{
	if (!error_wnd)
	{
		init_error();

		if (!error_wnd) return;
	}

	error_window_update_logg();

	set(error_wnd, MUIA_Window_Open, TRUE);
}

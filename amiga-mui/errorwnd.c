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
** $Id$
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
#include "support_indep.h"

#include "muistuff.h"

struct error_node
{
	struct node node;
	char *text;
};

static struct list error_list;

static Object *error_wnd;
static Object *all_errors_list;
static Object *text_list;
static Object *delete_button;
static Object *close_button;

static void error_select(void)
{
	int num = xget(all_errors_list, MUIA_NList_Active);
	struct error_node *node = (struct error_node *)list_find(&error_list,num-1);

	set(text_list, MUIA_NList_Quiet, TRUE);
	DoMethod(text_list, MUIM_NList_Clear);

	if (node)
	{
		DoMethod(text_list, MUIM_NList_InsertSingleWrap, (ULONG)node->text, MUIV_NList_Insert_Bottom, WRAPCOL0, ALIGN_LEFT);
	}

	set(text_list, MUIA_NList_Quiet, FALSE);
}

static void delete_messages(void)
{
	struct error_node *node;

	set(error_wnd, MUIA_Window_Open, FALSE);

	DoMethod(all_errors_list, MUIM_NList_Clear);
	while ((node = (struct error_node *)list_remove_tail(&error_list)))
	{
		if (node->text) free(node->text);
		free(node);
	}

}

static void init_error(void)
{
	if (!MUIMasterBase) return;

	list_init(&error_list);

	error_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('E','R','R','O'),
    MUIA_Window_Title, "SimpleMail - Error",
    WindowContents, VGroup,
			Child, NListviewObject,
				MUIA_CycleChain, 1,
				MUIA_NListview_NList, all_errors_list = NListObject,
					End,
				End,
			Child, NListviewObject,
				MUIA_Weight, 33,
				MUIA_CycleChain, 1,
				MUIA_NListview_NList, text_list = NListObject,
					MUIA_NList_TypeSelect, MUIV_NList_TypeSelect_Char,
					End,
				End,
			Child, HGroup,
				Child, delete_button = MakeButton("_Delete messages"),
				Child, close_button = MakeButton("_Close window"),
				End,
			End,
		End;

	if (error_wnd)
	{
		DoMethod(App, OM_ADDMEMBER, (ULONG)error_wnd);
		DoMethod(error_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)error_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(delete_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)delete_button, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)delete_messages);
		DoMethod(close_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)error_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(all_errors_list, MUIM_Notify, MUIA_NList_Active, (ULONG)all_errors_list, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)error_select);
	}
}

void error_add_message(char *msg)
{
	if (!error_wnd) init_error();
	if (error_wnd)
	{
		struct error_node *enode = (struct error_node*)malloc(sizeof(struct error_node));
		if (enode)
		{
			if ((enode->text = mystrdup(msg)))
			{
				set(text_list, MUIA_NList_Quiet, TRUE);
				DoMethod(text_list, MUIM_NList_Clear);
				DoMethod(text_list, MUIM_NList_InsertSingleWrap, (ULONG)enode->text, MUIV_NList_Insert_Bottom, WRAPCOL0, ALIGN_LEFT);
				set(text_list, MUIA_NList_Quiet, FALSE);

				list_insert_tail(&error_list, &enode->node);

				DoMethod(all_errors_list, MUIM_NList_InsertSingle, enode->text, MUIV_NList_Insert_Bottom);
			} else free(enode);
		}
	}
}

void error_window_open(void)
{
	if (!error_wnd) init_error();
	if (!error_wnd) return;

	set(error_wnd, MUIA_Window_Open, TRUE);
}

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
#include <mui/nlistview_mcc.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "lists.h"
#include "support_indep.h"

#include "muistuff.h"

struct error_node
{
	struct node node;
	char *text;
};

struct list error_list;

static Object *error_wnd;
static Object *previous_button;
static Object *next_button;
static Object *error_numeric;
static Object *text_list;
static Object *delete_button;
static Object *close_button;

static void error_select(void)
{
	int num = xget(error_numeric, MUIA_Numeric_Value);
	struct error_node *node = (struct error_node *)list_find(&error_list,num-1);

	set(text_list, MUIA_NList_Quiet, TRUE);
	DoMethod(text_list, MUIM_NList_Clear);

	if (node)
	{
		DoMethod(text_list, MUIM_NList_InsertSingleWrap, node->text, MUIV_NList_Insert_Bottom, WRAPCOL0, ALIGN_LEFT);
	}

	set(text_list, MUIA_NList_Quiet, FALSE);
}

static void delete_messages(void)
{
	struct error_node *node;

	set(error_wnd, MUIA_Window_Open, FALSE);

	while ((node = (struct error_node *)list_remove_tail(&error_list)))
	{
		if (node->text) free(node->text);
		free(node);
	}

	SetAttrs(error_numeric,
			MUIA_Numeric_Min, 0,
			MUIA_Numeric_Max, 0,
			MUIA_Numeric_Value, 0,
			MUIA_Numeric_Format, "Error 0/0",
			TAG_DONE);
}

static void init_error(void)
{
	if (!MUIMasterBase) return;

	list_init(&error_list);

	error_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('E','R','R','O'),
    MUIA_Window_Title, "SimpleMail - Error",
    WindowContents, VGroup,
    	Child, HGroup,
    		Child, previous_button = MakeButton("_Previous error"),
				Child, error_numeric = NumericbuttonObject,
					MUIA_CycleChain, 1,
					MUIA_Numeric_Min, 0,
					MUIA_Numeric_Max, 0,
					MUIA_Numeric_Value, 1,
					MUIA_Numeric_Format, "Error 0/0",
					End,
				Child, next_button = MakeButton("_Next error"),
				End,
			Child, NListviewObject,
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
		DoMethod(App, OM_ADDMEMBER, error_wnd);
		DoMethod(error_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, error_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(delete_button, MUIM_Notify, MUIA_Pressed, FALSE, delete_button, 3, MUIM_CallHook, &hook_standard, delete_messages);
		DoMethod(close_button, MUIM_Notify, MUIA_Pressed, FALSE, error_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(previous_button, MUIM_Notify, MUIA_Pressed, FALSE, error_numeric, 2, MUIM_Numeric_Decrease, 1);
		DoMethod(next_button, MUIM_Notify, MUIA_Pressed, FALSE, error_numeric, 2, MUIM_Numeric_Increase, 1);
		DoMethod(error_numeric, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime, error_numeric, 3, MUIM_CallHook, &hook_standard, error_select);
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
				static char error_label[32];

				set(text_list, MUIA_NList_Quiet, TRUE);
				DoMethod(text_list, MUIM_NList_Clear);
				DoMethod(text_list, MUIM_NList_InsertSingleWrap, enode->text, MUIV_NList_Insert_Bottom, WRAPCOL0, ALIGN_LEFT);
				set(text_list, MUIA_NList_Quiet, FALSE);

				list_insert_tail(&error_list, &enode->node);

				sprintf(error_label, "Error %%ld/%ld",list_length(&error_list));

				SetAttrs(error_numeric,
						MUIA_Numeric_Min, 1,
						MUIA_Numeric_Max, list_length(&error_list),
						MUIA_Numeric_Value, list_length(&error_list),
						MUIA_Numeric_Format, error_label,
						TAG_DONE);
			} else free(enode);
		}

		set(error_wnd, MUIA_Window_Open, TRUE);
	}
}

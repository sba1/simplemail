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

#include "folder.h"
#include "simplemail.h"


#if 0

#include "folderwnd.h"
#include "muistuff.h"

static Object *folder_wnd;
static Object *name_string;
static Object *path_string;
static Object *type_cycle;

struct folder *changed_folder;

static void folder_ok(void)
{
	set(folder_wnd, MUIA_Window_Open, FALSE);
	callback_change_folder_attrs();
	changed_folder = NULL;
}

#endif

struct folder *folder_get_changed_folder(void)
{
#if 0
	return changed_folder;
#endif
}

char *folder_get_changed_name(void)
{
#if 0
	return (char*)xget(name_string,MUIA_String_Contents);
#endif
}

char *folder_get_changed_path(void)
{
#if 0
	return (char*)xget(path_string,MUIA_String_Contents);
#endif
}

int folder_get_changed_type(void)
{
#if 0
	return (int)xget(type_cycle, MUIA_Cycle_Active);
#endif
}

#if 0

static void init_folder(void)
{
	Object *ok_button, *cancel_button;
	static char *type_array[5];

	type_array[0] = "received";
	type_array[1] = "sent";
	type_array[2] = "received and sent";
	type_array[3] = "mailinglist";

	folder_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('F','O','L','D'),
		MUIA_Window_Title, "SimpleMail - Edit folder",
		WindowContents, VGroup,
			Child, ColGroup(2),
				Child, MakeLabel("N_ame"),
				Child, name_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_ControlChar, 'a',
					MUIA_String_AdvanceOnCR, TRUE,
					End,

				Child, MakeLabel("Path"),
				Child, path_string = BetterStringObject,
					TextFrame,
					MUIA_BetterString_NoInput, TRUE,
					End,

				Child, MakeLabel("_Type"),
				Child, type_cycle = MakeCycle("_Type",type_array),
				End,

			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton("_Ok"),
				Child, cancel_button = MakeButton("_Cancel"),
				End,
			End,
		End;

	if (folder_wnd)
	{
		DoMethod(App, OM_ADDMEMBER, folder_wnd);
		DoMethod(folder_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, folder_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, folder_wnd, 3, MUIM_CallHook, &hook_standard, folder_ok);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, folder_wnd, 3, MUIM_Set, MUIA_Window_Open, FALSE);
	}
}
#endif

void folder_edit(struct folder *f)
{
#if 0
	if (!f) return;

	if (!folder_wnd)
	{
		init_folder();
		if (!folder_wnd) return;
	}

	set(name_string, MUIA_String_Contents, f->name);
	set(path_string, MUIA_String_Contents, f->path);
	set(type_cycle, MUIA_Cycle_Active, f->type);
	changed_folder = f;
	set(folder_wnd, MUIA_Window_Open, TRUE);

#endif
}



void folder_edit_new_path(char *init_path)
{
}

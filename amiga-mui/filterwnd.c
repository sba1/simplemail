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
#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "filterwnd.h"
#include "muistuff.h"

static Object *filter_wnd;

static void filter_ok(void)
{
	set(filter_wnd,MUIA_Window_Open,FALSE);
}

static void init_filter(void)
{
	Object *ok_button, *cancel_button;

	filter_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('F','I','L','T'),
		MUIA_Window_Title, "SimpleMail - Edit filter",
		WindowContents, VGroup,
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
	}
}

void filter_open(void)
{
	if (!filter_wnd)
	{
		init_filter();
		if (!filter_wnd) return;
	}

	set(filter_wnd, MUIA_Window_Open, TRUE);
}


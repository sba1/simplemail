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
#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"

#include "muistuff.h"
#include "statuswnd.h"
#include "transwndclass.h"

struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata);

static Object *status_wnd;

static char *status_title;

/**************************************************************************
 Open the status window
**************************************************************************/
int statuswnd_open(int active)
{
	if (!status_wnd)
	{
		status_wnd = transwndObject,
			MUIA_Window_ID,	MAKE_ID('S','T','A','T'),
			MUIA_Window_Title, status_title,
			
		End;

		if (status_wnd)
		{
//			DoMethod(win_up, MUIM_Notify, MUIA_transwnd_Aborted, TRUE, win_up, 3, MUIM_CallHook, &hook_standard, up_abort);
			DoMethod(App, OM_ADDMEMBER, status_wnd);
		}
	}
	
	if (status_wnd)
	{
		set(status_wnd, MUIA_Window_Open, TRUE);
		return 1;
	}
	return 0;
}

/**************************************************************************
 Open the status window
**************************************************************************/
void statuswnd_close(void)
{
	if (status_wnd)
	{
		set(status_wnd, MUIA_Window_Open, FALSE);
	}
}

/**************************************************************************
 Open the status window
**************************************************************************/
void statuswnd_set_title(char *title)
{
	free(status_title);
	status_title = mystrdup(title);

	if (!status_wnd) return;
	set(status_wnd, MUIA_Window_Title,status_title);
}

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

#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"

#include "statuswnd.h"
#include "subthreads.h"

#if 0

struct MUI_NListtree_TreeNode *FindListtreeUserData(Object *tree, APTR udata);

static Object *status_wnd;

static char *status_title;
#endif
/**************************************************************************
 Called when abort button presses
**************************************************************************/
static void statuswnd_abort(void)
{
	thread_abort();
}

/**************************************************************************
 Open the status window
**************************************************************************/
int statuswnd_open(int active)
{
#if 0
	if (!status_wnd)
	{
		status_wnd = transwndObject,
			MUIA_Window_ID,	MAKE_ID('S','T','A','T'),
			MUIA_Window_Title, status_title,

		End;

		if (status_wnd)
		{
			DoMethod(status_wnd, MUIM_Notify, MUIA_transwnd_Aborted, TRUE, status_wnd, 3, MUIM_CallHook, &hook_standard, statuswnd_abort);
			DoMethod(App, OM_ADDMEMBER, status_wnd);

			statuswnd_set_head(NULL);
		}
	}

	if (status_wnd)
	{
		set(status_wnd, MUIA_Window_Open, TRUE);
		return 1;
	}
#endif
	return 0;
}

/**************************************************************************
 Open the status window
**************************************************************************/
void statuswnd_close(void)
{
#if 0
	if (status_wnd)
	{
		set(status_wnd, MUIA_Window_Open, FALSE);
	}
#endif
}

/**************************************************************************
 Open the status window
**************************************************************************/
void statuswnd_set_title(char *title)
{
#if 0
	free(status_title);
	status_title = mystrdup(title);

	if (!status_wnd) return;
	set(status_wnd, MUIA_Window_Title,status_title);
#endif
}

/**************************************************************************
 Initialize the gauge bar with the given maximum value
**************************************************************************/
void statuswnd_init_gauge(int maximal)
{
#if 0
	if (!status_wnd) return;
	set(status_wnd, MUIA_transwnd_Gauge1_Max, maximal);
#endif
}

/**************************************************************************
 Set the current gauge value
**************************************************************************/
void statuswnd_set_gauge(int value)
{
#if 0
	if (!status_wnd) return;
	set(status_wnd, MUIA_transwnd_Gauge1_Val, value);
#endif
}

/**************************************************************************
 Set the current gauge text
**************************************************************************/
void statuswnd_set_gauge_text(char *text)
{
#if 0
	set(status_wnd, MUIA_transwnd_Gauge1_Str, text);
#endif
}

/**************************************************************************
 Set the status line
**************************************************************************/
void statuswnd_set_status(char *text)
{
#if 0
	if (!status_wnd) return;
	set(status_wnd, MUIA_transwnd_Status, text);
#endif
}

/**************************************************************************
 Set the head of the status window
**************************************************************************/
void statuswnd_set_head(char *text)
{
#if 0
	static char *head_text;
	if (text != NULL) head_text = text;
	if (!status_wnd) return;
	set(status_wnd, MUIA_transwnd_Head, head_text);
#endif
}

/**************************************************************************
 Insert mail
**************************************************************************/
void statuswnd_mail_list_insert(int mno, int mflags, int msize)
{
#if 0
	DoMethod(status_wnd, MUIM_transwnd_InsertMailSize, mno, mflags, msize);
#endif
}

/**************************************************************************
 Insert mail info
**************************************************************************/
void statuswnd_mail_list_set_info(int mno, char *from, char *subject, char *date)
{
#if 0
	DoMethod(status_wnd, MUIM_transwnd_InsertMailInfo, mno, from, subject, date);
#endif
}

/**************************************************************************
 returns -1 if the mail is not in the mail selection
**************************************************************************/
int statuswnd_mail_list_get_flags(int mno)
{
#if 0
	return (int)DoMethod(status_wnd, MUIM_transwnd_GetMailFlags, mno);
#endif
}

/**************************************************************************

**************************************************************************/
void statuswnd_mail_list_clear(void)
{
#if 0
	DoMethod(status_wnd, MUIM_transwnd_Clear);
#endif
}

/**************************************************************************

**************************************************************************/
void statuswnd_mail_list_freeze(void)
{
//	set(status_wnd,MUIA_transwnd_QuietList,TRUE);
}

/**************************************************************************

**************************************************************************/
void statuswnd_mail_list_thaw(void)
{
//	set(status_wnd,MUIA_transwnd_QuietList,FALSE);
}

/**************************************************************************
 Wait for user interaction. Return -1 for aborting
**************************************************************************/
int statuswnd_wait(void)
{
//	return (int)DoMethod(status_wnd, MUIM_transwnd_Wait);
	return 0;
}

/***************************************************************************
 Returns 0 if user has aborted the statistic listing
**************************************************************************/
int statuswnd_more_statistics(void)
{
//  LONG start_pressed = xget(status_wnd,MUIA_transwnd_StartPressed);
 // return !start_pressed;
	return 1;
}


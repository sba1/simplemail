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
** dlwnd.c
*/

#include <stdio.h>
#include <stdlib.h>

#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "muistuff.h"
#include "smintl.h"
#include "subthreads.h"
#include "support_indep.h"
#include "transwndclass.h"

static Object *win_dl;

void dl_set_status(char *str)
{
	set(win_dl, MUIA_transwnd_Status, _(str));
}

void dl_connect_to_server(char *server)
{
	static char buf[400];
	static char *title; /* this is not really perfect but it works */
	free(title);
	title = mystrdup(server);
	set(win_dl, MUIA_Window_Title, title);

	sprintf(buf,_("Connecting to server %s..."),server);
	set(win_dl, MUIA_transwnd_Status, buf);
}

static int max_mail;
static int cur_mail;
static int max_mail_sum;

void dl_init_gauge_mail(int amm)
{
	static char str[128];

	max_mail = amm;
	cur_mail = 0;

	sprintf(str, _("-/- Bytes (-/%d Mails)"),max_mail);

	SetAttrs(win_dl, MUIA_transwnd_Gauge1_Str, str,
									 MUIA_transwnd_Gauge1_Max, 1,
									 MUIA_transwnd_Gauge1_Val, 0,
									 TAG_DONE);
}

void dl_set_gauge_mail(int current)
{
	cur_mail = current;
}

/* The number of bytes which we are going to download */
void dl_init_mail_size_sum(int sum)
{
	static char str[128];
	max_mail_sum = sum;

	sprintf(str, _("%%ld/%d Bytes (%d/%d Mails)"),sum,cur_mail,max_mail);

	SetAttrs(win_dl, MUIA_transwnd_Gauge1_Str, str,
									 MUIA_transwnd_Gauge1_Max, sum,
									 MUIA_transwnd_Gauge1_Val, 0,
									 TAG_DONE);
}

/* The number of bytes which we have downloaded */
void dl_set_mail_size_sum(int sum)
{
	static char str[128];
	sprintf(str, _("%%ld/%d Bytes (%d/%d Mails)"),max_mail_sum,cur_mail,max_mail);

	SetAttrs(win_dl, MUIA_transwnd_Gauge1_Str, str,
									 MUIA_transwnd_Gauge1_Val, sum,
									 TAG_DONE);
}

void dl_init_gauge_byte(int size)
{
	static char str[256];
	
	sprintf(str, _("%%ld/%d bytes"), size);

	set(win_dl, MUIA_transwnd_Gauge2_Str, str);
	set(win_dl, MUIA_transwnd_Gauge2_Max, size);
	set(win_dl, MUIA_transwnd_Gauge2_Val, 0);
}

void dl_set_gauge_byte(int current)
{
	set(win_dl, MUIA_transwnd_Gauge2_Val, current);
}

void dl_abort(void)
{
	thread_abort();
}

int dl_window_open(int active)
{
	int rc;
	
	rc = FALSE;

	if (!win_dl)
	{
		win_dl = transwndObject,
			MUIA_Window_ID,	MAKE_ID('T','R','D','L'),
			MUIA_Window_Activate, active,
		End;

		if (win_dl)
		{
			DoMethod(win_dl, MUIM_Notify, MUIA_transwnd_Aborted, TRUE, win_dl, 3, MUIM_CallHook, &hook_standard, dl_abort);
			DoMethod(App, OM_ADDMEMBER, win_dl);
		}
	}
	
	if(win_dl != NULL)
	{
		SetAttrs(win_dl,
				MUIA_Window_Open, TRUE,
				MUIA_Window_Activate, active,
				TAG_DONE);
		rc = TRUE;
	}
	
	return(rc);
}

void dl_window_close(void)
{
	if(win_dl != NULL)
	{
		set(win_dl, MUIA_Window_Open, FALSE);
	}
}

int dl_checkabort(void)
{
	/* unneccessary in the amiga client, because it supports subthreading */
	return 0;
}

void dl_insert_mail(int mno, int mflags, int msize)
{
	DoMethod(win_dl, MUIM_transwnd_InsertMailSize, mno, mflags, msize);
}

void dl_insert_mail_info(int mno, char *from, char *subject, char *date)
{
	DoMethod(win_dl, MUIM_transwnd_InsertMailInfo, mno, from, subject, date);
}

/* returns -1 if the mail is not in the mail selection */
int dl_get_mail_flags(int mno)
{
	return (int)DoMethod(win_dl, MUIM_transwnd_GetMailFlags, mno);
}

void dl_clear(void)
{
	DoMethod(win_dl, MUIM_transwnd_Clear);
}

int dl_wait(void)
{
	return (int)DoMethod(win_dl, MUIM_transwnd_Wait);
}

void dl_freeze_list(void)
{
	set(win_dl,MUIA_transwnd_QuietList,TRUE);
}

void dl_thaw_list(void)
{
	set(win_dl,MUIA_transwnd_QuietList,FALSE);
}

int dl_more_statistics(void)
{
  LONG start_pressed = xget(win_dl,MUIA_transwnd_StartPressed);
  return !start_pressed;
}


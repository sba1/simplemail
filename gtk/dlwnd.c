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

#include "subthreads.h"

void dl_set_title(char *str)
{
#if 0
	set(win_dl, MUIA_Window_Title, str);
#endif
	printf("Titlebar: %s\n",str);
}

void dl_set_status(char *str)
{
#if 0
	set(win_dl, MUIA_transwnd_Status, str);
#endif
	printf("%s\n",str);
}

void dl_init_gauge_mail(int amm)
{
#if 0
	static char str[256];
	
	sprintf(str, "Mail %%ld/%d", amm);

	set(win_dl, MUIA_transwnd_Gauge1_Str, str);
	set(win_dl, MUIA_transwnd_Gauge1_Max, amm);
	set(win_dl, MUIA_transwnd_Gauge1_Val, 0);
#endif
	printf("%d mails\n",amm);
}

void dl_set_gauge_mail(int current)
{
#if 0
	set(win_dl, MUIA_transwnd_Gauge1_Val, current);
#endif
}

void dl_init_gauge_byte(int size)
{
#if 0
	static char str[256];
	
	sprintf(str, "%%ld/%d bytes", size);

	set(win_dl, MUIA_transwnd_Gauge2_Str, str);
	set(win_dl, MUIA_transwnd_Gauge2_Max, size);
	set(win_dl, MUIA_transwnd_Gauge2_Val, 0);
#endif
}

void dl_set_gauge_byte(int current)
{
#if 0
	set(win_dl, MUIA_transwnd_Gauge2_Val, current);
#endif
}

void dl_abort(void)
{
#if 0
	thread_abort();
#endif
}

int dl_window_open(void)
{
#if 0
	int rc;
	
	rc = FALSE;

	if (!win_dl)
	{
		win_dl = transwndObject,
			MUIA_Window_ID,	MAKE_ID('T','R','D','L'),
		End;

		if (win_dl)
		{
			DoMethod(win_dl, MUIM_Notify, MUIA_transwnd_Aborted, TRUE, win_dl, 3, MUIM_CallHook, &hook_standard, dl_abort);
			DoMethod(App, OM_ADDMEMBER, win_dl);
		}
	}
	
	if(win_dl != NULL)
	{
		set(win_dl, MUIA_Window_Open, TRUE);
		rc = TRUE;
	}
	
	return(rc);
#endif
	return 1;
}

void dl_window_close(void)
{
#if 0
	if(win_dl != NULL)
	{
		set(win_dl, MUIA_Window_Open, FALSE);
	}
#endif
}

int dl_checkabort(void)
{
	return 0;
}

void dl_insert_mail(int mno, int mflags, int msize)
{
#if 0
	DoMethod(win_dl, MUIM_transwnd_InsertMailSize, mno, mflags, msize);
#endif
}

void dl_insert_mail_info(int mno, char *from, char *subject, unsigned int seconds)
{
#if 0
	DoMethod(win_dl, MUIM_transwnd_InsertMailInfo, mno, from, subject, seconds);
#endif
}

/* returns -1 if the mail is not in the mail selection */
int dl_get_mail_flags(int mno)
{
#if 0
	return (int)DoMethod(win_dl, MUIM_transwnd_GetMailFlags, mno);
#endif
	return -1;
}

void dl_clear(void)
{
#if 0
	DoMethod(win_dl, MUIM_transwnd_Clear);
#endif
}

void dl_wait(void)
{
}





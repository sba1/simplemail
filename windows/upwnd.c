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
** upwnd.c
*/

#include <stdio.h>

void up_set_status(char *str)
{
#if 0
	set(win_up, MUIA_transwnd_Status, str);
#endif
}

void up_connect_to_server(char *server)
{
}

void up_init_gauge_mail(int amm)
{
#if 0
	static char str[256];
	
	sprintf(str, "Mail %%ld/%d", amm);
	
	set(win_up, MUIA_transwnd_Gauge1_Str, str);
	set(win_up, MUIA_transwnd_Gauge1_Max, amm);
	set(win_up, MUIA_transwnd_Gauge1_Val, 0);
#endif
}

void up_set_gauge_mail(int current)
{
#if 0
	set(win_up, MUIA_transwnd_Gauge1_Val, current);
#endif
}

void up_init_gauge_byte(int size)
{
#if 0
	static char str[256];
	
	sprintf(str, "%%ld/%d bytes", size);

	set(win_up, MUIA_transwnd_Gauge2_Str, str);
	set(win_up, MUIA_transwnd_Gauge2_Max, size);
	set(win_up, MUIA_transwnd_Gauge2_Val, 0);
#endif
}

void up_set_gauge_byte(int current)
{
#if 0
	set(win_up, MUIA_transwnd_Gauge2_Val, current);
#endif
}

void up_abort(void)
{
#if 0
	thread_abort();
#endif
}

int up_window_open(void)
{
#if 0
	int rc;
	
	rc = FALSE;

	if (!win_up)
	{
		win_up = transwndObject,
			MUIA_Window_ID,	MAKE_ID('T','R','U','P'),
		End;

		if (win_up)
		{
			DoMethod(win_up, MUIM_Notify, MUIA_transwnd_Aborted, TRUE, win_up, 3, MUIM_CallHook, &hook_standard, up_abort);
			DoMethod(App, OM_ADDMEMBER, win_up);
		}
	}
	
	if(win_up != NULL)
	{
		set(win_up, MUIA_Window_Open, TRUE);
		rc = TRUE;
	}
	
	return(rc);
#endif
}

void up_window_close(void)
{
#if 0
	if(win_up != NULL)
	{
		set(win_up, MUIA_Window_Open, FALSE);
	}
#endif
}

int up_checkabort(void)
{
	return 0;
}



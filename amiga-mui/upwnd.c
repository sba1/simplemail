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
#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "muistuff.h"

#include "transwndclass.h"

static Object *win_up;

void up_set_title(char *str)
{
	set(win_up, MUIA_Window_Title, str);
}

void up_set_status(char *str)
{
	set(win_up, MUIA_transwnd_Status, str);
}

void up_init_gauge_mail(int amm)
{
	static char str[256];
	
	sprintf(str, "Mail %%ld/%d", amm);
	
	set(win_up, MUIA_transwnd_Gauge1_Str, str);
	set(win_up, MUIA_transwnd_Gauge1_Max, amm);
	set(win_up, MUIA_transwnd_Gauge1_Val, 0);
}

void up_set_gauge_mail(int current)
{
	set(win_up, MUIA_transwnd_Gauge1_Val, current);
}

void up_init_gauge_byte(int size)
{
	static char str[256];
	
	sprintf(str, "%%ld/%d bytes", size);

	set(win_up, MUIA_transwnd_Gauge2_Str, str);
	set(win_up, MUIA_transwnd_Gauge2_Max, size);
	set(win_up, MUIA_transwnd_Gauge2_Val, 0);
}

void up_set_gauge_byte(int current)
{
	set(win_up, MUIA_transwnd_Gauge2_Val, current);
}

int up_window_open(void)
{
	int rc;
	
	rc = FALSE;

	if (!win_up)
	{
		win_up = transwndObject,
			MUIA_Window_ID,	MAKE_ID('T','R','U','P'),
		End;

		if (win_up)
		{
			DoMethod(App, OM_ADDMEMBER, win_up);
		}
	}
	
	if(win_up != NULL)
	{
		set(win_up, MUIA_Window_Open, TRUE);
		rc = TRUE;
	}
	
	return(rc);
}

void up_window_close(void)
{
	if(win_up != NULL)
	{
		set(win_up, MUIA_Window_Open, FALSE);
	}
}

int up_checkabort(void)
{
	/* unneccessary in the amiga client, because it supports subthreading */
	return 0;
}
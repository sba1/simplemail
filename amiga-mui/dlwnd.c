/*
** dlwnd.c
*/

#include <stdio.h>
#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <proto/muimaster.h>

#include "muistuff.h"

#include "transwndclass.h"

static Object *win_dl;

int dl_window_init(void)
{
	int rc;
	
	if(create_transwnd_class())
	{
		rc = TRUE;
	}
	
	return(rc);
}

void set_dl_title(char *str)
{
	/*set(win_dl, MUIA_Window_Title, str);*/
	set(win_dl, MUIA_transwnd_WinTitle, str);
}

void set_dl_status(char *str)
{
	/*set(status, MUIA_Text_Contents, str);*/
	set(win_dl, MUIA_transwnd_Status, str);
}

void init_dl_gauge_mail(int amm)
{
	static char str[256];
	
	sprintf(str, "Mail %%ld/%d", amm);
	
	/*set(gauge_mail, MUIA_Gauge_InfoText, str);
	set(gauge_mail, MUIA_Gauge_Max, amm);
	set(gauge_mail, MUIA_Gauge_Current, 0);*/
	
	set(win_dl, MUIA_transwnd_Gauge1_Str, str);
	set(win_dl, MUIA_transwnd_Gauge1_Max, amm);
	set(win_dl, MUIA_transwnd_Gauge1_Val, 0);
}

void set_dl_gauge_mail(int current)
{
	/*set(gauge_mail, MUIA_Gauge_Current, current);*/
	set(win_dl, MUIA_transwnd_Gauge1_Val, current);
}

void init_dl_gauge_byte(int size)
{
	static char str[256];
	
	sprintf(str, "%%ld/%d bytes", size);

/*	set(gauge_byte, MUIA_Gauge_InfoText, str);
	set(gauge_byte, MUIA_Gauge_Max, size);
	set(gauge_byte, MUIA_Gauge_Current, 0);*/
	
	set(win_dl, MUIA_transwnd_Gauge2_Str, str);
	set(win_dl, MUIA_transwnd_Gauge2_Max, size);
	set(win_dl, MUIA_transwnd_Gauge2_Val, 0);
}

void set_dl_gauge_byte(int current)
{
	/*set(gauge_byte, MUIA_Gauge_Current, current);*/
	set(win_dl, MUIA_transwnd_Gauge2_Val, current);
}

int dl_window_open(void)
{
	int rc;
	
	rc = FALSE;
	
	win_dl = transwndObject,
	End;
	
	if(win_dl != NULL)
	{
		rc = TRUE;
	}
	
	return(rc);
}

void dl_window_close(void)
{
	if(win_dl != NULL)
	{
		set(win_dl, MUIA_transwnd_Open, FALSE);
	}
}
/*
** dlwnd.c
*/

#include <stdio.h>
#include <libraries/iffparse.h> /* MAKE_IND */
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <proto/muimaster.h>

#include "muistuff.h"

static Object *win_dl;
static Object *gauge_mail, *gauge_byte;
static Object *status;

int dl_window_init(void)
{
	int rc;
	rc = FALSE;
	
	win_dl = WindowObject,
		MUIA_Window_ID,					MAKE_ID('D','O','W','N'),
        
        WindowContents, VGroup,
        
        	Child, gauge_mail = GaugeObject,
        		GaugeFrame,
        		MUIA_Gauge_InfoText,		"Mail 0/0",
        		MUIA_Gauge_Horiz,			TRUE,
        	End,
        	
        	Child, gauge_byte = GaugeObject,
        		GaugeFrame,
        		MUIA_Gauge_InfoText,		"0/0 bytes",
        		MUIA_Gauge_Horiz,			TRUE,
        	End,
        	
        	Child, status = TextObject,
        		TextFrame,
        	End,	
        	
        End,
	End;
	
	if(win_dl != NULL)
	{
		DoMethod(App, OM_ADDMEMBER, win_dl);

		rc = TRUE;
	}
	
	return(rc);
}

void set_dl_title(char *str)
{
	set(win_dl, MUIA_Window_Title, str);
}

void set_dl_status(char *str)
{
	set(status, MUIA_Text_Contents, str);
}

void init_dl_gauge_mail(int amm)
{
	static char str[256];
	
	sprintf(str, "Mail %%ld/%d", amm);
	
	set(gauge_mail, MUIA_Gauge_InfoText, str);
	set(gauge_mail, MUIA_Gauge_Max, amm);
	set(gauge_mail, MUIA_Gauge_Current, 0);
}

void set_dl_gauge_mail(int current)
{
	set(gauge_mail, MUIA_Gauge_Current, current);
}

void init_dl_gauge_byte(int size)
{
	static char str[256];
	
	sprintf(str, "%%ld/%d bytes", size);

	set(gauge_byte, MUIA_Gauge_InfoText, str);
	set(gauge_byte, MUIA_Gauge_Max, size);
	set(gauge_byte, MUIA_Gauge_Current, 0);
}

void set_dl_gauge_byte(int current)
{
	set(gauge_byte, MUIA_Gauge_Current, current);
}

int dl_window_open(void)
{
	int rc;
	rc = FALSE;
	
	if(win_dl != NULL)
	{
		set(win_dl, MUIA_Window_Open, TRUE);
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

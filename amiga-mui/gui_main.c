/*
** gui_main.c
*/

#include <dos/dos.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/locale.h>

#include "SimpleMail_rev.h"

/* nongui parts */
#include "mail.h"
#include "io.h"

/* gui parts */
#include "addressstringclass.h"
#include "attachmentlistclass.h"
#include "dlwnd.h"
#include "foldertreelistclass.h"
#include "mainwnd.h"
#include "mailtreelistclass.h"
#include "muistuff.h"

struct Library *MUIMasterBase;
struct Locale *DefaultLocale;

Object *App;

void loop(void)
{
	ULONG sigs = 0;

	while((LONG) DoMethod(App, MUIM_Application_NewInput, &sigs) != MUIV_Application_ReturnID_Quit)
  {
		if (sigs)
		{
			sigs = Wait(sigs | SIGBREAKF_CTRL_C);
			if (sigs & SIGBREAKF_CTRL_C) break;
		}
	}
}

int app_init(void)
{
	int rc;
	rc = FALSE;

	App = ApplicationObject,
		MUIA_Application_Title,			"SimpleMail",
		MUIA_Application_Version,		VERSTAG+1,
		MUIA_Application_Copyright,		"Copyright (c) 2000 by Sebastian Bauer & Hynek Schlawack",
		MUIA_Application_Author,		"Sebastian Bauer & Hynek Schlawack",
		MUIA_Application_Description,	"A mailer.",
		MUIA_Application_Base,			"SIMPLEMAIL",
	End;
	
	if(App != NULL)
	{
		rc = TRUE;
	}
	
	return(rc);
}

void app_del(void)
{
	if(App != NULL)
	{
		MUI_DisposeObject(App);
		App = NULL;
	}	
}

void all_del(void)
{
	if(MUIMasterBase != NULL)	
	{
		app_del();

		delete_attachmentlist_class();
		delete_addressstring_class();
		delete_foldertreelist_class();
		delete_mailtreelist_class();
		
		CloseLibrary(MUIMasterBase);
		MUIMasterBase = NULL;
	}

	if (DefaultLocale) CloseLocale(DefaultLocale);
}

int all_init(void)
{
	int rc;
	rc = FALSE;
	
	MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN);
	if(MUIMasterBase != NULL)
	{
		DefaultLocale = OpenLocale(NULL);

		init_hook_standard();
		if (create_foldertreelist_class() && create_mailtreelist_class() &&
				create_addressstring_class() && create_attachmentlist_class())
		{
			if (app_init())
			{
				if (main_window_init())
				{
					if (dl_window_init())
					{
						rc = TRUE;
					}	
				}
			}	
		}	
	}
	else
	{
		missing_lib(MUIMASTER_NAME, MUIMASTER_VMIN);
	}

	return(rc);
}

int gui_main(void)
{
	int rc;
	rc = 0;
	
	if(all_init())
	{
		main_refresh_folders();

		if(main_window_open())
		{
			loop();
			rc = 1;
		}
	}

	all_del();
	
	return rc;
}


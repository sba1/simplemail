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
** gui_main.c
*/

#include <stdio.h>

#include <dos/dos.h>
#include <libraries/mui.h>
#include "simplehtml_mcc.h"
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/locale.h>

#include "version.h"

/* nongui parts */
#include "mail.h"

/* gui parts */
#include "addressstringclass.h"
#include "attachmentlistclass.h"
#include "composeeditorclass.h"
#include "configwnd.h"
#include "configtreelistclass.h"
#include "datatypescache.h"
#include "datatypesclass.h"
#include "filterruleclass.h"
#include "foldertreelistclass.h"
#include "iconclass.h"
#include "mainwnd.h"
#include "mailtreelistclass.h"
#include "muistuff.h"
#include "picturebuttonclass.h"
#include "popupmenuclass.h"
#include "readlistclass.h"
#include "subthreads.h"
#include "transwndclass.h"

__near long __stack = 30000;

#ifdef _DCC
extern
#endif
struct Library *MUIMasterBase;
struct Locale *DefaultLocale;

Object *App;

void loop(void)
{
	ULONG sigs = 0;
	ULONG thread_m = thread_mask();

	while((LONG) DoMethod(App, MUIM_Application_NewInput, &sigs) != MUIV_Application_ReturnID_Quit)
	{
		if (sigs)
		{
			sigs = Wait(sigs | SIGBREAKF_CTRL_C | thread_m);
			if (sigs & SIGBREAKF_CTRL_C) break;
			if (sigs & thread_m)
			{
				thread_handle();
			}
		}
	}
}

int app_init(void)
{
	int rc;
	rc = FALSE;

	App = ApplicationObject,
		MUIA_Application_Title,			"SimpleMail",
		MUIA_Application_Version,		VERSTAG,
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

		delete_filterrule_class();
		delete_configtreelist_class();
		delete_icon_class();
		delete_popupmenu_class();
		delete_picturebutton_class();
		delete_simplehtml_class();
		delete_composeeditor_class();
		delete_readlist_class();
		delete_transwnd_class();
		delete_datatypes_class();
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
				create_addressstring_class() && create_attachmentlist_class() &&
				create_datatypes_class() && create_transwnd_class() &&
				create_readlist_class() && create_composeeditor_class() &&
				create_simplehtml_class() && create_picturebutton_class() &&
				create_popupmenu_class() && create_icon_class() && 
				create_configtreelist_class() && create_filterrule_class())
		{
			if (app_init())
			{
				if (main_window_init())
				{
					rc = TRUE;
				}
			}	
		}	
	}
	else
	{
		printf("Couldn't open " MUIMASTER_NAME " version %ld!\n",MUIMASTER_VMIN);
	}

	return(rc);
}

int gui_main(void)
{
	int rc;
	rc = 0;

	dt_init();
	
	if(all_init())
	{
		main_refresh_folders();

		if(main_window_open())
		{
			loop();
			rc = 1;
			close_config();
		}
	}

	all_del();

	dt_cleanup();
	
	return rc;
}


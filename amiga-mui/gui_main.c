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
#include "folder.h"
#include "mail.h"
#include "simplemail.h"

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
#include "multistringclass.h"
#include "picturebuttonclass.h"
#include "popupmenuclass.h"
#include "readlistclass.h"
#include "subthreads.h"
#include "transwndclass.h"

__near long __stack = 30000;

struct Library *MUIMasterBase;
struct Locale *DefaultLocale;
Object *App;

static struct MsgPort *timer_port;
static struct timerequest *timer_req;
static ULONG timer_outstanding;

/****************************************************************
 Sends a timer
*****************************************************************/
static struct timerequest *timer_send(ULONG secs, ULONG mics)
{
  struct timerequest *treq = (struct timerequest *) AllocVec(sizeof(struct timerequest), MEMF_CLEAR | MEMF_PUBLIC);
  if (treq)
  {
    *treq = *timer_req;
    treq->tr_node.io_Command = TR_ADDREQUEST;
    treq->tr_time.tv_secs = secs;
    treq->tr_time.tv_micro = mics;
    SendIO((struct IORequest *) treq);
    timer_outstanding++;
  }
  return treq;
}

/****************************************************************
 Cleanup the timer initialisations
*****************************************************************/
static void timer_free(void)
{
  if (timer_req)
  {
    if (timer_req->tr_node.io_Device)
    {
      while (timer_outstanding)
      {
				if (Wait(1L << timer_port->mp_SigBit | 4096) & 4096)
					break;
				timer_outstanding--;
      }

      CloseDevice((struct IORequest *) timer_req);
    }
    DeleteIORequest(timer_req);
  }

  if (timer_port)
    DeleteMsgPort(timer_port);
}

/****************************************************************
 Initialize the timer stuff
*****************************************************************/
static int timer_init(void)
{
	if (!(timer_port = CreateMsgPort())) return 0;
	if (timer_req = (struct timerequest *) CreateIORequest(timer_port, sizeof(struct timerequest)))
	{
		if (!OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *) timer_req, 0))
		{
			return 1;
		}
		timer_free();
	}
  return 0;
}

/****************************************************************
 Handle the timer events
*****************************************************************/
static void timer_handle(void)
{
	struct timerequest *tr;

	/* Remove the timer from the port */
	while ((tr = (struct timerequest *)GetMsg(timer_port)))
	{
	  FreeVec(tr);
	  timer_outstanding--;
	}

	callback_timer();

	if (!timer_outstanding) timer_send(1, 0);
}

/****************************************************************
 The main loop
*****************************************************************/
void loop(void)
{
	ULONG sigs = 0;
	ULONG thread_m = thread_mask();
	ULONG timer_m = 1UL << timer_port->mp_SigBit;

	while((LONG) DoMethod(App, MUIM_Application_NewInput, &sigs) != MUIV_Application_ReturnID_Quit)
	{
		if (sigs)
		{
			sigs = Wait(sigs | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | thread_m | timer_m);
			if (sigs & SIGBREAKF_CTRL_C) break;
			if (sigs & thread_m) thread_handle();
			if (sigs & timer_m) timer_handle();
		}
	}
}


/****************************************************************
 Initialize the application object
*****************************************************************/
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

/****************************************************************
 Delete the Application Object
*****************************************************************/
void app_del(void)
{
	if(App != NULL)
	{
		MUI_DisposeObject(App);
		App = NULL;
	}	
}

/****************************************************************
 Delete the rest
*****************************************************************/
void all_del(void)
{
	if(MUIMasterBase != NULL)	
	{
		app_del();

		delete_multistring_class();
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

		timer_free();

		CloseLibrary(MUIMasterBase);
		MUIMasterBase = NULL;
	}

	if (DefaultLocale) CloseLocale(DefaultLocale);
}

/****************************************************************
 Initialize everything
*****************************************************************/
int all_init(void)
{
	int rc;
	rc = FALSE;
	
	MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN);
	if(MUIMasterBase != NULL)
	{
		DefaultLocale = OpenLocale(NULL);

		init_hook_standard();

		if (timer_init())
		{
			if (create_foldertreelist_class() && create_mailtreelist_class() &&
					create_addressstring_class() && create_attachmentlist_class() &&
					create_datatypes_class() && create_transwnd_class() &&
					create_readlist_class() && create_composeeditor_class() &&
					create_simplehtml_class() && create_picturebutton_class() &&
					create_popupmenu_class() && create_icon_class() && 
					create_configtreelist_class() && create_filterrule_class() &&
					create_multistring_class())
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
	}
	else
	{
		printf("Couldn't open " MUIMASTER_NAME " version %ld!\n",MUIMASTER_VMIN);
	}

	return(rc);
}

/****************************************************************
 Entry point for the GUI depend part
*****************************************************************/
int gui_main(void)
{
	int rc;
	rc = 0;

	dt_init();
	
	if(all_init())
	{
		main_refresh_folders();
		main_set_folder_active(folder_incoming());

		if(main_window_open())
		{
			/* send the first timer */
			timer_send(1,0);
			callback_autocheck_refresh();

			loop();
			rc = 1;
			close_config();
		}
	}

	all_del();

	dt_cleanup();
	
	return rc;
}


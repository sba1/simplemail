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
#include <string.h>
#include <stdlib.h>

#include <dos/rdargs.h>

#include <dos/dos.h>
#include <libraries/mui.h>
#include <datatypes/soundclass.h>
#include "simplehtml_mcc.h"
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/locale.h>

#include "version.h"

/* nongui parts */
#include "folder.h"
#include "mail.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"

/* gui parts */
#include "addresstreelistclass.h"
#include "addressstringclass.h"
#include "amigasupport.h"
#include "arexx.h"
#include "attachmentlistclass.h"
#include "audioselectgroupclass.h"
#include "composeeditorclass.h"
#include "configwnd.h"
#include "datatypescache.h"
#include "datatypesclass.h"
#include "filterruleclass.h"
#include "foldertreelistclass.h"
#include "iconclass.h"
#include "mainwnd.h"
#include "mailtreelistclass.h"
#include "muistuff.h"
#include "multistringclass.h"
#include "pgplistclass.h"
#include "picturebuttonclass.h"
#include "popupmenuclass.h"
#include "subthreads.h"
#include "transwndclass.h"
#include "utf8stringclass.h"

__near long __stack = 30000;

struct Library *MUIMasterBase;
struct Library *RexxSysBase;
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

static Object *sound_obj;

/****************************************************************
 Play a sound (from support.c)
*****************************************************************/
void sm_play_sound(char *filename)
{
	if (sound_obj) DisposeObject(sound_obj);

	if ((sound_obj = NewDTObject(filename,
			DTA_GroupID, GID_SOUND,
			TAG_DONE)))
	{
		struct dtTrigger dtt;

		/* Fill out the method message */
		dtt.MethodID     = DTM_TRIGGER;
		dtt.dtt_GInfo    = NULL;
		dtt.dtt_Function = STM_PLAY;
		dtt.dtt_Data     = NULL;

		/* Play the sound */
		DoMethodA(sound_obj, (Msg)&dtt);
	}
}


/****************************************************************
 The main loop
*****************************************************************/
void loop(void)
{
	ULONG sigs = 0;
	ULONG thread_m = thread_mask();
	ULONG timer_m = 1UL << timer_port->mp_SigBit;
	ULONG arexx_m = arexx_mask();

	while((LONG) DoMethod(App, MUIM_Application_NewInput, &sigs) != MUIV_Application_ReturnID_Quit)
	{
		if (sigs)
		{
			sigs = Wait(sigs | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | thread_m | timer_m | arexx_m);
			if (sigs & SIGBREAKF_CTRL_C) break;
			if (sigs & thread_m) thread_handle();
			if (sigs & timer_m) timer_handle();
			if (sigs & arexx_m) arexx_handle();
		}
	}
}


/****************************************************************
 Initialize the application object
*****************************************************************/
int app_init(void)
{
	App = ApplicationObject,
		MUIA_Application_Title,			"SimpleMail",
		MUIA_Application_Version,		VERSTAG,
		MUIA_Application_Copyright,		"Copyright (c) 2000-2002 by Sebastian Bauer & Hynek Schlawack",
		MUIA_Application_Author,		"Sebastian Bauer & Hynek Schlawack",
		MUIA_Application_Description,	"A mailer.",
		MUIA_Application_Base,			"SIMPLEMAIL",
		MUIA_Application_UseRexx, FALSE,
		MUIA_Application_HelpFile, "PROGDIR:SimpleMail.guide",
	End;
	
	return !!App;
}

/****************************************************************
 Delete the Application Object
*****************************************************************/
void app_del(void)
{
	if (App)
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
	if (MUIMasterBase)
	{
		if (RexxSysBase)
		{
			app_del();
	
			delete_audioselectgroup_class();
			delete_pgplist_class();
			delete_addresstreelist_class();
			delete_multistring_class();
			delete_filterrule_class();
			delete_icon_class();
			delete_popupmenu_class();
			delete_picturebutton_class();
			delete_simplehtml_class();
			delete_composeeditor_class();
			delete_transwnd_class();
			delete_datatypes_class();
			delete_attachmentlist_class();
			delete_addressstring_class();
			delete_foldertreelist_class();
			delete_mailtreelist_class();
			delete_utf8string_class();

			arexx_cleanup();
			timer_free();

			/* free the sound object */
			if (sound_obj) DisposeObject(sound_obj);

			CloseLibrary(RexxSysBase);
			RexxSysBase = NULL;
		}

		CloseLibrary(MUIMasterBase);
		MUIMasterBase = NULL;
	}

	if (DefaultLocale) CloseLocale(DefaultLocale);
}

#undef printf

/****************************************************************
 Initialize everything
*****************************************************************/
int all_init(void)
{
	if ((MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN)))
	{
		if ((RexxSysBase = OpenLibrary("rexxsyslib.library",0)))
		{
			DefaultLocale = OpenLocale(NULL);
			init_hook_standard();

			if (timer_init())
			{
				if (arexx_init())
				{
					if (create_utf8string_class() && create_foldertreelist_class() && create_mailtreelist_class() &&
							create_addressstring_class() && create_attachmentlist_class() &&
							create_datatypes_class() && create_transwnd_class() && create_composeeditor_class() &&
							create_simplehtml_class() && create_picturebutton_class() &&
							create_popupmenu_class() && create_icon_class() && 
							create_filterrule_class() &&
							create_multistring_class() && create_addresstreelist_class() &&
							create_pgplist_class() && create_audioselectgroup_class())
					{
						if (app_init())
						{
							if (main_window_init())
							{
								DoMethod(App,MUIM_Application_Load,MUIV_Application_Load_ENV);
								return 1;
							}
						} else puts(_("Failed to create the application\n"));
					} else puts(_("Could not create mui custom classes\n"));
				} else puts(_("Couldn't create arexx port\n"));
			} else puts(_("Couldn't initialize timer\n"));
		} else printf(_("Couldn't open %s version %d\n"),"rexxsyslib.library",0);
	} else printf(_("Couldn't open %s version %d\n"),MUIMASTER_NAME,MUIMASTER_VMIN);

	return 0;
}

/****************************************************************
 Hides the application
*****************************************************************/
void app_hide(void)
{
	set(App,MUIA_Application_Iconified,TRUE);
}

/****************************************************************
 Shows the application
*****************************************************************/
void app_show(void)
{
	set(App,MUIA_Application_Iconified,FALSE);
}

char *initial_mailto;
char *initial_subject;

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

		if (main_window_open())
		{
			/* if we should open the compose window soon after start */
			if (initial_mailto)
				callback_write_mail_to_str(initial_mailto, initial_subject);

			free(initial_mailto);
			free(initial_subject);
			initial_mailto = NULL;
			initial_subject = NULL;

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


/****************************************************************
 Parse the start arguments
*****************************************************************/
int gui_parseargs(int argc, char *argv[])
{
	struct command_args {
	  char *mailto;
	  char *subject;
	} shell_args;

	struct RDArgs *rdargs;

	memset(&shell_args,0,sizeof(shell_args));

	if ((rdargs = ReadArgs("MAILTO/K,SUBJECT/K",(LONG*)&shell_args, NULL)))
	{
		initial_mailto = mystrdup(shell_args.mailto);
		initial_subject = mystrdup(shell_args.subject);
		FreeArgs(rdargs);
	}

	if (arexx_find())
	{
		char result[40];
		/* SimpleMail is already running */
		if (initial_mailto || initial_subject)
		{
			char *buf = malloc(mystrlen(initial_mailto)+mystrlen(initial_subject)+100);
			if (buf)
			{
				sprintf(buf,"MAILWRITE MAILTO=\"%s\" SUBJECT=\"%s\"",initial_mailto?initial_mailto:"",initial_subject?initial_subject:"");
				SendRexxCommand("SIMPLEMAIL.1", buf, result, 40);
				free(buf);
			}
		} else
		{
			SendRexxCommand("SIMPLEMAIL.1", "MAINTOFRONT", result, 40);
		}
		return 0;
	}

	return 1;
}

/****************************************************************
 Execute an ARexx script
*****************************************************************/
int gui_execute_arexx(char *filename)
{
	return arexx_execute_script(filename);
}


/****************************************************************
 The main entry point
*****************************************************************/
void main(int argc, char *argv[])
{
	simplemail_main();
}

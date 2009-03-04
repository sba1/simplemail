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
#include <proto/icon.h>

#ifdef __AMIGAOS4__
#include <diskfont/diskfonttag.h>
#include <proto/diskfont.h>
#endif


#include "version.h"

/* nongui parts */
#include "configuration.h"
#include "debug.h"
#include "folder.h"
#include "mail.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"

/* gui parts */
#include "accountpopclass.h"
#include "signaturecycleclass.h"
#include "addressentrylistclass.h"
#include "addressgrouplistclass.h"
#include "addressmatchlistclass.h"
#include "addressstringclass.h"
#include "amigasupport.h"
#include "appicon.h"
#include "arexx.h"
#include "attachmentlistclass.h"
#include "audioselectgroupclass.h"
#include "composeeditorclass.h"
#include "configwnd.h"
#include "datatypescache.h"
#include "datatypesclass.h"
#include "filterlistclass.h"
#include "filterruleclass.h"
#include "foldertreelistclass.h"
#include "iconclass.h"
#include "mainwnd.h"
#include "mailinfoclass.h"
#include "mailtreelistclass.h"
#include "messageviewclass.h"
#include "muistuff.h"
#include "multistringclass.h"
#include "pgplistclass.h"
#include "picturebuttonclass.h"
#include "popupmenuclass.h"
#include "startupwnd.h"
#include "shutdownwnd.h"
#include "smtoolbarclass.h"
#include "subthreads.h"
#include "subthreads_amiga.h"
#include "support.h"
#include "transwndclass.h"
#include "utf8stringclass.h"

struct Library *MUIMasterBase;
struct Library *RexxSysBase;
struct Library *SimpleHTMLBase;
struct Library *TTEngineBase;
struct Library *CyberGfxBase;

#ifdef __AMIGAOS4__
struct MUIMasterIFace *IMUIMaster;
struct Interface *IRexxSys;
struct SimpleHTMLIFace *ISimpleHTML;
struct Interface *ITTEngine;
struct CyberGfxIFace *ICyberGfx;
struct Library *OpenLibraryInterface(STRPTR name, int version, void *interface_ptr);
void CloseLibraryInterface(struct Library *lib, void *interface);
#else
void *IMUIMaster;
void *IRexxSys;
void *ISimpleHTML;
void *ITTEngine;
void *ICyberGfx;

struct Library *OpenLibraryInterface(STRPTR name, int version, void *interface_ptr)
{
	return OpenLibrary(name,version);
}

void CloseLibraryInterface(struct Library *lib, void *interface)
{
	CloseLibrary(lib);
}
#endif

struct Locale *DefaultLocale;
Object *App;

/* New since MUI V20 */
static STRPTR UsedClasses[] =
{
	"NList.mcc",
	"NListview.mcc",
	"NListviews.mcc",                /* Because the NList prefs are called NListviews.mcp */
	"NListtree.mcc",
	"TextEditor.mcc",
	"BetterString.mcc",
	"Popplaceholder.mcc",
	"TheBar.mcc",
	NULL
};

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
	ULONG arexx_m = arexx_mask();
	ULONG appicon_m = appicon_mask();

	while((LONG) DoMethod(App, MUIM_Application_NewInput, (ULONG)&sigs) != MUIV_Application_ReturnID_Quit)
	{
		if (sigs)
		{
			sigs = Wait(sigs | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | thread_m | arexx_m | appicon_m);
			if (sigs & SIGBREAKF_CTRL_C) break;
			if (sigs & thread_m) thread_handle(thread_m);
			if (sigs & arexx_m) arexx_handle();
			if (sigs & appicon_m) appicon_handle();
		}
	}
}

/****************************************************************
 The app is getting (un)iconified
*****************************************************************/
void app_change_iconified_state(void)
{
	if (user.config.appicon_show == 1) appicon_refresh(1);
}

/****************************************************************
 Initialize the application object
*****************************************************************/
int app_init(void)
{
	struct DiskObject *HideIcon = appicon_get_hide_icon();
	SM_ENTER;

	App = ApplicationObject,
		MUIA_Application_Title,			"SimpleMail",
		MUIA_Application_Version,		VERSTAG,
		MUIA_Application_Copyright,		"Copyright (c) 2000-2009 by Sebastian Bauer & Hynek Schlawack",
		MUIA_Application_Author,		"Sebastian Bauer & Hynek Schlawack",
		MUIA_Application_Description,	"A mailer.",
		MUIA_Application_Base,			"SIMPLEMAIL",
		MUIA_Application_UseRexx, FALSE,
		MUIA_Application_HelpFile, "PROGDIR:SimpleMail.guide",
#ifdef MUIA_Application_UsedClasses
		/* MUI V20, no includes right now, but found it in YAM :) */
		MUIA_Application_UsedClasses, UsedClasses,
#endif
		HideIcon ? MUIA_Application_DiskObject : TAG_IGNORE, HideIcon,
	End;

	if (App)
		DoMethod(App, MUIM_Notify, MUIA_Application_Iconified, MUIV_EveryTime, (ULONG)App, 3, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)app_change_iconified_state);

	SM_LEAVE;
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
 Quit the Application
*****************************************************************/
void app_quit(void)
{
	if (App)
		DoMethod(App, MUIM_Application_PushMethod, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
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

			delete_smtoolbar_class();
			delete_mailinfo_class();
			delete_addressmatchlist_class();
			delete_pgplist_class();
			delete_addressentrylist_class();
			delete_addressgrouplist_class();
			delete_messageview_class();
			delete_accountpop_class();
			delete_signaturecycle_class();
			delete_audioselectgroup_class();
			delete_multistring_class();
			delete_filterrule_class();
			delete_filterlist_class();
			delete_icon_class();
			delete_popupmenu_class();
			delete_picturebutton_class();
			delete_composeeditor_class();
			delete_transwnd_class();
			delete_datatypes_class();
			delete_attachmentlist_class();
			delete_addressstring_class();
			delete_foldertreelist_class();
			delete_mailtreelist_class();
			delete_utf8string_class();

			arexx_cleanup();
			appicon_free();

			/* free the sound object */
			if (sound_obj) DisposeObject(sound_obj);

			CloseLibraryInterface(CyberGfxBase, ICyberGfx);
			CloseLibraryInterface(TTEngineBase, ITTEngine);

#ifdef __AMIGAOS4__
			DropInterface((struct Interface*)ISimpleHTML);
#endif
			CloseLibrary(SimpleHTMLBase); /* accepts NULL */
			CloseLibraryInterface(RexxSysBase,IRexxSys);
			RexxSysBase = NULL;
		}

		CloseLibraryInterface(MUIMasterBase,IMUIMaster);
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
	int rexxsyslib_version;

	SM_ENTER;

#ifdef __AMIGAOS4__
	rexxsyslib_version = 45;
#else
	rexxsyslib_version = 0;
#endif

	if ((MUIMasterBase = OpenLibraryInterface(MUIMASTER_NAME, MUIMASTER_VMIN,&IMUIMaster)))
	{
		if ((RexxSysBase = OpenLibraryInterface("rexxsyslib.library",rexxsyslib_version,&IRexxSys)))
		{
#ifndef __AROS__ /*no simple html*/
			SimpleHTMLBase = OpenLibrary("PROGDIR:Libs/simplehtml.library",0);
			if (!SimpleHTMLBase) SimpleHTMLBase = OpenLibrary("PROGDIR:simplehtml.library",0);
			if (!SimpleHTMLBase) SimpleHTMLBase = OpenLibrary("simplehtml.library",0);

			if (SimpleHTMLBase)
			{
#endif /* AROS */

#ifdef __AMIGAOS4__
				/* The Interface of this library is optional. If not available on OS4,
				 * the function are called via the emulator */
				ISimpleHTML = (struct SimpleHTMLIFace*)GetInterface(SimpleHTMLBase, "main", 1, NULL);
#endif

				TTEngineBase = OpenLibraryInterface("ttengine.library",7,&ITTEngine);
				CyberGfxBase = OpenLibraryInterface("cybergraphics.library",40,&ICyberGfx);

				DefaultLocale = OpenLocale(NULL);
				init_hook_standard();

				if (arexx_init())
				{
					if (appicon_init())
					{
						if (create_utf8string_class() && create_foldertreelist_class() &&
						    create_mailtreelist_class() && create_addressstring_class() &&
						    create_attachmentlist_class() && create_datatypes_class() &&
						    create_transwnd_class() && create_composeeditor_class() &&
						    create_picturebutton_class() && create_popupmenu_class() &&
						    create_icon_class() && create_filterlist_class() &&
						    create_filterrule_class() && create_multistring_class() &&
						    create_audioselectgroup_class() && create_accountpop_class() &&
						    create_signaturecycle_class() && create_messageview_class() &&
								create_addressgrouplist_class() && create_addressentrylist_class() &&
								create_pgplist_class() && create_addressmatchlist_class() &&
								create_mailinfo_class() && create_smtoolbar_class())
						{
							if (app_init())
							{
								if (main_window_init())
								{
									SM_LEAVE;
									return 1;
								}
							} else puts(_("Failed to create the application\n"));
						} else puts(_("Could not create mui custom classes\n"));
					} else puts(_("Couldn't create appicon port\n"));
				} else puts(_("Couldn't create arexx port\n"));
#ifndef __AROS__
			} else printf(_("Couldn't open %s version %d\n"),"simplehtml.library",0);
#endif
		} else printf(_("Couldn't open %s version %d\n"),"rexxsyslib.library",rexxsyslib_version);
	} else printf(_("Couldn't open %s version %d\n"),MUIMASTER_NAME,MUIMASTER_VMIN);

	SM_LEAVE;
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
	main_set_folder_active(main_get_folder());
}

/****************************************************************
 The app is busy
*****************************************************************/
void app_busy(void)
{
	set(App,MUIA_Application_Sleep,TRUE);
}

/****************************************************************
 The app is ready
*****************************************************************/
void app_unbusy(void)
{
	set(App,MUIA_Application_Sleep,FALSE);
}

char *initial_mailto;
char *initial_subject;
char *initial_message;

/****************************************************************
 Called if the appicons needs to be refreshed
*****************************************************************/
static void refresh_appicon(void)
{
	appicon_refresh(0);
	thread_push_function_delayed(2000,refresh_appicon,0);
}

/****************************************************************
 Initialize the GUI
*****************************************************************/
int gui_init(void)
{
	int rc;
	int open_config_window = 0;

	SM_ENTER;

#ifdef __AMIGAOS4__
	{
		LONG default_charset = GetDiskFontCtrl(DFCTRL_CHARSET);
		STRPTR charset = (STRPTR)ObtainCharsetInfo(DFCS_NUMBER, default_charset, DFCS_MIMENAME);
		struct codeset *cs = codesets_find(charset);

		if (cs)
		{
			if (!user.config.from_disk)
				user.config.default_codeset = cs;
			else
			{
				BPTR lock = Lock("PROGDIR:.ignorecharset",ACCESS_READ);

				if (cs != user.config.default_codeset && !lock)
				{
					int rc;

					rc = sm_request(NULL,_("The charset configured for SimpleMail (%s) doesn't match\n"
														"the system's default charset (%s)! How to proceed?"),
													_("Change|Ignore once|Ignore always"),
													user.config.default_codeset->name,
													cs->name);

					if (rc == 1)
					{
						user.config.default_codeset = cs;
						open_config_window = 1;
					}
					else if (rc == 0)
					{
						BPTR fh = Open("PROGDIR:.ignorecharset",MODE_NEWFILE);
						if (fh) Close(fh);
					}
				} else UnLock(lock);
			}
		}
	}
#endif

	rc = 0;

	dt_init();

	if (all_init())
	{
		main_refresh_folders();

		if (main_window_open())
		{
			struct folder *f = NULL;

			startupwnd_close();
			if (user.config.startup_folder_name)
			{
				f = folder_find_by_name(user.config.startup_folder_name);
			}
			if (!f)
			{
				f = folder_incoming();
			}
			main_set_folder_active(f);

			if (open_config_window) open_config();

			/* if we should open the compose window soon after start */
			if (initial_mailto)
				callback_write_mail_to_str(initial_mailto, initial_subject);

			if (initial_message)
				callback_open_message(initial_message,-1);

			free(initial_message);
			free(initial_mailto);
			free(initial_subject);
			initial_mailto = NULL;
			initial_subject = NULL;
			initial_message = NULL;

			/* register appicon refresh function which is called every 2 seconds */
			thread_push_function_delayed(2000,refresh_appicon,0);
			rc = 1;
		}
	} else
	{
		all_del();
	}
	SM_RETURN(rc,"%ld");
}

/****************************************************************
 The GUI loop. Also cleans the gui.
*****************************************************************/
void gui_loop(void)
{
	SM_ENTER;

	/* Now really loop */
	loop();

	/* Cleanup the stuff */
	close_config();
	shutdownwnd_open();
	all_del();
	dt_cleanup();

	SM_LEAVE;
}

/****************************************************************
 Parse the start arguments
*****************************************************************/
int gui_parseargs(int argc, char *argv[])
{
	struct command_args {
		char *message;
	  char *mailto;
	  char *subject;
	  LONG *debuglevel;
	  char *debugout;
	  char *debugmodules;
	} shell_args;

	struct RDArgs *rdargs;

	memset(&shell_args,0,sizeof(shell_args));

	if ((rdargs = ReadArgs("MESSAGE,MAILTO/K,SUBJECT/K,DEBUG=DEBUGLEVEL/N/K,DEBUGOUT/K,DEBUGMODULES/K",(LONG*)&shell_args, NULL)))
	{
		initial_message = mystrdup(shell_args.message);
		initial_mailto = mystrdup(shell_args.mailto);
		initial_subject = mystrdup(shell_args.subject);
		if (shell_args.debugout) debug_set_out(shell_args.debugout);
		if (shell_args.debuglevel) debug_set_level(*shell_args.debuglevel);
		if (shell_args.debugmodules) debug_set_modules(shell_args.debugmodules);
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
			if (initial_message)
			{
				char *buf = malloc(mystrlen(initial_message)+100);
				if (buf)
				{
					sprintf(buf,"OPENMESSAGE \"\"\"%s\"",initial_message); /* Don't ask me why the quotating marks have to be stated so strange...it only works that way */
					SendRexxCommand("SIMPLEMAIL.1", buf, result, 40);
					free(buf);
				}
			} else
			{
				SendRexxCommand("SIMPLEMAIL.1", "MAINTOFRONT", result, 40);
			}
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
 The main entry point.
*****************************************************************/
#ifndef __AROS__
/* main() for AROS is in start-aros.c */
int main(int argc, char *argv[])
{
	return simplemail_main();
}
#endif

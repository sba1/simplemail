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

/**
 * @file appicon.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/wb.h>

#include "configuration.h"
#include "folder.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"
#include "debug.h"

#include "appicon.h"
#include "gui_main_amiga.h"

#include "mainwnd.h"
#include "statuswnd.h"

struct AppIcon_Stat
{
	int total_msg;
	int total_new;
	int total_unread;
	int total_sent;
	int total_outgoing;
	int total_deleted;
};

struct AppIcon_Config
{
	char *filename;

	LONG position_X;
	LONG position_Y;
};

static struct MsgPort *appicon_port;
static struct AppIcon *appicon;
static struct DiskObject *appicon_diskobject[SM_APPICON_MAX];
static struct AppIcon_Stat appicon_stat;
static struct AppIcon_Config appicon_config;
static int appicon_last_mode;
static int appicon_last_show;

static struct DiskObject *HideIcon;

static CONST_STRPTR appicon_names[SM_APPICON_MAX] =
{
	"check",
	"empty",
	"new",
	"old"
};

static void appicon_load_position(void);

/*****************************************************************************/

int appicon_init(void)
{
	int i;
	BPTR dirlock;
	BPTR odir;

	memset(&appicon_config,0,sizeof(struct AppIcon_Config));
	memset(&appicon_stat,0,sizeof(struct AppIcon_Stat));
	appicon_last_mode = -1;
	appicon = NULL;

	HideIcon = GetDiskObject("PROGDIR:SimpleMail");

	appicon_config.position_X = NO_ICON_POSITION;
	appicon_config.position_Y = NO_ICON_POSITION;
	appicon_load_position();

	dirlock = Lock(gui_get_images_directory(),ACCESS_READ);
	if (dirlock)
	{
		odir = CurrentDir(dirlock);

		for (i=0;i<SM_APPICON_MAX;i++)
		{
			if ((appicon_diskobject[i]=GetDiskObject(appicon_names[i])))
			{
				appicon_diskobject[i]->do_CurrentX = appicon_config.position_X;
				appicon_diskobject[i]->do_CurrentY = appicon_config.position_Y;
			}
		}
		CurrentDir(odir);
		UnLock(dirlock);
	}
	if (!(appicon_port = CreateMsgPort())) return 0;

	return 1;
}

/*****************************************************************************/

void appicon_free(void)
{
	int i;

	if (appicon) RemoveAppIcon(appicon);
	if (HideIcon) FreeDiskObject(HideIcon);
  if (appicon_port) DeleteMsgPort(appicon_port);
	for (i=0;i<SM_APPICON_MAX;i++)
	{
		if (appicon_diskobject[i]) FreeDiskObject(appicon_diskobject[i]);
	}
	if (appicon_config.filename) free(appicon_config.filename);
}

/*****************************************************************************/

ULONG appicon_mask(void)
{
	if (!appicon_port) return 0UL;
	return 1UL << appicon_port->mp_SigBit;
}

/*****************************************************************************/

void appicon_handle(void)
{
	struct AppMessage *appicon_msg;

	while ((appicon_msg = (struct AppMessage *)GetMsg(appicon_port)))
	{
		if (appicon_msg->am_Type == AMTYPE_APPICON)
		{
			switch (appicon_msg->am_Class)
			{
				case AMCLASSICON_Open:
					/* The "Open" menu item was invoked */
					if (main_is_iconified())
					{
						app_show();
					} else
					{
						main_window_open();
					}
					break;

				case AMCLASSICON_Delete:
					/* The "Delete" menu item was invoked */
					SM_DEBUGF(15,("Delete AppIcon (= Quit)\n"));
					app_quit();
					break;

				case AMCLASSICON_Snapshot:
					/* The "Snapshot" menu item was invoked */
					SM_DEBUGF(15,("Snapshot AppIcon\n"));
					appicon_snapshot();
					break;

				case AMCLASSICON_UnSnapshot:
					/* The "UnSnapshot" menu item was invoked */
					SM_DEBUGF(15,("Unsnapshot AppIcon\n"));
					appicon_unsnapshot();
					break;

				default:
					if (appicon_msg->am_NumArgs == 0)
					{
						/* The AppIcon was double-clicked */
						if (main_is_iconified())
						{
							app_show();
						} else
						{
							main_window_open();
						}
					}
			}
		}
		ReplyMsg(&appicon_msg->am_Message);
	}
}

/*****************************************************************************/

void appicon_refresh(int force)
{
	static char appicon_label[256];
	int appicon_mode;
	int total_msg,total_unread,total_new,total_sent,total_outgoing,total_deleted;
	struct folder *f;

	folder_get_stats(&total_msg,&total_unread,&total_new);

	/* The notcase indead should never happen, an assert would be appropriate */
	if ((f = folder_sent())) total_sent = f->num_index_mails;
	else total_sent = 0;
	if ((f = folder_outgoing())) total_outgoing = f->num_index_mails;
	else total_outgoing = 0;
	if ((f = folder_deleted())) total_deleted = f->num_index_mails;
	else total_deleted = 0;

	if (statuswnd_is_opened())
	{
		appicon_mode = SM_APPICON_CHECK;
	} else
	{
		appicon_mode = total_unread ? (total_new ? SM_APPICON_NEW : SM_APPICON_OLD) : SM_APPICON_EMPTY;
	}

	/* only refresh the appicon if something had changed to avoid flickering */
	if (!force &&
	    (appicon_stat.total_msg == total_msg) &&
	    (appicon_stat.total_new == total_new) &&
	    (appicon_stat.total_unread == total_unread) &&
	    (appicon_stat.total_sent == total_sent) &&
	    (appicon_stat.total_outgoing == total_outgoing) &&
	    (appicon_stat.total_deleted == total_deleted) &&
	    (appicon_last_mode == appicon_mode) &&
			(appicon_last_show == user.config.appicon_show))
	{
		return;
	}

	appicon_stat.total_msg = total_msg;
	appicon_stat.total_new = total_new;
	appicon_stat.total_unread = total_unread;
	appicon_stat.total_sent = total_sent;
	appicon_stat.total_outgoing = total_outgoing;
	appicon_stat.total_deleted = total_deleted;

	if (appicon)
	{
		RemoveAppIcon(appicon);
		appicon=NULL;
	}

	appicon_last_show = user.config.appicon_show;
	switch (user.config.appicon_show)
	{
		case 2: return;                           /* never */
		case 1: if (!main_is_iconified()) return; /* only when iconified */
	}

	if (appicon_diskobject[appicon_mode])
	{
		char *src;
		char buf[10];

		/* now build the label for the app icon, thx to YAM :) */
		appicon_label[0] = '\0';
		for (src = user.config.appicon_label; *src; src++)
		{
			int l;

			if (*src == '%')
			{
				switch (*++src)
				{
					case '%': strcpy(buf, "%");                   break;
					case 't': sprintf(buf, "%d", total_msg);      break;
					case 'n': sprintf(buf, "%d", total_new);      break;
					case 'u': sprintf(buf, "%d", total_unread);   break;
					case 's': sprintf(buf, "%d", total_sent);     break;
					case 'o': sprintf(buf, "%d", total_outgoing); break;
					case 'd': sprintf(buf, "%d", total_deleted);  break;
				}
			} else
			{
				sprintf(buf, "%c", *src);
			}
			/* TODO: Keep pointer */
			l = strlen(appicon_label);
			mystrlcpy(appicon_label + l, buf, sizeof(appicon_label) - l);
		}

		/* get the position of the last appicon */
		if ((appicon_last_mode >= 0) && (appicon_last_mode < SM_APPICON_MAX))
		{
			if (appicon_mode != appicon_last_mode)
			{
				appicon_diskobject[appicon_mode]->do_CurrentX = appicon_diskobject[appicon_last_mode]->do_CurrentX;
				appicon_diskobject[appicon_mode]->do_CurrentY = appicon_diskobject[appicon_last_mode]->do_CurrentY;
			}
		}

		if (WorkbenchBase->lib_Version < 44)
		{
			appicon = AddAppIcon(0, 0, (char *)appicon_label, appicon_port, 0, appicon_diskobject[appicon_mode],
				TAG_DONE);
		} else
		{
			/* Only V44+ of wb supports this tags */
			appicon = AddAppIcon(0, 0, (char *)appicon_label, appicon_port, 0, appicon_diskobject[appicon_mode],
				WBAPPICONA_SupportsOpen, TRUE,
				WBAPPICONA_SupportsDelete, TRUE,
				WBAPPICONA_PropagatePosition, TRUE,
				WBAPPICONA_SupportsSnapshot, TRUE,
				WBAPPICONA_SupportsUnSnapshot, TRUE,
				TAG_DONE);
		}
		if (appicon) appicon_last_mode = appicon_mode;
	}
}

/*****************************************************************************/

/**
 * Load the AppIcon position
 */
static void appicon_load_position(void)
{
	char *buf;

	/* first, try to get the position from the tooltypes */
	if (HideIcon)
	{
		char *c_pos;
		c_pos = FindToolType(HideIcon->do_ToolTypes, "APPICON_POSX");
		if (c_pos) appicon_config.position_X = atoi(c_pos);
		c_pos = FindToolType(HideIcon->do_ToolTypes, "APPICON_POSY");
		if (c_pos) appicon_config.position_Y = atoi(c_pos);
	}

	/* now, try to load the position from the appicon config file */
	if ((buf = (char *)malloc(512)))
	{
		FILE *fh;

		if (user.directory) strcpy(buf,user.directory);
		else buf[0] = 0;

		sm_add_part(buf,".appicon",512);

		if ((appicon_config.filename = mystrdup(buf)))
		{
			if ((fh = fopen(appicon_config.filename,"r")))
			{
				SM_DEBUGF(15,("Parsing config file: \"%s\"\n",buf));
				myreadline(fh,buf);
				if (!strncmp("SMAI",buf,4))
				{
					while (myreadline(fh,buf))
					{
						char *result;

						SM_DEBUGF(15,("Parsing config string: \"%s\"\n",buf));

						if ((result = get_key_value(buf,"AppIcon.PositionX")))
							appicon_config.position_X = atoi(result);
						if ((result = get_key_value(buf,"AppIcon.PositionY")))
							appicon_config.position_Y = atoi(result);

					}
				}
				fclose(fh);
			}
		}
		free(buf);
	}
}

/**
 * Save the AppIcon position.
 */
static void appicon_save_position(void)
{
	if (appicon_config.filename)
	{
		FILE *fh = fopen(appicon_config.filename, "w");
		if (fh)
		{
			fputs("SMAI\n\n",fh);

			fprintf(fh,"AppIcon.PositionX=%ld\n",appicon_config.position_X);
			fprintf(fh,"AppIcon.PositionY=%ld\n",appicon_config.position_Y);

			fclose(fh);
		}
	}
}

/*****************************************************************************/

void appicon_snapshot(void)
{
	if ((appicon_last_mode >= 0) && (appicon_last_mode < SM_APPICON_MAX))
	{
		appicon_config.position_X = appicon_diskobject[appicon_last_mode]->do_CurrentX;
		appicon_config.position_Y = appicon_diskobject[appicon_last_mode]->do_CurrentY;
		appicon_save_position();
	}
}

/*****************************************************************************/

void appicon_unsnapshot(void)
{
	int i;

	appicon_config.position_X = NO_ICON_POSITION;
	appicon_config.position_Y = NO_ICON_POSITION;
	if (appicon_config.filename) remove(appicon_config.filename);
	for(i=0;i<SM_APPICON_MAX;i++)
	{
		if (appicon_diskobject[i])
		{
			appicon_diskobject[i]->do_CurrentX = appicon_config.position_X;
			appicon_diskobject[i]->do_CurrentY = appicon_config.position_Y;
		}
	}
	appicon_refresh(1);
}

/*****************************************************************************/

struct DiskObject *appicon_get_hide_icon(void)
{
	return HideIcon;
}

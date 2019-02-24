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
 * @file mainwnd.c
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <ncurses.h>
#include <panel.h>

#include "folder.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"

#include "gui_main_ncurses.h"
#include "mainwnd.h"

/*****************************************************************************/

static WINDOW *messagelist_wnd;
static WINDOW *folders_wnd;
static WINDOW *status_wnd;
static int folders_width = 20;

static struct folder *main_active_folder;

static struct gui_key_listener prev_folder_listener;
static struct gui_key_listener next_folder_listener;
static struct gui_key_listener fetch_mail_listener;

/*****************************************************************************/

static void main_folder_next(void)
{
	if (!main_active_folder)
	{
		main_active_folder = folder_first();
	} else
	{
		main_active_folder = folder_next(main_active_folder);
	}
	main_refresh_folders();
	callback_folder_active();
}

/*****************************************************************************/

static void main_folder_prev(void)
{
	if (!main_active_folder)
	{
		main_active_folder = folder_last();
	} else
	{
		main_active_folder = folder_prev(main_active_folder);
	}
	main_refresh_folders();
	callback_folder_active();
}

/*****************************************************************************/

int main_window_open(void)
{
	int w, h;

	getmaxyx(stdscr, h, w);
	h -= 2;

	messagelist_wnd = newwin(h, w - folders_width, 0, folders_width);
	folders_wnd = newwin(h, folders_width, 0, 0);
	status_wnd = newwin(1, w, h, 0);
	refresh();

	wrefresh(messagelist_wnd);
	wrefresh(folders_wnd);
	wrefresh(status_wnd);

	gui_add_key_listener(&next_folder_listener, 'n', _("Next folder"), main_folder_next);
	gui_add_key_listener(&prev_folder_listener, 'p', _("Prev folder"), main_folder_prev);
	gui_add_key_listener(&fetch_mail_listener, 'f', _("Fetch"), callback_fetch_mails);

	return 1;
}

/*****************************************************************************/

void main_refresh_folders(void)
{
	int row = 0;
	struct folder *f;
	char text[folders_width + 1];

	for (f = folder_first(); f; f = folder_next(f))
	{
		unsigned int level;
		unsigned int i;

		level = folder_level(f);
		if (level > 10)
		{
			level = 10;
		}

		if (f == main_active_folder)
		{
			text[0] = '*';
		} else
		{
			text[0] = ' ';
		}

		for (i = 0; i < level; i++)
		{
			text[1+i] = ' ';
		}

		mystrlcpy(&text[level + 1], folder_name(f), sizeof(text) - 2);
		mvwprintw(folders_wnd, row++, 0 , text);
	}
	wrefresh(folders_wnd);
}

/*****************************************************************************/

void main_refresh_folder(struct folder *folder)
{
}

/*****************************************************************************/

struct folder *main_get_folder(void)
{
	return main_active_folder;
}

/*****************************************************************************/

void main_set_folder_active(struct folder *folder)
{
	main_active_folder = folder;
	main_refresh_folders();
	callback_folder_active();
}

/*****************************************************************************/

void main_set_folder_mails(struct folder *folder)
{
	void *handle = NULL;
	struct mail_info *mi;
	int row = 0;

	while ((mi = folder_next_mail(folder, &handle)))
	{
		const char *from = mail_info_get_from(mi);
		mvwprintw(messagelist_wnd, row++, 0, from?from:"Unknown");
	}
	wrefresh(messagelist_wnd);
}

/*****************************************************************************/

void main_insert_mail(struct mail_info *mail)
{
}

/*****************************************************************************/

void main_insert_mail_pos(struct mail_info *mail, int after)
{
}

/*****************************************************************************/

void main_set_progress(unsigned int max_work, unsigned int work)
{
}

/*****************************************************************************/

void main_set_status_text(char *txt)
{
	mvwprintw(status_wnd, 0, 0, txt);
	wclrtoeol(status_wnd);
	wrefresh(status_wnd);
}


/*****************************************************************************/

void main_freeze_mail_list(void)
{
}

/*****************************************************************************/

void main_thaw_mail_list(void)
{
}

/*****************************************************************************/

int main_is_iconified(void)
{
	return 0;
}

/*****************************************************************************/

void main_hide_progress(void)
{
}

/*****************************************************************************/

void main_refresh_window_title(const char *title)
{
}

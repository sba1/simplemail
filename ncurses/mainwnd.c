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

#include "folder.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"

#include "gadgets.h"
#include "gui_main_ncurses.h"
#include "mainwnd.h"
#include "timesupport.h"

/*****************************************************************************/

static int messagelist_active = -1;

static struct window main_win;
static struct simple_text_label main_status_label;
static struct listview main_folder_listview;

static int main_messagelist_from_width = 0;
static int main_messagelist_subject_width = 0;
static int main_messagelist_date_width = 0;
static struct listview main_message_listview;

static int folders_width = 20;

static struct folder *main_active_folder;

static struct key_listener prev_folder_listener;
static struct key_listener next_folder_listener;
static struct key_listener fetch_mail_listener;
static struct key_listener next_mail_listener;
static struct key_listener prev_mail_listener;
static struct key_listener read_mail_listener;

/*****************************************************************************/

static void main_folder_next(void)
{
	struct folder *new_main_active_folder;
	if (!main_active_folder)
	{
		new_main_active_folder = folder_first();
	} else
	{
		new_main_active_folder = folder_next(main_active_folder);
	}
	main_set_folder_active(new_main_active_folder);
}

/*****************************************************************************/

static void main_folder_prev(void)
{
	struct folder *new_main_active_folder;
	if (!main_active_folder)
	{
		new_main_active_folder = folder_last();
	} else
	{
		new_main_active_folder = folder_prev(main_active_folder);
	}
	main_set_folder_active(new_main_active_folder);
}

/*****************************************************************************/

static void main_next_mail(void)
{
	if (!main_active_folder)
	{
		return;
	}

	if (messagelist_active != folder_number_of_mails(main_active_folder))
	{
		messagelist_active++;
		main_set_folder_mails(main_active_folder);
	}
}

/*****************************************************************************/

static void main_prev_mail(void)
{
	if (!main_active_folder)
	{
		return;
	}

	if (messagelist_active >= 0)
	{
		messagelist_active--;
		main_set_folder_mails(main_active_folder);
	}
}

/*****************************************************************************/

static void main_read_mail(void)
{
	callback_read_active_mail();
}

/*****************************************************************************/

/**
 * Find folder mail by its position.
 */
static struct mail_info *folder_find_mail_by_pos(struct folder *f, int pos)
{
	void *handle = NULL;
	struct mail_info *mi;

	while ((mi = folder_next_mail(f, &handle)))
	{
		if (!pos--)
		{
			return mi;
		}
	}
	return NULL;
}

/*****************************************************************************/

static void main_folder_render(int pos, char *buf, int bufsize)
{
	struct folder *f;
	int level;
	int i;

	buf[0] = 0;

	if (!(f = folder_find(pos)))
	{
		return;
	}

	level = folder_level(f);
	if (level > 10)
	{
		level = 10;
	}

	if (level > bufsize)
	{
		level = bufsize;
	}

	for (i = 0; i < level; i++)
	{
		buf[i] = ' ';
	}

	if (level < bufsize)
	{
		mystrlcpy(&buf[level], folder_name(f), bufsize - level);
	}
}

/*****************************************************************************/

static void main_message_render(int pos, char *buf, int bufsize)
{
	struct mail_info *mi;
	const char *from;
	const char *date;
	char from_buf[100];

	buf[0] = 0;

	if (!main_active_folder)
	{
		return;
	}

	if (!(mi = folder_find_mail_by_pos(main_active_folder, pos)))
	{
		return;
	}

	from = mail_info_get_from(mi);
	date = sm_get_date_str(mi->seconds);

	mystrlcpy(from_buf, from?from:"Unknown", sizeof(from_buf));

	snprintf(buf, bufsize, "%-*s %-*s %s",
		main_messagelist_from_width, from,
		main_messagelist_subject_width, mi->subject,
		date);
}

/*****************************************************************************/

static int folder_count()
{
	struct folder *f = folder_first();
	int count = 0;
	while (f)
	{
		count++;
		f = folder_next(f);
	}
	return count;
}

/*****************************************************************************/

int main_window_open(void)
{
	windows_init(&main_win);
	screen_add_window(&gui_screen, &main_win);
	gadgets_set_extend(&main_win.g.g, 0, 0, gui_screen.w, gui_screen.h - 1);

	gadgets_init_listview(&main_folder_listview, main_folder_render);
	gadgets_set_extend(&main_folder_listview.g, 0, 0, folders_width, gui_screen.h - 2);
	gadgets_add(&main_win.g, &main_folder_listview.g);
	main_folder_listview.rows = folder_count();

	gadgets_init_listview(&main_message_listview, main_message_render);
	gadgets_set_extend(&main_message_listview.g, folders_width, 0, gui_screen.w - folders_width, gui_screen.h - 2);
	gadgets_add(&main_win.g, &main_message_listview.g);

	gadgets_init_simple_text_label(&main_status_label, "");
	gadgets_set_extend(&main_status_label.tl.g, 0, gui_screen.h - 2, gui_screen.w, 1);
	gadgets_add(&main_win.g, &main_status_label.tl.g);

	windows_display(&main_win, &gui_screen);

	screen_add_key_listener(&gui_screen, &next_folder_listener, 'n', _("Next folder"), main_folder_next);
	screen_add_key_listener(&gui_screen, &prev_folder_listener, 'p', _("Prev folder"), main_folder_prev);
	screen_add_key_listener(&gui_screen, &fetch_mail_listener, 'f', _("Fetch"), callback_fetch_mails);
	screen_add_key_listener(&gui_screen, &next_mail_listener, GADS_KEY_DOWN, NULL, main_next_mail);
	screen_add_key_listener(&gui_screen, &prev_mail_listener, GADS_KEY_UP, NULL, main_prev_mail);
	screen_add_key_listener(&gui_screen, &read_mail_listener, '\n', NULL, main_read_mail);

	return 1;
}

/*****************************************************************************/

void main_refresh_folders(void)
{
	main_folder_listview.rows = folder_count();
	windows_display(&main_win, &gui_screen);
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

void main_refresh_mail(struct mail_info *m)
{
	if (!main_active_folder)
	{
		return;
	}
	main_set_folder_mails(main_active_folder);
}

/*****************************************************************************/

void main_set_folder_active(struct folder *folder)
{
	main_active_folder = folder;
	main_folder_listview.active = folder_position(folder);

	main_refresh_folders();
	callback_folder_active();
}

/*****************************************************************************/

void main_set_folder_mails(struct folder *folder)
{
	void *handle = NULL;
	struct mail_info *mi;

	char from_buf[128];

	/* Update column widths */
	main_messagelist_from_width = 0;
	main_messagelist_subject_width = 0;
	main_messagelist_date_width = 0;

	/* Determine dimensions */
	while ((mi = folder_next_mail(folder, &handle)))
	{
		int l;

		const char *from = mail_info_get_from(mi);
		const char *subject = mi->subject;
		const char *date = sm_get_date_str(mi->seconds);

		if (!from) from = "Unknown";

		l = strlen(from);
		if (l > main_messagelist_from_width)
		{
			main_messagelist_from_width = l;
		}

		l = mystrlen(subject);
		if (l > main_messagelist_subject_width)
		{
			main_messagelist_subject_width = l;
		}

		l = strlen(date);
		if (l > main_messagelist_date_width)
		{
			main_messagelist_date_width = l;
		}
	}

	if (main_messagelist_from_width >= sizeof(from_buf))
	{
		main_messagelist_from_width = sizeof(from_buf) - 1;
	}

	main_message_listview.active = messagelist_active;
	main_message_listview.rows = folder_number_of_mails(folder);
	windows_display(&main_win, &gui_screen);
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
	gadgets_set_label_text(&main_status_label, txt);
	windows_display(&main_win, &gui_screen);
}

/*****************************************************************************/

struct mail_info *main_get_active_mail(void)
{
	void *handle = NULL;
	struct mail_info *mi;
	int row = 0;

	if (!main_active_folder || messagelist_active < 0)
	{
		return NULL;
	}

	while ((mi = folder_next_mail(main_active_folder, &handle)))
	{
		if (row == messagelist_active)
		{
			break;
		}
		row++;
	}
	return mi;
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

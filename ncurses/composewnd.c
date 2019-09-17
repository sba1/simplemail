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
 * @file composewnd.c
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "account.h"
#include "addresslist.h"
#include "configuration.h"
#include "mail.h"
#include "support_indep.h"

#include "composewnd.h"
#include "gadgets.h"

/*****************************************************************************/

static struct window compose_win;
static int compose_win_initialized;
static int compose_win_removed;

static struct group compose_group;
static struct text_edit compose_edit;

static struct key_listener send_listener;
static struct key_listener later_listener;
static struct key_listener close_listener;

/*****************************************************************************/

static void compose_current()
{
}

/*****************************************************************************/

static void compose_window_close_current(void)
{
	if (compose_win_initialized)
	{
		screen_remove_window(&gui_screen, &compose_win);
		compose_win_removed = 1;
	}
}

/*****************************************************************************/

static void compose_window_send_current(void)
{
	compose_window_close_current();
}

/*****************************************************************************/

static void compose_window_send_later_current(void)
{
	compose_window_close_current();
}

/*****************************************************************************/

int compose_window_editable(struct text_edit *te, int ch, int x, int y, void *udata)
{
	if (y == 0 && x < 6)
	{
		return 0;
	}

	if (y == 1 && x < 4)
	{
		return 0;
	}

	return 1;
}

/*****************************************************************************/

int compose_window_open(struct compose_args *args)
{
	if (!compose_win_initialized)
	{
		string txt;

		/* FIXME: Doesn't work with multiple windows yet */
		windows_init(&compose_win);
		screen_add_window(&gui_screen, &compose_win);
		gadgets_set_extend(&compose_win.g.g, 0, 0, gui_screen.w, gui_screen.h - 2);

		gadgets_init_group(&compose_group);
		gadgets_set_extend(&compose_group.g, 0, 0, gui_screen.w, gui_screen.h - 2);

		gadgets_init_text_edit(&compose_edit);
		gadgets_set_extend(&compose_edit.g, 0, 0, gui_screen.w, gui_screen.h - 2);
		gadgets_add(&compose_group, &compose_edit.g);

		gadgets_add(&compose_win.g, &compose_group.g);

		windows_activate_gadget(&compose_win, &compose_edit.g);
		compose_win_initialized = 1;
		compose_edit.editable = compose_window_editable;

		string_initialize(&txt, 1000);

		string_append(&txt, "From: ");

		if (args->to_change)
		{
			char *from;

			/* Find and set the correct account */
			if ((from = mail_find_header_contents(args->to_change, "from")))
			{
				struct account *ac;

				if ((ac = account_find_by_from(from)))
				{
					string_append(&txt, ac->email);
				}
			}
		}
		string_append(&txt, "\n");

		string_append(&txt, "To: ");

		if (args->to_change)
		{
			if (args->to_change->info->to_list)
			{
				utf8 *to_str;

				if ((to_str = address_list_to_utf8_codeset_safe(args->to_change->info->to_list,user.config.default_codeset)))
				{
					string_append(&txt, to_str);
					free(to_str);
				}
			}
		}

		string_append(&txt, "\n");

		if (args->to_change)
		{
			if (args->to_change->info->cc_list)
			{
				utf8 *cc_str;

				string_append(&txt, "CC: ");
				if ((cc_str = address_list_to_utf8_codeset_safe(args->to_change->info->cc_list,user.config.default_codeset)))
				{
					string_append(&txt, cc_str);
					free(cc_str);
				}
			}

			string_append(&txt, "\n");

			string_append(&txt, "Subject: ");
			string_append(&txt, "\n");

			string_append(&txt, "\n");
		}

		gadgets_set_text_edit_contents(&compose_edit, txt.str);
		free(txt.str);

		windows_add_key_listener(&compose_win, &send_listener, 's', "Send", compose_window_send_current);
		windows_add_key_listener(&compose_win, &later_listener, 'l', "Later", compose_window_send_later_current);
		windows_add_key_listener(&compose_win, &close_listener, 'c', "Close", compose_window_close_current);
	} else if (compose_win_removed)
	{
		screen_add_window(&gui_screen, &compose_win);
	}
	windows_display(&compose_win, &gui_screen);

	return 0;
}

/*****************************************************************************/

void compose_window_activate(int num)
{
}

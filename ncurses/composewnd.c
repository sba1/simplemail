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

#include "composewnd.h"

#include "gadgets.h"

/*****************************************************************************/

static struct window compose_win;
static int compose_win_initialized;
static int compose_win_removed;

static struct group compose_group;
static struct text_edit compose_edit_group;

static struct key_listener close_listener;

static void compose_window_close_current(void)
{
	if (compose_win_initialized)
	{
		screen_remove_window(&gui_screen, &compose_win);
		compose_win_removed = 1;
	}
}

/*****************************************************************************/

int compose_window_open(struct compose_args *args)
{
	if (!compose_win_initialized)
	{
		/* FIXME: Doesn't work with multiple windows yet */
		windows_init(&compose_win);
		screen_add_window(&gui_screen, &compose_win);
		gadgets_set_extend(&compose_win.g.g, 0, 0, gui_screen.w, gui_screen.h - 2);

		gadgets_init_group(&compose_group);
		gadgets_set_extend(&compose_group.g, 0, 0, gui_screen.w, gui_screen.h - 2);

		gadgets_init_text_edit(&compose_edit_group);
		gadgets_set_extend(&compose_edit_group.g, 0, 0, gui_screen.w, gui_screen.h - 2);

		gadgets_add(&compose_win.g, &compose_group.g);

		compose_win_initialized = 1;

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

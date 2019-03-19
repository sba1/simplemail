/**
 * @file
 */

#include "readwnd.h"

#include "gui_main_ncurses.h"

#include <ncurses.h>
#include <panel.h>

/******************************************************************************/

static WINDOW *read_wnd;
static PANEL *read_panel;

static struct gui_key_listener close;

/******************************************************************************/

static void read_window_close_current(void)
{
	gui_remove_key_listener(&close);

	hide_panel(read_panel);
	update_panels();
	doupdate();
}

/******************************************************************************/

void read_refresh_prevnext_button(struct folder *f)
{
}

/******************************************************************************/

int read_window_open(const char *folder, struct mail_info *mail, int window)
{
	int w, h;

	getmaxyx(stdscr, h, w);
	h -= 2;

	if (!read_wnd)
	{
		read_wnd = newwin(h, w, 0, 0);
		read_panel = new_panel(read_wnd);
		show_panel(read_panel);
	} else
	{
		show_panel(read_panel);
		update_panels();
		doupdate();
	}

	gui_add_key_listener(&close, 'c', "Close", read_window_close_current);
	refresh();

	wrefresh(read_wnd);

	return 1;
}

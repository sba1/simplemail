/**
 * @file
 */

#include "readwnd.h"

#include <ncurses.h>
#include <panel.h>

/******************************************************************************/

static WINDOW *read_wnd;
static PANEL *read_panel;

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

	read_wnd = newwin(h, w, 0, 0);
	read_panel = new_panel(read_wnd);

	refresh();

	wrefresh(read_wnd);

	return 1;
}

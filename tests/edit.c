/**
 * @file
 *
 * Playground for the text edit gadget.
 */

#include "gadgets.h"

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

/******************************************************************************/

static void edit_exit(void)
{
	endwin();
}

/******************************************************************************/

static void gui_segf_handler (int signo)
{
	endwin();

	/* Let the default handler handle the rest */
	signal(signo, SIG_DFL);
}

/*****************************************************************************/

int main(int argc, char **argv)
{
	struct screen scr;
	struct window win;
	struct text_edit te;

	signal(SIGSEGV, gui_segf_handler);

	atexit(edit_exit);

	screen_init(&scr);

	windows_init(&win);
	gadgets_set_extend(&win.g.g, 0, 0, scr.w, scr.h);
	screen_add_window(&scr, &win);

	gadgets_init_text_edit(&te);
	gadgets_set_extend(&te.g, 0, 0, scr.w, scr.h);
	gadgets_add(&win.g, &te.g);

	windows_activate_gadget(&win, &te.g);
	windows_display(&win, &scr);

	while(!screen_handle(&scr))
	{
		usleep(10000);
	}
	return 0;
}

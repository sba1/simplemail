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

	signal(SIGSEGV, gui_segf_handler);

	atexit(edit_exit);

	screen_init(&scr);

	while(!screen_handle(&scr))
	{
		usleep(10000);
	}
	return 0;
}

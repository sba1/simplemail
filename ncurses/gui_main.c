#include <stdlib.h>
#include <stdio.h>

#include <ncurses.h>

#include "debug.h"
#include "gui_main.h"

/*****************************************************************************/

int gui_init(void)
{
	atexit(endwin);
	initscr();

	return 1;
}

/*****************************************************************************/

void gui_loop(void)
{
}

/*****************************************************************************/

void gui_deinit(void)
{
}

/*****************************************************************************/

static int gui_argc;
static char **gui_argv;

int gui_parseargs(int argc, char *argv[])
{
	int i = 0;

	for (i = 0; i < argc; i++)
	{
		if (!strcmp("--debug"))
		{
			debug_set_level(20);
		}
	}
	return 1;
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
	gui_argc = argc;
	gui_argv = argv;

	return simplemail_main();
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ncurses.h>

#include "debug.h"
#include "lists.h"
#include "simplemail.h"

#include "gui_main.h"
#include "mainwnd.h"

#include "gui_main_ncurses.h"

/*****************************************************************************/

static struct list gui_key_listeners;

/*****************************************************************************/

void gui_add_key_listener(struct gui_key_listener *listener, char ch, void (*callback)(void))
{
	listener->ch = ch;
	listener->callback = callback;
	list_insert_tail(&gui_key_listeners, &listener->n);
}

/*****************************************************************************/

void gui_remove_key_listener(struct gui_key_listener *listener)
{
	node_remove(&listener->n);
}

/*****************************************************************************/

static void gui_atexit(void)
{
	endwin();
}

/*****************************************************************************/

int gui_init(void)
{
	atexit(gui_atexit);
	initscr();
	noecho();

	list_init(&gui_key_listeners);
	main_window_open();

	return 1;
}

/*****************************************************************************/

void gui_loop(void)
{
	char ch;

	while ((ch = getch()) != 'q')
	{
		struct gui_key_listener *l;

		l = (struct gui_key_listener *)list_first(&gui_key_listeners);
		while (l)
		{
			struct gui_key_listener *n;

			n = (struct gui_key_listener *)node_next(&l->n);
			if (l->ch == ch)
			{
				l->callback();
			}

			l = n;
		}
	}
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
	int i;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp("--debug", argv[i]))
		{
			debug_set_level(20);
		}

		if (!strcmp("-h", argv[i]) || !strcmp("--help", argv[i]))
		{
			fprintf(stderr, "Usage: %s [--debug] [--help]\n");
			exit(0);
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

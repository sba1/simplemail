#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ncurses.h>

#include "debug.h"
#include "lists.h"
#include "folder.h"
#include "simplemail.h"

#include "gui_main.h"
#include "mainwnd.h"

#include "gui_main_ncurses.h"

/*****************************************************************************/

static struct list gui_key_listeners;
static WINDOW *keyinfo_wnd;

/*****************************************************************************/

static void gui_update_keyinfo(void)
{
	struct gui_key_listener *l;
	int col = 0;

	l = (struct gui_key_listener *)list_first(&gui_key_listeners);
	while (l)
	{
		char buf[20];
		sm_snprintf(buf, sizeof(buf), "%c: %s", l->ch, l->short_description);
		mvwprintw(keyinfo_wnd, 0, col, buf);

		col += strlen(buf) + 2;
		l = (struct gui_key_listener *)node_next(&l->n);
	}

	wrefresh(keyinfo_wnd);
}

/*****************************************************************************/

void gui_add_key_listener(struct gui_key_listener *listener, char ch, const char *short_description, void (*callback)(void))
{
	listener->ch = ch;
	listener->callback = callback;
	listener->short_description = short_description;
	list_insert_tail(&gui_key_listeners, &listener->n);

	gui_update_keyinfo();
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
	int w, h;

	atexit(gui_atexit);
	initscr();
	noecho();
	curs_set(0);

	list_init(&gui_key_listeners);

	getmaxyx(stdscr, h, w);
	keyinfo_wnd = newwin(1, w, h - 1, 0);

	main_window_open();
	main_refresh_folders();
	main_set_folder_active(folder_incoming());

	return 1;
}

/*****************************************************************************/

void gui_loop(void)
{
	char ch;

	save_config();

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

	for (i = 1; i < gui_argc; i++)
	{
		if (!strcmp("--debug", gui_argv[i]))
		{
			debug_set_level(20);
		}

		if (!strcmp("-h", gui_argv[i]) || !strcmp("--help", gui_argv[i]))
		{
			fprintf(stderr, "Usage: %s [--debug] [--help]\n", gui_argv[0]);
			exit(0);
		}
	}
	return 1;
}

/*****************************************************************************/

void app_show(void)
{
}

/*****************************************************************************/

void app_busy(void)
{
}

/*****************************************************************************/

void app_unbusy(void)
{
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
	gui_argc = argc;
	gui_argv = argv;

	return simplemail_main();
}

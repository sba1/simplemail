#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ncurses.h>

#include "configuration.h"
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

		if (l->ch == NCURSES_UP || l->ch == NCURSES_DOWN)
		{
			goto next;
		}

		sm_snprintf(buf, sizeof(buf), "%c: %s", l->ch, l->short_description);
		mvwprintw(keyinfo_wnd, 0, col, buf);

		col += strlen(buf) + 2;

next:
		l = (struct gui_key_listener *)node_next(&l->n);
	}

	wrefresh(keyinfo_wnd);
}

/*****************************************************************************/

void gui_add_key_listener(struct gui_key_listener *listener, int ch, const char *short_description, void (*callback)(void))
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
	nodelay(stdscr, TRUE);

	list_init(&gui_key_listeners);

	getmaxyx(stdscr, h, w);
	keyinfo_wnd = newwin(1, w, h - 1, 0);

	main_window_open();
	main_refresh_folders();
	main_set_folder_active(folder_incoming());

	return 1;
}

/*****************************************************************************/

/**
 * Handle timer events. For now, assume nodelay and thus,
 * poll stdin.
 *
 * @param userdata
 */
static void *gui_timer(void *userdata)
{
	int ch;
	while ((ch = getch()) != 'q')
	{
		struct gui_key_listener *l;

		if (ch == '\033')
		{
			if (getch() == '[')
			{
				switch (getch())
				{
				case	'A': /* up */
					ch = NCURSES_UP;
					break;
				case	'B': /* down */
					ch = NCURSES_DOWN;
					break;
				default:
					ch = -1;
					break;
				}
			} else
			{
				ch = -1;
			}
		}

		if (ch == -1)
		{
			return;
		}

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

	if (ch == 'q')
	{
		thread_abort(thread_get_main());
	}
}

/******************************************************************************/

void gui_loop(void)
{
	save_config();

	thread_wait(NULL, gui_timer, NULL, 10);
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

		if (!strcmp("--debug-file", gui_argv[i]) && i + 1 < argc)
		{
			debug_set_out(gui_argv[i+1]);
		}

		if (!strncmp("--debug-file=", gui_argv[i], 13))
		{
			debug_set_out(&gui_argv[i][13]);
		}

		if (!strcmp("-h", gui_argv[i]) || !strcmp("--help", gui_argv[i]))
		{
			fprintf(stderr, "Usage: %s [--debug] [--debug-file=<filename>] [--help]\n", gui_argv[0]);
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

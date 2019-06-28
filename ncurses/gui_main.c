#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <ncurses.h>

#include "configuration.h"
#include "debug.h"
#include "lists.h"
#include "folder.h"
#include "simplemail.h"
#include "support.h"

#include "gadgets.h"
#include "gui_main.h"
#include "mainwnd.h"

#include "gui_main_ncurses.h"

/*****************************************************************************/

static WINDOW *keyinfo_wnd;

struct screen gui_screen;

/*****************************************************************************/

static void gui_atexit(void)
{
	endwin();
}

/*****************************************************************************/

static void gui_segf_handler (int signo)
{
	gui_atexit();

	/* Let the default handler handle the rest */
	signal(signo, SIG_DFL);
}

/*****************************************************************************/

void gui_screen_keys_changed(struct screen *scr)
{
	char buf[256];
	screen_key_description_line(scr, buf, sizeof(buf));
	mvwprintw(keyinfo_wnd, 0, 0, buf);
	wclrtoeol(keyinfo_wnd);
	wrefresh(keyinfo_wnd);
}

/*****************************************************************************/

int gui_init(void)
{
	int w, h;

	atexit(gui_atexit);

	signal(SIGSEGV, gui_segf_handler);

	initscr();
	noecho();
	curs_set(0);
	nodelay(stdscr, TRUE);

	screen_init(&gui_screen);
	gui_screen.keys_changed = gui_screen_keys_changed;

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
	while ((ch = wgetch(gui_screen.handle)) != 'q')
	{
		if (ch == '\033')
		{
			if (getch() == '[')
			{
				switch (getch())
				{
				case	'A': /* up */
					ch = GADS_KEY_UP;
					break;
				case	'B': /* down */
					ch = GADS_KEY_DOWN;
					break;
				default:
					ch = -1;
					break;
				}
			} else
			{
				ch = GADS_KEY_NONE;
			}
		} else if (ch == KEY_RESIZE)
		{
			screen_invoke_resize_listener(&gui_screen);
			continue;
		}

		if (ch == -1)
		{
			return NULL;
		}

		if (gui_screen.active)
		{
			struct gadget *g;

			if ((g = gui_screen.active->active))
			{
				if (g->input)
				{
					if (g->input(g, ch))
					{
						continue;
					}
				}
			}
		}
		screen_invoke_key_listener(&gui_screen, ch);
	}

	if (ch == 'q')
	{
		thread_abort(thread_get_main());
	}
	return NULL;
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

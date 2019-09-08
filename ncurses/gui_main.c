#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

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

struct screen gui_screen;

static struct window keyinfo_win;
static struct simple_text_label keyinfo_label;

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
	gadgets_set_label_text(&keyinfo_label, buf);
	windows_display(&keyinfo_win, &gui_screen);
}

/*****************************************************************************/

int gui_init(void)
{
	atexit(gui_atexit);

	signal(SIGSEGV, gui_segf_handler);

	screen_init(&gui_screen);
	gui_screen.keys_changed = gui_screen_keys_changed;

	/* Setup and display the key info status bar */
	windows_init(&keyinfo_win);
	keyinfo_win.no_input = 1;
	screen_add_window(&gui_screen, &keyinfo_win);
	gadgets_set_extend(&keyinfo_win.g.g, 0, gui_screen.h - 1, gui_screen.w, 1);
	gadgets_init_simple_text_label(&keyinfo_label, "");
	gadgets_set_extend(&keyinfo_label.tl.g, 0, 0, gui_screen.w, 1);
	gadgets_add(&keyinfo_win.g, &keyinfo_label.tl.g);
	windows_display(&keyinfo_win, &gui_screen);

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
		if (ch == KEY_RESIZE)
		{
			screen_invoke_resize_listener(&gui_screen);
			continue;
		} else if (ch >= 0x100)
		{
			switch (ch)
			{
			case KEY_DOWN:
				ch = GADS_KEY_DOWN;
				break;
			case KEY_UP:
				ch = GADS_KEY_UP;
				break;
			case KEY_LEFT:
				ch = GADS_KEY_LEFT;
				break;
			case KEY_RIGHT:
				ch = GADS_KEY_RIGHT;
				break;
			case KEY_BACKSPACE:
				ch = GADG_KEY_BACKSPACE;
				break;
			case KEY_DC:
				ch = GADS_KEY_DELETE;
				break;
			default:
				ch = GADS_KEY_NONE;
				break;
			}
		}

		if (ch == GADS_KEY_NONE)
		{
			return NULL;
		}

		if (gui_screen.active)
		{
			struct gadget *g;

			/* Try active gagdet first */
			if ((g = gui_screen.active->active))
			{
				if (g->input)
				{
					if (g->input(g, ch))
					{
						/* Redisplay the entire window */
						/* TODO: Obviously, this can be optimized */
						windows_display(gui_screen.active, &gui_screen);
						continue;
					}
				}
			}

			/* Now invoke possible window-related listeners */
			if (window_invoke_key_listener(gui_screen.active, ch))
			{
				continue;
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

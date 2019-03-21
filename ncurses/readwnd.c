/**
 * @file
 */

#include "readwnd.h"

#include "debug.h"
#include "gui_main_ncurses.h"
#include "mail.h"

#include <ncurses.h>
#include <panel.h>
#include <unistd.h>

/******************************************************************************/

static WINDOW *read_wnd;
static PANEL *read_panel;

static struct gui_key_listener close_listener;

static struct mail_complete *read_current_mail;

/******************************************************************************/

static void read_window_close_current(void)
{
	gui_remove_key_listener(&close_listener);

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
	char buf[256];

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


	if (read_current_mail)
	{
		mail_complete_free(read_current_mail);
	}

	getcwd(buf, sizeof(buf));
	chdir(folder);

	if ((read_current_mail = mail_complete_create_from_file(NULL, mail->filename)))
	{
		mail_read_contents(NULL,read_current_mail);
	} else
	{
		SM_DEBUGF(20, ("Unable to create mail \"%s\"\n", mail->filename));
	}

	chdir(buf);

	gui_add_key_listener(&close_listener, 'c', "Close", read_window_close_current);
	refresh();

	wrefresh(read_wnd);

	return 1;
}

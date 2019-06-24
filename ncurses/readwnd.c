/**
 * @file
 */

#include "readwnd.h"

#include "debug.h"
#include "gadgets.h"
#include "gui_main_ncurses.h"
#include "mail.h"
#include "smintl.h"
#include "support.h"
#include "timesupport.h"

#include <ncurses.h>
#include <panel.h>
#include <unistd.h>

/******************************************************************************/

static WINDOW *read_wnd;
static PANEL *read_panel;

static struct window read_win;

static struct key_listener close_listener;
static struct screen_resize_listener resize_listener;
static int resize_listener_added;

static struct mail_complete *read_current_mail;

static struct group read_group;
static struct simple_text_label from_label;
static struct simple_text_label date_label;
static struct simple_text_label subject_label;
static struct text_view text_view;

/******************************************************************************/

static void read_window_close_current(void)
{
	screen_remove_key_listener(&gui_screen, &close_listener);
	if (resize_listener_added)
	{
		screen_remove_resize_listener(&resize_listener);
		resize_listener_added = 0;
	}

	screen_remove_window(&gui_screen, &read_win);

	hide_panel(read_panel);
	update_panels();
	doupdate();
}

/******************************************************************************/

static void read_window_layout_size(int w, int h)
{
	h -= 2;

	gadgets_set_extend(&from_label.tl.g, 0, 0, w, 1);
	gadgets_set_extend(&date_label.tl.g, 0, 1, w, 1);
	gadgets_set_extend(&subject_label.tl.g, 0, 2, w, 1);
	gadgets_set_extend(&text_view.tl.tl.g, 0, 3, w, h - 3);

	windows_display(&read_win, &gui_screen);
}

/******************************************************************************/

static void read_window_layout()
{
	int w, h;
	getmaxyx(stdscr, h, w);
	read_window_layout_size(w, h);
}

/******************************************************************************/

static void read_window_resize(void *udata, int x, int y, int w, int h)
{
	read_window_layout_size(w, h);
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

	if (!resize_listener_added)
	{
		screen_add_resize_listener(&gui_screen, &resize_listener, read_window_resize, NULL);
		resize_listener_added = 1;
	}

	if (read_current_mail)
	{
		mail_complete_free(read_current_mail);
	}

	getcwd(buf, sizeof(buf));
	chdir(folder);

	if ((read_current_mail = mail_complete_create_from_file(NULL, mail->filename)))
	{
		struct mail_complete *initial;
		mail_read_contents(NULL,read_current_mail);
		if ((initial = mail_find_initial(read_current_mail)))
		{
			char buf[380];
			utf8 *from_phrase = mail_info_get_from_phrase(read_current_mail->info);
			utf8 *from_addr = mail_info_get_from_addr(read_current_mail->info);
			if (from_phrase)
			{
				sm_snprintf(buf, sizeof(buf), "%s: %s <%s>", _("From"), from_phrase, from_addr);
			} else
			{
				sm_snprintf(buf, sizeof(buf), "%s: %s", _("From"), from_addr);
			}

			/* FIXME: Doesn't work with multiple windows yet */
			windows_init(&read_win);
			screen_add_window(&gui_screen, &read_win);

			gadgets_init_simple_text_label(&from_label, buf);

			sm_snprintf(buf, sizeof(buf), "%s: %s", _("Date"), sm_get_date_str(read_current_mail->info->seconds));
			gadgets_init_simple_text_label(&date_label, buf);

			sm_snprintf(buf, sizeof(buf), "%s: %s", _("Subject"), read_current_mail->info->subject);
			gadgets_init_simple_text_label(&subject_label, buf);

			mail_decode(initial);

			gadgets_init_group(&read_group);
			gadgets_init_text_view(&text_view, initial->decoded_data);

			gadgets_add(&read_group, &from_label.tl.g);
			gadgets_add(&read_group, &date_label.tl.g);
			gadgets_add(&read_group, &subject_label.tl.g);
			gadgets_add(&read_group, &text_view.tl.tl.g);

			gadgets_add(&read_win.g, &read_group.g);
			read_window_layout();

			windows_display(&read_win, &gui_screen);
		}
	} else
	{
		SM_DEBUGF(20, ("Unable to create mail \"%s\"\n", mail->filename));
	}

	chdir(buf);

	screen_add_key_listener(&gui_screen, &close_listener, 'c', "Close", read_window_close_current);
	refresh();

	wrefresh(read_wnd);

	return 1;
}

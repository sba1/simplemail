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

static struct gui_key_listener close_listener;
static struct gui_resize_listener resize_listener;
static int resize_listener_added;

static struct mail_complete *read_current_mail;

static struct simple_text_label from_label;
static struct simple_text_label date_label;
static struct simple_text_label subject_label;
static struct text_view text_view;

/******************************************************************************/

static void read_window_close_current(void)
{
	gui_remove_key_listener(&close_listener);

	hide_panel(read_panel);
	update_panels();
	doupdate();
}

/******************************************************************************/

static void read_window_resize(void *udata)
{
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
		gui_add_resize_listener(&resize_listener, read_window_resize, NULL);
		resize_listener_added = 1;
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
			gadgets_init_simple_text_label(&from_label, 0, 0, w, buf);

			sm_snprintf(buf, sizeof(buf), "%s: %s", _("Date"), sm_get_date_str(read_current_mail->info->seconds));
			gadgets_init_simple_text_label(&date_label, 0, 1, w, buf);

			sm_snprintf(buf, sizeof(buf), "%s: %s", _("Subject"), read_current_mail->info->subject);
			gadgets_init_simple_text_label(&subject_label, 0, 2, w, buf);

			mail_decode(initial);

			gadgets_init_text_view(&text_view, 0, 3, w, h - 3, initial->decoded_data);

			gadgets_display(read_wnd, &from_label.tl);
			gadgets_display(read_wnd, &date_label.tl);
			gadgets_display(read_wnd, &subject_label.tl);
			gadgets_display(read_wnd, &text_view.tl.tl);
		}
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

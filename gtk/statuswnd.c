/***************************************************************************
 SimpleMail - Copyright (C) 2000 Hynek Schlawack and Sebastian Bauer

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
***************************************************************************/

/*
** filterwnd.c
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"

#include "statuswnd.h"
#include "subthreads.h"

static GtkWidget *status_wnd;
static GtkWidget *status_head_label;
static GtkWidget *status_progress;
static GtkWidget *status_status_label;

static char *status_title;

/**************************************************************************
 Called when abort button presses
**************************************************************************/
static void statuswnd_abort(void)
{
	thread_abort(NULL);
}

/**************************************************************************
 Open the status window
**************************************************************************/
int statuswnd_open(int active)
{
	if (!status_wnd)
	{
		GtkWidget *vbox2;
		GtkWidget *hbox3;
		GtkWidget *status_abort_button;

		status_wnd = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (status_wnd), _("SimpleMail - Status"));
		gtk_window_set_position(GTK_WINDOW(status_wnd),GTK_WIN_POS_CENTER);

		vbox2 = gtk_vbox_new (FALSE, 0);
		gtk_container_add (GTK_CONTAINER (status_wnd), vbox2);

		status_head_label = gtk_label_new (_("Head"));
		gtk_box_pack_start (GTK_BOX (vbox2), status_head_label, FALSE, FALSE, 0);
		gtk_label_set_justify (GTK_LABEL (status_head_label), GTK_JUSTIFY_LEFT);

		status_progress = gtk_progress_bar_new ();
		gtk_box_pack_start (GTK_BOX (vbox2), status_progress, FALSE, FALSE, 0);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(status_progress),"");

		hbox3 = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);

		status_status_label = gtk_label_new (_("Status"));
		gtk_box_pack_start (GTK_BOX (hbox3), status_status_label, TRUE, TRUE, 0);
		gtk_label_set_justify (GTK_LABEL (status_status_label), GTK_JUSTIFY_LEFT);

		status_abort_button = gtk_button_new_from_stock ("gtk-stop");
		gtk_box_pack_start (GTK_BOX (hbox3), status_abort_button, FALSE, FALSE, 0);
	}

	if (status_wnd)
	{
		gtk_widget_show_all(status_wnd);
	}

	return 0;
}

/**************************************************************************
 Open the status window
**************************************************************************/
void statuswnd_close(void)
{
	if (status_wnd)
	{
		gtk_widget_hide_all(status_wnd);
	}
}

/**************************************************************************
 Open the status window
**************************************************************************/
void statuswnd_set_title(char *title)
{
#if 0
	free(status_title);
	status_title = mystrdup(title);

	if (!status_wnd) return;
	set(status_wnd, MUIA_Window_Title,status_title);
#endif
}

static int gauge_maximal;

/**************************************************************************
 Initialize the gauge bar with the given maximum value
**************************************************************************/
void statuswnd_init_gauge(int maximal)
{
	gauge_maximal = maximal;
}

/**************************************************************************
 Set the current gauge value
**************************************************************************/
void statuswnd_set_gauge(int value)
{
	if (!status_wnd) return;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(status_progress),(double)value / gauge_maximal);
}

/**************************************************************************
 Set the current gauge text
**************************************************************************/
void statuswnd_set_gauge_text(char *text)
{
	if (!status_wnd) return;
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(status_progress),text);
}

/**************************************************************************
 Set the status line
**************************************************************************/
void statuswnd_set_status(char *text)
{
	if (!status_wnd) return;
	gtk_label_set_text(GTK_LABEL(status_status_label),text);
}

/**************************************************************************
 Set the head of the status window
**************************************************************************/
void statuswnd_set_head(char *text)
{
	gtk_label_set_text(GTK_LABEL(status_head_label),text);
}

/**************************************************************************
 Insert mail
**************************************************************************/
void statuswnd_mail_list_insert(int mno, int mflags, int msize)
{
#if 0
	DoMethod(status_wnd, MUIM_transwnd_InsertMailSize, mno, mflags, msize);
#endif
}

/**************************************************************************
 Set flags
**************************************************************************/
void statuswnd_mail_list_set_flags(int mno, int mflags)
{
}

/**************************************************************************
 Insert mail info
**************************************************************************/
void statuswnd_mail_list_set_info(int mno, char *from, char *subject, char *date)
{
#if 0
	DoMethod(status_wnd, MUIM_transwnd_InsertMailInfo, mno, from, subject, date);
#endif
}

/**************************************************************************
 returns -1 if the mail is not in the mail selection
**************************************************************************/
int statuswnd_mail_list_get_flags(int mno)
{
#if 0
	return (int)DoMethod(status_wnd, MUIM_transwnd_GetMailFlags, mno);
#endif
}

/**************************************************************************

**************************************************************************/
void statuswnd_mail_list_clear(void)
{
#if 0
	DoMethod(status_wnd, MUIM_transwnd_Clear);
#endif
}

/**************************************************************************

**************************************************************************/
void statuswnd_mail_list_freeze(void)
{
//	set(status_wnd,MUIA_transwnd_QuietList,TRUE);
}

/**************************************************************************

**************************************************************************/
void statuswnd_mail_list_thaw(void)
{
//	set(status_wnd,MUIA_transwnd_QuietList,FALSE);
}

/**************************************************************************
 Wait for user interaction. Return -1 for aborting
**************************************************************************/
int statuswnd_wait(void)
{
//	return (int)DoMethod(status_wnd, MUIM_transwnd_Wait);
	return 0;
}

/***************************************************************************
 Returns 0 if user has aborted the statistic listing
**************************************************************************/
int statuswnd_more_statistics(void)
{
//  LONG start_pressed = xget(status_wnd,MUIA_transwnd_StartPressed);
 // return !start_pressed;
	return 1;
}


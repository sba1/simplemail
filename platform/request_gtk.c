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

/**
 * @file
 */

#include "request.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_GTK_GTK_H
#include <gtk/gtk.h>
#endif

/*****************************************************************************/

char *sm_request_file(char *title, char *path, int save, char *extension)
{
	return NULL;
}

/*****************************************************************************/

int sm_request(const char *title, const char *text, const char *gadgets, ...)
{
#ifdef HAVE_GTK_GTK_H
	int gadno = 1;
	char *gadget_end = gadgets;
	char buf[200];
	char c;
	int i;
	int rc;

	GtkDialog *dialog1;
	GtkWidget *dialog_vbox1;
	GtkWidget *dialog_action_area1;
	GtkWidget *label;

	dialog1 = GTK_DIALOG(gtk_dialog_new());
	gtk_window_set_title (GTK_WINDOW (dialog1), title);

	dialog_vbox1 = gtk_dialog_get_content_area(dialog1);

	label = gtk_label_new(text);
	gtk_box_pack_start (GTK_BOX (dialog_vbox1), label, TRUE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	dialog_action_area1 = gtk_dialog_get_action_area(dialog1);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);

	i = 0;
	while (1)
	{
		c = *gadget_end++;

		if (!c || c == '|')
		{
			buf[i] = 0;

			if (c)
			{
				gtk_dialog_add_button(GTK_DIALOG(dialog1),buf,gadno++);
				i=0;
				continue;
			} else
			{
				gtk_dialog_add_button(GTK_DIALOG(dialog1),buf,0);
				break;
			}
		}

		buf[i++] = c;
	}

	gtk_widget_show_all(GTK_WIDGET(dialog1));
	rc = gtk_dialog_run(dialog1);
	gtk_widget_destroy(GTK_WIDGET(dialog1));

	return rc;
#else
	va_list ap;
	char *text_buf;

	if (!(text_buf = malloc(2048)))
		return 0;

	va_start(ap, gadgets);
	vsnprintf(text_buf, 2048, text, ap);
	va_end(ap);

	printf("%s\n", text_buf);

	free(text_buf);
	return 0;
#endif
}

/******************************************************************
 Opens a requester to enter a string. Returns NULL on error.
 Otherwise the malloc()ed string
*******************************************************************/
char *sm_request_string(const char *title, const char *text, const char *contents, int secret)
{
	printf("sm_request_string()\n");
	return NULL;
}

/*****************************************************************************/

int sm_request_login(char *text, char *login, char *password, int len)
{
	return 0;
}

/*****************************************************************************/

char *sm_request_pgp_id(char *text)
{
	return NULL;
}

/*****************************************************************************/

struct folder *sm_request_folder(char *text, struct folder *exclude)
{
	return NULL;
}


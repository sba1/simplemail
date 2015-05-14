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
** support.c
*/

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <glib.h>
#ifdef HAVE_GTK_GTK_H
#include <gtk/gtk.h>
#endif
#include <openssl/ssl.h>

#include "errorwnd.h"
#include "subthreads.h"
#include "support.h"

#include "smintl.h"
#include "support_indep.h"

/******************************************************************
 Creates a directory including all necessaries parent directories.
 Nothing will happen if the directory already exists
*******************************************************************/
int sm_makedir(const char *path)
{
	int rc;

#define MKDIR_FLAGS 0xffff //O_READ|O_WRITE

	if (mkdir(path,MKDIR_FLAGS) == 0) return 1;
	if (errno == EEXIST) return 1;

	rc = 0;

	if (errno == ENOENT)
	{
		char *buf = (char*)malloc(strlen(path)+1);
		if (buf)
		{
			char *buf2;
			char *last_buf = NULL;
			int times = 0; /* how many paths has been splitted? */

			rc = 1;

			strcpy(buf,path);

			/* split every path */
			while ((buf2 = sm_path_part(buf)))
			{
				/* path cannot be be splitted more */
				if (buf2 == last_buf) break;
				times++;

				/* clear the '/' sign */
				*buf2 = 0;

				/* try if directory can be created */
				if (mkdir(buf,MKDIR_FLAGS) == 0) break; /* yes, so leave the loop */
				if (errno != ENOENT) break;
				last_buf = buf2;
			}

			/* put back the '/' sign and create the directories */
			while(times--)
			{
				buf[strlen(buf)] = '/';
				if (mkdir(buf,MKDIR_FLAGS) != 0) break;
			}

			free(buf);
		}
	}
	return rc;
}

/******************************************************************
 Add a filename component to the drawer
*******************************************************************/
int sm_add_part(char *drawer, const char *filename, int buf_size)
{
	int drawer_len = strlen(drawer);

	if (drawer_len)
	{
		if (drawer[drawer_len-1] != '/')
		{
			if (drawer_len + 1 >= buf_size) return 0;
			drawer[drawer_len] = '/';
			drawer_len++;
		}
	}
	mystrlcpy(&drawer[drawer_len],filename,buf_size - drawer_len);
	return 1;
}

/******************************************************************
 Return the file component of a path
*******************************************************************/
char *sm_file_part(char *filename)
{
	int filename_len = strlen(filename);
	if (!filename_len) return filename;
	if (filename[filename_len-1] == '/') filename_len--;
	while (filename_len)
	{
		if (filename[filename_len-1] == '/') break;
		filename_len--;
	}
	return &filename[filename_len];
}

/******************************************************************
 Return the character after the last path component
*******************************************************************/
char *sm_path_part(char *filename)
{
	char *file_part = sm_file_part(filename);
	if (file_part == filename) return filename;
	return file_part-1;
}

/******************************************************************
 Returns the full path of a selected file.
******************************************************************/
char *sm_request_file(char *title, char *path, int save, char *extension)
{
	return NULL;
}

/******************************************************************
 Opens a requester. Returns 0 if the rightmost gadgets is pressed
 otherwise the position of the gadget from left to right
*******************************************************************/
int sm_request(char *title, char *text, char *gadgets, ...)
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
char *sm_request_string(char *title, char *text, char *contents, int secret)
{
	printf("sm_request_string()\n");
	return NULL;
}

/******************************************************************
 Opens a requester to enter a user id and a passwort. Returns 1 on
 success, else 0. The strings are filled in a previously alloced
 login and password buffer but not more than len bytes
*******************************************************************/
int sm_request_login(char *text, char *login, char *password, int len)
{
	return 0;
}

/******************************************************************
 Returns a malloc()ed string of a selected pgp. NULL for cancel
 or an error
*******************************************************************/
char *sm_request_pgp_id(char *text)
{
	return NULL;
}

struct folder *sm_request_folder(char *text, struct folder *exclude)
{
	return NULL;
}

/******************************************************************
 Get environment variables
*******************************************************************/
char *sm_getenv(char *name)
{
	return getenv(name);
}

/******************************************************************
 Set environment variables
*******************************************************************/
void sm_setenv(char *name, char *value)
{
}

/******************************************************************
 Unset environment variables
*******************************************************************/
void sm_unsetenv(char *name)
{
}

/******************************************************************
 An system() replacement
*******************************************************************/
int sm_system(char *command, char *output)
{
  return 20;
}

/******************************************************************
 Checks weather a file is in the given drawer. Returns 1 for
 success.
*******************************************************************/
int sm_file_is_in_drawer(char *filename, char *path)
{
	return 0;
}

/******************************************************************
 Like sprintf() but buffer overrun safe
*******************************************************************/
int sm_snprintf(char *buf, int n, const char *fmt, ...)
{
  int r;

  extern int vsnprintf(char *buffer, size_t buffersize, const char *fmt0, va_list ap);

  va_list ap;
  
  va_start(ap, fmt);
  r = vsnprintf(buf, n, fmt, ap);
  va_end(ap);
  return r;
}

/******************************************************************
 Tells an error message
*******************************************************************/
void tell_str(const char *str)
{
	printf("%s\n",str);
	error_add_message((char*)str);
}

/******************************************************************
 Tells an error message from a subtask
*******************************************************************/
void tell_from_subtask(const char *str)
{
	int success;

	thread_call_parent_function_sync(&success,tell_str,1,str);
}


static PKCS7 *pkcs7_get_data(PKCS7 *pkcs7)
{
	if (PKCS7_type_is_signed(pkcs7))
	{
		return pkcs7_get_data(pkcs7->d.sign->contents);
	}
	if (PKCS7_type_is_data(pkcs7))
	{
		return pkcs7;
	}
	return NULL;
}

/******************************************************************
 Decodes an pkcs7...API is unfinished! This is a temp solution.
*******************************************************************/
int pkcs7_decode(char *buf, int len, char **dest_ptr, int *len_ptr)
{
	int rc = 0;

	const unsigned char *p = buf;
	PKCS7 *pkcs7;

	if ((pkcs7 = d2i_PKCS7(NULL, &p, len)))
	{
		PKCS7 *pkcs7_data = pkcs7_get_data(pkcs7);
		if (pkcs7_data)
		{
			char *mem = malloc(pkcs7_data->d.data->length+1);
			if (*dest_ptr)
			{
				memcpy(mem,pkcs7_data->d.data->data,pkcs7_data->d.data->length);
				mem[pkcs7_data->d.data->length]=0;
				*dest_ptr = mem;
				*len_ptr = pkcs7_data->d.data->length;
				rc = 1;
			}
		}
		PKCS7_free(pkcs7);
	}
	return rc;
}

/***************************************************************************************/

void sm_put_on_serial_line(char *txt)
{
	fprintf(stderr,"%s", txt);
}

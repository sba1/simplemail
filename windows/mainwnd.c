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
** mainwnd.c
*/

#include <string.h>
#include <stdio.h>

#include <windows.h>

#include "SimpleMail_rev.h"

#include "folder.h"
#include "lists.h"
#include "mail.h"
#include "simplemail.h"
#include "support.h"

/******************************************************************
 Initialize the main window
*******************************************************************/
int main_window_init(void)
{
	return 0;
}

/******************************************************************
 Open the main window
*******************************************************************/
int main_window_open(void)
{
	return 0;
}

/******************************************************************
 Refreshs the folder text line
*******************************************************************/
static void main_refresh_folders_text(void)
{
}

/******************************************************************
 Refreshs the folder list
*******************************************************************/
void main_refresh_folders(void)
{
	struct folder *f = folder_first();

	while (f)
        {
		f = folder_next(f);
        }
}

/******************************************************************
 Refreshs a single folder entry
*******************************************************************/
void main_refresh_folder(struct folder *folder)
{
}

/******************************************************************
 Inserts a new mail into the listview
*******************************************************************/
void main_insert_mail(struct mail *mail)
{
}

/******************************************************************
 Inserts a new mail into the listview after a given position
*******************************************************************/
void main_insert_mail_pos(struct mail *mail, int after)
{
	printf("main_insert_mail_pos() not implemented");
}



/******************************************************************
 Remove a given mail from the listview
*******************************************************************/
void main_remove_mail(struct mail *mail)
{
}

/******************************************************************
 Replaces a mail with a new mail
*******************************************************************/
void main_replace_mail(struct mail *oldmail, struct mail *newmail)
{
}

/******************************************************************
 Refresh a mail (if it status has changed fx)
*******************************************************************/
void main_refresh_mail(struct mail *m)
{
}

/******************************************************************
 Updates the mail trees with the mails in the given folder
*******************************************************************/
static void main_insert_mail_threaded(struct folder *folder, struct mail *mail, void *parentnode)
{
}

/******************************************************************
 Clears the folder list
*******************************************************************/
void main_clear_folder_mails(void)
{
}

/******************************************************************
 Updates the mail trees with the mails in the given folder
*******************************************************************/
void main_set_folder_mails(struct folder *folder)
{ 
	void *handle = NULL;
	struct mail *m;
	int primary_sort = folder_get_primary_sort(folder)&FOLDER_SORT_MODEMASK;
	int threaded = folder->type == FOLDER_TYPE_MAILINGLIST;

	while ((m = folder_next_mail(folder,&handle)))
	{
	}
}

/******************************************************************
 Returns the current selected folder, NULL if no real folder
 has been selected. It returns the true folder like it is
 in the folder list
*******************************************************************/
struct folder *main_get_folder(void)
{
	return NULL;
}

/******************************************************************
 Returns the current selected folder drawer, NULL if no folder
 has been selected
*******************************************************************/
char *main_get_folder_drawer(void)
{
	return NULL;
}

/******************************************************************
 Returns the active mail. NULL if no one is active
*******************************************************************/
struct mail *main_get_active_mail(void)
{
	return NULL;
}

/******************************************************************
 Returns the filename of the active mail, NULL if no thing is
 selected
*******************************************************************/
char *main_get_mail_filename(void)
{
	return NULL;
}

/******************************************************************
 Returns the first selected mail. NULL if no mail is selected
*******************************************************************/
struct mail *main_get_mail_first_selected(void *handle)
{
	return NULL;
}

/******************************************************************
 Returns the next selected mail. NULL if no more mails are
 selected
*******************************************************************/
struct mail *main_get_mail_next_selected(void *handle)
{
	return NULL;
}

/******************************************************************
 Remove all selected mails,
*******************************************************************/
void main_remove_mails_selected(void)
{
}

/******************************************************************
 Build the check singe account menu
*******************************************************************/
void main_build_accounts(void)
{
}

/******************************************************************
 Freeze the mail list
*******************************************************************/
void main_freeze_mail_list(void)
{
}

/******************************************************************
 Thaws the mail list
*******************************************************************/
void main_thaw_mail_list(void)
{
}


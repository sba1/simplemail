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
** simplemail.c
*/

#include <stdlib.h>
#include <stdio.h>

#include "addressbookwnd.h"
#include "composewnd.h"
#include "gui_main.h"
#include "mainwnd.h"
#include "readwnd.h"

#include "addressbook.h"
#include "configuration.h"
#include "dl.h"
#include "folder.h"
#include "simplemail.h"

/* the current mail should be viewed */
void callback_read_mail(void)
{
	char *filename;

	if ((filename = main_get_mail_filename()))
	{
		read_window_open(main_get_folder_drawer(), filename);
	}
}

/* the selected mails should be deleted */
void callback_delete_mails(void)
{
	struct folder *from_folder = main_get_folder();
	if (from_folder != folder_deleted())
	{
		void *handle;
		struct mail *mail = main_get_mail_first_selected(&handle);
		while (mail)
		{
			folder_delete_mail(from_folder,mail);
			mail = main_get_mail_next_selected(&handle);
		}
		main_refresh_folder(from_folder);
		main_refresh_folder(folder_deleted());
		main_remove_mails_selected();
	}
}

/* a new mail should be composed */
void callback_new_mail(void)
{
	compose_window_open(NULL,NULL);
}

/* a mail should be replied */
void callback_reply_mail(void)
{
	char *filename;

	if ((filename = main_get_mail_filename()))
	{
		struct mail *mail;
		char buf[256];

		getcwd(buf, sizeof(buf));
		chdir(main_get_folder_drawer());

		if ((mail = mail_create_from_file(filename)))
		{
			struct mail *reply;
			mail_read_contents("",mail);
			if ((reply = mail_create_reply(mail)))
			{
				compose_window_open(NULL,reply);
				mail_free(reply);
			}
			mail_free(mail);
		}

		chdir(buf);
	}
}

/* the currently selected mail should be changed */
void callback_change_mail(void)
{
	char *filename;

	if ((filename = main_get_mail_filename()))
	{
		struct mail *mail;
		char buf[256];

		getcwd(buf, sizeof(buf));
		chdir(main_get_folder_drawer());

		if ((mail = mail_create_from_file(filename)))
		{
			mail_read_contents("",mail);
			compose_window_open(NULL,mail);
			mail_free(mail);
		}

		chdir(buf);
	}
}

/* mails should be fetched */
void callback_fetch_mails(void)
{
	mails_dl();
}

/* mails should be sent */
void callback_send_mails(void)
{
	mails_upload();
}

/* addressbook should be opened */
void callback_addressbook(void)
{
	addressbook_open();
}

/* a new folder has been activated */
void callback_folder_active(void)
{
	struct folder *folder = main_get_folder();
	if (folder)
	{
		main_set_folder_mails(folder);
	}
}

/* a new mail has arrived */
void callback_new_mail_arrived(struct mail *mail)
{
	folder_add_mail_incoming(mail);
	if (main_get_folder() == folder_incoming())
	{
		main_insert_mail(mail);
	}

	main_refresh_folder(folder_incoming());
}

/* a new mail has been written */
void callback_new_mail_written(struct mail *mail)
{
	folder_add_mail(folder_outgoing(),mail);
	if (main_get_folder() == folder_outgoing())
	{
		main_insert_mail(mail);
	}
	main_refresh_folder(folder_outgoing());
}

/* a mail has been changed/replaced by the user */
void callback_mail_changed(struct folder *folder, struct mail *oldmail, struct mail *newmail)
{
	if (main_get_folder() == folder)
	{
		main_replace_mail(oldmail, newmail);
	}
}

/* a single mail should be moved from a folder to another folder */
void callback_move_mail(struct mail *mail, struct folder *from_folder, struct folder *dest_folder)
{
	if (from_folder != dest_folder)
	{
		folder_move_mail(from_folder,dest_folder,mail);

		/* If outgoing folder is visible remove the mail */
		if (main_get_folder() == from_folder)
			main_remove_mail(mail);

		/* If sent folder is visible insert the mail */
		if (main_get_folder() == dest_folder)
			main_insert_mail(mail);

		main_refresh_folder(from_folder);
		main_refresh_folder(dest_folder);
	}
}

/* mails has been droped onto the folder */
void callback_maildrop(struct folder *dest_folder)
{
	struct folder *from_folder = main_get_folder();
	if (from_folder != dest_folder)
	{
		void *handle;
		struct mail *mail = main_get_mail_first_selected(&handle);
		while (mail)
		{
			folder_move_mail(from_folder,dest_folder,mail);
			mail = main_get_mail_next_selected(&handle);
		}
		main_refresh_folder(from_folder);
		main_refresh_folder(dest_folder);
		main_remove_mails_selected();
	}
}

/* a new mail should be written to the given address */
void callback_write_mail_to(struct addressbook_entry *address)
{
	char *to = addressbook_get_address_str(address);
	compose_window_open(to,NULL);
	if (to) free(to);
}

int main(void)
{
	load_config();
	init_addressbook();
	if (init_folders())
	{
		gui_main();
	}
	cleanup_addressbook();
	return 0;
}
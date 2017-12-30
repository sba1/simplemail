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
 * @brief Controller of SimpleMail.
 *
 * @file simplemail.c
 */

#include "simplemail.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "account.h"
#include "addressbook.h"
#include "atcleanup.h"
#include "codesets.h"
#include "configuration.h"
#include "dbx.h"
#include "debug.h"
#include "filter.h"
#include "folder.h"
#include "folder_search_thread.h"
#include "imap_thread.h" /* imap_thread_xxx() */
#include "lists.h"
#include "logging.h"
#include "mail.h"
#include "mailinfo_extractor.h"
#include "mbox.h"
#include "progmon.h"
#include "smintl.h"
#include "spam.h"
#include "ssl.h"
#include "status.h"
#include "support_indep.h"
#include "trans.h"

#include "addressbookwnd.h"
#include "composewnd.h"
#include "configwnd.h"
#include "filterwnd.h"
#include "folderwnd.h"
#include "gui_main.h"
#include "mainwnd.h"
#include "progmonwnd.h"
#include "readwnd.h"
#include "request.h"
#include "searchwnd.h"
#include "shutdownwnd.h"
#include "startupwnd.h"
#include "subthreads.h"
#include "support.h"
#include "tcpip.h"
#include "timesupport.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

/** for the Auto-Timer functions **/
static unsigned int autocheck_seconds_start; /* to compare with this */

/*****************************************************************************/

int callback_read_active_mail(void)
{
	char *filename;
	struct mail_info *m;
	struct folder *f;

	if (!(f = main_get_folder())) return -1;
	if (!(filename = main_get_mail_filename())) return -1;
	if (!(m = main_get_active_mail())) return -1;

	return callback_read_mail(f,m,-1);
}

/*****************************************************************************/

void callback_save_active_mail(void)
{
	char *dest;
	char *mail_filename;
	struct folder *f;

	if (!(f = main_get_folder())) return;
	if (!(f->path)) return;
	if (!(mail_filename = main_get_mail_filename())) return;

	if ((dest = sm_request_file("SimpleMail", "", 1, NULL)))
	{
		int src_len = strlen(f->path) + strlen(mail_filename) + 10;
		char *src = malloc(src_len);
		if (src)
		{
			strcpy(src,f->path);
			sm_add_part(src,mail_filename,src_len);

			if (!myfilecopy(src, dest))
				sm_request(NULL,_("Unable to save the active mail.\n"),_("Ok"));

			free(src);
		}
		free(dest);
	}
}

/**
 * Touches the mail in the given folder such that it gets a READ state.
 *
 * @param f the folder where the mail is located.-
 * @param mail the mail that should be touched.
 * @return whether the mail status has actually been changed.
 */
static int touch_mail(struct folder *f, struct mail_info *mail)
{
	int refresh = 0;

	if (mail_info_get_status_type(mail) == MAIL_STATUS_UNREAD)
	{
		folder_set_mail_status(f, mail, MAIL_STATUS_READ | (mail->status & (~MAIL_STATUS_MASK)));
		refresh = 1;
	}

	if (mail->flags & MAIL_FLAGS_NEW)
	{
		if (f->new_mails) f->new_mails--;
		mail->flags &= ~MAIL_FLAGS_NEW;
		refresh = 1;
	}

	if (refresh)
	{
		main_refresh_mail(mail);
		main_refresh_folder(f);
	}
	return refresh;
}

/*****************************************************************************/

int callback_read_mail(struct folder *f, struct mail_info *mail, int window)
{
	int num;

	if (!f) f = folder_find_by_mail(mail);
	if (!f) return -1;

	if (mail->flags & MAIL_FLAGS_PARTIAL)
	{
		imap_download_mail(f,mail);
		main_refresh_mail(mail);
	}

	num = read_window_open(f->path, mail, window);
	if (num >= 0)
	{
		touch_mail(f,mail);
	}
	return num;
}

/*****************************************************************************/

int callback_delete_mails_silent(int permanent)
{
	struct folder *from_folder = main_get_folder();
	struct mail_info *mail;
	void *handle;

	if (from_folder)
	{
		int search_has;

		/* Check if folder is not used */
		if (!folder_attempt_lock(from_folder))
			return 0;

		if (from_folder->is_imap)
		{
			mail = main_get_mail_first_selected(&handle);
			while (mail)
			{
				if (mail_is_marked_as_deleted(mail)) folder_mark_mail_as_undeleted(from_folder,mail);
				else folder_mark_mail_as_deleted(from_folder,mail);
				mail = main_get_mail_next_selected(&handle);
			}
			main_refresh_mails_selected();
		} else
		{
			search_has = search_has_mails();

			mail = main_get_mail_first_selected(&handle);
			while (mail)
			{
				if (search_has) search_remove_mail(mail);
				if (permanent) folder_delete_mail(from_folder,mail);
				else folder_move_mail(from_folder,folder_deleted(),mail);
				mail = main_get_mail_next_selected(&handle);
			}
			main_refresh_folder(from_folder);
			if (!permanent) main_refresh_folder(folder_deleted());

			read_refresh_prevnext_button(from_folder);
			if (!permanent) read_refresh_prevnext_button(folder_deleted());

			main_remove_mails_selected();
		}

		folder_unlock(from_folder);

		return 1;
	}

	return 0;
}

/*****************************************************************************/

void callback_delete_mails(void)
{
	struct folder *from_folder = main_get_folder();
	struct mail_info *mail;
	void *handle;
	int num;
	int permanent; /* 1 if mails should be deleted permanently */

	if (!from_folder)
		return;

	/* Check if folder is not used */
	if (!folder_attempt_lock(from_folder))
	{
		sm_request(NULL,_("Cannot delete mails, because folder is in use.\n"),_("Ok"));
		return;
	}

	/* Count the number of selected mails first */
	mail = main_get_mail_first_selected(&handle);
	num = 0;
	while (mail)
	{
		num++;
		mail = main_get_mail_next_selected(&handle);
	}

	if (!num) return;
	if (from_folder == folder_deleted())
	{
		char buf[256];
		if (num == 1) mystrlcpy(buf,_("Do you really want to delete the selected mail permanently?"),sizeof(buf));
		else sm_snprintf(buf,sizeof(buf),_("Do you really want to delete %d mails permanently?"),num);
		if (!sm_request(NULL,buf,_("_Yes|_No"))) return;
		permanent = 1;
	} else permanent = 0;

	callback_delete_mails_silent(permanent);

	folder_unlock(from_folder);
}

/*****************************************************************************/

int callback_delete_mail(struct mail_info *mail)
{
	struct folder *f = folder_find_by_mail(mail);
	struct folder *fd = folder_deleted();
	if (f)
	{
		if (!folder_attempt_lock(f))
		{
			sm_request(NULL,_("Cannot delete mails, because folder is in use.\n"),_("Ok"));
			return 0;
		}

		if (f != fd)
		{
			folder_move_mail(f,fd,mail);
			main_refresh_folder(f);
		} else folder_delete_mail(f,mail);
		main_refresh_folder(fd);
		read_refresh_prevnext_button(f);
		read_refresh_prevnext_button(fd);
		if (main_get_folder() == f) main_remove_mail(mail);
		if (search_has_mails()) search_remove_mail(mail);
		folder_unlock(f);
		return 1;
	}
	return 0;
}

/*****************************************************************************/

void callback_delete_mail_by_uid(char *user, char *server, char *path, unsigned int uid)
{
	struct folder *f;
	struct mail_info *mail;

	folders_lock();

	f = folder_find_by_imap(user,server,path);
	if (!f)
	{
		folders_unlock();
		return;
	}

	folder_lock(f);

	mail = folder_imap_find_mail_by_uid(f, uid);
	if (!mail)
	{
		SM_DEBUGF(20, ("Couldn't find mail with uid %u\n", uid));
		folder_unlock(f);
		folders_unlock();
		return;
	}

	SM_DEBUGF(20, ("Deleting local mail for %s@server://%s with uid %u\n", user, server, path, uid));

	folder_move_mail(f,folder_deleted(),mail);
  main_refresh_folder(f);
  main_refresh_folder(folder_deleted());

	read_refresh_prevnext_button(f);
	read_refresh_prevnext_button(folder_deleted());

	if (main_get_folder() == f) main_remove_mail(mail);
	if (search_has_mails()) search_remove_mail(mail);

	folder_unlock(f);
	folders_unlock();
}

/*****************************************************************************/

void callback_get_address(void)
{
	struct mail_info *mail = main_get_active_mail();
	if (mail)
	{
		char *addr;
		utf8 *phrase;
		struct folder *f;

		f = main_get_folder();

		if (folder_get_type(f) == FOLDER_TYPE_SEND)
		{
			phrase = mail_get_to_phrase(mail);
			addr = mail_get_to_addr(mail);
		} else
		{
			phrase = mail->from_phrase;
			addr = mail->from_addr;
		}

		if (addr && !addressbook_find_entry_by_address(addr))
		{
			struct addressbook_entry_new entry;
			memset(&entry,0,sizeof(entry));

			entry.realname = (char*)phrase;
			entry.email_array = array_add_string(NULL,addr);

			addressbookwnd_create_entry(&entry);

			array_free(entry.email_array);
		}
	}
}

/**
 * Callback for opening a compose window.
 *
 * @param from
 * @param to
 * @param replyto
 * @param subject
 * @param body
 * @return the newly openend compose window number.
 */
static int callback_write_mail(char *from, char *to, char *replyto, char *subject, char *body)
{
	struct compose_args ca;
	int win_num;
	memset(&ca,0,sizeof(ca));

	ca.action = COMPOSE_ACTION_NEW;
	ca.to_change = mail_create_for(from,to,replyto,subject,body);

	win_num = compose_window_open(&ca);

	if (ca.to_change) mail_complete_free(ca.to_change);
	return win_num;
}

/*****************************************************************************/

void callback_write_mail_to(struct addressbook_entry_new *address)
{
	char *addr = address->email_array?address->email_array[0]:NULL;
	callback_write_mail_to_str(addr,NULL);
}

/*****************************************************************************/

int callback_write_mail_to_str(char *str, char *subject)
{
	return callback_write_mail(NULL,str,NULL,subject,NULL);
}

/*****************************************************************************/

int callback_write_mail_to_str_with_body(char *str, char *subject, char *body)
{
	return callback_write_mail(NULL,str,NULL,subject,body);
}

/*****************************************************************************/

int callback_open_message(char *mail_filename, int window)
{
	char buf[380];
	static char stored_dir[512];
	char *path;
	int num = -1;

	if (!mail_filename)
	{
		path = sm_request_file("SimpleMail", stored_dir, 0, "");
	} else
	{
		path = mystrdup(mail_filename);
	}

	if (path && *path)
	{
		struct mail_info *mail;
		char *filename;
		char *dir;

		filename = mystrdup(sm_file_part(path));
		dir = sm_path_part(path);
		if (dir)
		{
			*dir = 0;
			dir = path;
		}
		else dir = "";

		if (!mail_filename)
			mystrlcpy(stored_dir,dir,sizeof(stored_dir));

		if (getcwd(buf, sizeof(buf)) == NULL) return -1;
		chdir(dir);

		if ((mail = mail_info_create_from_file(NULL, filename)))
		{
			chdir(buf);

			num = read_window_open(dir, mail, window);
			mail_info_free(mail);
		} else
		{
			chdir(buf);
		}
		free(filename);
	}
	free(path);
	return num;
}

/*****************************************************************************/

void callback_new_mail(void)
{
	struct folder *f = main_get_folder();
	if (!f) return;
	callback_write_mail(f->def_from,f->def_to,f->def_replyto,NULL,NULL);
}

/*****************************************************************************/

void callback_reply_mails(char *folder_path, int num, struct mail_info **to_reply_array)
{
	struct folder *f = folder_find_by_path(folder_path);
	struct mail_complete **mail_array;
	char buf[380];
	int i;

	if (!f) return;

	/* Download the mails if needed */
	for (i=0;i<num;i++)
	{
		if (to_reply_array[i]->flags & MAIL_FLAGS_PARTIAL)
		{
			imap_download_mail(f,to_reply_array[i]);
			main_refresh_mail(to_reply_array[i]);
		}
	}

	if (getcwd(buf, sizeof(buf)) == NULL) return;

	if ((mail_array = malloc(num*sizeof(struct mail_complete *))))
	{
		struct mail_complete *reply;
		int err = 0;

		chdir(folder_path);

		for (i=0;i<num;i++)
		{
			if (!(mail_array[i] = mail_complete_create_from_file(NULL, to_reply_array[i]->filename)))
			{
				err = 1;
				break;
			}
		}

		if (!err)
		{
			for (i=0;i<num;i++)
			{
				/* we are already changed into the directory */
				mail_read_contents(NULL,mail_array[i]);
			}
		}

		chdir(buf);

		if (!err)
		{
			if ((reply = mail_create_reply(num,mail_array)))
			{
				struct compose_args ca;
				memset(&ca,0,sizeof(ca));
				ca.to_change = reply;
				ca.action = COMPOSE_ACTION_REPLY;
				ca.ref_mail = to_reply_array[0];
				compose_window_open(&ca);

				mail_complete_free(reply);
			}
		}

		for (i=0;i<num;i++)
		{
			if (mail_array[i]) mail_complete_free(mail_array[i]);
			else break;
		}
		free(mail_array);
	}
}

/*****************************************************************************/

void callback_reply_selected_mails(void)
{
	struct mail_info *mail;
	struct mail_info **mail_array;
	void *handle;
	int num;

	/* Count the number of selected mails first */
	mail = main_get_mail_first_selected(&handle);
	num = 0;
	while (mail)
	{
		num++;
		mail = main_get_mail_next_selected(&handle);
	}

	if (!num) return;

	if ((mail_array = malloc(sizeof(struct mail_info *)*num)))
	{
		int i = 0;

		mail = main_get_mail_first_selected(&handle);
		while (mail)
		{
			mail_array[i++] = mail;
			mail = main_get_mail_next_selected(&handle);
		}

		callback_reply_mails(main_get_folder_drawer(), num, mail_array);
		free(mail_array);
	}
}

/*****************************************************************************/

void callback_forward_mails(char *folder_path, int num, struct mail_info **to_forward_array)
{
	struct folder *f = folder_find_by_path(folder_path);
	char **filename_array;
	char buf[380];
	int i;

	if (!f) return;

	/* Download the mails if needed */
	for (i=0;i<num;i++)
	{
		if (to_forward_array[i]->flags & MAIL_FLAGS_PARTIAL)
		{
			imap_download_mail(f,to_forward_array[i]);
			main_refresh_mail(to_forward_array[i]);
		}
	}

	getcwd(buf, sizeof(buf));
	if (getcwd(buf, sizeof(buf)) == NULL) return;

	chdir(folder_path);

	if ((filename_array = malloc(num*sizeof(char *))))
	{
		struct mail_complete *forward;

		for (i=0;i<num;i++)
		{
			filename_array[i] = to_forward_array[i]->filename;
		}

		if ((forward = mail_create_forward(num,filename_array)))
		{
			struct compose_args ca;
			memset(&ca,0,sizeof(ca));
			ca.to_change = forward;
			ca.action = COMPOSE_ACTION_FORWARD;
			ca.ref_mail = to_forward_array[0];
			compose_window_open(&ca);

			mail_complete_free(forward);
		}
	}

	chdir(buf);
}

/* a single or multiple mails should be forwarded */
void callback_forward_selected_mails(void)
{
	struct mail_info *mail;
	struct mail_info **mail_array;
	void *handle;
	int num;

	/* Count the number of selected mails first */
	mail = main_get_mail_first_selected(&handle);
	num = 0;
	while (mail)
	{
		num++;
		mail = main_get_mail_next_selected(&handle);
	}

	if (!num) return;

	if ((mail_array = malloc(sizeof(struct mail_info *)*num)))
	{
		int i = 0;

		mail = main_get_mail_first_selected(&handle);
		while (mail)
		{
			mail_array[i++] = mail;
			mail = main_get_mail_next_selected(&handle);
		}

		callback_forward_mails(main_get_folder_drawer(), num, mail_array);
		free(mail_array);
	}
}

/* This is a helper function used by several other move functions. Its purpose is to move
   a mail from a folder to another with support for IMAP */
static int move_mail_helper(struct mail_info *mail, struct folder *from_folder, struct folder *dest_folder)
{
	int same_server;
	int success = 0;

	/* At first check if the dest folder is the spam folder */
	if (dest_folder == folder_spam())
	{
		/* Into a spam folder only mails marked as spam can be moved unless the option
		 * to change the mails to spam mails if moved to the spam folder is selected */
		if (!mail_is_spam(mail) && !(mail->flags & MAIL_FLAGS_AUTOSPAM) && !user.config.spam_mark_moved)
			return 0;

		if (!mail_is_spam(mail))
		{
			/* If we are here then because the mail has an autospam flag or user has configured that
			 * all mails which goes to the spam folder get the status changes.
			 * In both cases we change the mail status to spam and add it into the spam statistics */
			if (mail->flags & MAIL_FLAGS_PARTIAL)
			{
				if (!(imap_download_mail(from_folder,mail)))
					return 0;
			}

			if (!spam_feed_mail_as_spam(from_folder,mail))
				return 0;

			folder_set_mail_status(from_folder,mail,MAIL_STATUS_SPAM);
		}

		/* Reset the autoflag since the mail now is really marked as spam */
		folder_set_mail_flags(from_folder,mail,mail->flags & (~MAIL_FLAGS_AUTOSPAM));

		/* remove the new flag */
		if (mail->flags & MAIL_FLAGS_NEW && from_folder->new_mails) from_folder->new_mails--;
		mail->flags &= ~MAIL_FLAGS_NEW;
	}

	same_server = folder_on_same_imap_server(from_folder,dest_folder); /* is 0 if local only */

	if (!same_server)
	{
		if (mail->flags & MAIL_FLAGS_PARTIAL)
		{
			imap_download_mail(from_folder,mail);
			main_refresh_mail(mail);
		}

		if (dest_folder->is_imap)
		{
			if (imap_append_mail(mail, from_folder->path, dest_folder))
			{
				if (from_folder->is_imap)
					imap_delete_mail_by_filename(mail->filename,from_folder);

				folder_delete_mail(from_folder,mail);
				success = 1;
			}
		} else
		if (from_folder->is_imap)
		{
			char *filename = mystrdup(mail->filename);
			if (filename)
			{
				/* folder_move_mail() might change the filename */
				if (folder_move_mail(from_folder,dest_folder,mail))
				{
					imap_delete_mail_by_filename(filename,from_folder);
					success = 1;
				}
				free(filename);
			}
		} else
		{
			success = folder_move_mail(from_folder,dest_folder,mail);
		}
	} else
	{
		/* EMail should be moved on the same imap server */
		if (imap_move_mail(mail,from_folder,dest_folder))
		{
			folder_delete_mail(from_folder,mail);
			success = 1;
		}
	}
	return success;
}

/*****************************************************************************/

void callback_move_mail(struct mail_info *mail, struct folder *from_folder, struct folder *dest_folder)
{
	if (from_folder != dest_folder)
	{
		if (move_mail_helper(mail,from_folder,dest_folder))
		{
			/* If outgoing folder is visible remove the mail */
			if (main_get_folder() == from_folder)
				main_remove_mail(mail);

			/* If sent folder is visible insert the mail */
			if (main_get_folder() == dest_folder)
				main_insert_mail_pos(mail,folder_get_index_of_mail(dest_folder, mail)-1);

			main_refresh_folder(from_folder);
			main_refresh_folder(dest_folder);
			main_display_active_mail();

			read_refresh_prevnext_button(from_folder);
			read_refresh_prevnext_button(dest_folder);
		}
	}
}

/*****************************************************************************/

void callback_maildrop(struct folder *dest_folder)
{
	struct folder *from_folder = main_get_folder();
	if (from_folder != dest_folder)
	{
		void *handle;
		struct mail_info *mail;
		struct mail_info **mail_array;
		int num,moved_num;

		/* find out the number of selected mails */
		num = 0;
		mail = main_get_mail_first_selected(&handle);
		while (mail)
		{
			num++;
			mail = main_get_mail_next_selected(&handle);
		}

		if (!num) return;
		if (!(mail_array = (struct mail_info**)malloc(num*sizeof(struct mail_info*))))
			return;

		app_busy();

		/* move mail per mail, store all success full moves into the array */
		moved_num = 0;
		mail = main_get_mail_first_selected(&handle);
		while (mail)
		{
			if (move_mail_helper(mail,from_folder,dest_folder))
				mail_array[moved_num++] = mail;
			mail = main_get_mail_next_selected(&handle);
		}

		main_refresh_folder(from_folder);
		main_refresh_folder(dest_folder);
		read_refresh_prevnext_button(from_folder);
		read_refresh_prevnext_button(dest_folder);

		/* remove all successful removed mails within the main window */
		if (moved_num == num)
		{
			main_remove_mails_selected();
		} else
		{
			main_freeze_mail_list();
			while (moved_num)
			{
				moved_num--;
				main_remove_mail(mail_array[moved_num]);
			}
			main_thaw_mail_list();
		}
		free(mail_array);
		app_unbusy();

		main_display_active_mail();
	}
}

/*****************************************************************************/

int callback_move_mail_request(char *folder_path, struct mail_info *mail)
{
	struct folder *src_folder = folder_find_by_path(folder_path);
	struct folder *dest_folder;

	if (!src_folder) return 0;

	if ((dest_folder = sm_request_folder(_("Please select the folder where to move the mail"),src_folder)))
	{
		if (move_mail_helper(mail,src_folder,dest_folder))
		{
			main_remove_mail(mail);
			main_refresh_folder(src_folder);
			main_refresh_folder(dest_folder);
			read_refresh_prevnext_button(src_folder);
			read_refresh_prevnext_button(dest_folder);

			if (main_get_folder() == dest_folder)
				main_insert_mail(mail);

			main_display_active_mail();

			return 1;
		}
	}
	return 0;
}

/*****************************************************************************/

void callback_move_selected_mails(void)
{
	struct mail_info *mail;
	void *handle;
	int num;
	struct folder *src_folder;
	struct folder *dest_folder;

	if (!(src_folder = main_get_folder())) return;

	/* Count the number of selected mails first */
	mail = main_get_mail_first_selected(&handle);
	num = 0;
	while (mail)
	{
		num++;
		mail = main_get_mail_next_selected(&handle);
	}

	if (!num) return;

	app_busy();

	if ((dest_folder = sm_request_folder(_("Please select the folder where to move the mails"),src_folder)))
	{
		if (src_folder != dest_folder)
		{
			struct mail_info *mail = main_get_mail_first_selected(&handle);
			while (mail)
			{
				move_mail_helper(mail,src_folder,dest_folder);
				mail = main_get_mail_next_selected(&handle);
			}
			main_refresh_folder(src_folder);
			main_refresh_folder(dest_folder);
			read_refresh_prevnext_button(src_folder);
			read_refresh_prevnext_button(dest_folder);
			main_remove_mails_selected();
			main_display_active_mail();
		}
	}

	app_unbusy();
}

/*****************************************************************************/

void callback_quick_filter_changed(void)
{
	callback_folder_active();
}

/*****************************************************************************/

void callback_progmon_button_pressed(void)
{
	progmonwnd_open();
}

/**
 * Touch the currently selected mail if it (still) matches the given one.
 *
 * @param f the folder
 * @param m the mail
 */
static void touch_active_mail(struct folder *f, struct mail_info *m)
{
	/* Only touch mail if it is still the active one because
	   another mail might be selected during the delay */
	if (m && f && main_get_active_mail() == m && main_get_folder() == f)
	{
		touch_mail(f,m);
	}
}

/**
 * Display the active mail if it (still) matches the given one.
 *
 * @param f the folder
 * @param m the mail
 */
static void really_display_active_mail(struct folder *f, struct mail_info *m)
{
	SM_ENTER;

	if (main_get_active_mail() == m)
	{
		main_display_active_mail();
		if (main_is_message_view_displayed())
		{
			/* TODO: Make the delay user configurable */
			thread_push_function_delayed(2000, touch_active_mail, 2, f, m);
		}
	}
	SM_LEAVE;
}

/**
 * Function that is called, when a mail was downloaded.
 *
 * @param m
 * @param userdata
 */
static void imap_mail_downloaded_callback(struct mail_info *m, void *userdata)
{
	struct folder *f = (struct folder*)userdata;
	if (m && f && main_get_active_mail() == m && main_get_folder() == f)
	{
		main_refresh_mail(m);
		really_display_active_mail(f,m);
	}
}

/**
 * Display the active mail.
 *
 * @param f
 * @param m
 */
static void display_active_mail(struct folder *f, struct mail_info *m)
{
	/* Only display mail if it is still the active one because
	 * another mail might be selected during the delay */
	if (main_get_active_mail() == m)
	{
		if (m && (m->flags & MAIL_FLAGS_PARTIAL))
			imap_download_mail_async(main_get_folder(), m, imap_mail_downloaded_callback, f);
		else
			really_display_active_mail(f, m);
	}
}

/*****************************************************************************/

void callback_mail_within_main_selected(void)
{
	/* delay the displaying, so it is still possible to select multiple mails
     * without problems */
	thread_push_function_delayed(250, display_active_mail, 2, main_get_folder(), main_get_active_mail());
}

/*****************************************************************************/

void callback_check_selected_folder_for_spam(void)
{
	struct folder *folder = main_get_folder();
	void *handle = NULL;
	struct mail_info *m;
	char **white;

	int spams = spam_num_of_spam_classified_mails();
	int hams = spam_num_of_ham_classified_mails();

	if (spams < user.config.min_classified_mails || hams < user.config.min_classified_mails)
	{
		char *which_txt;

		if (spams < user.config.min_classified_mails)
		{
			if (hams < user.config.min_classified_mails) which_txt = _("spam and ham");
			else which_txt = _("spam");
		} else which_txt = _("ham");

		if (!(sm_request(NULL,_("Currently there are too few mails classified as %s.\nStatistical spam identification works only reliable if you classify\n"
											"enough mails before.\n"
											"500 mails for both classes are considered enough, but the more\nyou classify correctly the better it works.\n\n"
											"Number of mails classified as spam (bad mails): %ld\n"
											"Number of mails classified as ham (good mails): %ld\n\nContinue to try to look for spam mails?"),
										_("_Look for spam mails|_Cancel"),which_txt,spams,hams))) return;
	}

	app_busy();

	/* build the white list */
	if (user.config.spam_addrbook_is_white) white = addressbook_get_array_of_email_addresses();
	else white = NULL;
	white = array_add_array(white,user.config.spam_white_emails);

	while ((m = folder_next_mail_info(folder, &handle)))
	{
		if (m->flags & MAIL_FLAGS_PARTIAL)
			imap_download_mail(folder,m);

		if (spam_is_mail_spam(folder->path,m,white,user.config.spam_black_emails))
		{
			folder_set_mail_flags(folder, m, m->flags | MAIL_FLAGS_AUTOSPAM);
			if ((m->flags & MAIL_FLAGS_NEW) && folder->new_mails) folder->new_mails--;
			m->flags &= ~MAIL_FLAGS_NEW;
			main_refresh_mail(m);
		}
	}

	array_free(white);

	app_unbusy();
}

/*****************************************************************************/

void callback_move_spam_marked_mails(void)
{
	struct folder *folder = main_get_folder();
	struct folder *spam_folder = folder_spam();
	struct mail_info **mail_array;
	struct mail_info *m;
	int i;

	if (folder == spam_folder) return;
	if (!spam_folder) return;

	app_busy();

	if ((mail_array = folder_query_mails(folder, FOLDER_QUERY_MAILS_PROP_SPAM)))
	{
		i = 0;
		main_freeze_mail_list();
		while ((m = mail_array[i]))
		{
			callback_move_mail(m, folder, spam_folder);
			i++;
		}
		free(mail_array);
		main_thaw_mail_list();
	}

	app_unbusy();
}

/*****************************************************************************/

void callback_add_spam_folder_to_statistics(void)
{
	struct folder *spam_folder = folder_spam();
	void *handle = NULL;
	struct mail_info *m;

	if (!spam_folder) return;

	app_busy();

	while ((m = folder_next_mail(spam_folder,&handle)))
	{
		spam_feed_mail_as_spam(spam_folder,m);
	}

	app_unbusy();
}

/*****************************************************************************/

void callback_classify_selected_folder_as_ham(void)
{
	struct folder *folder;
	void *handle = NULL;
	struct mail_info *m;

	if (!(folder = main_get_folder()))
		return;

	app_busy();

	while ((m = folder_next_mail(folder,&handle)))
	{
		if (!(mail_is_spam(m) || (m->flags & MAIL_FLAGS_AUTOSPAM)))
		{
			spam_feed_mail_as_ham(folder,m);
		}
	}

	app_unbusy();
}

/*****************************************************************************/

void callback_change_mail(void)
{
	char *filename;

	if ((filename = main_get_mail_filename()))
	{
		struct mail_complete *mail;
		char buf[256];

		getcwd(buf, sizeof(buf));
		chdir(main_get_folder_drawer());

		if ((mail = mail_complete_create_from_file(NULL, filename)))
		{
			struct compose_args ca;
			mail_read_contents("",mail);
			memset(&ca,0,sizeof(ca));
			ca.to_change = mail;
			ca.action = COMPOSE_ACTION_EDIT;
			compose_window_open(&ca);
			mail_complete_free(mail);
		}

		chdir(buf);
	}
}

/*****************************************************************************/

void callback_show_raw(void)
{
	char *filename;

	if ((filename = main_get_mail_filename()))
	{
		sm_show_ascii_file(main_get_folder_drawer(), filename);
	}
}

/**
 * Used for mail iterator callback.
 *
 * @param handle
 * @param userdata
 * @return
 */
static struct mail_info *simplemail_get_first_mail_info_of_main(void *handle, void *userdata)
{
	return main_get_mail_first_selected(handle);
}

/**
 * Uses for mail iterator callback.
 *
 * @param handle
 * @param userdata
 * @return
 */
static struct mail_info *simplemail_get_next_mail_info_of_main(void *handle, void *userdata)
{
	return main_get_mail_next_selected(handle);
}

/**
 * Helper function to create a filter of specified type from the current
 * mail selection.
 *
 * @param type
 */
static void simplemail_create_filter_from_current_mail_selection(enum filter_rule_create_type type)
{
	struct filter *f;
	struct filter_rule *fr;

	if (!(f = filter_create()))
		return;

	if (!(fr = filter_rule_create_from_mail_iterator(type,-1,
			simplemail_get_first_mail_info_of_main,
			simplemail_get_next_mail_info_of_main,
			NULL)))
		goto out;

	filter_add_rule(f,fr);
	filter_open_with_new_filter(f);
out:
	filter_dispose(f);

}

/*****************************************************************************/

void callback_create_sender_filter(void)
{
	simplemail_create_filter_from_current_mail_selection(FRCT_FROM);
}

/*****************************************************************************/

void callback_create_subject_filter(void)
{
	simplemail_create_filter_from_current_mail_selection(FRCT_SUBJECT);
}

/*****************************************************************************/

void callback_create_recipient_filter(void)
{
	simplemail_create_filter_from_current_mail_selection(FRCT_RECEPIENTS);
}

/*****************************************************************************/

void callback_fetch_mails(void)
{
	mails_dl(0);
}

/*****************************************************************************/

void callback_send_mails(void)
{
	mails_upload();
}

/*****************************************************************************/

void callback_check_single_account(int account_num)
{
	struct account *ac = (struct account*)list_find(&user.config.account_list,account_num);
	if (ac)
	{
		mails_dl_single_account(ac);
	}
}

/*****************************************************************************/

void callback_search(void)
{
	struct folder *f = main_get_folder();
	search_open(f->name);
}

/*****************************************************************************/

void callback_start_search(struct search_options *so)
{
	search_clear_results();
	folder_start_search(so);
}

/*****************************************************************************/

void callback_stop_search(void)
{
	thread_abort(NULL);
}

/*****************************************************************************/

void callback_filter(void)
{
	struct folder *f = main_get_folder();
	if (f)
	{
		main_freeze_mail_list();
		folder_filter(f);
		main_thaw_mail_list();
	}
}

/*****************************************************************************/

void callback_edit_filter(void)
{
	filter_open();
}

/*****************************************************************************/

void callback_addressbook(void)
{
	addressbookwnd_open();
}

/*****************************************************************************/

void callback_config(void)
{
	open_config();
}

/*****************************************************************************/

/**
 * Called when accounts have been tested. Called on the context of the main
 * thread.
 *
 * @param success whether login was successful.
 */
static void mail_test_account_callback_on_main_context(account_tested_callback_success_t success)
{
	config_accounts_set_recv_failed_state(!!(success & (POP3_FAILED | IMAP4_FAILED)));
	config_accounts_set_send_failed_state(!!(success & (SMTP_FAILED)));
	config_accounts_can_be_tested(1);
}

/**
 * Called when accounts have been tested. Called not on the context of the main
 * thread!
 *
 * @param success whether login was successful.
 */
static void mails_test_account_callback(account_tested_callback_success_t success)
{
	thread_call_function_async(thread_get_main(), mail_test_account_callback_on_main_context, 1, success);
}

/*****************************************************************************/

void callback_test_account(struct account *ac)
{
	if (mails_test_account(ac, mails_test_account_callback))
	{
		config_accounts_can_be_tested(0);
	}
}

/*****************************************************************************/

void callback_folder_active(void)
{
	struct folder *folder = main_get_folder();
	if (folder)
	{
		app_busy();
		if (folder->is_imap)
			imap_thread_connect(folder);
		lazy_clean_list();
		main_set_folder_mails(folder);
		app_unbusy();
	}
}

/*****************************************************************************/

int callback_folder_count_signatures(char *def_signature)
{
	return folder_count_signatures(def_signature);
}

/*****************************************************************************/

struct mail_info *callback_new_mail_to_folder(char *filename, struct folder *folder)
{
	int pos;
	char buf[256];
	struct mail_info *mail = NULL;

	if (!folder) return NULL;

	getcwd(buf, sizeof(buf));
	chdir(folder->path);

	if (!sm_file_is_in_drawer(filename,folder->path))
	{
		char *newname;

		if ((newname = mail_get_new_name(MAIL_STATUS_UNREAD)))
		{
			myfilecopy(filename,newname);
			mail = mail_info_create_from_file(NULL, newname);
			free(newname);
		}
	} else
	{
		mail = mail_info_create_from_file(NULL, filename);
	}

	if (mail)
	{
		pos = folder_add_mail(folder,mail,1);
		if (main_get_folder() == folder && pos != -1)
			main_insert_mail_pos(mail,pos-1);

		main_refresh_folder(folder);
		read_refresh_prevnext_button(folder);

		/* TODO: Perhaps it would be better to do omit the previous copy operation if this condition holds */
		if (folder->is_imap)
		{
			imap_append_mail(mail, folder->path, folder);
		}
	}

	chdir(buf);
	return mail;
}

/*****************************************************************************/

struct mail_info *callback_new_mail_to_folder_by_file(char *filename)
{
	int pos;
	char buf[256];
	struct mail_info *mail = NULL;
	struct folder *folder;

	if (!(folder = folder_find_by_file(filename))) return NULL;

	getcwd(buf, sizeof(buf));
	chdir(folder->path);

	/* TODO: Use common mail context here! */
	if ((mail = mail_info_create_from_file(NULL, filename)))
	{
		pos = folder_add_mail(folder,mail,1);
		if (main_get_folder() == folder && pos != -1)
			main_insert_mail_pos(mail,pos-1);

		main_refresh_folder(folder);
		read_refresh_prevnext_button(folder);
	}

	chdir(buf);
	return mail;
}

/**
 * Adds the given mail to the folder (both internally and visually).
 *
 * @param mail the new mail which has arrived
 * @param folder specifies the folder which contains the mail
 * @param refresh_folder_data specifies whether the folder data should be refreshed
 */
static void simplemail_new_mail_arrived(struct mail_info *mail, struct folder *folder, int refresh_folder_data)
{
	struct filter *f;
	int pos;

	mail->flags |= MAIL_FLAGS_NEW;
	pos = folder_add_mail(folder,mail,1);

	if (main_get_folder() == folder && pos != -1)
	{
		main_insert_mail_pos(mail,pos-1);
	}

	/* This has to be optimized! */
	if ((f = folder_mail_can_be_filtered(folder, mail, 1)))
	{
		if (f->use_dest_folder && f->dest_folder)
		{
			struct folder *dest_folder = folder_find_by_name(f->dest_folder);
			if (dest_folder)
			{
				/* very slow, because the sorted array is rebuilt in the both folders! */
				callback_move_mail(mail, folder, dest_folder);
			}
		}
	}

	if (refresh_folder_data)
		main_refresh_folder(folder);

	read_refresh_prevnext_button(folder);
}

/*****************************************************************************/

int callback_remote_filter_mail(struct mail_info *mail)
{
	struct filter *f = filter_list_first();
	while (f)
	{
		if (f->flags & FILTER_FLAG_REMOTE)
		{
			if (mail_matches_filter(NULL,mail,f))
			{
				return 1;
			}
		}
		f = filter_list_next(f);
	}
	return 0;
}

/*****************************************************************************/

void callback_import_mbox(void)
{
	char *filename;

	filename = sm_request_file(_("Choose the file which you like to import"),"",0,NULL);
	if (filename && *filename)
	{
		if (!mbox_import_to_folder(NULL,filename))
		{
			sm_request(NULL,_("Couldn't start process for importing.\n"),_("Ok"));
		}
	}
	free(filename);
}

/*****************************************************************************/

void callback_import_dbx(void)
{
	char *filename;

	filename = sm_request_file(_("Choose the file which you like to import"),"",0,".dbx");
	if (filename && *filename)
	{
		if (!dbx_import_to_folder(NULL,filename))
		{
			sm_request(NULL,_("Couldn't start process for importing.\n"),_("Ok"));
		}
	}
	free(filename);
}

/*****************************************************************************/

void callback_export(void)
{
	struct folder *f;
	char *filename;

	if (!(f = main_get_folder())) return;

	filename = sm_request_file(_("Choose export filename"), "",1,NULL);
	if (filename && *filename)
	{
		if (!(mbox_export_folder(f,filename)))
		{
			sm_request(NULL,_("Couldn't start process for exporting.\n"),_("Ok"));
		}
	}
	free(filename); /* accepts NULL pointer */
}

/**
 * Used for collecting new mail instances. This is the head of
 * the list.
 */
static struct mail_info *simplemail_mail_collector_first;

/**
 * Used for collecting new mail instances. This is the tail of
 * the list.
 */
static struct mail_info *simplemail_mail_collector_last;

/**
 * @brief Gathers the mails from the collector and places them
 * into the view.
 */
static void simplemail_gather_mails(void)
{
	struct mail_info *next;
	struct folder *incoming = folder_incoming();
	char buf[256];

	if (!simplemail_mail_collector_first && !incoming) return;

	getcwd(buf, sizeof(buf));
	chdir(incoming->path);

	if (simplemail_mail_collector_first != simplemail_mail_collector_last)
		main_freeze_mail_list();

	next = simplemail_mail_collector_first;
	do
	{
		struct mail_info *next_next;
		next_next = next->next_thread_mail;
		next->next_thread_mail = NULL;
		simplemail_new_mail_arrived(next,incoming,0);
		next = next_next;
	} while (next);

	if (simplemail_mail_collector_first != simplemail_mail_collector_last)
		main_thaw_mail_list();
	main_refresh_folder(incoming);

	simplemail_mail_collector_first = NULL;
	simplemail_mail_collector_last = NULL;

	chdir(buf);
}

/*****************************************************************************/

void callback_new_mail_arrived_filename(char *filename, int is_spam)
{
	struct mail_info *mail;
	char buf[256];

	getcwd(buf, sizeof(buf));
	chdir(folder_incoming()->path);

	/* TODO: Use common mail context here! */
	if ((mail = mail_info_create_from_file(NULL, filename)))
	{
		if (is_spam) mail->flags |= MAIL_FLAGS_AUTOSPAM;

		if (!simplemail_mail_collector_first)
		{
			simplemail_mail_collector_first = mail;
			simplemail_mail_collector_last = mail;
			mail->next_thread_mail = NULL; /* FIXME: Misused this field! */

			thread_push_function_delayed(333,simplemail_gather_mails,0);
		} else
		{
			simplemail_mail_collector_last->next_thread_mail = mail;
			simplemail_mail_collector_last = mail;
		}
	}

	chdir(buf);
}

/*****************************************************************************/

void callback_new_imap_mail_arrived(char *filename, char *user, char *server, char *path)
{
	callback_new_imap_mails_arrived(1,&filename,user,server,path);
}

/*****************************************************************************/

void callback_new_imap_mails_arrived(int num_filenames, char **filenames, char *user, char *server, char *path)
{
	struct mail_info *mail;
	struct folder *f;
	char buf[256];
	int i;

	f = folder_find_by_imap(user, server, path);
	if (!f) return;

	getcwd(buf, sizeof(buf));
	chdir(f->path);

	if (num_filenames > 1)
		main_freeze_mail_list();

	for (i=0;i<num_filenames;i++)
	{
		/* TODO: Use common mail context here! */
		if ((mail = mail_info_create_from_file(NULL, filenames[i])))
		{
			simplemail_new_mail_arrived(mail,f,0);
		}
	}

	if (num_filenames > 1)
		main_thaw_mail_list();

	main_refresh_folder(f);

	chdir(buf);
}

/*****************************************************************************/

void callback_new_imap_uids(unsigned int uid_validity, unsigned int uid_next, char *user, char *server, char *path)
{
	struct folder *f;
	f = folder_find_by_imap(user, server, path);
	if (!f) return;

	SM_DEBUGF(15,("Setting new UIDs for folder %s: uid_validity=%d, uid_next = %d\n",f->name,uid_validity, uid_next));

	f->imap_uid_validity = uid_validity;
	f->imap_uid_next = uid_next;
	folder_config_save(f);
}

/*****************************************************************************/

void callback_number_of_mails_downloaded(int num)
{
	if (num && user.config.receive_sound)
	{
		sm_play_sound(user.config.receive_sound_file);
	}

  if (num && user.config.receive_arexx)
  {
  	gui_execute_arexx(user.config.receive_arexx_file);
  }
}

/*****************************************************************************/

void callback_new_mail_written(struct mail_info *mail)
{
	folder_add_mail(folder_outgoing(),mail,1);
	if (main_get_folder() == folder_outgoing())
	{
		main_insert_mail(mail);
	}
	main_refresh_folder(folder_outgoing());
	read_refresh_prevnext_button(folder_outgoing());
}

/*****************************************************************************/

void callback_mail_has_been_sent(char *filename)
{
	struct filter *f;
	struct folder *out = folder_outgoing();
	struct folder *sent = folder_sent();
	struct mail_info *m;
	if (!out || !sent) return;

	if ((m = folder_find_mail_by_filename(out,filename)))
	{
		/* set the new mail status */
		folder_set_mail_status(out,m,MAIL_STATUS_SENT);
		callback_move_mail(m,out,sent);

		/* This has to be optimized! */
		if ((f = folder_mail_can_be_filtered(folder_sent(), m, 2)))
		{
			if (f->use_dest_folder && f->dest_folder)
			{
				struct folder *dest_folder = folder_find_by_name(f->dest_folder);
				if (dest_folder)
				{
					/* very slow, because the sorted array is rebuilt in the both folders! */
					callback_move_mail(m, folder_sent(), dest_folder);
				}
			}
		}
	}
}

/*****************************************************************************/

void callback_mail_has_not_been_sent(char *filename)
{
	struct folder *out = folder_outgoing();
	struct mail_info *m;
	if (!out) return;

	if ((m = folder_find_mail_by_filename(out,filename)))
	{
		/* set the new mail status to error */
		folder_set_mail_status(out,m,MAIL_STATUS_ERROR);
		/* refresh the folder if visible */
		main_refresh_mail(m);
		main_refresh_folder(out);
	}
}

/*****************************************************************************/

void callback_add_imap_folder(char *user, char *server, char *path)
{
	struct folder *folder;

	folders_lock();
	if ((folder = folder_find_by_imap(user, server,"")))
	{
		folder_add_imap(folder, path);
	}
	folders_unlock();
}

/*****************************************************************************/

void callback_mail_changed(struct folder *folder, struct mail_info *oldmail, struct mail_info *newmail)
{
	if (main_get_folder() == folder)
	{
		if (search_has_mails()) search_remove_mail(oldmail);
		main_replace_mail(oldmail, newmail);
	}
}

/*****************************************************************************/

void callback_mails_mark(int mark)
{
	struct folder *folder = main_get_folder();
	struct mail_info *mail;
	void *handle;
	if (!folder) return;

	mail = main_get_mail_first_selected(&handle);
	while (mail)
	{
		int new_status;

		if (mark) new_status = mail->status | MAIL_STATUS_FLAG_MARKED;
		else new_status = mail->status & (~MAIL_STATUS_FLAG_MARKED);

		if (new_status != mail->status)
		{
			folder_set_mail_status(folder,mail,new_status);
			main_refresh_mail(mail);
		}

		mail = main_get_mail_next_selected(&handle);
	}
}

/*****************************************************************************/

void callback_mails_set_status(int status)
{
	struct folder *folder = main_get_folder();
	struct mail_info *mail;
	void *handle;
	if (!folder) return;

	mail = main_get_mail_first_selected(&handle);
	while (mail)
	{
		int new_status = mail->status;

		if (status == MAIL_STATUS_HOLD || status == MAIL_STATUS_WAITSEND)
		{
			if (!(mail->flags & MAIL_FLAGS_NORCPT))
			{
				/* Only change the status if mail has an recipient. All new mails with no recipient
				 * Will automatically get the hold state */
				if (mail_get_status_type(mail) == MAIL_STATUS_HOLD || mail_get_status_type(mail) == MAIL_STATUS_WAITSEND || mail_get_status_type(mail) == MAIL_STATUS_SENT)
					new_status = status;
			}
		} else
		{
			if (user.config.set_all_stati)
			{
				new_status = status;
			}
			else if (status == MAIL_STATUS_READ || status == MAIL_STATUS_UNREAD)
			{
				if (mail_get_status_type(mail) == MAIL_STATUS_READ || mail_get_status_type(mail) == MAIL_STATUS_UNREAD)
					new_status = status;
			}
		}

		new_status |= mail->status & MAIL_STATUS_FLAG_MARKED;

		if (new_status != mail->status)
		{
			folder_set_mail_status(folder,mail,new_status);
			if (mail->flags & MAIL_FLAGS_NEW && folder->new_mails) folder->new_mails--;
			mail->flags &= ~MAIL_FLAGS_NEW;
			main_refresh_mail(mail);
			main_refresh_folder(folder);
		}

		mail = main_get_mail_next_selected(&handle);
	}
}

/*****************************************************************************/

void callback_selected_mails_are_spam(void)
{
	struct folder *folder = main_get_folder();
	struct mail_info *mail;
	void *handle;
	if (!folder) return;
	mail = main_get_mail_first_selected(&handle);

	while (mail)
	{
		if (mail->flags & MAIL_FLAGS_PARTIAL)
			imap_download_mail(folder,mail);

		if (spam_feed_mail_as_spam(folder,mail))
		{
			folder_set_mail_status(folder,mail,MAIL_STATUS_SPAM);
			if (mail->flags & MAIL_FLAGS_NEW && folder->new_mails) folder->new_mails--;
			mail->flags &= ~MAIL_FLAGS_NEW;
			main_refresh_mail(mail);
		}
		mail = main_get_mail_next_selected(&handle);
	}
}

/*****************************************************************************/

void callback_selected_mails_are_ham(void)
{
	struct folder *folder = main_get_folder();
	struct mail_info *mail;
	void *handle;
	if (!folder) return;
	mail = main_get_mail_first_selected(&handle);

	while (mail)
	{
		if (mail->flags & MAIL_FLAGS_PARTIAL)
			imap_download_mail(folder,mail);

		if (spam_feed_mail_as_ham(folder,mail))
		{
			int refresh = 0;

			if (mail->flags & MAIL_FLAGS_AUTOSPAM)
			{
				folder_set_mail_flags(folder, mail, (mail->flags & (~MAIL_FLAGS_AUTOSPAM)));
				refresh = 1;
			}

			if (mail_is_spam(mail))
			{
				folder_set_mail_status(folder,mail,MAIL_STATUS_UNREAD);
				refresh = 1;
			}

			if (refresh)
				main_refresh_mail(mail);
		}
		mail = main_get_mail_next_selected(&handle);
	}
}

/*****************************************************************************/

void callback_check_selected_mails_if_spam(void)
{
	struct folder *folder = main_get_folder();
	struct mail_info *mail;
	char **white;
	void *handle;
	if (!folder) return;

	/* build the white list */
	if (user.config.spam_addrbook_is_white) white = addressbook_get_array_of_email_addresses();
	else white = NULL;
	white = array_add_array(white,user.config.spam_white_emails);

	mail = main_get_mail_first_selected(&handle);
	while (mail)
	{
		if (mail->flags & MAIL_FLAGS_PARTIAL)
			imap_download_mail(folder,mail);

		if (!mail_is_spam(mail))
		{
			if (spam_is_mail_spam(folder->path,mail,white,user.config.spam_black_emails))
			{
				folder_set_mail_flags(folder, mail, mail->flags | MAIL_FLAGS_AUTOSPAM);
				if ((mail->flags & MAIL_FLAGS_NEW) && folder->new_mails) folder->new_mails--;
				mail->flags &= ~MAIL_FLAGS_NEW;
				main_refresh_mail(mail);
			}
		}
		mail = main_get_mail_next_selected(&handle);
	}
	array_free(white);
}

/*****************************************************************************/

int callback_import_addressbook(void)
{
	int rc = 0;
	char *filename;

	filename = sm_request_file(_("Select an address book file"), "",0,NULL);
	if (filename && *filename)
	{
		addressbook_import_file(filename,1);
		main_build_addressbook();
		addressbookwnd_refresh();
		free(filename);
	}

	return rc;
}

/*****************************************************************************/

void callback_apply_folder(struct filter *filter)
{
	struct folder *folder = main_get_folder();
	if (folder)
	{
		main_freeze_mail_list();
		folder_apply_filter(folder,filter);
		main_thaw_mail_list();
	}
}

/*****************************************************************************/

void callback_new_folder(void)
{
	folder_edit_new_path(folder_get_possible_path());
}

/*****************************************************************************/

void callback_new_group(void)
{
	folder_add_group(_("New Group"));
	main_refresh_folders();
	search_refresh_folders();
	config_refresh_folders();
	filter_update_folder_list();
}

/*****************************************************************************/

void callback_new_folder_path(char *path, char *name)
{
	folder_add_with_name(path, name);
	main_refresh_folders();
	search_refresh_folders();
	config_refresh_folders();
	filter_update_folder_list();
}

/*****************************************************************************/

void callback_remove_folder(void)
{
	struct folder *f = main_get_folder();
	if (f)
	{
		app_busy();

		if (folder_remove(f))
		{
			main_refresh_folders();
			search_refresh_folders();
			config_refresh_folders();
			filter_update_folder_list();

			f = main_get_folder();
			main_set_folder_mails(f);
		}

		app_unbusy();
	}
}

/*****************************************************************************/

int callback_failed_ssl_verification(const char *server_name, const char *reason, const char *cert_summary, const char *sha1_ascii, const char *sha256_ascii)
{
	int choice;
	int sha256_supported = sha256_ascii[0];

	if (sha256_supported)
	{
		if (account_is_server_trustworthy(server_name, sha256_ascii))
			return 1;
	}
	if (account_is_server_trustworthy(server_name, sha1_ascii))
		return 1;

	choice = sm_request(NULL, _("Failed to verify certificate for server\n%s\n\n%s\n\n%s\nSHA1: %s\nSHA256: %s"),
			_("Connect anyway|Trust always|Abort"), server_name, reason, cert_summary, sha1_ascii, sha256_supported?sha256_ascii:_("Not supported"));
	if (choice == 2)
	{
		/* Trust always */
		const char *fingerprint = sha256_supported?sha256_ascii:sha1_ascii;
		account_trust_server(server_name, fingerprint);
		/* Inform the config window:
		 *
		 * TODO: The config window's test function should use an own hook for
		 * this so that the fingerprint updated is  restricted to the config
		 * window */
		config_accounts_update_fingerprint(server_name, fingerprint);
		sm_request(NULL,_("In order to trust the server permanently,\nyou must save the configuration."), _("OK"));
	}
	return !!choice;
}

/**
 * Callback that is called when the imap folders are received.
 *
 * @param server
 * @param all_folder_list
 * @param sub_folder_list
 */
static void callback_received_imap_folders(struct imap_server *server, struct string_list *all_folder_list, struct string_list *sub_folder_list)
{
	struct folder *f = folder_find_by_imap(server->login, server->name, "");
	if (!f) return;
	folder_imap_set_folders(f, all_folder_list, sub_folder_list);
	folder_fill_lists(all_folder_list, sub_folder_list);
	folder_config_save(f);
}

/*****************************************************************************/

void callback_imap_get_folders(struct folder *f)
{
	if (f->is_imap && f->special == FOLDER_SPECIAL_GROUP)
	{
		struct imap_server *server = account_find_imap_server_by_folder(f);
		if (server)
		{
			imap_get_folder_list(server,callback_received_imap_folders);
			return;
		}
	}
}

/*****************************************************************************/

void callback_imap_submit_folders(struct folder *f, struct string_list *list)
{
	if (f->is_imap && f->special == FOLDER_SPECIAL_GROUP)
	{
		struct imap_server *server = account_find_imap_server_by_folder(f);
		if (server)
		{
			imap_submit_folder_list(server,list);
			return;
		}

	}
}

/*****************************************************************************/

void callback_edit_folder(void)
{
	struct folder *from_folder = main_get_folder();
	if (from_folder)
	{
		folder_edit(from_folder);
	}
}

/*****************************************************************************/

void callback_change_folder_attrs(void)
{
	struct folder *f = folder_get_changed_folder();
	int refresh;

	if (!f) return;

	if (folder_set_would_need_reload(f, folder_get_changed_name(), folder_get_changed_path(), folder_get_changed_type(), folder_get_changed_defto()))
	{
		/* Remove the mails, as they get's geloaded */
		main_clear_folder_mails();
		refresh = 1;
	} else refresh = 0;

	f->imap_download = folder_get_imap_download();

	if (folder_set(f, folder_get_changed_name(), folder_get_changed_path(), folder_get_changed_type(), folder_get_changed_defto(),
	                  folder_get_changed_deffrom(), folder_get_changed_defreplyto(), folder_get_changed_defsignature(),
	                  folder_get_changed_primary_sort(), folder_get_changed_secondary_sort()))
	{
		if (main_get_folder() == f || refresh)
		{
			main_set_folder_mails(f);
		}
	}

	main_refresh_folder(f);
	read_refresh_prevnext_button(f);
	search_refresh_folders();
	config_refresh_folders();
	filter_update_folder_list();
}

/*****************************************************************************/

void callback_reload_folder_order(void)
{
	folder_load_order();
	callback_refresh_folders();
}

/*****************************************************************************/

void callback_refresh_folders(void)
{
	main_refresh_folders();
	search_refresh_folders();
	config_refresh_folders();
	filter_update_folder_list();
}

/*****************************************************************************/

void callback_config_changed(void)
{
	/* Build the check single account menu */
	main_build_accounts();
	main_refresh_window_title(autocheck_seconds_start);
	folder_create_imap();
	callback_refresh_folders();
	compose_refresh_signature_cycle();
	folder_refresh_signature_cycle();
}

/*****************************************************************************/

void callback_select_mail(int num)
{
	main_select_mail(num);
}

/*****************************************************************************/

void callback_delete_all_indexfiles(void)
{
	app_busy();
	folder_delete_all_indexfiles();
	app_unbusy();
}

/*****************************************************************************/

void callback_save_all_indexfiles(void)
{
	app_busy();
	folder_save_all_indexfiles();
	app_unbusy();
}

/*****************************************************************************/

void callback_rescan_folder_completed(char *folder_path,  void *udata)
{
	struct folder *f;

	if (!(f = folder_find_by_path(folder_path)))
		return;

	if (main_get_folder() != f)
		return;

	app_busy();
	main_set_folder_mails(f);
	main_refresh_folder(f);
	read_refresh_prevnext_button(f);
	app_unbusy();
}

/*****************************************************************************/

void callback_refresh_folder(struct folder *f)
{
	main_refresh_folder(f);
}

/*****************************************************************************/

void callback_rescan_folder(void)
{
	struct folder *f = main_get_folder();
	if (f)
	{
		/* Because this means deleting all mails we safely remove all found mails as it
		 * could reference an old mail */
		search_clear_results();
		folder_rescan_async(f, status_set_status, callback_rescan_folder_completed, NULL);
	}
}

/** @brief 1, if downloading partial mails is in process */
static int downloading_partial_mail;

/** @brief Progress monitor for downloading all mails */
static struct progmon *downloading_partial_mail_progmon;

static int simplemail_download_next_partial_mail(void);

/**
 * Callback that is invoked if a partial mail has been downloaded.
 *
 * @param m
 * @param userdata
 */
static void simplemail_partial_mail_downloaded(struct mail_info *m, void *userdata)
{
	downloading_partial_mail = 0;

	if (downloading_partial_mail_progmon)
	{
		downloading_partial_mail_progmon->work(downloading_partial_mail_progmon,1);
	}

	main_refresh_mail(m);

	if (!simplemail_download_next_partial_mail())
	{
		status_set_status(_("All mails downloaded completely"));

		if (downloading_partial_mail_progmon)
		{
			downloading_partial_mail_progmon->done(downloading_partial_mail_progmon);
			progmon_delete(downloading_partial_mail_progmon);
			downloading_partial_mail_progmon = NULL;
		}
	}
}

/**
 * Proceeds in downloading partial mails.
 *
 * @return 1 if one mail is scheduled for download (or in process to be downloaded)
 */
static int simplemail_download_next_partial_mail(void)
{
	static unsigned int last_ticks;

	struct folder *f;

	/* If the active folder is an imap folder and in download mode
	 * "complete mails" we download the next partial mail.
	 */
	f = main_get_folder();
	if (f && f->is_imap && f->imap_download && !downloading_partial_mail && f->partial_mails)
	{
		void *handle;
		struct mail_info *m;

		handle = NULL;

		/* Find a partial mail */
		while ((m = folder_next_mail(f,&handle)))
		{
			if (m->flags & MAIL_FLAGS_PARTIAL)
				break;
		}

		if (m)
		{
			if (!downloading_partial_mail_progmon)
			{
				utf8 *name;

				downloading_partial_mail_progmon = progmon_create();
				name = utf8create(_("Downloading complete mails"), user.config.default_codeset?user.config.default_codeset->name:NULL);
				downloading_partial_mail_progmon->begin(downloading_partial_mail_progmon,f->partial_mails,name);
				free(name);
			}

			SM_DEBUGF(10,("Issuing download of next partial mail \"%s\"\n",m->filename));

			if (time_ms_passed(last_ticks) > 500)
			{
				char status_txt[120];

				sm_snprintf(status_txt,sizeof(status_txt),_("Downloading all complete mails for folder \"%s\": %ld mails to go"),f->name,f->partial_mails);
				status_set_status(status_txt);

				if (downloading_partial_mail_progmon)
				{
					utf8 *info;
					utf8 buf[80];

					sm_snprintf((char*)buf,sizeof(buf),_("%ld mails to go"),f->partial_mails);

					info = utf8create(buf, user.config.default_codeset?user.config.default_codeset->name:NULL);
					downloading_partial_mail_progmon->working_on(downloading_partial_mail_progmon,info);
					free(info);
				}

				last_ticks = time_reference_ticks();
			}

			if (imap_download_mail_async(f,m,simplemail_partial_mail_downloaded,NULL))
			{
				downloading_partial_mail = 1;
			} else
			{
				status_set_status(_("Could not download complete mail"));
			}
		}
	}
	return downloading_partial_mail;
}

/** Auto-Timer functions **/

/**
 * That's the function which is called every second
 */
static void callback_timer(void)
{
	if (user.config.receive_autocheck || (user.config.receive_autoonstartup && autocheck_seconds_start == 0))
	{
		if (sm_get_current_seconds() - autocheck_seconds_start >= user.config.receive_autocheck * 60)
		{
			/* nothing should happen when mails_dl() is called twice,
			   this could happen if a mail downloading takes very long */
			callback_autocheck_reset();

			/* If socket library cannot be opened we also shouldn't try
				 to download the mails */
			if (open_socket_lib())
			{
				close_socket_lib();

				if (!user.config.receive_autoifonline || is_online("mi0"))
				{
					mails_dl(1);
				}
			}
		}
	}

	simplemail_download_next_partial_mail();

	/* Register us again */
	thread_push_function_delayed(1000, callback_timer, 0);
}

/*****************************************************************************/

void callback_autocheck_reset(void)
{
	autocheck_seconds_start = sm_get_current_seconds();
	main_refresh_window_title(autocheck_seconds_start);
}

static int about_to_update_progmonwnd;

/**
 * Updates the progress monitor window.
 */
static void simplemail_update_progmonwnd(void)
{
	about_to_update_progmonwnd = 0;
	progmonwnd_update(0);
}

/*****************************************************************************/

void simplemail_update_progress_monitors(void)
{
	SM_ENTER;

	if (progmon_get_number_of_actives())
	{
		main_set_progress(progmon_get_total_work(),progmon_get_total_work_done());
	} else
	{
		main_hide_progress();
	}

	/* Avoid calling simplemail_update_progmonwnd() too often */
	if (!about_to_update_progmonwnd)
	{
		thread_push_function_delayed(250, simplemail_update_progmonwnd, 0);
		about_to_update_progmonwnd = 1;
	}

	SM_LEAVE;
}


/*****************************************************************************/

void simplemail_deinit(void)
{
	SM_ENTER;

	gui_deinit();
	if (user.config.delete_deleted)
		folder_delete_deleted();

	cleanup_threads();
	cleanup_mailinfo_extractor();

	ssl_cleanup();
	spam_cleanup();

	del_folders();

	free_config();
	codesets_cleanup();

	cleanup_addressbook();

	progmon_deinit();

	startupwnd_close();
	shutdownwnd_close();
	atcleanup_finalize();

	SM_LEAVE;

	logg_dispose();
	debug_deinit();
}

/*****************************************************************************/

int simplemail_init(void)
{
	logg_options_t logg_opts = {0};

	if (!debug_init())
		goto out;

	logg_init(&logg_opts);

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	if (!gui_parseargs(0,NULL))
	{
		SM_DEBUGF(1,("Unable to parse arguments!\n"));
		goto out;
	}

	startupwnd_open();
	if (!progmon_init())
	{
		SM_DEBUGF(1,("Couldn't initialize progress monitor system!\n"));
		goto out;
	}

	if (!init_threads())
	{
		SM_DEBUGF(1,("Couldn't initialize thread system!\n"));
		goto out;
	}

	if (!codesets_init())
	{
		SM_DEBUGF(1,("Couldn't initialize codesets!\n"));
		goto out;
	}

	load_config();
	if (!init_addressbook())
	{
		SM_DEBUGF(1,("Couldn't initialize addressbook!\n"));
		goto out;
	}

	if (!init_folders())
	{
		SM_DEBUGF(1,("Couldn't initialize folders!\n"));
		goto out;
	}

	if (!(spam_init()))
	{
		SM_DEBUGF(1,("Couldn't initialize spam system!\n"));
		goto out;
	}

	if (!ssl_init())
	{
		SM_DEBUGF(1,("Couldn't initialize ssl subsystem!\n"));
		goto out;
	}

	if (!gui_init())
	{
		SM_DEBUGF(1,("Couldn't initialize GUI!\n"));
		goto out;
	}

	/* Perform an email check if requested by configuration */
	if (user.config.receive_autoonstartup) autocheck_seconds_start = 0;
	else callback_autocheck_reset();
	callback_timer();

	SM_DEBUGF(10,("SimpleMail initialized\n"));
	return 1;

out:
	simplemail_deinit();
	return 0;
}

/*****************************************************************************/

int simplemail_main(void)
{
	if (simplemail_init())
	{
		gui_loop();
		simplemail_deinit();
		return 0;
	} else
		return 20;
}

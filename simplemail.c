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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "addressbook.h"
#include "atcleanup.h"
#include "codesets.h"
#include "configuration.h"
#include "dbx.h"
#include "debug.h"
#include "estimate.h"
#include "filter.h"
#include "folder.h"
#include "imap.h" /* imap_thread_xxx() */
#include "mail.h"
#include "mbox.h"
#include "simplemail.h"
#include "smintl.h"
#include "spam.h"
#include "support_indep.h"
#include "status.h"
#include "trans.h"
#include "debug.h"

#include "addressbookwnd.h"
#include "composewnd.h"
#include "configwnd.h"
#include "filterwnd.h"
#include "folderwnd.h"
#include "gui_main.h"
#include "mainwnd.h"
#include "parse.h"
#include "readwnd.h"
#include "searchwnd.h"
#include "startupwnd.h"
#include "shutdownwnd.h"
#include "subthreads.h"
#include "support.h"
#include "tcpip.h"
#include "upwnd.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

/** for the Auto-Timer functions **/
static unsigned int autocheck_seconds_start; /* to compare with this */

static void lazy_clean_list(void);

/* the current mail should be viewed, returns the number of the window
	which the function has opened or -1 for an error */
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

/* Save the currently activated mail */
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

/* Touches the mail in the given folder */
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

/* the selected mails should be deleted */
void callback_delete_mails(void)
{
	struct folder *from_folder = main_get_folder();
	struct mail_info *mail;
	void *handle;
	int num;
	int permanent; /* 1 if mails should be deleted permanently */

	if (from_folder)
	{
		int search_has;

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
	}
}

/* a single mail of any folder should be deleted */
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

/* delete mails by uid and folder */
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
		folder_unlock(f);
		folders_unlock();
		return;
	}

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

/* get the address */
void callback_get_address(void)
{
	struct mail_info *mail = main_get_active_mail();
	if (mail)
	{
		char *addr,*phrase;
		struct folder *f;

		f = main_get_folder();

		if (folder_get_type(f) == FOLDER_TYPE_SEND)
		{
			phrase = mail->to_phrase;
			addr = mail->to_addr;
		} else
		{
			phrase = mail->from_phrase;
			addr = mail->from_addr;
		}

		if (addr && !addressbook_find_entry_by_address(addr))
		{
			struct addressbook_entry_new entry;
			memset(&entry,0,sizeof(entry));

			entry.realname = phrase;
			entry.email_array = array_add_string(NULL,addr);

			addressbookwnd_create_entry(&entry);

			array_free(entry.email_array);
		}
	}
}

int callback_write_mail(char *from, char *to, char *replyto, char *subject)
{
	struct compose_args ca;
	int win_num;
	memset(&ca,0,sizeof(ca));

	ca.action = COMPOSE_ACTION_NEW;
	ca.to_change = mail_create_for(from,to,replyto,subject);

	win_num = compose_window_open(&ca);

	if (ca.to_change) mail_complete_free(ca.to_change);
	return win_num;
}

/* a new mail should be written to the given address */
void callback_write_mail_to(struct addressbook_entry_new *address)
{
	char *addr = address->email_array?address->email_array[0]:NULL;
	callback_write_mail_to_str(addr,NULL);
}

/* a new mail should be written to a given address string */
int callback_write_mail_to_str(char *str, char *subject)
{
	return callback_write_mail(NULL,str,NULL,subject);
}

/* open an arbitrary message */
int callback_open_message(char *message, int window)
{
	char buf[380];
	static char stored_dir[512];
	char *path;
	int num = -1;

	if (!message)
	{
		path = sm_request_file("SimpleMail", stored_dir, 0, "");
	} else
	{
		path = mystrdup(message);
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

		if (!message)
			mystrlcpy(stored_dir,dir,sizeof(stored_dir));

		if (getcwd(buf, sizeof(buf)) == NULL) return -1;
		chdir(dir);

		if ((mail = mail_info_create_from_file(filename)))
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

/* a new mail should be composed */
void callback_new_mail(void)
{
	struct folder *f = main_get_folder();
	if (!f) return;
	callback_write_mail(f->def_from,f->def_to,f->def_replyto,NULL);
}

/* reply this mail */
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
			if (!(mail_array[i] = mail_complete_create_from_file(to_reply_array[i]->filename)))
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

/* a mail should be replied */
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

/* a mail should be forwarded */
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

/* a single mail should be moved from a folder to another folder */
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

/* mails has been droped onto the folder */
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

/* a single mail should be moved */
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

/* all selected mails should be moved */
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

/* Called whenever the contents of the quick filter has been acknowledged
 * TODO: Implement me properly */
void callback_quick_filter_changed(void)
{
	callback_folder_active();
}

static void touch_active_mail(struct folder *f, struct mail_info *m)
{
	/* Only touch mail if it is still the active one because
	   another mail might be selected during the delay */
	if (m && f && main_get_active_mail() == m && main_get_folder() == f)
	{
		touch_mail(f,m);
	}
}

static void display_active_mail(struct folder *f, struct mail_info *m)
{
	/* Only display mail if it is still the active one because
	   another mail might be selected during the delay */
	if (main_get_active_mail() == m)
	{
		main_display_active_mail();

		if (main_is_message_view_displayed())
		{
			/* TODO: Make the delay user configurable */
			thread_push_function_delayed(2000, touch_active_mail, 2, f, m);
		}
	}
}

/* a new mail within the main window has been selected */
void callback_mail_within_main_selected(void)
{
	/* delay the displaying, so it is still possible to select multiple mails
     without problems */
	thread_push_function_delayed(250, display_active_mail, 2, main_get_folder(), main_get_active_mail());
}

/* Process the current selected folder and mark all mails which are identified as spam */
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
			if (m->flags & MAIL_FLAGS_NEW && folder->new_mails) folder->new_mails--;
			m->flags &= ~MAIL_FLAGS_NEW;
			main_refresh_mail(m);
		}
	}

	array_free(white);

	app_unbusy();
}

/* Move all mails marked as spam into the spam folder */
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

/* Adds all mails into the spam folder into the spam statistics */
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

/* classifies all mails within the selected folder as ham */
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

/* the currently selected mail should be changed */
void callback_change_mail(void)
{
	char *filename;

	if ((filename = main_get_mail_filename()))
	{
		struct mail_complete *mail;
		char buf[256];

		getcwd(buf, sizeof(buf));
		chdir(main_get_folder_drawer());

		if ((mail = mail_complete_create_from_file(filename)))
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

/* the raw contents of the selected mail show be showed */
void callback_show_raw(void)
{
	char *filename;

	if ((filename = main_get_mail_filename()))
	{
		sm_show_ascii_file(main_get_folder_drawer(), filename);
	}
}

/* mails should be fetched */
void callback_fetch_mails(void)
{
	mails_dl(0);
}

/* mails should be sent */
void callback_send_mails(void)
{
	mails_upload();
}

/* Check the mails of a single acount */
void callback_check_single_account(int account_num)
{
	struct account *ac = (struct account*)list_find(&user.config.account_list,account_num);
	if (ac)
	{
		mails_dl_single_account(ac);
	}
}

/* open the search window */
void callback_search(void)
{
	struct folder *f = main_get_folder();
	search_open(f->name);
}

/* Start the search process with the given search options */
void callback_start_search(struct search_options *so)
{
	search_clear_results();
	folder_start_search(so);
}

/* Stop the search process */
void callback_stop_search(void)
{
	thread_abort(NULL);
}

/* filter the mails */
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

/* the filters should be edited */
void callback_edit_filter(void)
{
	filter_open();
}

/* addressbook should be opened */
void callback_addressbook(void)
{
	addressbookwnd_open();
}

/* Open the configuration window */
void callback_config(void)
{
	open_config();
}

/* a new folder has been activated */
void callback_folder_active(void)
{
	struct folder *folder = main_get_folder();
	if (folder)
	{
		if (folder->is_imap)
			imap_thread_connect(folder);
		lazy_clean_list();
		main_set_folder_mails(folder);
	}
}

/* count the signatures used in folders */
int callback_folder_count_signatures(char *def_signature)
{
	return folder_count_signatures(def_signature);
}

/* a new mail should be added to a given folder */
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
			mail = mail_info_create_from_file(newname);
			free(newname);
		}
	} else
	{
		mail = mail_info_create_from_file(filename);
	}

	if (mail)
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

/* a new mail should be added to a folder, only filename known */
struct mail_info *callback_new_mail_to_folder_by_file(char *filename)
{
	int pos;
	char buf[256];
	struct mail_info *mail = NULL;
	struct folder *folder;

	if (!(folder = folder_find_by_file(filename))) return NULL;

	getcwd(buf, sizeof(buf));
	chdir(folder->path);

	if ((mail = mail_info_create_from_file(filename)))
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

/* checks given mail with a remote filter. Returns 1 if mail should be ignored otherwise 0
 * (yes, this has to be extented in the future) */
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

/**
 * Import mails from a mbox file
 *
 * in_folder_ptr might be NULL.
 */
void callback_import_mbox(int *in_folder_ptr)
{
	int in_folder = in_folder_ptr?*in_folder_ptr:0;
	struct folder *f=NULL;
	char *filename;

	if (in_folder)
	{
		if (!(f = main_get_folder())) return;
	}

	filename = sm_request_file(_("Choose the file which you like to import"),"",0,NULL);
	if (filename && *filename)
	{
		if (!mbox_import_to_folder(f,filename))
		{
			sm_request(NULL,_("Couldn't start process for importing.\n"),_("Ok"));
		}
	}
	free(filename);
}

/**
 * Import dbx mailes (outlook express)
 */
void callback_import_dbx(int *in_folder_ptr)
{
	int in_folder = in_folder_ptr?*in_folder_ptr:0;
	struct folder *f=NULL;
	char *filename;

	if (in_folder)
	{
		if (!(f = main_get_folder())) return;
	}

	filename = sm_request_file(_("Choose the file which you like to import"),"",0,".dbx");
	if (filename && *filename)
	{
		if (!dbx_import_to_folder(f,filename))
		{
			sm_request(NULL,_("Couldn't start process for importing.\n"),_("Ok"));
		}
	}
	free(filename);
}

/* Export mails */
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

/**
 * @brief A new mail has been arrived for the incoming folder.
 *
 * A new mail has been arrived. In the current implementation the mail
 * is not immediately added to the viewer but collected and then added
 * to the viewer after a short period of time.
 *
 * @param filename defines the filename of the new mail
 * @param is_spam is the mail spam?
 *
 * @note FIXME: This functionality takes (mis)usage of next_thread_mail field
 *       FIXME: When SM is quit while mails are downloaded those mails get not presented to the user the next time.
 */
void callback_new_mail_arrived_filename(char *filename, int is_spam)
{
	struct mail_info *mail;
	char buf[256];

	getcwd(buf, sizeof(buf));
	chdir(folder_incoming()->path);

	if ((mail = mail_info_create_from_file(filename)))
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

/**
 * @brief A new mail arrived into an imap folder
 *
 * @param filename the name of the filename.
 * @param user defines the user name of the login
 * @param server defines the name of the imap server
 * @param path defines the path on the imap server
 */
void callback_new_imap_mail_arrived(char *filename, char *user, char *server, char *path)
{
	callback_new_imap_mails_arrived(1,&filename,user,server,path);
}

/**
 * New mails arrived into an imap folder
 *
 * @param num_filenames number of file names that are stored in filenames array
 * @param filenames an array of filenames.
 * @param user defines the user name of the login
 * @param server defines the name of the imap server
 * @param path defines the path on the imap server
 */
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
		if ((mail = mail_info_create_from_file(filenames[i])))
		{
			simplemail_new_mail_arrived(mail,f,0);
		}
	}

	if (num_filenames > 1)
		main_thaw_mail_list();

	main_refresh_folder(f);

	chdir(buf);
}

/* After downloading this function is called */
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

/* a new mail has been written */
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

/* a mail has been send so it can be moved to the "Sent" drawer now */
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

		/* This has to be optmized! */
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

/* a mail has NOT been send, something went wrong, set ERROR status */
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

/* adds a new imap folder */
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

/**
 * @brief the lazy thread instance
 *
 * The main purpose of this thread is to execute...
 */
static thread_t lazy_thread;

/**
 * @brief Allows the storage of mail_info into a list.
 */
struct mail_info_node
{
	struct node node;
	struct mail_info *mail_info;
	char *folder_path;
};

/**
 * @brief List containing mail infos of which the excerpt should be read.
 *
 * Entries in this list are referenced!
 */
static struct list lazy_mail_list;

/**
 * @brief The semaphore protecting lazy_mail_list.
 */
static semaphore_t lazy_semaphore;


/**
 * @brief Finished the stuff done in lazy_thread_work and derefernces the mail.
 *
 * @param excerpt
 * @param mail
 *
 * @note Needs to be called at the context of the main task
 */
static void lazy_thread_work_finished(utf8 *excerpt, struct mail_info *mail)
{
	mail_info_set_excerpt(mail,excerpt);
	main_refresh_mail(mail);
	mail_dereference(mail);
}

/**
 * @brief The worker function of the lazy thread.
 *
 * This worker opens the mail and reads an excerpt. This is
 * then forwarded to the main task which in turn notifies
 * the view.
 *
 * @param the path which is going to be freed here.
 * @param mail referenced mail
 *
 * @note needs to be called at the context of the lazy thread
 */
static void lazy_thread_work(char *path, struct mail_info *mail)
{
	char buf[380];

	struct mail_complete *mail_complete;
	utf8 *excerpt = NULL;

	if (mail->excerpt)
	{
		/* Just dereference the mail */
		thread_call_parent_function_async(mail_dereference,1,mail);
		return;
	}

	getcwd(buf, sizeof(buf));
	chdir(path);

	if ((mail_complete = mail_complete_create_from_file(mail->filename)))
	{
		struct mail_complete *mail_text;
		mail_read_contents("",mail_complete);
		if ((mail_text = mail_find_initial(mail_complete)))
		{
			if (!mystricmp(mail_text->content_type,"text"))
			{
				void *cont; /* mails content */
				int cont_len;

				string str;

				mail_decoded_data(mail_text,&cont,&cont_len);

				/* Create the excerpt, i.e., the first 100 characters
				 * with no new lines and no quotation
				 */
				if (string_initialize(&str,100))
				{
					int i,j;
					int space = 0;
					int eol = 1;
					int ignore_line = 0;

					for (i=0,j=0;i<cont_len && j<100;)
					{
						unsigned char c = ((char*)cont)[i];
						int bytes;

						if (!c) break;

						if (c == 10)
						{
							eol = 1;
							space = 1;
							ignore_line = 0;
							i++;
							continue;
						}

						if (isspace(c))
						{
							i++;
							space = 1;
							continue;
						}

						if (c == '>' && eol)
							ignore_line = 1;

						eol = 0;

						if (space && !ignore_line)
						{
							string_append_char(&str,' ');
							j++;
							if (j == 100) break;
							space = 0;
						}

						bytes = utf8bytes(&((utf8*)cont)[i]);
						if (!ignore_line)
							string_append_part(&str,&((char*)cont)[i],bytes);
						i+=bytes;
					}
					excerpt = (utf8*)str.str;
				}
			}
		}
	}

	mail_complete_free(mail_complete);
	chdir(buf);

	/* This also dereferences the given mail */
	thread_call_parent_function_async(lazy_thread_work_finished,2,excerpt,mail);
}

/**
 * @brief Entry for the lazy thread.
 *
 * @param user_data
 * @return
 */
static int lazy_entry(void *user_data)
{
	thread_parent_task_can_contiue();
	while (thread_wait(NULL,NULL,0))
	{
		/* Scan through the lazy mail list */
		while (1)
		{
			struct mail_info_node *mail_node;

			thread_lock_semaphore(lazy_semaphore);
			mail_node = (struct mail_info_node*)list_first(&lazy_mail_list);
			if (mail_node) node_remove(&mail_node->node);
			thread_unlock_semaphore(lazy_semaphore);
			if (!mail_node) break;
			lazy_thread_work(mail_node->folder_path,mail_node->mail_info);

			free(mail_node->folder_path);
			free(mail_node);
		}

	}
	return 1;
}

/**
 * @brief Removes and frees all entries from the lazy list.
 *
 * @note needs to be called at the context of the main thread.
 */
static void lazy_clean_list(void)
{
	struct mail_info_node *mail_node;

	if (!lazy_semaphore) return;

	thread_lock_semaphore(lazy_semaphore);

	while ((mail_node = (struct mail_info_node*)list_remove_tail(&lazy_mail_list)))
	{
		mail_dereference(mail_node->mail_info);
		free(mail_node->folder_path);
		free(mail_node);
	}

	thread_unlock_semaphore(lazy_semaphore);
}

/**
 * @brief Loads the excerpt of the given mail of the current folder
 * in a lazy fashion.
 *
 * In order to load the excerpt in a lazy fashion this functions spawns a
 * sub thread.
 *
 * @param mail
 * @return whether successful
 */
int simplemail_get_mail_info_excerpt_lazy(struct mail_info *mail)
{
	struct mail_info_node *node;
	struct folder *f = main_get_folder();
	char *folder_path;

	if (!lazy_thread)
	{
		lazy_thread = thread_add("SimpleMail - Lazy",lazy_entry,NULL);
		list_init(&lazy_mail_list);
		lazy_semaphore = thread_create_semaphore();
	}

	if (!lazy_thread) return 0;

	if (!(folder_path = mystrdup(f->path)))
		return 0;
	if (!(node = malloc(sizeof(*node))))
	{
		free(folder_path);
		return 0;
	}

	mail_reference(mail);
	node->mail_info = mail;
	node->folder_path = folder_path;

	thread_lock_semaphore(lazy_semaphore);
	list_insert_tail(&lazy_mail_list,&node->node);
	thread_unlock_semaphore(lazy_semaphore);

	thread_signal(lazy_thread);

	return 1;
}

/* a mail has been changed/replaced by the user */
void callback_mail_changed(struct folder *folder, struct mail_info *oldmail, struct mail_info *newmail)
{
	if (main_get_folder() == folder)
	{
		if (search_has_mails()) search_remove_mail(oldmail);
		main_replace_mail(oldmail, newmail);
	}
}

/* mark/unmark all selected mails */
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

/* set the status of all selected mails */
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

/* Selected mails are spam */
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

/* Selected mails are ham */
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

/* Check if selected mails are spam */
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
				if (mail->flags & MAIL_FLAGS_NEW && folder->new_mails) folder->new_mails--;
				mail->flags &= ~MAIL_FLAGS_NEW;
				main_refresh_mail(mail);
			}
		}
		mail = main_get_mail_next_selected(&handle);
	}
	array_free(white);
}

/* import a addressbook into SimpleMail, return 1 for success */
int callback_import_addressbook(void)
{
	int rc = 0;
	char *filename;

	filename = sm_request_file(_("Select an addressbook-file."), "",0,NULL);
	if (filename && *filename)
	{
		addressbook_import_file(filename,1);
		main_build_addressbook();
		addressbookwnd_refresh();
		free(filename);
	}

	return rc;
}

/* apply a single filter in the active folder */
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

/* create a new folder */
void callback_new_folder(void)
{
	folder_edit_new_path(new_folder_path());
}

/* create a new group */
void callback_new_group(void)
{
	folder_add_group(_("New Group"));
	main_refresh_folders();
	search_refresh_folders();
	config_refresh_folders();
	filter_update_folder_list();
}

/* create a new folder */
void callback_new_folder_path(char *path, char *name)
{
	folder_add_with_name(path, name);
	main_refresh_folders();
	search_refresh_folders();
	config_refresh_folders();
	filter_update_folder_list();
}

/* Remove the selected folder */
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

/* called when imap folders has been received */
static void callback_received_imap_folders(struct imap_server *server, struct list *all_folder_list, struct list *sub_folder_list)
{
	struct folder *f = folder_find_by_imap(server->login, server->name, "");
	if (!f) return;
	folder_imap_set_folders(f, all_folder_list, sub_folder_list);
	folder_fill_lists(all_folder_list, sub_folder_list);
	folder_config_save(f);
}

/*  */
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

/* */
void callback_imap_submit_folders(struct folder *f, struct list *list)
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

/* edit folder settings */
void callback_edit_folder(void)
{
	struct folder *from_folder = main_get_folder();
	if (from_folder)
	{
		folder_edit(from_folder);
	}
}

/* set the attributes of a folder like in the folder window */
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

/* reload the folders order */
void callback_reload_folder_order(void)
{
	folder_load_order();
	callback_refresh_folders();
}

void callback_refresh_folders(void)
{
	main_refresh_folders();
	search_refresh_folders();
	config_refresh_folders();
	filter_update_folder_list();
}

/* the configuration has been changed */
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

/* select an mail */
void callback_select_mail(int num)
{
	main_select_mail(num);
}

/* delete all indexfiles */
void callback_delete_all_indexfiles(void)
{
	app_busy();
	folder_delete_all_indexfiles();
	app_unbusy();
}

/* save all indexfiles */
void callback_save_all_indexfiles(void)
{
	app_busy();
	folder_save_all_indexfiles();
	app_unbusy();
}

/* rescan the current selected folder */
void callback_rescan_folder(void)
{
	struct folder *f = main_get_folder();
	if (f)
	{
		app_busy();
		/* Because this means deleting free all mails we safely remove all found mails as it
     * could reference a old mail */
		search_clear_results();
		folder_rescan(f);
		main_set_folder_mails(f);
		main_refresh_folder(f);
		read_refresh_prevnext_button(f);
		app_unbusy();
	}
}

/** Auto-Timer functions **/

/***************************************************
 That's the function which is calleded every second
****************************************************/
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

	/* Register us again */
	thread_push_function_delayed(1000, callback_timer, 0);
}

/**************************************************
 Resets the auto timer
***************************************************/
void callback_autocheck_reset(void)
{
	autocheck_seconds_start = sm_get_current_seconds();
	main_refresh_window_title(autocheck_seconds_start);
}

/**************************************************
 The entry point
***************************************************/
int simplemail_main(void)
{
	if (!debug_init()) return 0;

#ifdef ENABLE_NLS
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
#endif
	if (!gui_parseargs(0,NULL))
	{
		debug_deinit();
		return 0;
	}

	startupwnd_open();
	if (!init_threads())
	{
		debug_deinit();
		return 0;
	}

	if (codesets_init())
	{
		load_config();
		init_addressbook();
		if (init_folders())
		{
			if (spam_init())
			{
				if (gui_init())
				{
					/* Perform an email check if requested by configuration */
					if (user.config.receive_autoonstartup) autocheck_seconds_start = 0;
					else callback_autocheck_reset();
					callback_timer();

					gui_loop();
				}
				folder_delete_deleted();
#ifdef __AROS__ /*folders doesn't get saved ?*/
			folder_save_order();
#endif
				spam_cleanup();
			}
			del_folders();
		}
		free_config();
		codesets_cleanup();
	}
	cleanup_addressbook();
	if (lazy_thread) lazy_clean_list();
	cleanup_threads();
	/* Lazy thread is already aborted by  above call */
	if (lazy_semaphore) thread_dispose_semaphore(lazy_semaphore);
	startupwnd_close();
	shutdownwnd_close();
	atcleanup_finalize();
	debug_deinit();
	return 0;
}

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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "addressbook.h"
#include "configuration.h"
#include "smintl.h"
#include "filter.h"
#include "folder.h"
#include "mail.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"
#include "trans.h"

#include "addressbookwnd.h"
#include "composewnd.h"
#include "configwnd.h"
#include "filterwnd.h"
#include "folderwnd.h"
#include "gui_main.h"
#include "mainwnd.h"
#include "parse.h"
#include "readwnd.h"
#include "subthreads.h"
#include "support.h"
#include "tcpip.h"

/* the current mail should be viewed, returns the number of the window
	which the function has opened or -1 for an error */
int callback_read_mail(void)
{
	char *filename;
	struct mail *m;
	struct folder *f;
	int num;

	if (!(f = main_get_folder())) return -1;
	if (!(filename = main_get_mail_filename())) return -1;
	if (!(m = main_get_active_mail())) return -1;

	num = read_window_open(main_get_folder_drawer(), m);

	if (num >= 0)
	{
		if (mail_get_status_type(m) == MAIL_STATUS_UNREAD)
		{
			folder_set_mail_status(f,m,MAIL_STATUS_READ | (m->status & (~MAIL_STATUS_MASK)));
			if (m->flags & MAIL_FLAGS_NEW && f->new_mails) f->new_mails--;
			m->flags &= ~MAIL_FLAGS_NEW;
			main_refresh_mail(m);
		}
	}
	return num;
}

/* the selected mails should be deleted */
void callback_delete_mails(void)
{
	struct folder *from_folder = main_get_folder();
	struct mail *mail;
	void *handle;
	int num;
	int permanent; /* 1 if mails should be deleted permanently */

	if (from_folder)
	{
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
			if (num == 1) strcpy(buf,_("Do you really want to delete the selected mail permanently?"));
			else sprintf(buf,_("Do you really want to delete %d mails permanently?"),num);
			if (!sm_request(NULL,buf,_("_Yes|_No"))) return;
			permanent = 1;
		} else permanent = 0;

		mail = main_get_mail_first_selected(&handle);
		while (mail)
		{
			if (permanent) folder_delete_mail(from_folder,mail);
			else folder_move_mail(from_folder,folder_deleted(),mail);
			mail = main_get_mail_next_selected(&handle);
		}
		main_refresh_folder(from_folder);
		if (!permanent) main_refresh_folder(folder_deleted());

		main_remove_mails_selected();
	}
}

/* a single mail of any folder should be deleted */
int callback_delete_mail(struct mail *mail)
{
	struct folder *f = folder_find_by_mail(mail);
	if (f && f != folder_deleted())
	{
		folder_delete_mail(f,mail);
		main_refresh_folder(f);
		main_refresh_folder(folder_deleted());
		if (main_get_folder() == f) main_remove_mail(mail);
		return 1;
	}
	return 0;
}

/* get the address */
void callback_get_address(void)
{
	struct mail *mail = main_get_active_mail();
	if (mail)
	{
		char *addr;
		struct folder *f;
		char *header;

		f = main_get_folder();

		if (folder_get_type(f) == FOLDER_TYPE_SEND) header = "to";
		else header = "from";

		addr = mail_find_header_contents(mail,header);

		if (addr)
		{
			struct mailbox mb;
			if (parse_mailbox(addr,&mb))
			{
				if (!addressbook_get_realname(mb.addr_spec))
				{
					addressbook_open_with_new_address_from_mail_header(mail,header);
				}
			}
		}
	}
}

/* a new mail should be written to the given address */
void callback_write_mail_to(struct addressbook_entry *address)
{
	char *to_str = addressbook_get_address_str(address);
	callback_write_mail_to_str(to_str,NULL);
	free(to_str);
}

/* a new mail should be written to a given address string */
int callback_write_mail_to_str(char *str, char *subject)
{
	struct compose_args ca;
	int win_num;
	memset(&ca,0,sizeof(ca));

	ca.action = COMPOSE_ACTION_NEW;
	ca.to_change = mail_create_for(str,subject);

	win_num = compose_window_open(&ca);

	if (ca.to_change) mail_free(ca.to_change);
	return win_num;
}

/* a new mail should be composed */
void callback_new_mail(void)
{
	struct folder *f = main_get_folder();
	callback_write_mail_to_str(f->def_to,NULL);
}

/* reply this mail */
void callback_reply_this_mail(char *folder_path, int num, struct mail **to_reply_array)
{
	struct mail **mail_array;
	char buf[256];
	int i;

	getcwd(buf, sizeof(buf));
	chdir(folder_path);

	if ((mail_array = malloc(num*sizeof(struct mail *))))
	{
		struct mail *reply;
		int err = 0;

		for (i=0;i<num;i++)
		{
			if ((mail_array[i] = mail_create_from_file(to_reply_array[i]->filename)))
			{
				mail_read_contents("",mail_array[i]);
			} else 
			{
				err = 1;
				break;
			}
		}

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

				mail_free(reply);
			}
		}

		for (i=0;i<num;i++)
		{
			if (mail_array[i]) mail_free(mail_array[i]);
			else break;
		}
		free(mail_array);
	}

	chdir(buf);
}

/* a mail should be replied */
void callback_reply_mail(void)
{
	struct mail *mail;
	struct mail **mail_array;
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

	if ((mail_array = malloc(sizeof(struct mail *)*num)))
	{
		int i = 0;

		mail = main_get_mail_first_selected(&handle);
		while (mail)
		{
			mail_array[i++] = mail;
			mail = main_get_mail_next_selected(&handle);
		}

		callback_reply_this_mail(main_get_folder_drawer(), num, mail_array);
		free(mail_array);
	}
}

/* a mail should be forwarded */
void callback_forward_this_mail(char *folder_path, int num, struct mail **to_forward_array)
{
	struct mail **mail_array;
	char buf[256];
	int i;

	getcwd(buf, sizeof(buf));
	chdir(folder_path);

	if ((mail_array = malloc(num*sizeof(struct mail *))))
	{
		struct mail *forward;
		int err = 0;

		for (i=0;i<num;i++)
		{
			if ((mail_array[i] = mail_create_from_file(to_forward_array[i]->filename)))
			{
				mail_read_contents("",mail_array[i]);
			} else 
			{
				err = 1;
				break;
			}
		}

		if (!err)
		{
			if ((forward = mail_create_forward(num,mail_array)))
			{
				struct compose_args ca;
				memset(&ca,0,sizeof(ca));
				ca.to_change = forward;
				ca.action = COMPOSE_ACTION_FORWARD;
				ca.ref_mail = to_forward_array[0];
				compose_window_open(&ca);

				mail_free(forward);
			}
		}

		for (i=0;i<num;i++)
		{
			if (mail_array[i]) mail_free(mail_array[i]);
			else break;
		}
		free(mail_array);
	}

	chdir(buf);
}

/* a single or multiple mails should be forwarded */
void callback_forward_mail(void)
{
	struct mail *mail;
	struct mail **mail_array;
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

	if ((mail_array = malloc(sizeof(struct mail *)*num)))
	{
		int i = 0;

		mail = main_get_mail_first_selected(&handle);
		while (mail)
		{
			mail_array[i++] = mail;
			mail = main_get_mail_next_selected(&handle);
		}

		callback_forward_this_mail(main_get_folder_drawer(), num, mail_array);
		free(mail_array);
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
			struct compose_args ca;
			mail_read_contents("",mail);
			memset(&ca,0,sizeof(ca));
			ca.to_change = mail;
			ca.action = COMPOSE_ACTION_EDIT;
			compose_window_open(&ca);
			mail_free(mail);
		}

		chdir(buf);
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
	addressbook_open();
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
		main_set_folder_mails(folder);
	}
}

/* a new mail should be added to a given folder */
struct mail *callback_new_mail_to_folder(char *filename, struct folder *folder)
{
	int pos;
	char buf[256];
	struct mail *mail = NULL;

	if (!folder) return NULL;

	getcwd(buf, sizeof(buf));
	chdir(folder->path);

	if (!sm_file_is_in_drawer(filename,folder->path))
	{
		char *newname;

		if ((newname = mail_get_new_name(MAIL_STATUS_UNREAD)))
		{
			myfilecopy(filename,newname);
			mail = mail_create_from_file(newname);
			free(newname);
		}
	} else
	{
		mail = mail_create_from_file(filename);
	}

	if (mail)
	{
		pos = folder_add_mail(folder,mail,1);
		if (main_get_folder() == folder && pos != -1)
			main_insert_mail_pos(mail,pos-1);

		main_refresh_folder(folder);
	}

	chdir(buf);
	return mail;
}

/* a new mail has arrived */
static void callback_new_mail_arrived(struct mail *mail)
{
	struct filter *f;
	int pos;

	pos = folder_add_mail_incoming(mail);
	if (main_get_folder() == folder_incoming() && pos != -1)
	{
		main_insert_mail_pos(mail,pos-1);
	}

	/* This has to be optmized! */
	if ((f = folder_mail_can_be_filtered(folder_incoming(), mail, 1)))
	{
		if (f->use_dest_folder && f->dest_folder)
		{
			struct folder *dest_folder = folder_find_by_name(f->dest_folder);
			if (dest_folder)
			{
				/* very slow, because the sorted array is rebuilded in the both folders! */
				callback_move_mail(mail, folder_incoming(), dest_folder);
			}
		}
	}

	main_refresh_folder(folder_incoming());
}

/* a new mail has been arrived, only the filename is given */
void callback_new_mail_arrived_filename(char *filename)
{
	struct mail *mail;
	char buf[256];

	getcwd(buf, sizeof(buf));
	chdir(folder_incoming()->path);

	if ((mail = mail_create_from_file(filename)))
		callback_new_mail_arrived(mail);

	chdir(buf);
}

/* After downloading this function is called */
void callback_number_of_mails_downloaded(int num)
{
	if (num && user.config.receive_sound)
	{
		sm_play_sound(user.config.receive_sound_file);
	}
}

/* a new mail has been written */
void callback_new_mail_written(struct mail *mail)
{
	folder_add_mail(folder_outgoing(),mail,1);
	if (main_get_folder() == folder_outgoing())
	{
		main_insert_mail(mail);
	}
	main_refresh_folder(folder_outgoing());
}

/* a mail has been send so it can be moved to the "Sent" drawer now */
void callback_mail_has_been_sent(char *filename)
{
	struct filter *f;
	struct folder *out = folder_outgoing();
	struct folder *sent = folder_sent();
	struct mail *m;
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
					/* very slow, because the sorted array is rebuilded in the both folders! */
					callback_move_mail(m, folder_sent(), dest_folder);
				}
			}
		}


	}
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
			main_insert_mail_pos(mail,folder_get_index_of_mail(dest_folder, mail)-1);

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

/* mark/unmark all selected mails */
void callback_mails_mark(int mark)
{
	struct folder *folder = main_get_folder();
	struct mail *mail;
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
	struct mail *mail;
	void *handle;
	if (!folder) return;

	mail = main_get_mail_first_selected(&handle);
	while (mail)
	{
		int new_status = mail->status;

		if (status == MAIL_STATUS_HOLD || status == MAIL_STATUS_WAITSEND)
		{
			if (mail_get_status_type(mail) == MAIL_STATUS_HOLD || mail_get_status_type(mail) == MAIL_STATUS_WAITSEND || mail_get_status_type(mail) == MAIL_STATUS_SENT)
				new_status = status;
		} else
		{
			if (status == MAIL_STATUS_READ || status == MAIL_STATUS_UNREAD)
			{
				if (mail_get_status_type(mail) == MAIL_STATUS_READ || mail_get_status_type(mail) == MAIL_STATUS_UNREAD)
					new_status = status;
			}
		}

		new_status |= mail->status & MAIL_STATUS_FLAG_MARKED;

		if (new_status != mail->status)
		{
			folder_set_mail_status(folder,mail,new_status);
			main_refresh_mail(mail);
		}

		mail = main_get_mail_next_selected(&handle);
	}	
}

/* import a addressbook into SimpleMail, return 1 for success */
int callback_import_addressbook(void)
{
	int rc = 0;
	char *filename;
	
	filename = sm_request_file(_("Select an addressbook-file."), "PROGDIR:");
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
	folder_add_group("New Group");
	main_refresh_folders();
	filter_update_folder_list();
}

/* create a new folder */
void callback_new_folder_path(char *path, char *name)
{
	folder_add_with_name(path, name);
	main_refresh_folders();
	filter_update_folder_list();
}

/* Remove the selected folder */
void callback_remove_folder(void)
{
	struct folder *f = main_get_folder();
	if (f)
	{
		if (folder_remove(f))
		{
			main_refresh_folders();
			filter_update_folder_list();
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

	if (folder_set(f, folder_get_changed_name(), folder_get_changed_path(), folder_get_changed_type(), folder_get_changed_defto()))
	{
		if (main_get_folder() == f || refresh)
		{
			main_set_folder_mails(f);
		}
	}

	main_refresh_folder(f);
	filter_update_folder_list();
}

/* reload the folders order */
void callback_reload_folder_order(void)
{
	folder_load_order();
	main_refresh_folders();
	filter_update_folder_list();
}

/* the configuration has been changed */
void callback_config_changed(void)
{
	/* Build the check single account menu */
	main_build_accounts();
}

static int autocheck_minutes_start; /* to compare with this */

/* initializes the autocheck function */
void callback_autocheck_refresh(void)
{
	autocheck_minutes_start = sm_get_current_seconds();
}

/* called every second */
void callback_timer(void)
{
	if (user.config.receive_autocheck)
	{
		if (sm_get_current_seconds() - autocheck_minutes_start > user.config.receive_autocheck * 60)
		{
			/* nothing should happen when mails_dl() is called twice,
			   this could happen if a mail downloading takes very long */
			callback_autocheck_refresh();

			/* If socket library cannot be opened we also shouldn't try
				 to download the mails */
			if (open_socket_lib())
			{
				close_socket_lib();
				mails_dl(1);
			}
		}
	}
}

/* select an mail */
void callback_select_mail(int num)
{
	main_select_mail(num);
}

/* delete all indexfiles */
void callback_delete_all_indexfiles(void)
{
	folder_delete_all_indexfiles();
}

int simplemail_main(void)
{
#ifdef ENABLE_NLS
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
#endif

	if (!gui_parseargs(0,NULL)) return 0;

	load_config();
	init_addressbook();
	if (init_folders())
	{
		if (init_threads())
		{
			gui_main(0,NULL);
			folder_delete_deleted();
			cleanup_threads();
		}
/*		folder_save_order();*/
		del_folders();
	}
	cleanup_addressbook();
	return 0;
}


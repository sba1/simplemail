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
 * @file trans.c
 */

#include "trans.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "account.h"
#include "addressbook.h"
#include "addresslist.h"
#include "configuration.h"
#include "debug.h"
#include "filter.h"
#include "folder.h"
#include "imap.h"
#include "lists.h"
#include "mail.h"
#include "mainwnd.h"
#include "parse.h"
#include "pop3.h"
#include "smtp.h"
#include "spam.h"
#include "status.h"
#include "support_indep.h"

#include "request.h"
#include "simplemail.h"
#include "subthreads.h"
#include "support.h"

/*****************************************************************************/

static void trans_set_status(const char *str)
{
	thread_call_parent_function_async_string(status_set_status, 1, str);
}

static void trans_set_status_static(const char *str)
{
	thread_call_function_async(thread_get_main(), status_set_status, 1, str);
}

static void trans_set_connect_to_server(const char *server)
{
	thread_call_parent_function_async_string(status_set_connect_to_server, 1, server);
}

static void trans_set_head(const char *head)
{
	thread_call_parent_function_async_string(status_set_head, 1, head);
}

static void trans_set_title_utf8(const char *title)
{
	thread_call_parent_function_async_string(status_set_title_utf8, 1, title);
}

static void trans_set_title(const char *title)
{
	thread_call_parent_function_async_string(status_set_title, 1, title);
}

static void trans_init_gauge_as_bytes(int maximal)
{
	thread_call_function_async(thread_get_main(), status_init_gauge_as_bytes, 1, maximal);
}

static void trans_set_gauge(int value)
{
	thread_call_function_async(thread_get_main(), status_set_gauge, 1, value);
}

static void trans_init_mail(int maximal)
{
	thread_call_function_async(thread_get_main(), status_init_mail, 1, maximal);
}

static void trans_set_mail(int current, int current_size)
{
	thread_call_function_async(thread_get_main(), status_set_mail, current, current_size);
}

static int trans_request_login(char *text, char *login, char *password, int len)
{
	return thread_call_parent_function_sync(NULL,sm_request_login,4,text,login,password,len);
}

static void trans_mail_list_clear(void)
{
	thread_call_function_async(thread_get_main(),status_mail_list_clear,0);
}

static void trans_mail_list_freeze(void)
{
	thread_call_function_async(thread_get_main(),status_mail_list_freeze,0);
}

static void trans_mail_list_thaw(void)
{
	thread_call_function_async(thread_get_main(),status_mail_list_thaw,0);
}

static void trans_mail_list_insert(int mno, int mflags, int msize)
{
	thread_call_function_async(thread_get_main(),status_mail_list_insert,3,mno,mflags,msize);
}

static int trans_mail_list_get_flags(int mno)
{
	return (int)thread_call_parent_function_sync(NULL,status_mail_list_get_flags,1,mno);
}

static void trans_mail_list_set_flags(int mno, int mflags)
{
	thread_call_function_async(thread_get_main(),status_mail_list_set_flags,2,mno,mflags);
}

static void trans_mail_list_set_info(int mno, char *from, char *subject, char *date)
{
	thread_call_parent_function_sync(NULL,status_mail_list_set_info, 4, mno, from, subject, date);
}

static int trans_mail_ignore(struct mail_info *info)
{
	return (int)thread_call_parent_function_sync(NULL,callback_remote_filter_mail,1,info);
}

static int trans_more_statistics(void)
{
	return (int)thread_call_parent_function_sync(NULL, status_more_statistics,0);
}

static void trans_new_mail_arrived_filename(char *filename, int is_spam)
{
	thread_call_parent_function_async_string(callback_new_mail_arrived_filename, 2, filename, is_spam);
}

static int trans_skip_server(void)
{
	return (int)thread_call_parent_function_sync(NULL,status_skipped,0);
}

static void trans_number_of_mails_downloaded(int nummails)
{
	thread_call_function_async(thread_get_main(),callback_number_of_mails_downloaded,1,nummails);
}

static int trans_wait(void (*period_callback)(void *arg), void *arg, int millis)
{
	return thread_call_parent_function_sync_timer_callback(period_callback, arg, millis, status_wait,0);
}

static void trans_mail_has_not_been_sent(char *filename)
{
	thread_call_parent_function_async_string(callback_mail_has_not_been_sent,1,filename);
}

static void trans_mail_has_been_sent(char *filename)
{
	thread_call_parent_function_async_string(callback_mail_has_been_sent,1,filename);
}

static void trans_add_imap_folder(char *user, char *server, char *path)
{
	thread_call_parent_function_sync(NULL,callback_add_imap_folder,3, user, server, path);
}

static void trans_refresh_folders(void)
{
	thread_call_parent_function_sync(NULL,callback_refresh_folders,0);
}

static void trans_new_imap_mail_arrived(char *filename, char *user, char *server, char *path)
{
	thread_call_parent_function_sync(NULL,callback_new_imap_mail_arrived, 4, filename, user, server, path);
}

static void trans_delete_mail_by_uid(char *user, char *server, char *path, unsigned int uid)
{
	thread_call_parent_function_sync(NULL, callback_delete_mail_by_uid, 4, user, server, path, uid);
}

/*****************************************************************************/

struct mails_dl_msg
{
	int called_by_auto;
	int iconified;
	struct account *single_account;
};

/* Entry point of the mail download process */
static int mails_dl_entry(struct mails_dl_msg *msg)
{
	struct account *account;

	struct list pop_list;
	struct list imap_list;
	char *incoming_path;
	char *folder_directory;
	char **white = NULL, **black = NULL;
	int receive_preselection = user.config.receive_preselection;
	int receive_size = user.config.receive_size;
	int has_remote_filter = filter_list_has_remote();
	int called_by_auto;
	int auto_spam;
	int single_account;
	struct pop3_server *pop;
	struct imap_server *imap;

	called_by_auto = msg->called_by_auto;

	/* We can access configuration data here because the parent task is still waiting */

	if (!(incoming_path = mystrdup(folder_incoming()->path)))
		return 0;
	if (!(folder_directory = mystrdup(user.folder_directory)))
	{
		free(incoming_path);
		return 0;
	}

	list_init(&pop_list);
	list_init(&imap_list);

	if (msg->single_account)
	{
		single_account = 1;

		account = msg->single_account;
		if (!account->recv_type)
		{
			if (account->pop && account->pop->name)
			{
					struct pop3_server *pop3 = pop_duplicate(account->pop);
					if (pop3)
					{
						if (!pop3->title) pop3->title = mystrdup(account->account_name);
						list_insert_tail(&pop_list,&pop3->node);
					}
			}
		} else
		{
				if (account->imap && account->imap->name)
				{
					struct imap_server *imap = imap_duplicate(account->imap);
					if (imap)
					{
						if (!imap->title) imap->title = mystrdup(account->account_name);
						list_insert_tail(&imap_list,&imap->node);
					}
				}
		}
	} else
	{
		single_account = 0;
		account = (struct account*)list_first(&user.config.account_list);
		while (account)
		{
			if (!account->recv_type)
			{
				if (account->pop && account->pop->active && account->pop->name)
				{
					struct pop3_server *pop3 = pop_duplicate(account->pop);
					if (pop3)
					{
						if (!pop3->title) pop3->title = mystrdup(account->account_name);
						list_insert_tail(&pop_list,&pop3->node);
					}
				}
			} else
			{
				if (account->imap && account->imap->active && account->imap->name)
				{
					struct imap_server *imap = imap_duplicate(account->imap);
					if (imap)
					{
						if (!imap->title) imap->title = mystrdup(account->account_name);
						list_insert_tail(&imap_list,&imap->node);
					}
				}
			}
			account = (struct account*)node_next(&account->node);
		}
	}

	SM_DEBUGF(10,("Checking %ld pop and %ld imap accounts\n",list_length(&pop_list),list_length(&imap_list)));

	if ((auto_spam = user.config.spam_auto_check))
	{
		int spams = spam_num_of_spam_classified_mails();
		int hams = spam_num_of_ham_classified_mails();

		if ((spams < user.config.min_classified_mails) || (hams < user.config.min_classified_mails)) auto_spam = 0;
		else
		{
			black = array_duplicate(user.config.spam_black_emails);
			if (user.config.spam_addrbook_is_white) white = addressbook_get_array_of_email_addresses();
			white = array_add_array(white,user.config.spam_white_emails);
		}
	}

	if (thread_parent_task_can_contiue())
	{
		struct pop3_dl_options dl_options = {0};
		struct imap_synchronize_options imap_sync_options = {0};

		thread_call_function_async(thread_get_main(),status_init,1,0);
		if (called_by_auto) thread_call_function_async(thread_get_main(),status_open_notactivated,0);
		else thread_call_function_async(thread_get_main(),status_open,0);

		dl_options.pop_list = &pop_list;
		dl_options.dest_dir = incoming_path;
		dl_options.receive_preselection = receive_preselection;
		dl_options.receive_size = receive_size;
		dl_options.has_remote_filter = has_remote_filter;
		dl_options.folder_directory = folder_directory;
		dl_options.auto_spam = auto_spam;
		dl_options.white = white;
		dl_options.black = black;

		dl_options.callbacks.init_mail = trans_init_mail;
		dl_options.callbacks.init_gauge_as_bytes = trans_init_gauge_as_bytes;
		dl_options.callbacks.set_connect_to_server = trans_set_connect_to_server;
		dl_options.callbacks.set_gauge = trans_set_gauge;
		dl_options.callbacks.set_head = trans_set_head;
		dl_options.callbacks.set_mail = trans_set_mail;
		dl_options.callbacks.set_status = trans_set_status;
		dl_options.callbacks.set_status_static = trans_set_status_static;
		dl_options.callbacks.set_title = trans_set_title;
		dl_options.callbacks.set_title_utf8 = trans_set_title_utf8;
		dl_options.callbacks.request_login = trans_request_login;
		dl_options.callbacks.mail_list_clear = trans_mail_list_clear;
		dl_options.callbacks.mail_list_freeze = trans_mail_list_freeze;
		dl_options.callbacks.mail_list_thaw = trans_mail_list_thaw;
		dl_options.callbacks.mail_list_insert = trans_mail_list_insert;
		dl_options.callbacks.mail_list_get_flags = trans_mail_list_get_flags;
		dl_options.callbacks.mail_list_set_flags = trans_mail_list_set_flags;
		dl_options.callbacks.mail_list_set_info = trans_mail_list_set_info;
		dl_options.callbacks.mail_ignore = trans_mail_ignore;
		dl_options.callbacks.more_statitics = trans_more_statistics;
		dl_options.callbacks.new_mail_arrived_filename = trans_new_mail_arrived_filename;
		dl_options.callbacks.skip_server = trans_skip_server;
		dl_options.callbacks.number_of_mails_downloaded = trans_number_of_mails_downloaded;
		dl_options.callbacks.wait = trans_wait;

		imap_sync_options.imap_list = &imap_list;
		imap_sync_options.quiet = called_by_auto;
		imap_sync_options.callbacks.set_connect_to_server = trans_set_connect_to_server;
		imap_sync_options.callbacks.set_title = trans_set_title;
		imap_sync_options.callbacks.set_title_utf8 = trans_set_title_utf8;
		imap_sync_options.callbacks.set_status = trans_set_status;
		imap_sync_options.callbacks.set_status_static = trans_set_status_static;
		imap_sync_options.callbacks.set_head = trans_set_head;
		imap_sync_options.callbacks.request_login = trans_request_login;
		imap_sync_options.callbacks.add_imap_folder = trans_add_imap_folder;
		imap_sync_options.callbacks.refresh_folders = trans_refresh_folders;
		imap_sync_options.callbacks.init_gauge_as_bytes = trans_init_gauge_as_bytes;
		imap_sync_options.callbacks.set_gauge = trans_set_gauge;
		imap_sync_options.callbacks.new_mail_arrived = trans_new_imap_mail_arrived;
		imap_sync_options.callbacks.delete_mail_by_uid = trans_delete_mail_by_uid;

		if (pop3_really_dl(&dl_options))
		{
			imap_synchronize_really(&imap_sync_options);
		} else if (single_account)
		{
			imap_synchronize_really(&imap_sync_options);
		}

		thread_call_function_async(thread_get_main(),status_close,0);
	}

	array_free(black);
	array_free(white);

	free(incoming_path);
	free(folder_directory);

	while ((pop = (struct pop3_server*)list_remove_tail(&pop_list)))
		pop_free(pop);
	while ((imap = (struct imap_server*)list_remove_tail(&imap_list)))
		imap_free(imap);

	return 1;
}

/*****************************************************************************/

int mails_dl(int called_by_auto)
{
	struct mails_dl_msg msg = {0};
	msg.called_by_auto = called_by_auto;
	msg.iconified = main_is_iconified();
	return thread_start(THREAD_FUNCTION(&mails_dl_entry),&msg);
}

/*****************************************************************************/

int mails_dl_single_account(struct account *ac)
{
	struct mails_dl_msg msg = {0};
	msg.single_account = ac;
	return thread_start(THREAD_FUNCTION(&mails_dl_entry),&msg);
}

/*****************************************************************************/

struct mails_upload_entry_msg
{
	struct outmail **out_array;
	struct list *account_list;
	char *folder_path;
};

/**
 * Entry point for the send mail process
 * @param msg
 * @return
 */
static int mails_upload_entry(struct mails_upload_entry_msg *msg)
{
	struct list copy_of_account_list;
	struct account *account;
	struct outmail **outmail;
	char path[256];
	char *outgoing_folder_path;

	list_init(&copy_of_account_list);

	for (account = (struct account*)list_first(msg->account_list);account;account = (struct account*)node_next(&account->node))
	{
		struct account *new_account;
		if (!account->smtp || !account->smtp->name) continue;

		new_account = account_duplicate(account);
		if (new_account) list_insert_tail(&copy_of_account_list,&new_account->node);
	}

	if (!(outmail = duplicate_outmail_array(msg->out_array)))
		goto out;
	if (!((outgoing_folder_path = mystrdup(msg->folder_path))))
		goto out;

	if (getcwd(path, sizeof(path)))
	{
		if (chdir(outgoing_folder_path) == 0)
		{
			if (thread_parent_task_can_contiue())
			{
				struct smtp_send_options smtp_send_options = {0};

				smtp_send_options.account_list = &copy_of_account_list;
				smtp_send_options.outmail = outmail;
				smtp_send_options.callbacks.set_status_static = trans_set_status_static;
				smtp_send_options.callbacks.set_connect_to_server = trans_set_connect_to_server;
				smtp_send_options.callbacks.set_head = trans_set_head;
				smtp_send_options.callbacks.set_title = trans_set_title;
				smtp_send_options.callbacks.set_title_utf8 = trans_set_title_utf8;
				smtp_send_options.callbacks.skip_server = trans_skip_server;
				smtp_send_options.callbacks.init_mail = trans_init_mail;
				smtp_send_options.callbacks.set_mail = trans_set_mail;
				smtp_send_options.callbacks.init_gauge_as_bytes = trans_init_gauge_as_bytes;
				smtp_send_options.callbacks.set_gauge = trans_set_gauge;
				smtp_send_options.callbacks.mail_has_not_been_sent = trans_mail_has_not_been_sent;
				smtp_send_options.callbacks.mail_has_been_sent = trans_mail_has_been_sent;

				thread_call_function_async(thread_get_main(),status_init,1,0);
				thread_call_function_async(thread_get_main(),status_open,0);
				smtp_send_really(&smtp_send_options);
				thread_call_function_async(thread_get_main(),status_close,0);
			}

			chdir(path);
		}
	}
out:
	return 0;
}
/**
 * Send the mails. Starts a subthread.
 *
 * @param send_options options for sending.
 * @return 0 for failure, 1 for success.
 */
static int mails_upload_async(struct mails_upload_entry_msg *msg)
{
	return thread_start(THREAD_FUNCTION(mails_upload_entry),msg);
}

/*****************************************************************************/

int mails_upload(void)
{
	void *handle = NULL; /* folder_next_mail() */
	struct folder *out_folder = folder_outgoing();
	struct outmail **out_array;
	struct mail_info *m_iter;
	int i,num_mails;
	char path[512];

	struct mails_upload_entry_msg msg = {0};

	/* count the number of mails which could be be sent */
	num_mails = 0;
	while ((m_iter = folder_next_mail(out_folder, &handle)))
	{
		/* Only waitsend mails should be counted */
		if (mail_get_status_type(m_iter) == MAIL_STATUS_WAITSEND) num_mails++;
	}
	if (!num_mails) return 0;
	if (!(out_array = create_outmail_array(num_mails))) return 0;

	handle = NULL; /* for folder_next_mail() */
	i=0; /* the current mail no */

	getcwd(path, sizeof(path));
	if (chdir(out_folder->path) == -1)
	{
		free_outmail_array(out_array);
		return 0;
	}

	/* initialize the arrays */
	while ((m_iter = folder_next_mail(out_folder, &handle)))
	{
		struct mail_complete *m;
		char *from, *to, *cc, *bcc;
		struct mailbox mb;
		struct address_list *list; /* "To" address list */
		struct outmail *out;

		if (mail_get_status_type(m_iter) != MAIL_STATUS_WAITSEND) continue;

		if (!(m = mail_complete_create_from_file(m_iter->filename)))
		{
			free_outmail_array(out_array);
			chdir(path);
			return 0;
		}

		out = out_array[i++];
		to = mail_find_header_contents(m,"To");
		cc = mail_find_header_contents(m,"CC");
		bcc = mail_find_header_contents(m,"BCC");
		from = mail_find_header_contents(m,"From");

		memset(&mb,0,sizeof(struct mailbox));

		if (!to || !from)
		{
			free_outmail_array(out_array);
			mail_complete_free(m);
			chdir(path);
			return 0;
		}
		if (!parse_mailbox(from,&mb))
		{
			tell_str("No valid sender address!");
			free_outmail_array(out_array);
			mail_complete_free(m);
			chdir(path);
			return 0;
		}

		out->size = m->info->size;
		out->mailfile = mystrdup(m->info->filename); /* will be freed in free_outmail_array() */
		out->from = mb.addr_spec; /* must be not freed here */

		/* fill in the recipients */
		if ((list = address_list_create(to)))
		{
			int length = list_length(&list->list);
			if (length)
			{
				struct address_list *cc_list = address_list_create(cc);
				struct address_list *bcc_list = address_list_create(bcc);
				if (cc_list) length += list_length(&cc_list->list);
				if (bcc_list) length += list_length(&bcc_list->list);

				if ((out->rcp = (char**)malloc(sizeof(char*)*(length+1)))) /* not freed */
				{
					struct address *addr = (struct address*)list_first(&list->list);
					int i=0;
					while (addr)
					{
						if (!(out->rcp[i++] = mystrdup(addr->email))) /* not freed */
							break;
						addr = (struct address*)node_next(&addr->node);
					}

					if (cc_list)
					{
						addr = (struct address*)list_first(&cc_list->list);
						while (addr)
						{
							if (!(out->rcp[i++] = mystrdup(addr->email))) /* not freed */
								break;
							addr = (struct address*)node_next(&addr->node);
						}
					}

					if (bcc_list)
					{
						addr = (struct address*)list_first(&bcc_list->list);
						while (addr)
						{
							if (!(out->rcp[i++] = mystrdup(addr->email))) /* not freed */
								break;
							addr = (struct address*)node_next(&addr->node);
						}
					}
					out->rcp[i] = NULL;
				}
				if (cc_list) address_list_free(cc_list);
				if (bcc_list) address_list_free(bcc_list);
			}
			address_list_free(list);
		}

		if (mb.phrase) free(mb.phrase); /* phrase is not necessary */
/*		if (mb.addr_spec) free(mb.addr_spec); */
		mail_complete_free(m);
	}

	chdir(path);
	
	/* now send all mails */
	msg.folder_path = out_folder->path;
	msg.out_array = out_array;
	mails_upload_async(&msg);

	free_outmail_array(out_array);
	return 1;
}

/*****************************************************************************/

int mails_upload_single(struct mail_info *mi)
{
	char *from, *to, *cc, *bcc;
	struct outmail **out_array;
	struct mail_complete *m;
	struct mailbox mb;
	struct address_list *list; /* "To" address list */
	struct folder *out_folder = folder_outgoing();

	struct mails_upload_entry_msg msg = {0};

	if (!mi) return 0;
	if (!(out_array = create_outmail_array(1))) return 0;
	if (!(m = mail_complete_create_from_file(mi->filename)))
	{
		free_outmail_array(out_array);
		return 0;
	}

	to = mail_find_header_contents(m,"To");
	cc = mail_find_header_contents(m,"CC");
	bcc = mail_find_header_contents(m,"BCC");
	from = mail_find_header_contents(m,"From");

	memset(&mb,0,sizeof(struct mailbox));

	if (!to || !from)
	{
		free_outmail_array(out_array);
		return 0;
	}
	if (!parse_mailbox(from,&mb))
	{
		tell_str("No valid sender address!");
		free_outmail_array(out_array);
		return 0;
	}

	out_array[0]->size = m->info->size;
	out_array[0]->mailfile = mystrdup(m->info->filename);
	out_array[0]->from = mb.addr_spec;

	/* fill in the recipients */
	if ((list = address_list_create(to)))
	{
		int length = list_length(&list->list);
		if (length)
		{
			struct address_list *cc_list = address_list_create(cc);
			struct address_list *bcc_list = address_list_create(bcc);
			if (cc_list) length += list_length(&cc_list->list);
			if (bcc_list) length += list_length(&bcc_list->list);

			if ((out_array[0]->rcp = (char**)malloc(sizeof(char*)*(length+1)))) /* not freed */
			{
				struct address *addr = (struct address*)list_first(&list->list);
				int i=0;
				while (addr)
				{
					if (!(out_array[0]->rcp[i++] = mystrdup(addr->email))) /* not freed */
						break;
					addr = (struct address*)node_next(&addr->node);
				}

				if (cc_list)
				{
					addr = (struct address*)list_first(&cc_list->list);
					while (addr)
					{
						if (!(out_array[0]->rcp[i++] = mystrdup(addr->email))) /* not freed */
							break;
						addr = (struct address*)node_next(&addr->node);
					}
				}

				if (bcc_list)
				{
					addr = (struct address*)list_first(&bcc_list->list);
					while (addr)
					{
						if (!(out_array[0]->rcp[i++] = mystrdup(addr->email))) /* not freed */
							break;
						addr = (struct address*)node_next(&addr->node);
					}
				}

				out_array[0]->rcp[i] = NULL;
			}
			if (cc_list) address_list_free(cc_list);
			if (bcc_list) address_list_free(bcc_list);
		}
		address_list_free(list);
	}

	if (mb.phrase) free(mb.phrase); /* phrase is not necessary */
/*		if (mb.addr_spec) free(mb.addr_spec); */

	/* Send the mail now */
	msg.folder_path = out_folder->path;
	msg.account_list = &user.config.account_list;
	msg.out_array = out_array;
	mails_upload_async(&msg);

	free_outmail_array(out_array);
	mail_complete_free(m);
	return 1;
}

/*****************************************************************************/

static thread_t test_account_thread;

static int mails_test_account_entry(void *udata)
{
	struct account *ac = account_duplicate((struct account*)udata);
	if (!ac) return 0;
	if (!thread_parent_task_can_contiue())
		goto bailout;

bailout:
	if (ac) account_free(ac);

	/* It is okay if the thread is shortly yielded after setting this to NULL */
	test_account_thread = NULL;
	return 0; /* Return value not really relevant */
}

int mails_test_account(struct account *ac)
{
	if (test_account_thread) return 0;

	if (!(test_account_thread = thread_add("SimpleMail - Test Account", mails_test_account_entry, ac)))
		return 0;
	return 1;
}

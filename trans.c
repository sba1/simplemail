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
** trans.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "account.h"
#include "addressbook.h"
#include "configuration.h"
#include "debug.h"
#include "filter.h"
#include "folder.h"
#include "imap.h"
#include "parse.h"
#include "pop3.h"
#include "simplemail.h"
#include "smtp.h"
#include "spam.h"
#include "support_indep.h"
#include "status.h"
#include "trans.h"
#include "tcp.h"

#include "dlwnd.h"
#include "subthreads.h"
#include "support.h"
#include "upwnd.h"

struct mails_dl_msg
{
	int called_by_auto;
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
		thread_call_parent_function_async(status_init,1,0);
		if (called_by_auto) thread_call_parent_function_async(status_open_notactivated,0);
		else thread_call_parent_function_async(status_open,0);

		if (pop3_really_dl(&pop_list, incoming_path, receive_preselection, receive_size, has_remote_filter, folder_directory, auto_spam, white, black))
		{
			imap_synchronize_really(&imap_list, called_by_auto);
		} else if (single_account)
		{
			imap_synchronize_really(&imap_list, called_by_auto);
		}

		thread_call_parent_function_async(status_close,0);
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

int mails_dl(int called_by_auto)
{
	struct mails_dl_msg msg;
	msg.called_by_auto = called_by_auto;
	msg.single_account = NULL;
	return thread_start(THREAD_FUNCTION(&mails_dl_entry),&msg);
}

int mails_dl_single_account(struct account *ac)
{
	struct mails_dl_msg msg;
	msg.called_by_auto = 0;
	msg.single_account = ac;
	return thread_start(THREAD_FUNCTION(&mails_dl_entry),&msg);
}

int mails_upload(void)
{
	void *handle = NULL; /* folder_next_mail() */
	struct folder *out_folder = folder_outgoing();
	struct outmail **out_array;
	struct mail_info *m_iter;
	int i,num_mails;
	char path[512];

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
		char *from, *to, *cc;
		struct mailbox mb;
		struct list *list; /* "To" address list */
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
		if ((list = create_address_list(to)))
		{
			int length = list_length(list);
			if (length)
			{
				struct list *cc_list = create_address_list(cc);
				if (cc_list) length += list_length(cc_list);

				if ((out->rcp = (char**)malloc(sizeof(char*)*(length+1)))) /* not freed */
				{
					struct address *addr = (struct address*)list_first(list);
					int i=0;
					while (addr)
					{
						if (!(out->rcp[i++] = mystrdup(addr->email))) /* not freed */
							break;
						addr = (struct address*)node_next(&addr->node);
					}

					if (cc_list)
					{
						addr = (struct address*)list_first(cc_list);
						while (addr)
						{
							if (!(out->rcp[i++] = mystrdup(addr->email))) /* not freed */
								break;
							addr = (struct address*)node_next(&addr->node);
						}
					}
					out->rcp[i] = NULL;
				}
				if (cc_list) free_address_list(cc_list);
			}
			free_address_list(list);
		}

		if (mb.phrase) free(mb.phrase); /* phrase is not necessary */
/*		if (mb.addr_spec) free(mb.addr_spec); */
		mail_complete_free(m);
	}

	chdir(path);
	
	/* now send all mails */
	smtp_send(&user.config.account_list,out_array,out_folder->path);

	free_outmail_array(out_array);
	return 1;
}

/* Note: the mail must contain all headers in the header list */
int mails_upload_signle(struct mail_info *m)
{
#pragma TODO: implement me
#if 0
	char *from, *to, *cc;
	struct outmail **out_array;
	struct mailbox mb;
	struct list *list; /* "To" address list */
	struct folder *out_folder = folder_outgoing();

	if (!m) return 0;
	if (!(out_array = create_outmail_array(1))) return 0;

	to = mail_find_header_contents(m,"To");
	cc = mail_find_header_contents(m,"CC");
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
	if ((list = create_address_list(to)))
	{
		int length = list_length(list);
		if (length)
		{
			struct list *cc_list = create_address_list(cc);
			if (cc_list) length += list_length(cc_list);

			if ((out_array[0]->rcp = (char**)malloc(sizeof(char*)*(length+1)))) /* not freed */
			{
				struct address *addr = (struct address*)list_first(list);
				int i=0;
				while (addr)
				{
					if (!(out_array[0]->rcp[i++] = mystrdup(addr->email))) /* not freed */
						break;
					addr = (struct address*)node_next(&addr->node);
				}

				if (cc_list)
				{
					addr = (struct address*)list_first(cc_list);
					while (addr)
					{
						if (!(out_array[0]->rcp[i++] = mystrdup(addr->email))) /* not freed */
							break;
						addr = (struct address*)node_next(&addr->node);
					}
				}

				out_array[0]->rcp[i] = NULL;
			}
			if (cc_list) free_address_list(cc_list);
		}
		free_address_list(list);
	}

	if (mb.phrase) free(mb.phrase); /* phrase is not necessary */
/*		if (mb.addr_spec) free(mb.addr_spec); */

	/* Send the mail now */
	smtp_send(&user.config.account_list,out_array,out_folder->path);

	free_outmail_array(out_array);
	return 1;
#else
	return 0;
#endif
}



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

#include "subthreads.h"
#include "status.h"

#include "account.h"
#include "configuration.h"
#include "dlwnd.h"
#include "folder.h"
#include "imap.h"
#include "parse.h"
#include "pop3.h"
#include "simplemail.h"
#include "smtp.h"
#include "support.h"
#include "support_indep.h"
#include "tcp.h"
#include "upwnd.h"

static int mails_dl_entry(int called_by_auto)
{
	struct account *account;

	struct list pop_list;
	struct list imap_list;
	char *incoming_path;
	char *folder_directory;
	int receive_preselection = user.config.receive_preselection;
	int receive_size = user.config.receive_size;
	struct pop3_server *pop;
	struct imap_server *imap;

	if (!(incoming_path = mystrdup(folder_incoming()->path)))
		return 0;
	if (!(folder_directory = mystrdup(user.folder_directory)))
	{
		free(incoming_path);
		return 0;
	}

	list_init(&pop_list);
	list_init(&imap_list);

	account = (struct account*)list_first(&user.config.account_list);
	while (account)
	{
		if (!account->recv_type)
		{
			if (account->pop && account->pop->active && account->pop->name)
			{
				struct pop3_server *pop3 = pop_duplicate(account->pop);
				if (pop3) list_insert_tail(&pop_list,&pop3->node);
			}
		} else
		{
			if (account->imap && account->imap->active && account->imap->name)
			{
				struct imap_server *imap = imap_duplicate(account->imap);
				if (imap) list_insert_tail(&imap_list,&imap->node);
			}
		}
		account = (struct account*)node_next(&account->node);
	}

	if (thread_parent_task_can_contiue())
	{
		thread_call_parent_function_async(status_init,1,0);
		if (called_by_auto) thread_call_parent_function_async(status_open_notactivated,0);
		else thread_call_parent_function_async(status_open,0);

		if (pop3_really_dl(&pop_list, incoming_path, receive_preselection, receive_size, folder_directory))
		{
			imap_synchronize_really(&imap_list, called_by_auto);
		}

		thread_call_parent_function_async(status_close,0);
	}

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
	return thread_start(THREAD_FUNCTION(&mails_dl_entry),(void*)called_by_auto);
}

int mails_dl_single_account(struct account *ac)
{
	struct list pop_list;
	if (!ac) return 0;

	list_init(&pop_list);

	if (!ac->recv_type)
	{
		list_insert_tail(&pop_list,&ac->pop->node);
		pop3_dl(&pop_list,folder_incoming()->path,user.config.receive_preselection,user.config.receive_size,0);
	} else
	{
		list_insert_tail(&pop_list,&ac->imap->node);
		imap_synchronize(&pop_list,0);
	}
	return 0;
}

int mails_upload(void)
{
	void *handle = NULL; /* folder_next_mail() */
	struct folder *out_folder = folder_outgoing();
	struct outmail **out_array;
	struct mail *m_iter;
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
		struct mail *m;
		char *from, *to, *cc;
		struct mailbox mb;
		struct list *list; /* "To" address list */
		struct outmail *out;

		if (mail_get_status_type(m_iter) != MAIL_STATUS_WAITSEND) continue;

		if (!(m = mail_create_from_file(m_iter->filename)))
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
			mail_free(m);
			chdir(path);
			return 0;
		}
		if (!parse_mailbox(from,&mb))
		{
			tell_str("No valid sender address!");
			free_outmail_array(out_array);
			mail_free(m);
			chdir(path);
			return 0;
		}

		out->size = m->size;
		out->mailfile = mystrdup(m->filename); /* will be freed in free_outmail_array() */
		out->from = mb.addr_spec; /* must be not freed here */

		/* fill in the recipients */
		if ((list = create_address_list(to)))
		{
			int length = list_length(list);
			if (length)
			{
				struct list *cc_list = create_address_list(cc);
				if (cc_list) length += list_length(list);

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
		mail_free(m);
	}

	chdir(path);
	
	/* now send all mails */
	smtp_send(&user.config.account_list,out_array,out_folder->path);

	free_outmail_array(out_array);
	return 1;
}

/* Note: the mail must contain all headers in the header list */
int mails_upload_signle(struct mail *m)
{
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

	out_array[0]->mailfile = mystrdup(m->filename);
	out_array[0]->from = mb.addr_spec;

	/* fill in the recipients */
	if ((list = create_address_list(to)))
	{
		int length = list_length(list);
		if (length)
		{
			struct list *cc_list = create_address_list(cc);
			if (cc_list) length += list_length(list);

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
}



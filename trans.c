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

#include "account.h"
#include "configuration.h"
#include "dlwnd.h"
#include "folder.h"
#include "parse.h"
#include "pop3.h"
#include "simplemail.h"
#include "smtp.h"
#include "support.h"
#include "support_indep.h"
#include "tcp.h"
#include "upwnd.h"

int mails_dl(void)
{
	struct list pop_list;
	struct account *account;

	/* build up the pop list, we take the pop nodes it self,		*
	 * they are not used anywhere else (the pop function should *
   * be modified to use the account list 										*/
	list_init(&pop_list);

	account = (struct account*)list_first(&user.config.account_list);
	while (account)
	{
		if (account->pop)
			list_insert_tail(&pop_list,&account->pop->node);
		account = (struct account*)node_next(&account->node);
	}

	dl_window_open();

	if (!pop3_dl(&pop_list,folder_incoming()->path,user.config.receive_preselection,user.config.receive_size))
	{
		dl_window_close();
	}
	return 0;
}

int mails_upload(void)
{
	void *handle = NULL; /* folder_next_mail() */
	struct folder *out_folder = folder_outgoing();
	struct outmail **out_array;
	struct mail *m;
	int i,num_mails;

  /* count the number of mails which could be be sent */
	num_mails = 0;
	while ((m = folder_next_mail(out_folder, &handle)))
	{
		/* Only waitsend mails should be counted */
		if (mail_get_status_type(m) == MAIL_STATUS_WAITSEND) num_mails++;
	}
	if (!num_mails) return 0;
  if (!(out_array = create_outmail_array(num_mails))) return 0;

	handle = NULL; /* for folder_next_mail() */
	i=0; /* the current mail no */

	/* initialize the arrays */
	while ((m = folder_next_mail(out_folder, &handle)))
	{
		char *from, *to;
		struct mailbox mb;
		struct list *list; /* "To" address list */
		struct outmail *out;

		if (mail_get_status_type(m) != MAIL_STATUS_WAITSEND) continue;

		out = out_array[i++];
		to = mail_find_header_contents(m,"To");
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

		out->mailfile = mystrdup(m->filename); /* will be freed in free_outmail_array() */
		out->from = mb.addr_spec; /* must be not freed here */

		/* fill in the recipients */
		if ((list = create_address_list(to)))
		{
			int length = list_length(list);
			if (length)
			{
				if ((out->rcp = (char**)malloc(sizeof(char*)*(length+1)))) /* not freed */
				{
					struct address *addr = (struct address*)list_first(list);
					int i=0;
					while (addr)
					{
						if (!(out->rcp[i++] = strdup(addr->email))) /* not freed */
							break;
						addr = (struct address*)node_next(&addr->node);
					}
					out->rcp[i] = NULL;
				}
			}
			free_address_list(list);
		}

		if (mb.phrase) free(mb.phrase); /* phrase is not necessary */
/*		if (mb.addr_spec) free(mb.addr_spec); */
	}

/*	up_set_title(server.name); */
	up_window_open();

	/* now send all mails */
	if (!(smtp_send(&user.config.account_list,out_array,out_folder->path)))
	{
		up_window_close();
	}

	free_outmail_array(out_array);
	return 1;
}


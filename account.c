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
** account.c
*/

#include <stdlib.h>
#include <string.h>

#include "account.h"
#include "configuration.h"
#include "folder.h"
#include "parse.h"
#include "imap.h"
#include "pop3.h"
#include "smtp.h"
#include "support_indep.h"

/**************************************************************************
 Allocates a new account
**************************************************************************/
struct account *account_malloc(void)
{
	struct pop3_server *pop;

	if ((pop = pop_malloc()))
	{
		struct smtp_server *smtp;
		if ((smtp = smtp_malloc()))
		{
			struct imap_server *imap;
			if ((imap = imap_malloc()))
			{
				struct account *account;
				if ((account = (struct account*)malloc(sizeof(struct account))))
				{
					memset(account,0,sizeof(struct account));
					account->pop = pop;
					account->smtp = smtp;
					account->imap = imap;
					return account;
				}
				imap_free(imap);
			}
			smtp_free(smtp);
		}
		pop_free(pop);
	}
	return NULL;
}

/**************************************************************************
 Duplicates an account
**************************************************************************/
struct account *account_duplicate(struct account *a)
{
	struct pop3_server *pop;

	if ((pop = pop_duplicate(a->pop)))
	{
		struct smtp_server *smtp;
		if ((smtp = smtp_duplicate(a->smtp)))
		{
			struct imap_server *imap;
			if ((imap = imap_duplicate(a->imap)))
			{
				struct account *account = (struct account*)malloc(sizeof(struct account));
				if (account)
				{
					account->account_name = mystrdup(a->account_name);
					account->name = mystrdup(a->name);
					account->email = mystrdup(a->email);
					account->reply = mystrdup(a->reply);
					account->def_signature = a->def_signature;
					account->recv_type = a->recv_type;
					account->pop = pop;
					account->smtp = smtp;
					account->imap = imap;
					return account;
				}
				imap_free(imap);
			}
			smtp_free(smtp);
		}
		pop_free(pop);
	}
	return NULL;
}

/**************************************************************************
 Frees an account
**************************************************************************/
void account_free(struct account *a)
{
	if (a->account_name) free(a->account_name);
	if (a->name) free(a->name);
	if (a->email) free(a->email);
	if (a->reply) free(a->reply);

	if (a->pop) pop_free(a->pop);
	if (a->smtp) smtp_free(a->smtp);
	if (a->imap) imap_free(a->imap);

	free(a);
}


/**************************************************************************
 Find an account by e-mail address
**************************************************************************/
struct account *account_find_by_from(char *from)
{
	struct account *ac = (struct account*)list_first(&user.config.account_list);

	struct parse_address addr;

	if (!from) return NULL;
	if (!(parse_address(from, &addr))) return NULL;

	while (ac)
	{
		struct mailbox *mb = (struct mailbox*)list_first(&addr.mailbox_list);
		while (mb)
		{
			if (!mystricmp(mb->addr_spec,ac->email)) return ac;
			mb = (struct mailbox*)node_next(&mb->node);
		}
		ac = (struct account*)node_next(&ac->node);
	}

	free_address(&addr);

	return NULL;
}

/**************************************************************************
 
**************************************************************************/
struct imap_server *account_find_imap_server_by_folder(struct folder *f)
{
	struct account *account;

	if (!f->is_imap) return NULL;

	account = (struct account*)list_first(&user.config.account_list);
	while (account)
	{
		if (account->recv_type && account->imap) /* imap */
		{
			if (!mystricmp(f->imap_server,account->imap->name) && !mystricmp(f->imap_user,account->imap->login))
			{
				return account->imap;
			}
		}
		account = (struct account*)node_next(&account->node);
	}
	return NULL;
}


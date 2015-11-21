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
 * @file account.c
 */

#include "account.h"

#include <stdlib.h>
#include <string.h>

#include "configuration.h"
#include "folder.h"
#include "parse.h"
#include "support_indep.h"

/*****************************************************************************/

struct account *account_malloc(void)
{
	struct pop3_server *pop = NULL;
	struct smtp_server *smtp = NULL;
	struct imap_server *imap = NULL;
	struct account *account = NULL;

	if (!(pop = pop_malloc()))
		goto bailout;

	if (!(smtp = smtp_malloc()))
		goto bailout;

	if (!(imap = imap_malloc()))
		goto bailout;

	if (!(account = (struct account*)malloc(sizeof(struct account))))
		goto bailout;

	memset(account,0,sizeof(struct account));
	account->pop = pop;
	account->smtp = smtp;
	account->imap = imap;
	return account;

bailout:
	if (imap) imap_free(imap);
	if (smtp) smtp_free(smtp);
	if (pop) pop_free(pop);
	return NULL;
}

/*****************************************************************************/

struct account *account_duplicate(struct account *a)
{
	struct pop3_server *pop = NULL;
	struct smtp_server *smtp = NULL;
	struct imap_server *imap = NULL;
	struct account *account;

	if (!(pop = pop_duplicate(a->pop)))
		goto bailout;

	if (!(smtp = smtp_duplicate(a->smtp)))
		goto bailout;

	if (!(imap = imap_duplicate(a->imap)))
		goto bailout;

	if (!(account = (struct account*)malloc(sizeof(struct account))))
		goto bailout;

	account->account_name = mystrdup(a->account_name);
	account->name = mystrdup(a->name);
	account->email = mystrdup(a->email);
	account->reply = mystrdup(a->reply);
	account->def_signature = mystrdup(a->def_signature);
	account->recv_type = a->recv_type;
	account->pop = pop;
	account->smtp = smtp;
	account->imap = imap;
	return account;
bailout:
	if (imap) imap_free(imap);
	if (smtp) smtp_free(smtp);
	if (pop) pop_free(pop);
	return NULL;
}

/*****************************************************************************/

void account_free(struct account *a)
{
	free(a->account_name);
	free(a->name);
	free(a->email);
	free(a->reply);
	free(a->def_signature);

	if (a->pop) pop_free(a->pop);
	if (a->smtp) smtp_free(a->smtp);
	if (a->imap) imap_free(a->imap);

	free(a);
}

/*****************************************************************************/

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
			if (!mystricmp(mb->addr_spec,ac->email))
			{
				free_address(&addr);
				return ac;
			}
			mb = (struct mailbox*)node_next(&mb->node);
		}
		ac = (struct account*)node_next(&ac->node);
	}

	free_address(&addr);

	return NULL;
}

/*****************************************************************************/

struct imap_server *account_find_imap_server_by_folder(struct folder *f)
{
	struct account *account;

	if (!f) return NULL;
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

/*****************************************************************************/

int account_is_server_trustworthy(const char *server_name, const char *fingerprint)
{
	struct account *account;

	account = (struct account*)list_first(&user.config.account_list);
	while (account)
	{
		if (mystrlen(account->smtp->fingerprint) && !mystricmp(account->smtp->name, server_name) && !mystricmp(account->smtp->fingerprint, fingerprint))
			return 1;

		if (mystrlen(account->pop->fingerprint) && !mystricmp(account->pop->name, server_name) && !mystricmp(account->pop->fingerprint, fingerprint))
			return 1;

		if (mystrlen(account->imap->fingerprint) && !mystricmp(account->imap->name, server_name) && !mystricmp(account->imap->fingerprint, fingerprint))
			return 1;

		account = (struct account*)node_next(&account->node);
	}
	return 0;
}

/*****************************************************************************/

void account_trust_server(const char *server_name, const char *fingerprint)
{
	struct account *account;

	account = (struct account*)list_first(&user.config.account_list);
	while (account)
	{
		if (!mystricmp(account->smtp->name, server_name))
		{
			free(account->smtp->fingerprint);
			account->smtp->fingerprint = mystrdup(fingerprint);
		}

		if (!mystricmp(account->pop->name, server_name))
		{
			free(account->pop->fingerprint);
			account->pop->fingerprint = mystrdup(fingerprint);
		}

		if (!mystricmp(account->imap->name, server_name))
		{
			free(account->imap->fingerprint);
			account->imap->fingerprint = mystrdup(fingerprint);
		}

		account = (struct account*)node_next(&account->node);
	}
}

/*****************************************************************************/

int account_is_imap(struct account *a)
{
	return a->recv_type == 1;
}

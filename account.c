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
			struct account *account = (struct account*)malloc(sizeof(struct account));
			if (account)
			{
				memset(account,0,sizeof(struct account));
				account->pop = pop;
				account->smtp = smtp;
				return account;
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
			struct account *account = (struct account*)malloc(sizeof(struct account));
			if (account)
			{
				account->account_name = mystrdup(a->account_name);
				account->name = mystrdup(a->name);
				account->email = mystrdup(a->email);
				account->reply = mystrdup(a->reply);
				account->pop = pop;
				account->smtp = smtp;
				return account;
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

	free(a);
}


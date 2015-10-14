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
 * @file phrase.c
 */

#include "phrase.h"

#include <stdlib.h>
#include <string.h>

#include "configuration.h"
#include "support_indep.h"

/*****************************************************************************/

struct phrase *phrase_malloc(void)
{
	struct phrase *sig;

	if ((sig = (struct phrase*)malloc(sizeof(struct phrase))))
	{
		memset(sig,0,sizeof(struct phrase));
	}
	return sig;
}

/*****************************************************************************/

struct phrase *phrase_duplicate(struct phrase *p)
{
	struct phrase *np = phrase_malloc();
	if (np && p)
	{
		np->addresses = mystrdup(p->addresses);
		np->write_welcome = mystrdup(p->write_welcome);
		np->write_welcome_repicient = mystrdup(p->write_welcome_repicient);
		np->write_closing = mystrdup(p->write_closing);
		np->reply_welcome = mystrdup(p->reply_welcome);
		np->reply_intro = mystrdup(p->reply_intro);
		np->reply_close = mystrdup(p->reply_close);
		np->forward_initial = mystrdup(p->forward_initial);
		np->forward_finish = mystrdup(p->forward_finish);
	}
	return np;
}

/*****************************************************************************/

void phrase_free(struct phrase *p)
{
	if (!p) return;
	free(p->addresses);
	free(p->write_welcome);
	free(p->write_welcome_repicient);
	free(p->write_closing);
	free(p->reply_welcome);
	free(p->reply_intro);
	free(p->reply_close);
	free(p->forward_initial);
	free(p->forward_finish);
	free(p);
}

/*****************************************************************************/

struct phrase *phrase_find_best(char *addr)
{
	struct phrase *phrase;
	struct phrase *phrase_best;

	phrase = (struct phrase*)list_first(&user.config.phrase_list);
	if (!addr) return phrase;

	phrase_best = NULL;

	while (phrase)
	{
		if (!phrase->addresses && !phrase_best) phrase_best = phrase;
		else
		{
			if (mystristr(addr,phrase->addresses))
			{
				if (!phrase_best) phrase_best = phrase;
				else if (!phrase_best->addresses) phrase_best = phrase;
			}
		}
		phrase = (struct phrase*)node_next(&phrase->node);
	}
	return phrase_best;
}

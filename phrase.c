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
** phrase.c
*/

#include "phrase.h"

#include <stdlib.h>
#include <string.h>

#include "configuration.h"
#include "support_indep.h"

/**************************************************************************
 Allocates a new phrase
**************************************************************************/
struct phrase *phrase_malloc(void)
{
	struct phrase *sig;

	if ((sig = (struct phrase*)malloc(sizeof(struct phrase))))
	{
		memset(sig,0,sizeof(struct phrase));
	}
	return sig;
}

/**************************************************************************
 Duplicates an account
**************************************************************************/
struct phrase *phrase_duplicate(struct phrase *s)
{
	struct phrase *ns = phrase_malloc();
	if (ns && s)
	{
		ns->addresses = mystrdup(s->addresses);
		ns->write_welcome = mystrdup(s->write_welcome);
		ns->write_welcome_repicient = mystrdup(s->write_welcome_repicient);
		ns->write_closing = mystrdup(s->write_closing);
		ns->reply_welcome = mystrdup(s->reply_welcome);
		ns->reply_intro = mystrdup(s->reply_intro);
		ns->reply_close = mystrdup(s->reply_close);
		ns->forward_initial = mystrdup(s->forward_initial);
		ns->forward_finish = mystrdup(s->forward_finish);
	}
	return ns;
}

/**************************************************************************
 Frees a phrase
**************************************************************************/
void phrase_free(struct phrase *s)
{
	if (!s) return;
	free(s->addresses);
	free(s->write_welcome);
	free(s->write_welcome_repicient);
	free(s->write_closing);
	free(s->reply_welcome);
	free(s->reply_intro);
	free(s->reply_close);
	free(s->forward_initial);
	free(s->forward_finish);
	free(s);
}

/**************************************************************************
 Finds the phrase which meets the address best (actually which first fits,
 expect the one which is for all).
 Might return NULL!
**************************************************************************/
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

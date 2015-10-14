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
 * @file phrase.h
 */

#ifndef SM__PHRASE_H
#define SM__PHRASE_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

struct phrase
{
	struct node node;
	char *addresses; /* addresses where to use this phrases, NULL all addresses */
	char *write_welcome;
	char *write_welcome_repicient;
	char *write_closing;
	char *reply_welcome;
	char *reply_intro;
	char *reply_close;
	char *forward_initial;
	char *forward_finish;
};

/**
 * Allocates a new phrase.
 *
 * @return the new phrase.
 */
struct phrase *phrase_malloc(void);

/**
 * Duplicates the given phrase.
 *
 * @param s the phrase to be duplicated
 * @return the duplicated phrase
 */
struct phrase *phrase_duplicate(struct phrase *s);

/**
 * Frees the given phrase.
 *
 * @param p the phrase to be freed.
 */
void phrase_free(struct phrase *s);

/**
 * Finds the phrase which meets the address best (actually which first fits,
 * expect the one which is for all).
 *
 * @param addr
 * @return the phrase or NULL.
 */
struct phrase *phrase_find_best(char *addr);

#endif

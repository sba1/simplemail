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
** phrase.h
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

struct phrase *phrase_malloc(void);
struct phrase *phrase_duplicate(struct phrase *s);
void phrase_free(struct phrase *s);
struct phrase *phrase_find_best(char *addr);

#endif

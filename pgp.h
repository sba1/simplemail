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
 * @file pgp.h
 */
#ifndef SM__PGP_H
#define SM__PGP_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

struct pgp_key
{
	struct node node;
	unsigned int keyid;
	char **userids;
};

/**
 * Updates the internal pgp list.
 *
 * @return the pgp list
 */
int pgp_update_key_list(void);

/**
 * Returns first pgp key.
 *
 * @return the first pgp key
 */
struct pgp_key *pgp_first(void);

/**
 * Returns the next pgp key.
 *
 * @param the pgp key whose successor needs to be returned.
 * @return the next pgp key
 */
struct pgp_key *pgp_next(struct pgp_key *);

/**
 * Duplicate a given pgp key.
 *
 * @param key the key to be duplicated.
 * @return the duplicate
 */
struct pgp_key *pgp_duplicate(struct pgp_key *key);

/**
 * Dispose memory associated with a duplicated key.
 *
 * @param key the key to dispose.
 */
void pgp_dispose(struct pgp_key *key);

/**
 * Starts a pgp operation. Needs to be generalized
 *
 * @param options defines command line options to be passed to the pgp tool.
 * @param output the output file
 * @return 1 on success, 0 otherwise.
 */
int pgp_operate(const char *options, char *output);

#endif

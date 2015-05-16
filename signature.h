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
 * @file
 */

#ifndef SM__SIGNATURE_H
#define SM__SIGNATURE_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

#ifndef SM__CODESETS_H
#include "codesets.h"
#endif

struct signature
{
	struct node node;
	char *name;
	utf8 *signature;
};

/**
 * Allocates a new signature.
 *
 * @return the signature
 */
struct signature *signature_malloc(void);

/**
 * Duplicates a signature.
 *
 * @param s the signature to be duplicated
 * @return the signature
 */
struct signature *signature_duplicate(struct signature *s);

/**
 * Frees the given signature.
 *
 * @param s the signature to be freed.
 */
void signature_free(struct signature *s);

#endif

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
 * @file string_pools.h
 */

#ifndef SM__STRING_POOLS_H
#define SM__STRING_POOLS_H

struct string_pool;

/**
 * Construct a string pool.
 *
 * @return the constructed string pool or NULL.
 */
struct string_pool *string_pool_create(void);

/**
 * Save the string pool to the given file.
 *
 * @param sp the string pool whose contents shall be saved.
 * @param filename the name of the file to which the pool shall be saved.
 * @return whether this operation was successful or not.
 */
int string_pool_save(struct string_pool *sp, char *filename);

/**
 * Delete the given string pool and all of the associated memory.
 * Note that any reference will be no longer valid after this call.
 *
 * @param p the pool to delete
 */
void string_pool_delete(struct string_pool *p);

/**
 * Reference the given string and return its id in context of the string pool.
 *
 * @param p
 * @param string
 * @return the id or -1 if the string could not be referenced (memory)
 */
int string_pool_ref(struct string_pool *p, char *string);

/**
 * Dereference the given string.
 *
 * @param p
 * @param string
 */
void string_pool_deref_by_str(struct string_pool *p, char *string);

/**
 * Dereference the given string.
 *
 * @param p
 * @param id
 */
void string_pool_deref_by_id(struct string_pool *p, int id);

/**
 * Return the string for a given id.
 *
 * @param p the string pool.
 * @param id the id
 * @return the utf8
 */
char *string_pool_get(struct string_pool *p, int id);

#endif

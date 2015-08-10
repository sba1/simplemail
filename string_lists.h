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
 * @file string_lists.h
 */

#ifndef SM__STRING_LISTS_H
#define SM__STRING_LISTS_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

struct string_node
{
	struct node node; /* embedded node struct */
	char *string;
};

struct string_list
{
	struct list l;
};

/**
 * Initialize the string list.
 *
 * @param list to be initialized
 */
void string_list_init(struct string_list *list);

/**
 * Return the first string node of the given string list.
 *
 * @param list of which the first element should be returned
 * @return the first element or NULL if the list is empty
 */
struct string_node *string_list_first(struct string_list *list);

/**
 * Insert the given string node at the tail of the given list.
 *
 * @param list the list at which the node should be inserted
 * @param node the node to be inserted
 */
void string_list_insert_tail_node(struct string_list *list, struct string_node *node);

/**
 * Inserts a string into the end of a string list. The string will
 * be duplicated.
 *
 * @param list the list to which to add the string.
 * @param string the string to be added. The string will be duplicated.
 * @return the newly created node that has just been inserted or NULL on memory
 *  failure.
 */
struct string_node *string_list_insert_tail(struct string_list *list, char *string);

/**
 * Remove the head from the given string list.
 *
 * @param list the list from which the node should be removed.
 * @return the head that has just been removed or NULL if the list was empty.
 */
struct string_node *string_list_remove_head(struct string_list *list);

/**
 * Remove the tail of the given string list.
 *
 * @param list the list from which the node should be removed.
 * @return the tail that has just been removed or NULL if the list was empty.
 */
struct string_node *string_list_remove_tail(struct string_list *list);

/**
 * Clears the complete list by freeing all memory (including strings) but does
 * not free the memory of the list itself.
 *
 * @param list the list whose element should be freed.
 */
void string_list_clear(struct string_list *list);

/**
 * Shortcut for calling string_list_clear() and free().
 *
 * @param list the list that should be cleared and freed.
 */
void string_list_free(struct string_list *list);

/**
 * Looks for a given string node in the list and returns it.
 * Search is case insensitive
 *
 * @param list
 * @param str
 * @return
 */
struct string_node *string_list_find(struct string_list *list, const char *str);

#endif

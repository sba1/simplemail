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

#ifndef SM__LISTS_H
#define SM__LISTS_H

struct node
{
	struct node *next;
	struct node *prev;
	struct list *list; /* To which list belongs this node? */
};

struct list
{
	struct node *first;
	struct node *last;
};

/**
 * Initializes a list.
 *
 * @param list
 */
void list_init(struct list *list);

/**
 * Inserts a node into the list.
 *
 * @param list the list that should contain the node
 * @param newnode the node to be inserted
 * @param prednode the node after which the new node should be inserted
 */
void list_insert(struct list *list, struct node *newnode, struct node *prednode);

/**
 * Inserts a node into the list at the last position
 *
 * @param list the list to which the node should be inserted
 * @param newnode the node to be inserted
 */
void list_insert_tail(struct list *list, struct node *newnode);

/**
 * Removes the first node and returns it.
 *
 * @param list the list whose first node should be removed.
 * @return the node or NULL if the list was empty.
 */
struct node *list_remove_head(struct list *list);

/**
 * Removes the last node and returns it.
 *
 * @param list on which the element should be removed
 * @return the element that has been removed or NULL
 */
struct node *list_remove_tail(struct list *list);

/**
 * Returns the num'th entry.
 *
 * @param list the list
 * @param num defines the index of the list element to be returned
 * @return the entry or NULL if the list doesn't contain that much entries.
 */
struct node *list_find(struct list *list, int num);

/**
 * Returns the length of the list, i.e., counts the number of nodes it contains.
 *
 * @param list the list whose length should be determined.
 * @return the count.
 */
int list_length(struct list *list);

/**
 * Returns the index of the node.
 *
 * @param node the whose index should be determined.
 * @return the index or -1 if the node was not attached to a list.
 */
int node_index(struct node *node);

/**
 * Removes the node from the list to which it was added. It is wrong to call
 * this function to a node that has not been linked to a list.
 *
 * @param node the node that should be removed.
 */
void node_remove(struct node *node);

#ifdef INLINEING
#define list_first(x) ((x)->first)
#define list_last(x) ((x)->last)
#define node_next(x) ((x)->next)
#define node_prev(x) ((x)->prev)
#define node_list(x) ((x)->list)
#else
/**
 * Returns the first entry
 *
 * @param list the list whose first entry should be returned-
 * @return the node or NULL if the list is empty.
 */
struct node *list_first(struct list *list);

/**
 * Returns the last entry.
 *
 * @param last the list whose last entry should be returned.
 * @return the node or NULL if the list is empty.
 */
struct node *list_last(struct list *last);

/**
 * Returns the node's successor
 *
 * @param node the node whose successor should be returned
 * @return the successor or NULL if node was the last one.
 */
struct node *node_next(struct node *node);

/**
 * Returns the node's predecessor.
 *
 * @param node the node whose predecessor should be returned
 * @return the predecessor or NULL if node was the head.
 */
struct node *node_prev(struct node *node);

/**
 * Returns the list where the node belongs to.
 *
 * @param node the node whose parent list should be determined.
 * @return the list or NULL if it is not linked to any list.
 */
struct list *node_list(struct node *node);
#endif

#endif




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
** lists.c
*/

/********************************************************************** 
 lists.c

 This is a implementation of double linked lists.
 (completly portable)
***********************************************************************/

#include <stdlib.h>

#include "lists.h"
#include "support_indep.h"
#include "debug.h"

/******************************************************************
 Initalizes a list
*******************************************************************/
void list_init(struct list *list)
{
  list->first = list->last = NULL;
}

/******************************************************************
 Inserts a node into the list
*******************************************************************/
void list_insert(struct list *list, struct node *newnode, struct node *prednode)
{
  if (list->last == prednode)
  {
    /* Insert the node at the list's end */
    list_insert_tail(list,newnode);
    return;
  }

  newnode->list = list;
  
  if (prednode)
  {
    /* Insert the list between two nodes */
    prednode->next->prev = newnode;
    newnode->next = prednode->next;
    newnode->prev = prednode;
    prednode->next = newnode;
    return;
  }

  list->first->prev = newnode;
  newnode->next = list->first;
  newnode->prev = NULL;
  list->first = newnode;
}

/******************************************************************
 Inserts a node into the list at the last position
*******************************************************************/
void list_insert_tail(struct list *list, struct node *newnode)
{
  newnode->list = list;

  if (!list->first)
  {
    /* List was empty */
    newnode->next = newnode->prev = NULL;
    list->first = list->last = newnode;
    return;
  }

  list->last->next = newnode;
  newnode->next = NULL;
  newnode->prev = list->last;
  list->last = newnode;
}

/******************************************************************
 Removes the last node and returns it
*******************************************************************/
struct node *list_remove_tail(struct list *list)
{
  struct node *node = list_last(list);
  if (node)
  {
    node_remove(node);
  }
  return node;
}

/******************************************************************
 Returns the first entry
*******************************************************************/
struct node *list_first(struct list *list)
{
	if (!list) SM_DEBUGF(5,("list_first() called with NULL pointer!\n"));
  return (list ? list->first : NULL);
}

/******************************************************************
 Returns the last entry
*******************************************************************/
struct node *list_last(struct list *list)
{
	if (!list) SM_DEBUGF(5,("list_last() called with NULL pointer!\n"));
  return (list ? list->last : NULL);
}

/******************************************************************
 Returns the n'th entry
*******************************************************************/
struct node *list_find(struct list *list, int num)
{
  struct node *n = list_first(list);
  while (n)
  {
    if (!num) return n;
    num--;
    n = node_next(n);
  }
  return n;
}

/******************************************************************
 Returns the length of the list
*******************************************************************/
int list_length(struct list *list)
{
  int len = 0;
  struct node *node = list_first(list);
  while (node)
  {
    len++;
    node = node_next(node);
  }
  return len;
}

/******************************************************************
 Returns the node's succersor
*******************************************************************/
struct node *node_next(struct node *node)
{
	if (!node) SM_DEBUGF(5,("node_next() called with NULL pointer!\n"));
  return (node ? node->next : NULL);
}

/******************************************************************
 Returns the node's pred
*******************************************************************/
struct node *node_prev(struct node *node)
{
	if (!node) SM_DEBUGF(5,("node_prev() called with NULL pointer!\n"));
  return (node ? node->prev : NULL);
}

/******************************************************************
 Returns the list where the node belongs to
 (or NULL if not linked to any list)
*******************************************************************/
struct list *node_list(struct node *node)
{
	if (!node) SM_DEBUGF(5,("node_list() called with NULL pointer!\n"));
  return (node ? node->list : NULL);
}

/******************************************************************
 Returns the index of the node, -1 if no node
*******************************************************************/
int node_index(struct node *node)
{
  int index = -1;
  while(node)
  {
    node = node_prev(node);
    index++;
  }
  return index;
}

/******************************************************************
 Removes the entry from the list
*******************************************************************/
void node_remove(struct node *node)
{
  struct list *list = node->list;

  node->list = NULL;

  if (list->first == node)
  {
    /* The node was the first one */
    if (list->last == node)
    {
      /* The node was the only one */
      list->first = list->last = NULL;
      return;
    }
    list->first = node->next;
    list->first->prev = NULL;
    return;
  }

  if (list->last == node)
  {
    /* The node was the last one */
    list->last = node->prev;
    list->last->next = NULL;
    return;
  }

  node->next->prev = node->prev;
  node->prev->next = node->next;
}


/******************************************************************
 Inserts a string into the end of a string list. The string will
 be duplicated. Returns 
*******************************************************************/
struct string_node *string_list_insert_tail(struct list *list, char *string)
{
	struct string_node *node = (struct string_node*)malloc(sizeof(struct string_node));
	if (node)
	{
		if ((node->string = mystrdup(string)))
		{
			list_insert_tail(list,&node->node);
		}
	}
	return node;
}

/******************************************************************
 Clears the complete list by freeing all memory (including strings).
*******************************************************************/
void string_list_clear(struct list *list)
{
	struct string_node *node;
	while ((node = (struct string_node*)list_remove_tail(list)))
	{
		if (node->string) free(node->string);
		free(node);
	}
}

/******************************************************************
 Looks for a given string node in the list and returns it.
 Search is caseinsensitive
*******************************************************************/
struct string_node *string_list_find(struct list *list, const char *str)
{
	struct string_node *node = (struct string_node*)list_first(list);
	while (node)
	{
		if (!mystricmp(str,node->string)) return node;
		node = (struct string_node*)node_next(&node->node);
	}
	return NULL;
}

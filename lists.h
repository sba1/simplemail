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
** lists.h
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

struct string_node
{
	struct node node; /* embedded node struct */
	char *string;
};
	

/* Prototypes */
void list_init(struct list *list);
void list_insert(struct list *list, struct node *newnode, struct node *prednode);
void list_insert_tail(struct list *list, struct node *newnode);
struct node *list_remove_head(struct list *list);
struct node *list_remove_tail(struct list *list);
struct node *list_find(struct list *list, int num);
int list_length(struct list *list);
int node_index(struct node *node);
void node_remove(struct node *node);

/* String lists */
struct string_node *string_list_insert_tail(struct list *list, char *string);
void string_list_clear(struct list *list);
struct string_node *string_list_find(struct list *list, const char *str);

#ifdef INLINEING
#define list_first(x) ((x)->first)
#define list_last(x) ((x)->last)
#define node_next(x) ((x)->next)
#define node_prev(x) ((x)->prev)
#define node_list(x) ((x)->list)
#else
struct node *list_first(struct list *list);
struct node *list_last(struct list *last);
struct node *node_next(struct node *node);
struct node *node_prev(struct node *node);
struct list *node_list(struct node *node);
#endif

#endif




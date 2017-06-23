/**
 * @file string_lists.c
 */

#include "string_lists.h"

#include <stdlib.h>

#include "support_indep.h"

/*****************************************************************************/

void string_list_init(struct string_list *list)
{
	list_init(&list->l);
}

/*****************************************************************************/

struct string_node *string_list_first(struct string_list *list)
{
	return (struct string_node*)list_first(&list->l);
}

/*****************************************************************************/

void string_list_insert_tail_node(struct string_list *list, struct string_node *node)
{
	list_insert_tail(&list->l, &node->node);
}

/*****************************************************************************/

struct string_node *string_list_remove_head(struct string_list *list)
{
	return (struct string_node *)list_remove_head(&list->l);
}

/*****************************************************************************/

struct string_node *string_list_remove_tail(struct string_list *list)
{
	return (struct string_node *)list_remove_tail(&list->l);
}

/*****************************************************************************/

struct string_node *string_list_insert_tail(struct string_list *list, const char *string)
{
	struct string_node *node = (struct string_node*)malloc(sizeof(struct string_node));
	if (node)
	{
		if ((node->string = mystrdup(string)))
		{
			list_insert_tail(&list->l,&node->node);
		}
	}
	return node;
}

/*****************************************************************************/

void string_list_clear(struct string_list *list)
{
	struct string_node *node;
	while ((node = string_list_remove_tail(list)))
	{
		if (node->string) free(node->string);
		free(node);
	}
}


/*****************************************************************************/

void string_list_free(struct string_list *list)
{
	if (!list) return;
	string_list_clear(list);
	free(list);
}

/*****************************************************************************/

struct string_node *string_list_find(struct string_list *list, const char *str)
{
	struct string_node *node = (struct string_node*)list_first(&list->l);
	while (node)
	{
		if (!mystricmp(str,node->string)) return node;
		node = (struct string_node*)node_next(&node->node);
	}
	return NULL;
}

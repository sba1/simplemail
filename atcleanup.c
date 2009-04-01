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
 *
 * Provides similar function as the atexit() function but happens still
 * when being in main().
 *
 * @file atcleanup.c
 */

#include "atcleanup.h"
#include "debug.h"
#include "lists.h"

static struct list atcleanup_list;
static int atcleanup_list_initialized;

struct atcleanup_node
{
	struct node node;
	void (*cleanup)(void *user_data);
	void *user_data;
};

/**
 * Register a cleanup function.
 *
 * @param cleanup
 * @param user_data
 * @return
 */
int atcleanup(void (*cleanup)(void *user_data),void *user_data)
{
	struct atcleanup_node *node;

	if (!atcleanup_list_initialized)
	{
		list_init(&atcleanup_list);
		atcleanup_list_initialized = 1;
	}

	if (!(node = (struct atcleanup_node*)malloc(sizeof(*node))))
		return 0;

	node->cleanup = cleanup;
	node->user_data = user_data;
	list_insert_tail(&atcleanup_list,&node->node);
}

/**
 * Performs the finalization, i.e., calls all functions registered
 * with atcleanup().
 */
void atcleanup_finalize(void)
{
	struct atcleanup_node *node;

	if (!atcleanup_list_initialized) return;

	while ((node = (struct atcleanup_node*)list_remove_tail(&atcleanup_list)))
	{
		node->cleanup(node->user_data);
		free(node);
	}
}

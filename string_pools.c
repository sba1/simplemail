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
 * @file string_pools.c
 *
 * A pool of strings resembles a set of strings with a reference count.
 */

#include "string_pools.h"

#include <stdlib.h>

#include "support_indep.h"
#include "hash.h"

/*****************************************************************************/

struct ref_string
{
	char *str;
	unsigned int count;
};

struct string_pool
{
	/** Number of total references */
	int total_ref_count;

	struct ref_string *ref_strings;
	unsigned int ref_strings_num;
	unsigned int ref_strings_allocated;

	unsigned int id_next;

	struct hash_table ht;
};

/*****************************************************************************/

struct string_pool *string_pool_create(void)
{
	struct string_pool *p = malloc(sizeof(*p));
	if (!p)
	{
		return NULL;
	}
	memset(p, 0, sizeof(*p));
	if (!(hash_table_init(&p->ht, 12, NULL)))
	{
		free(p);
		return NULL;
	}
	return p;
}

/*****************************************************************************/

void string_pool_delete(struct string_pool *p)
{
	unsigned int i;
	hash_table_clean(&p->ht);
	for (i=0; i < p->ref_strings_num; i++)
	{
		free(p->ref_strings[i].str);
	}
	free(p->ref_strings);
	free(p);
}

/*****************************************************************************/

int string_pool_ref(struct string_pool *p, char *string)
{
	struct hash_entry *he;

	if (!(he = hash_table_lookup(&p->ht, string)))
	{
		char *dupped_string;

		if (!(dupped_string = mystrdup(string)))
		{
			return -1;
		}

		if (!(he = hash_table_insert(&p->ht, dupped_string, 0)))
		{
			free(dupped_string);
			return -1;
		}

		he->data = p->id_next;

		/* Reserve more space if needed */
		if (p->id_next == p->ref_strings_allocated)
		{
			unsigned int new_ref_strings_allocated = (p->ref_strings_allocated+2)*2;
			struct ref_string *new_ref_strings = realloc(p->ref_strings, new_ref_strings_allocated * sizeof(*new_ref_strings));
			if (!new_ref_strings)
			{
				/* FIXME: Remove entry again */
				return -1;
			}
			p->ref_strings_allocated = new_ref_strings_allocated;
			p->ref_strings = new_ref_strings;
		}
		p->ref_strings[p->id_next].count = 0;
		p->ref_strings[p->id_next].str = dupped_string;
		p->id_next++;
	}
	p->ref_strings[he->data].count++;
	return he->data;
}

/*****************************************************************************/

void string_pool_deref_by_str(struct string_pool *p, char *string)
{
	struct hash_entry *he = hash_table_lookup(&p->ht, string);
	if (!he)
	{
		return;
	}
	p->ref_strings[he->data].count--;
}

/*****************************************************************************/

void string_pool_deref_by_id(struct string_pool *p, int id)
{
	p->ref_strings[id].count--;
}

/*****************************************************************************/

char *string_pool_get(struct string_pool *p, int id)
{
	return p->ref_strings[id].str;
}

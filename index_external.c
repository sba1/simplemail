/**
 * index_external.c - a external string index implementation for SimpleMail.
 * Copyright (C) 2012  Sebastian Bauer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file index_external.c
 */

#include <stdlib.h>
#include <string.h>

#include "lists.h"

#include "index.h"
#include "index_private.h"
#include "index_external.h"

/* Get va_vopy() with pre-C99 compilers */
#ifdef __SASC
#define va_copy(dest,src) ((dest) = (src))
#elif defined(__GNUC__) && (__GNUC__ < 3)
#include <varargs.h>
#endif

struct document_node
{
	struct node node;
	int did;
	char *txt;
};

struct index_external
{
	struct index index;
	struct list document_list;
};


struct index *index_external_create(const char *filename)
{
	struct index_external *idx;

	if (!(idx = (struct index_external*)malloc(sizeof(*idx))))
		return NULL;

	memset(idx,0,sizeof(*idx));
	list_init(&idx->document_list);

	return &idx->index;
}

void index_external_dispose(struct index *index)
{
	struct document_node *d;
	struct index_external *idx;

	idx = (struct index_external*)index;

	while ((d = (struct document_node *)list_remove_tail(&idx->document_list)))
	{
		free(d->txt);
		free(d);
	}

	free(index);
}

int index_external_put_document(struct index *index, int did, const char *text)
{
	struct document_node *d;
	struct index_external *idx;

	idx = (struct index_external*)index;

	if (!(d = (struct document_node*)malloc(sizeof(*d))))
		return 0;
	memset(d,0,sizeof(*d));

	d->did = did;
	if (!(d->txt = strdup(text)))
	{
		free(d);
		return 0;
	}

	list_insert_tail(&idx->document_list,&d->node);
	return 1;
}

int index_external_remove_document(struct index *index, int did)
{
	struct document_node *d;
	struct index_external *idx;
	int removed = 0;

	idx = (struct index_external*)index;

	d = (struct document_node*)list_first(&idx->document_list);
	while (d)
	{
		struct document_node *n = (struct document_node*)node_next(&d->node);

		if (d->did == did)
		{
			removed = 1;
			node_remove(&d->node);
			free(d->txt);
			free(d);
		}

		d = n;
	}
	return removed;
}

int index_external_find_documents(struct index *index, int (*callback)(int did, void *userdata), void *userdata, int num_substrings, va_list substrings)
{
	struct document_node *d;
	struct index_external *idx;
	int nd = 0;
	int i;

	idx = (struct index_external*)index;
	nd = 0;

	d = (struct document_node*)list_first(&idx->document_list);
	while (d)
	{
		int take = 1;
		va_list substrings_copy;

		va_copy(substrings_copy,substrings);

		for (i=0;i<num_substrings;i++)
		{
			if (!strstr(d->txt,va_arg(substrings_copy,char *)))
			{
				take = 0;
				break;
			}
		}

		va_end(substrings_copy);

		if (take)
		{
			callback(d->did,userdata);
			nd++;
		}
		d = (struct document_node*)node_next(&d->node);
	}

	return nd;
}

/*****************************************************/

struct index_algorithm index_external =
{
		index_external_create,
		index_external_dispose,
		index_external_put_document,
		index_external_remove_document,
		index_external_find_documents
};

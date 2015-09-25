/**
 * index.c - a simple string index interface for SimpleMail.
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
 * @file index.c
 */

#include "index.h"

#include <stdarg.h>
#include <string.h>

#include "index_private.h"

/*****************************************************************************/

struct index *index_create(struct index_algorithm *alg, const char *filename)
{
	struct index *idx;

	if (!(idx = alg->create(filename)))
		return NULL;
	idx->alg = alg;
	return idx;
}

/*****************************************************************************/

void index_dispose(struct index *index)
{
	if (!index) return;
	index->alg->dispose(index);
}

/*****************************************************************************/

int index_put_document(struct index *index, int did, const char *text)
{
	return index->alg->put_document(index,did,text);
}

/*****************************************************************************/

int index_remove_document(struct index *index, int did)
{
	return index->alg->remove_document(index,did);
}

/*****************************************************************************/

int index_find_documents(struct index *index, int (*callback)(int did, void *userdata), void *userdata, int num_substrings, ...)
{
	int rc;

	va_list list;

	va_start(list, num_substrings);
	rc = index->alg->find_documents(index,callback,userdata,num_substrings,list);
	va_end(list);

	return rc;
}

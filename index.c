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

#include <string.h>

#include "index.h"
#include "index_private.h"

/**
 * Create an index using the given algorithm and persistence layer
 * stored at the given name.
 *
 * @param alg
 * @param filename
 * @return
 */
struct index *index_create(struct index_algorithm *alg, const char *filename)
{
	struct index *idx;

	if (!(idx = alg->create(filename)))
		return NULL;
	idx->alg = alg;
	return idx;
}

/**
 * Free all resources associated with the given index. Does not affect
 * the persistence layer. The given index must not be accessed any longer
 * after a call to this function.
 *
 * @param index
 */
void index_dispose(struct index *index)
{
	if (!index) return;
	index->alg->dispose(index);
}

/**
 * Put a document of the given document id into the index.
 *
 * @param index
 * @param did
 * @param text
 * @return success or not.
 */
int index_put_document(struct index *index, int did, const char *text)
{
	return index->alg->put_document(index,did,text);
}

/**
 * Remove the document from the index.
 *
 * @param index
 * @param did
 * @return success or not.
 *
 */
int index_remove_document(struct index *index, int did)
{
	return index->alg->remove_document(index,did);
}

/**
 * Find documents that all contain the given string as exact substrings.
 *
 * @param index
 * @param callback
 * @param userdata
 * @param num_substrings
 * @return
 */
int index_find_documents(struct index *index, int (*callback)(int did, void *userdata), void *userdata, int num_substrings, ...)
{
	int rc;

	va_list list;

	va_start(list, num_substrings);
	rc = index->alg->find_documents(index,callback,userdata,num_substrings,list);
	va_end(list);

	return rc;
}

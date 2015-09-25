/**
 * index.h - a simple string index interface for SimpleMail.
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
 * @file index.h
 */

#ifndef SM__INDEX_H
#define SM__INDEX_H

struct index;
struct index_algorithm;

/**
 * Create an index using the given algorithm and persistence layer
 * stored at the given name.
 *
 * @param alg the algorithm to take
 * @param filename the local filename of the index
 * @return the intex
 */
struct index *index_create(struct index_algorithm *alg, const char *filename);

/**
 * Free all resources associated with the given index. Does not affect
 * the persistence layer. The given index must not be accessed any longer
 * after a call to this function.
 *
 * @param index defines the index to be disposed
 */
void index_dispose(struct index *index);

/**
 * Put a document of the given document id into the index.
 *
 * @param index the index to which a document is added
 * @param did the document id
 * @param text the text to be indexed
 * @return success or not.
 */
int index_put_document(struct index *index, int did, const char *text);

/**
 * Remove the document from the index.
 *
 * @param index the index from which the document shall be removed.
 * @param did id of the document to be removed
 * @return success or not.
 *
 */
int index_remove_document(struct index *index, int did);

/**
 * Find documents that all contain the given string as exact substrings.
 *
 * @param index
 * @param callback
 * @param userdata
 * @param num_substrings number of following strings of type const char *.
 * @return number of documents for which the callback was called.
 */
int index_find_documents(struct index *index, int (*callback)(int did, void *userdata), void *userdata, int num_substrings, ...);

#endif

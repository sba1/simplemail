/**
 * index_private.h - a simple string index interface for SimpleMail.
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
 * @file index_private.h
 */
#ifndef SM__INDEX_PRIVATE_H
#define SM__INDEX_PRIVATE_H

struct index_algorithm;

/**
 * Base structure for index instance data.
 */
struct index
{
	struct index_algorithm *alg;
};

/**
 * Defines the common interface for string index algorithms.
 * This is not directly exposed to clients only to
 * implementors.
 */
struct index_algorithm
{
	/**
	 * Creates a new index stored in the given filename.
	 *
	 * @param filename
	 * @return the newly created index.
	 */
	struct index *(*create)(const char *filename);

	/**
	 * Get rid of memory allocated by the index.
	 * Doesn't not affect the persistence layer.
	 *
	 * @param index
	 */
	void (*dispose)(struct index *index);

	/**
	 * Put a document with the associated id into the index.
	 *
	 * @param index
	 * @param did
	 * @param text
	 * @return
	 */
	int (*put_document)(struct index *index, int did, const char *text);

	/**
	 * Remove the document identified by the given document id from the
	 * index.
	 *
	 * @param index
	 * @param did
	 * @return
	 */
	int (*remove_document)(struct index *index, int did);

	/**
	 * Find documents that all contain the given string as exact substrings.
	 *
	 * @param index
	 * @param callback
	 * @param userdata
	 * @param num_substrings number of following strings of type const char *.
	 * @return number of documents for which the callback was called.
	 */
	int (*find_documents)(struct index *index, int (*callback)(int did, void *userdata), void *userdata, int num_substrings, ...);
};


#endif

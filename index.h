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

#ifndef SM__INDEX_H
#define SM__INDEX_H

struct index;
struct index_algorithm;

struct index *index_create(struct index_algorithm *alg, const char *filename);
void index_dispose(struct index *index);
int index_put_document(struct index *index, int did, const char *text);
int index_remove_document(struct index *index, int did);
int index_find_documents(struct index *index, int (*callback)(int did, void *userdata), void *userdata, int num_substrings, ...);

#endif

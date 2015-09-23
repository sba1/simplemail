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
 * @file boyermoore.h
 */

#ifndef SM__BOYERMOORE_H
#define SM__BOYERMOORE_H

struct boyermoore_context;

typedef int (*bm_callback)(char *x, unsigned int pos, void *user_data);

/**
 * Creates the boyermoore context for a given pattern and
 * length.
 *
 * @param p
 * @param plen
 * @return
 */
struct boyermoore_context *boyermoore_create_context(char *pattern, int pattern_length);

/**
 * Creates the boyermoore context.
 *
 * @param context
 */
void boyermoore_delete_context(struct boyermoore_context *context);

/**
 * Performs the boyermoore algorithm.
 *
 * @param context the context
 * @param str string to be searched through
 * @param n number of bytes to be searches through
 * @param callback function that is called for every hit. If callback returns 0,
 *        the search is aborted.
 * @param user_data data pointer that is fed into the callback function.
 *
 * @return the position of the last found pattern or -1 if the pattern could not be found.
 */

int boyermoore(struct boyermoore_context *context, char *str, int n, bm_callback callback, void *user_data);

#endif

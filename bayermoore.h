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

/*
** bayermoore.h
*/

#ifndef SM__BAYERMOORE_H
#define SM__BAYERMOORE_H

#include <limits.h>

struct bayermoore_context;

typedef int (*bm_callback)(char *x, unsigned int pos, void *user_data);

struct bayermoore_context *bayermoore_create_context(char *pattern, int pattern_length);
void bayermoore_delete_context(struct bayermoore_context *context);

int bayermoore(struct bayermoore_context *context, char *str, int n, bm_callback callback, void *user_data);

#endif

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
** support_indep.h
*/

#ifndef SM__SUPPORT_INDEP_H
#define SM__SUPPORT_INDEP_H

#include <stdio.h>

int mystrcmp(const char *str1, const char *str2);
int mystricmp(const char *str1, const char *str2);
int mystrnicmp(const char *str1, const char *str2, int n);
char *mystristr(const char *str1, const char *str2);
#define mystrlen(str) ((str)?strlen(str):0) /* you can call this with NULL */
char *mystrdup(const char *str);
char *mystrndup(const char *str, int len);
size_t mystrlcpy(char *dest, const char *src, size_t n);

unsigned int myfsize(FILE *file);

void wrap_text(char *text, int border);

int array_contains(char **strings, char *str);
char **array_add_string(char **strings, char *str);
int array_length(char **strings);
char **array_duplicate(char **rcp);
void array_free(char **string_array);

char *mycombinepath(char *drawer, char *file);

#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))

#endif

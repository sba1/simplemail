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
int mystrncmp(unsigned char *str1, unsigned char *str2,int len);
int mystricmp(const char *str1, const char *str2);
int mystrnicmp(const char *str1, const char *str2, int n);
char *mystristr(const char *str1, const char *str2);
#define mystrlen(str) ((str)?strlen(str):0) /* you can call this with NULL */
char *mystrdup(const char *str);
char *mystrndup(const char *str, int len);
size_t mystrlcpy(char *dest, const char *src, size_t n);
char *mystpcpy(char *dest, const char *src);
char *mystrcat(char *str1, char *str2);

unsigned int myfsize(FILE *file);
int myfiledatecmp(char *file1, char *file2);
int myfilecopy(const char *sourcename, const char *destname);
int mydeletedir(const char *path);

void wrap_text(char *text, int border);

int array_contains(char **strings, char *str);
int array_contains_utf8(char **strings, char *str);
int array_index(char **strings, char *str);
int array_index_utf8(char **strings, char *str);
char **array_add_string(char **strings, char *str);
char **array_add_array(char **dest, char **src);
int array_length(char **strings);
char **array_duplicate(char **rcp);
char **array_duplicate_parsed(char **str, int flags);
void array_free(char **string_array);
char **array_replace_idx(char **strings, int idx, char *str);
char **array_remove_idx(char **strings, int idx);

char *mycombinepath(char *drawer, char *file);

#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))

/* search a sorted array in O(log n) e.g.
   BIN_SEARCH(strings,0,sizeof(strings)/sizeof(strings[0]),strcmp(key,array[mid]),res); */
#define BIN_SEARCH(array,low,high,compare,result) \
	{\
		int l = low;\
		int h = high;\
		int m = (low+high)/2;\
		result = NULL;\
		while (l<=h)\
		{\
			int c = compare;\
			if (!c){ result = &array[m]; break; }\
			if (c < 0) h = m - 1;\
			else l = m + 1;\
			m = (l + h)/2;\
		}\
	}

typedef struct
{
	char *str;
	int len;
	int allocated;
} string;

int string_initialize(string *string, unsigned int size);
int string_append(string *string, char *appstr);
int string_append_part(string *string, char *appstr, int bytes);
void string_crop(string *string, int startpos, int endpos);

#endif

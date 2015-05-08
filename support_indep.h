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

/* Needed for __USE_XOPEN2K8 */
#include <string.h>

int has_spaces(const char *str);
char *get_key_value(char *buf, const char *item);

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
char *mystrreplace(const char *src, const char *from, const char *to);

/* some string functions, should be in strings.c or simliar */
char *strdupcat(const char *string1, const char *string2);

#ifndef __USE_XOPEN2K8
char *strndup(const char *str1, int n);
#endif

char *stradd(char *src, const char *str1);
char *strnadd(char *src, const char *str1, int n);

unsigned int myfsize(FILE *file);
int myreadline(FILE *fh, char *buf);
int myfiledatecmp(char *file1, char *file2);
int myfilecopy(const char *sourcename, const char *destname);
int mydeletedir(const char *path);

void wrap_text(char *text, int border);

int longest_common_prefix(char * const *strings, int num);
int longest_common_substring(const char **strings, int num, int *pos_in_a, int *len);

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

/**
 * Gives the length of an array
 */
#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))

/**
 * Search a sorted array in O(log n) e.g.
 *
 * BIN_SEARCH(strings,0,sizeof(strings)/sizeof(strings[0]),strcmp(key,array[mid]),res);
 *
 * @def BIN_SEARCH
 */
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

/**
 * @brief Represents a growable string.
 *
 */
typedef struct
{
	char *str; /**< the string */
	int len; /**< the current length of the string */
	int allocated; /**< number of allocated bytes for the string */
} string;

int string_initialize(string *string, unsigned int size);
int string_append(string *string, const char *appstr);
int string_append_part(string *string, const char *appstr, int bytes);
int string_append_char(string *string, char c);
void string_crop(string *string, int startpos, int endpos);


/**
 * Ticks per second
 */
#define TIME_TICKS_PER_SECOND 25

unsigned int time_reference_ticks(void);
unsigned int time_ticks_passed(unsigned int reference);
unsigned int time_ms_passed(unsigned int ref);

char *utf8strdup(char *str, int utf8);

#endif

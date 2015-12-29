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

/**
 * Return the current time in seconds and micro seconds.
 *
 * @param seconds where the seconds are stored.
 * @param mics where the micro seconds are strored.
 */
void sm_get_current_time(unsigned int *seconds, unsigned int *mics);

/**
 * Returns whether the given string has spaces.
 *
 * @param str
 * @return
 */
int has_spaces(const char *str);


/**
 * For 'key = value' with given key, return the pointer to value.
 *
 * @param str defines the string to parse
 * @param key the key to check
 *
 * @return pointer to value or NULL if the pattern didn't match.
 */
char *get_key_value(char *buf, const char *item);

/**
 * Compares a string in a case-sensitive manner. Accepts NULL pointers
 *
 * @param str1
 * @param str2
 * @return
 */

int mystrcmp(const char *str1, const char *str2);

/**
 * A replacement for strncmp() as this doesn't return the same values as
 * strcmp().
 *
 * @param str1
 * @param str2
 * @param len
 * @return
 */
int mystrncmp(unsigned char *str1, unsigned char *str2,int len);

/**
 * Compares a string case insensitive. Accepts NULL pointers.
 *
 * @param str1
 * @param str2
 * @return
 */
int mystricmp(const char *str1, const char *str2);

/**
 * Compares a string case insensitive but not more than n characters.
 * Accepts NULL pointers
 *
 * @param str1
 * @param str2
 * @param n
 * @return
 */
int mystrnicmp(const char *str1, const char *str2, int n);

/**
 * Checks if a string is inside a string (not case sensitive).
 *
 * @param str1
 * @param str2
 * @return
 */
char *mystristr(const char *str1, const char *str2);

#define mystrlen(str) ((str)?strlen(str):0) /* you can call this with NULL */

/**
 * Duplicates a string. NULL is accepted (will return NULL).
 * A null byte string will also return NULL.
 *
 * @param str
 * @return
 */
char *mystrdup(const char *str);

/**
 * Like mystrdup() but you can limit the chars. A 0 byte is guaranteed.
 * The string is allocated via malloc().
 *
 * @param str1
 * @param n
 * @return
 */
char *mystrndup(const char *str, int len);

/**
 * @brief Like strncpy() but ensures that the string is always 0 terminated
 *
 * @param dest where to copy the source
 * @param src the source
 * @param n number of bytes to be copied
 * @return the length of the source string.
 */
size_t mystrlcpy(char *dest, const char *src, size_t n);

/**
 * Like strcpy() but returns the end of the destination string (mean the
 * pointer to the NULL byte), after src has been copied. Useful to for
 * building strings from several pieces. src might be NULL which then does
 * nothing
 *
 * @param dest
 * @param src
 * @return
 */
char *mystpcpy(char *dest, const char *src);

/**
 * Concatenates str2 to str1, and frees str1.
 *
 * @param str1 defines the first part of the new string. This string is freed.
 * @param str2 defines the second part of the new string. This string is not freed.
 * @return the newly created concatenated string.
 */
char *mystrcat(char *str1, char *str2);

/**
 * Returns a new string derived from str in which all occurrences of
 * from are replaced by to.
 *
 * @param src
 * @param from
 * @param to
 * @return the new string or NULL. If not NULL, the string must be freed with free().
 */
char *mystrreplace(const char *src, const char *from, const char *to);

/* some string functions, should be in strings.c or simliar */
char *strdupcat(const char *string1, const char *string2);

#ifndef __USE_XOPEN2K8
/**
 * Like strdup() but you can limit the chars. A 0 byte is guaranteed.
 * The string is allocated via malloc().
 *
 * @param str1 the string of which some characters should be duplicated.
 * @param n the number of characters that shall be duplicated.
 * @return the newly allocated string.
 */
char *strndup(const char *str1, int n);
#endif

/**
 * Adds a string (str1) to a given string src. The first string is
 * deallocated (maybe NULL). (A real string library would be better)
 * The string is allocated via malloc().
 *
 * @param src
 * @param str1
 * @return
 */
char *stradd(char *src, const char *str1);

/**
 * Adds a string (str1) to a given string src but only n bytes of str1. The first
 * string is deallocated (maybe NULL). (A real string library would be better)
 * The string is allocated via malloc().
 *
 * @param src
 * @param str1
 * @param n
 * @return
 */
char *strnadd(char *src, const char *str1, int n);


/**
 * Returns the size of a given previously opened file.
 *
 * @param file
 * @return
 */
unsigned int myfsize(FILE *file);

/**
 * Reads a line but not more than 512 bytes. CRs and LFs are omitted.
 *
 * @param fh the file from which the line should be read.
 * @param buf the bufer to which the line is written. It must cover at least
 *  512 bytes.
 * @return 0 on success, else 1.
 */
int myreadline(FILE *fh, char *buf);

/**
 * Compares the dates of the two files. Returns > 0 if the first arg
 * is newer, 0 equal, < 0 older. Not existing files means very old.
 *
 * @param file1
 * @param file2
 * @return
 */
int myfiledatecmp(char *file1, char *file2);

/**
 * Copies the contents of file to another file. Always overwrites the
 * contents of the file pointed by destname.
 *
 * @param sourcename
 * @param destname
 * @return 1 on success, 0 otherwise.
 */
int myfilecopy(const char *sourcename, const char *destname);

/**
 * @brief Delete a given directory and all its contents.
 *
 * @param path defines the location of the directory that should be deleted.
 * @return 0 on failure, otherwise something different.
 * @note this function is recursive
 */
int mydeletedir(const char *path);

/**
 * Wraps a text at the given border
 *
 * @param text defines the text which should be wrapped. The contents is overwritten.
 * @param border
 */
void wrap_text(char *text, int border);

/**
 * Determine the longest common prefix of the given strings.
 *
 * @param strings
 * @param num
 * @return
 */
int longest_common_prefix(char * const *strings, int num);

/**
 * Returns the common longest substring of the given num strings.
 *
 * @param strings an array of strings
 * @param num number of strigs within the array
 * @param pos_in_a_ptr where the position of the substring with respect to a is stored
 * @param len_ptr where the length of a longest common substring is stored.
 * @return 1 when successful
 */
int longest_common_substring(const char **strings, int num, int *pos_in_a, int *len);

/**
 * Returns 1 if a given array contains a given string (case insensitive).
 *
 * @param strings
 * @param str
 * @return
 */
int array_contains(char **strings, const char *str);

/**
 * Returns 1 if a given array contains a given utf8 string (case insensitive).
 *
 * @param strings
 * @param str
 * @return
 */
int array_contains_utf8(char **strings, const char *str);

/**
 * Returns the index of the string within the array or -1 on failure.
 * The string matching function is case insensitive.
 * Assumes 8 bit encoding.
 *
 * @param strings
 * @param str
 * @return
 */
int array_index(char **strings, const char *str);

/**
 * Returns the index of the string within the array or -1 on failure. The
 * string matching function is case insensitive. Assumes utf8 encoding.
 *
 * @param strings
 * @param str
 * @return
 */
int array_index_utf8(char **strings, const char *str);

/**
 * Add the string str to a the given string array. Returns the new array which must
 * be used then. Use only rarly because its slow! Indented for easier creation of
 * small arrays. strings might be NULL.
 *
 * @param strings
 * @param str defines the string to be added. It is duplicated.
 * @return the new array or NULL.
 */
char **array_add_string(char **strings, const char *str);

/**
 * Add the strings of the src array to the dest array. Returns the new array
 * which must be used then. Use only rarely because its slow! Indented for easier
 * creation of small arrays. dest and src might be NULL. (if both are NULL,
 * NULL is returned). While src is not touched at all, dest must not be used
 * after the call.
 *
 * @param dest
 * @param src
 * @return the new array or NULL in case of an error.
 */
char **array_add_array(char **dest, char **src);

/**
 * Returns the length of a string array (safe to call with NULL pointer).
 *
 * @param strings
 * @return
 */
int array_length(char **strings);

/**
 * Duplicates an array of strings. Safe to call it with a NULL pointer.
 *
 * @param rcp
 * @return the duplicated string array or NULL.
 */
char **array_duplicate(char **rcp);

/**
 * Takes an array of strings and return an array of strings parsed with
 * sm_parese_pattern(). Safe to call it with a NULL pointer (returns NULL then)
 *
 * @param str
 * @param flags
 * @return
 */
char **array_duplicate_parsed(char **str, int flags);

/**
 * Frees an array of strings. Safe to call this with NULL pointer.
 *
 * @param string_array
 */
void array_free(char **string_array);

/**
 * Replace the string in the given array at the given index with
 * the given string. The memory of the old string is freed.
 *
 * @param strings defines the array in which the string should be replaced.
 * @param idx defines the index of the to-be-replaced string.
 * @param str the string which is going to replace the other.
 * @return the new array or NULL.
 */
char **array_replace_idx(char **strings, int idx, const char *str);

/**
 * Remove the string given by the index from the given string array.
 * Also frees the memory occupied by the removed entry.
 *
 * @param strings
 * @param idx
 * @return
 */
char **array_remove_idx(char **strings, int idx);

/**
 * Combines two path components and returns the result.
 *
 * @param drawer
 * @param file
 * @return the combined path allocated via malloc().
 */
char *mycombinepath(const char *drawer, const char *file);

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

/**
 * Initialize the given string and reserve the requested amount of memory
 * of the string.
 *
 * @param string
 * @param size
 * @return
 */
int string_initialize(string *string, unsigned int size);

/**
 * Appends appstring to the string.
 *
 * @param string
 * @param appstr
 * @return whether successful or not.
 */
int string_append(string *string, const char *appstr);

/**
 * Append the string given by appstr but not more than bytes bytes.
 *
 * @param string
 * @param appstr the string to be added.
 *
 * @param bytes
 * @return 1 on success, otherwise 0.
 */
int string_append_part(string *string, const char *appstr, int bytes);

/**
 * Appends a single character to the string.
 *
 * @param string
 * @param c
 * @return
 */
int string_append_char(string *string, char c);

/**
 * Crops a given string by the given (closed interval) coordinates.
 *
 * @param string
 * @param startpos
 * @param endpos
 */
void string_crop(string *string, int startpos, int endpos);


/**
 * Ticks per second
 */
#define TIME_TICKS_PER_SECOND 25

/**
 * @brief Returns a reference number of ticks.
 *
 * A is 1/TIME_TICKS_PER_SECOND of a second. You can use the reference number of
 * ticks with time_ticks_passed() in order to test for small time
 * differences.
 *
 * @return the reference which can be supplied to time_ticks_passed().
 */
unsigned int time_reference_ticks(void);

/**
 * @brief returns the number of ticks that have been passed since the reference.
 *
 * @param reference defines the reference.
 * @return number of ticks that have been passed
 * @see timer_ticks
 */
unsigned int time_ticks_passed(unsigned int reference);

/**
 * @brief returns the number of ms that have been passed since the reference
 * obtained via time_reference_ticks().
 *
 * @param ref defines the reference.
 * @return
 */
unsigned int time_ms_passed(unsigned int ref);

/**
 * Duplicates a string and converts it to utf8 if not already in utf8. The
 * given string is assumed to be encoded in iso latin.
 *
 * @param str the string to be converted.
 * @param utf8 specified if the string is already utf8 in which case no
 *  encoding will be performed.
 * @return the duplicated/encoded string or NULL on failure.
 */
char *utf8strdup(char *str, int utf8);

/**
 * Identifies a given file and return its mime type
 *
 * @param fname
 * @return
 */
char *identify_file(char *fname);

#endif

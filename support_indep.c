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
 * @brief Contains support function which are platform independent.
 *
 * @file support_indep.c
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h> /* dir stuff */
#if 0   /* FIXME */
#include <stat.h>
#endif
#include <sys/stat.h>

#include "codesets.h"
#include "debug.h"
#include "support.h"
#include "support_indep.h"

/**
 * Returns whether the given string has spaces.
 *
 * @param str
 * @return
 */
int has_spaces(const char *str)
{
	char c;

	while ((c = *str++))
		if (isspace((unsigned char)c))
			return 1;
	return 0;
}

/**
 * Compares a string in a case-sensitive manner. Accepts NULL pointers
 *
 * @param str1
 * @param str2
 * @return
 */
int mystrcmp(const char *str1, const char *str2)
{
	if (!str1)
	{
		if (str2) return -1;
		return 0;
	}

	if (!str2) return 1;

	return strcmp(str1,str2);
}

/**
 * A replacement for strncmp() as this doesn't return the same values as
 * strcmp().
 *
 * @param str1
 * @param str2
 * @param len
 * @return
 */
int mystrncmp(unsigned char *str1, unsigned char *str2,int len)
{
	while (len)
	{
		int d;
		if ((d = (int)*str1++ - (int)*str2++)) return d;
		len--;
	}
	return 0;
}

/**
 * Compares a string case insensitive. Accepts NULL pointers.
 *
 * @param str1
 * @param str2
 * @return
 */
int mystricmp(const char *str1, const char *str2)
{
	if (!str1)
	{
		if (str2) return -1;
		return 0;
	}

	if (!str2) return 1;

#ifdef HAVE_STRCASECMP
	return strcasecmp(str1,str2);
#else
	return stricmp(str1,str2);
#endif
}
/**
 * Compares a string case insensitive but not more than n characters.
 * Accepts NULL pointers
 *
 * @param str1
 * @param str2
 * @param n
 * @return
 */
int mystrnicmp(const char *str1, const char *str2, int n)
{
	if (!n) return 0;
	if (!str1)
	{
		if (str2) return -1;
		return 0;
	}

	if (!str2) return 1;

#ifdef HAVE_STRNCASECMP
	return strncasecmp(str1,str2,n);
#else
	return strnicmp(str1,str2,n);
#endif
}

/**
 * Checks if a string is inside a string (not case sensitive).
 *
 * @param str1
 * @param str2
 * @return
 */
char *mystristr(const char *str1, const char *str2)
{
	int str2_len;

	if (!str1 || !str2) return NULL;

	str2_len = strlen(str2);

	while (*str1)
	{
		if (!mystrnicmp(str1,str2,str2_len))
			return (char*)str1;
		str1++;
	}
	return NULL;
}

/**
 * Duplicates a string. NULL is accepted (will return NULL).
 * A null byte string will also return NULL.
 *
 * @param str
 * @return
 */
char *mystrdup(const char *str)
{
	char *new_str;
	int len;

	if (!str) return NULL;
	len = strlen(str);
	if (!len) return NULL;

	if ((new_str = (char*)malloc(len+1)))
		strcpy(new_str,str);

	return new_str;
}

/**
 * Like mystrdup() but you can limit the chars. A 0 byte is guaranteed.
 * The string is allocated via malloc().
 *
 * @param str1
 * @param n
 * @return
 */
char *mystrndup(const char *str1, int n)
{
	char *dest;

	if ((dest = (char*)malloc(n+1)))
	{
		if (str1) strncpy(dest,str1,n);
		else dest[0] = 0;

		dest[n]=0;
	}
	return dest;
}

/**
 * @brief Like strncpy() but ensures that the string is always 0 terminated
 *
 * @param dest where to copy the source
 * @param src the source
 * @param n number of bytes to be copied
 * @return the length of the source string.
 */
size_t mystrlcpy(char *dest, const char *src, size_t n)
{
	size_t len = strlen(src);
	size_t num_to_copy = (len >= n) ? n-1 : len;
	if (num_to_copy>0)
		memcpy(dest, src, num_to_copy);
	dest[num_to_copy] = '\0';
	return len;
}

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
char *mystpcpy(char *dest, const char *src)
{
	if (!src) return dest;
	while((*dest = *src))
	{
		dest++;
		src++;
	}
	return dest;
}

/**
 * Returns a new string derived from str in which all occurrences of
 * from are replaced by to.
 *
 * @param src
 * @param from
 * @param to
 * @return the new string or NULL. If not NULL, the string must be freed with free().
 */
char *mystrreplace(const char *src, const char *from, const char *to)
{
	const char *start;

	string d;
	if (!(string_initialize(&d,1024)))
		return NULL;

	start = src;
	while ((src = strstr(start,from)))
	{
		string_append_part(&d,start,src-start);
		string_append(&d,to);
		start = src + strlen(from);
	}
	string_append(&d,start);
	return d.str;
}

/**
 * Returns the size of a given previously opened file.
 *
 * @param file
 * @return
 */
unsigned int myfsize(FILE *file)
{
	unsigned int size;
	fseek(file, 0L, SEEK_END);
	size = ftell(file);
	fseek(file, 0L, SEEK_SET);
	return size;
}

/**
 * Concatenates str2 to str1, and frees str1.
 *
 * @param str1 defines the first part of the new string. This string is freed.
 * @param str2 defines the second part of the new string. This string is not freed.
 * @return the newly created concatenated string.
 */
char *mystrcat(char *str1, char *str2)
{
	int len = strlen(str1) + strlen(str2) + 1;
	char *rc = malloc(len);

	if(rc != NULL)
	{
		strcpy(rc, str1);
		strcat(rc, str2);
	}

	free(str1);

	return rc;
}

/**
 * Compares the dates of the two files. Returns > 0 if the first arg
 * is newer, 0 equal, < 0 older. Not existing files means very old.
 *
 * @param file1
 * @param file2
 * @return
 */
int myfiledatecmp(char *file1, char *file2)
{
	struct stat *s1 = NULL, *s2 = NULL;
	int rc1,rc2;
	int rc = 0;

	if ((s1 = (struct stat*)malloc(sizeof(struct stat))))
	{
		if ((s2 = (struct stat*)malloc(sizeof(struct stat))))
		{
			rc1 = stat(file1,s1);
			rc2 = stat(file2,s2);

			if (rc1 == -1 && rc2 == -1) rc = 0;
			else if (rc1 == -1) rc = -1;
			else if (rc2 == -1) rc = 1;
			else if (s1->st_mtime == s2->st_mtime) rc = 0;
			else if (s1->st_mtime > s2->st_mtime) rc = 1;
			else rc = -1;
		}
	}
	free(s2);
	free(s1);
	return rc;
}

/**
 * Copies the contents of file to another file. Always overwrites the
 * contents of the file pointed by destname.
 *
 * @param sourcename
 * @param destname
 * @return 1 on success, 0 otherwise.
 */
int myfilecopy(const char *sourcename, const char *destname)
{
	int rc = 0;
	FILE *in = fopen(sourcename,"rb");

	if (in)
	{
		FILE *out = fopen(destname,"wb");
		if (out)
		{
			void *buf = malloc(8192);
			if (buf)
			{
				rc = 1;
				while (!feof(in))
				{
					int blocks = fread(buf,1,8192,in);
					fwrite(buf,1,blocks,out);
				}
				free(buf);
			}
			fclose(out);
		}
		fclose(in);
	}
	return rc;
}

/**
 * @brief Delete a given directory and all its contents.
 *
 * @param path defines the location of the directory that should be deleted.
 * @return 0 on failure, otherwise something different.
 * @note this function is recursive
 */
int mydeletedir(const char *path)
{
	DIR *dfd; /* directory descriptor */
	struct dirent *dptr; /* dir entry */
	struct stat *st;
	char *buf;

	if (!(buf = malloc(512))) return 0;
	if (!(st = malloc(sizeof(struct stat))))
	{
		free(buf);
		return 0;
	}

	if ((dfd = opendir(path)))
	{
		while ((dptr = readdir(dfd)) != NULL)
		{
			if (!strcmp(".",dptr->d_name) || !strcmp("..",dptr->d_name)) continue;
			mystrlcpy(buf,path,512);
			sm_add_part(buf,dptr->d_name,512);

			if (!stat(buf,st))
			{
				if (st->st_mode & S_IFDIR)
				{
					mydeletedir(buf);
				} else
				{
					remove(buf);
				}
			}
		}
		closedir(dfd);
	}

	free(st);
	free(buf);

	remove(path);
	return 1;
}

/**
 * Wraps a text at the given border
 *
 * @param text defines the text which should be wrapped. The contents is overwritten.
 * @param border
 */
void wrap_text(char *text, int border)
{
	unsigned char *buf = (unsigned char*)text;
	unsigned char c;
	int pos = 0;

	unsigned char *last_space_buf = NULL;
	int last_space_pos = 0;

	border--;

	while ((c = *buf))
	{
		if (isspace(c))
		{
			if (c == '\n')
			{
				pos = 0;
				buf++;
				last_space_buf = NULL;
				continue;
			}
			last_space_buf = buf;
			last_space_pos = pos;
		}

		if (pos >= border && last_space_buf)
		{
			*last_space_buf = '\n';
			last_space_buf = NULL;
			pos = pos - last_space_pos;
			buf++;
			continue;
		}

		pos++;
		buf++;
	}
}

/**
 * Callback for qsort() using strcmp().
 *
 * @param a
 * @param b
 * @return
 */
static int qsort_strcmp_callback(const void *a, const void *b)
{
    return strcmp((const char *)*(char **)a, (const char *)*(char **)b);
}

/**
 * Determine the longest common prefix of the given strings.
 *
 * @param strings
 * @param num
 * @return
 */
int longest_common_prefix(char * const *strings, int num)
{
	int p = 0;

	if (num < 1)
		return 0;

	while (1)
	{
		int i;

		char c = strings[0][p];
		if (!c) break;

		for (i=1;i<num;i++)
		{
			if (strings[i][p] != c)
				return p;
		}
		p++;
	}

	return p;
}

/**
 * Returns the common longest substring of the given num strings.
 *
 * @param strings an array of strings
 * @param num number of strigs within the array
 * @param pos_in_a_ptr where the position of the substring with respect to a is stored
 * @param len_ptr where the length of a longest common substring is stored.
 * @return 1 when successful
 */
int longest_common_substring(const char **strings, int num, int *pos_in_a_ptr, int *len_ptr)
{
	/*
	 * The method that is used here to solve the problem is based on suffix
	 * array. Basically, at first all strings are concatenated. Then the
	 * suffixes of this string are sorted. We then traverse over the sorted
	 * suffixes and determine common prefixes, for each tuple that holds suffixes
	 * stemming from all the different strings. A longest common prefix of these
	 * suffixes is than equivalent to a longest common substring.
	 *
	 * The idea for the implementation has been come from:
	 *
	 *  http://www.roman10.net/suffix-array-part-3-longest-common-substring-lcs/
	 *
	 * Note that sorting etc. could be improved.
	 */

	int rc;
	int i,j;
	int lcs = 0;
	int pos_in_a = 0;
	int total_len = 0;

	int *starts;
	char **sp = NULL;
	char *s = NULL;
	char *encountered = NULL;

	rc = 0;

	if (!(starts = (int*)malloc(num*sizeof(starts[0]))))
		return 0;
	if (!(encountered = (char*)malloc(num*sizeof(encountered[0]))))
		goto out;

	for (i=0;i<num;i++)
		total_len += strlen(strings[i]);
	total_len++;

	/* Allocate memory */
	if (!(sp = (char**)malloc(sizeof(char*)*total_len)))
		goto out;
	if (!(s = (char*)malloc(sizeof(char)*total_len)))
		goto out;

	/* Build the big string s, it will be 0-terminated */
	s[0] = 0;
	for (i=0;i<num;i++)
	{
		starts[i] = strlen(s);
		strcat(s,strings[i]);
	}

	/* Every suffix is represented by a pointer which is set to the first character
	 * of the pointer */
	for (i=0;i<total_len;i++)
		sp[i] = &s[i];

	/* Sort suffixes (=the pointers) */
	qsort(sp, total_len, sizeof(char *), qsort_strcmp_callback);

	for (i=0;i<total_len - num;i++)
	{
		int from_different_strings = 1;

		memset(encountered,0,num*sizeof(char));

		for (j=0;j<num;j++)
		{
			int k;

			/* Relative position of the suffix within our big string s */
			int p = sp[i+j]-s;

			/* To which string belongs the suffix? */
			for (k=1;k<num;k++)
				if (p < starts[k])
					break;
			k--;

			if (k >= num)
				goto out;

			/* Mark string as encountered. If already encountered, we
			 * can leave the loop and conclude that at least two suffixes
			 * must be from the same string */
			if (encountered[k])
			{
				from_different_strings = 0;
				break;
			}
			encountered[k] = 1;
		}

		if (from_different_strings)
		{
			int lcp;

			/* It's clear now that subsequent sorted suffixes are all from
			 * different strings, thus we can determine the longest
			 * common prefix.
			 */

			lcp = longest_common_prefix(&sp[i],num);
			/* Remember it, if it is larger than a previous common prefix */
			if (lcp > lcs)
			{
				for (j=0;j<num-1;j++)
				{
					if (sp[i+j]-s < starts[1])
						break;
				}
				pos_in_a = sp[i+j]-s;
				lcs = lcp;
			}
		}
	}

	*pos_in_a_ptr = pos_in_a;
	*len_ptr = lcs;
	rc = 1;

out:
	free(sp);
	free(encountered);
	free(s);
	free(starts);
	return rc;
}

/**
 * Returns 1 if a given array contains a given string (case insensitive).
 *
 * @param strings
 * @param str
 * @return
 */
int array_contains(char **strings, char *str)
{
	return array_index(strings,str)!=-1;
}

/**
 * Returns 1 if a given array contains a given utf8 string (case insensitive).
 *
 * @param strings
 * @param str
 * @return
 */
int array_contains_utf8(char **strings, char *str)
{
	return array_index_utf8(strings,str)!=-1;
}

/**
 * Returns the index of the string within the array or -1 on failure.
 * The string matching function is case insensitive.
 * Assumes 8 bit encoding.
 *
 * @param strings
 * @param str
 * @return
 */
int array_index(char **strings, char *str)
{
	int i;
	if (!strings) return -1;
	for (i=0;strings[i];i++)
	{
		if (!mystricmp(strings[i],str)) return i;
	}
	return -1;
}

/**
 * Returns the index of the string within the array or -1 on failure. The
 * string matching function is case insensitive. Assumes utf8 encoding.
 *
 * @param strings
 * @param str
 * @return
 */
int array_index_utf8(char **strings, char *str)
{
	int i;
	if (!strings) return -1;
	for (i=0;strings[i];i++)
	{
		if (!utf8stricmp(strings[i],str)) return i;
	}
	return -1;
}


/**
 * Replace the string in the given array at the given index with
 * the given string. The memory of the old string is freed.
 *
 * @param strings defines the array in which the string should be replaced.
 * @param idx defines the index of the to-be-replaced string.
 * @param str the string which is going to replace the other.
 * @return the new array or NULL.
 */
char **array_replace_idx(char **strings, int idx, char *str)
{
	char *dup = mystrdup(str);
	if (!dup) return NULL;

	if (strings[idx]) free(strings[idx]);
	strings[idx] = dup;
	return strings;
}

/**
 * Remove the string given by the index from the given string array.
 * Also frees the memory occupied by the removed entry.
 *
 * @param strings
 * @param idx
 * @return
 */
char **array_remove_idx(char **strings, int idx)
{
	int len = array_length(strings);
	free(strings[idx]);
	memmove(&strings[idx],&strings[idx+1],(len - idx)*sizeof(char*));
	return strings;
}

/**
 * Add the string str to a the given string array. Returns the new array which must
 * be used then. Use only rarly because its slow! Indented for easier creation of
 * small arrays. strings might be NULL.
 *
 * @param strings
 * @param str defines the string to be added. It is duplicated.
 * @return the new array or NULL.
 */
char **array_add_string(char **strings, char *str)
{
	int length = array_length(strings);
	char **new_strings;

	if ((new_strings = (char**)realloc(strings,(length+2)*sizeof(char*))))
	{
		new_strings[length]=mystrdup(str);
		new_strings[length+1]=NULL;
	}
	return new_strings;
}

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
char **array_add_array(char **dest, char **src)
{
	int dest_len = array_length(dest);
	int src_len = array_length(src);
	char **new_strings;

	/* Nothing to add */
	if (!src_len) return dest;

	if ((new_strings = (char**)realloc(dest,(dest_len + src_len + 1)*sizeof(char*))))
	{
		int i,j;

		for (i=0,j=dest_len;i<src_len;i++,j++)
			new_strings[j] = mystrdup(src[i]);
		new_strings[j] = NULL;
	}
	return new_strings;
}

/**
 * Returns the length of a string array (safe to call with NULL pointer).
 *
 * @param strings
 * @return
 */
int array_length(char **strings)
{
	int i;
	if (!strings) return 0;
	i = 0;
	for (;strings[i];i++);
	return i;
}

/**
 * Duplicates an array of strings. Safe to call it with a NULL pointer.
 *
 * @param rcp
 * @return the duplicated string array or NULL.
 */
char **array_duplicate(char **rcp)
{
	char **newrcp;
	int rcps=0;
	if (!rcp) return NULL;
	while (rcp[rcps]) rcps++;

	if ((newrcp = (char**)malloc((rcps+1)*sizeof(char*))))
	{
		int i;
		for (i=0;i<rcps;i++)
		{
			newrcp[i] = mystrdup(rcp[i]);
		}
		newrcp[i] = NULL;
	}
	return newrcp;
}

/**
 * Takes an array of strings and return an array of strings parsed with
 * sm_parese_pattern(). Safe to call it with a NULL pointer (returns NULL then)
 *
 * @param str
 * @param flags
 * @return
 */
char **array_duplicate_parsed(char **str, int flags)
{
	char **newpat;
	int pats=0;
	if (!str) return NULL;
	while (str[pats]) pats++;

	if ((newpat = (char**)malloc((pats+1)*sizeof(char*))))
	{
		int i;
		for (i=0;i<pats;i++)
		{
			newpat[i] = sm_parse_pattern(str[i], flags);
		}
		newpat[i] = NULL;
	}
	return newpat;
}

/**
 * Callback for qsort() using utf8stricmp().
 *
 * @param a
 * @param b
 * @return
 */
static int qsort_utf8stricmp_callback(const void *a, const void *b)
{
    return utf8stricmp((const char *)*(char **)a, (const char *)*(char **)b);
}

/**
 * Sorts the given string array in a case-insenstive manner.
 *
 * @param string_array
 */
void array_sort_uft8(char **string_array)
{
	int len;

	len = array_length(string_array);

	if (len == 0)
		return;

	qsort(string_array, len, sizeof(char *), qsort_utf8stricmp_callback);
}

/**
 * Frees an array of strings. Safe to call this with NULL pointer.
 *
 * @param string_array
 */
void array_free(char **string_array)
{
	char *string;
	int i = 0;

	if (!string_array) return;

	while ((string = string_array[i++]))
		free(string);
	free(string_array);
}

/**
 * Initialize the given string and reserve the requested amount of memory
 * of the string.
 *
 * @param string
 * @param size
 * @return
 */
int string_initialize(string *string, unsigned int size)
{
	if (!size) size = 1;
	string->str = malloc(size);
	if (!string->str) return 0;
	string->str[0] = 0;
	string->allocated = size;
	string->len = 0;
	return 1;
}

/**
 * Append the string given by appstr but not more than bytes bytes.
 *
 * @param string
 * @param appstr the string to be added.
 *
 * @param bytes
 * @return 1 on success, otherwise 0.
 */
int string_append_part(string *string, const char *appstr, int bytes)
{
	int alloclen;

	if (!appstr || !bytes) return 1;

	alloclen = string->allocated;

	while (bytes + string->len >= alloclen) /* >= because of the ending 0 byte */
		alloclen *= 2;

	if (alloclen != string->allocated)
	{
		char *newstr;

		/* We have to allocate more memory */
		newstr = realloc(string->str,alloclen);
		if (!newstr) return 0;
		string->allocated = alloclen;
		string->str = newstr;
	}

	strncpy(&string->str[string->len],appstr,bytes);
	string->len += bytes;
	string->str[string->len] = 0;
	return 1;

}

/**
 * Appends appstring to the string.
 *
 * @param string
 * @param appstr
 * @return whether successful or not.
 */
int string_append(string *string, const char *appstr)
{
	if (!appstr) return 1;
	return string_append_part(string,appstr,strlen(appstr));
}

/**
 * Appends a single character to the string.
 *
 * @param string
 * @param c
 * @return
 */
int string_append_char(string *string, char c)
{
	return string_append_part(string,&c,1);
}

/**
 * Crops a given string by the given (closed interval) coordinates.
 *
 * @param string
 * @param startpos
 * @param endpos
 */
void string_crop(string *string, int startpos, int endpos)
{
	if (startpos == 0)
	{
		string->len = endpos;
		string->str[endpos] = 0;
	} else
	{
		int newlen = endpos - startpos + 1;
		memmove(string->str, &string->str[startpos], newlen);
		string->len = newlen;
		string->str[newlen] = 0;
	}
}

/**
 * Combines two path components and returns the result.
 *
 * @param drawer
 * @param file
 * @return the combined path allocated via malloc().
 */
char *mycombinepath(char *drawer, char *file)
{
	int len;
	char *dest;
	len = strlen(drawer)+strlen(file)+4;
	if ((dest = malloc(len)))
	{
		strcpy(dest,drawer);
		sm_add_part(dest,file,len);
	}
	return dest;
}

/**
 * @brief Returns a reference number of ticks.
 *
 * A is 1/TIME_TICKS_PER_SECOND of a second. You can use the reference number of
 * ticks with time_ticks_passed() in order to test for small time
 * differences.
 *
 * @return the reference which can be supplied to time_ticks_passed().
 */
unsigned int time_reference_ticks(void)
{
	unsigned int micros = sm_get_current_micros();
	unsigned int seconds = sm_get_current_seconds();

	return seconds * TIME_TICKS_PER_SECOND + micros / (1000000/TIME_TICKS_PER_SECOND);
}

/**
 * @brief returns the number of ticks that have been passed since the reference.
 *
 * @param reference defines the reference.
 * @return number of ticks that have been passed
 * @see timer_ticks
 */
unsigned int time_ticks_passed(unsigned int reference)
{
	unsigned int now = time_reference_ticks();
	if (now > reference) return now - reference;
	return (unsigned int)(reference - now);
}

/**
 * @brief returns the number of ms that have been passed since the reference
 * obtained via time_reference_ticks().
 *
 * @param ref defines the reference.
 * @return
 */
unsigned int time_ms_passed(unsigned int ref)
{
	unsigned int passed = time_ticks_passed(ref);
	return passed * 1000 / TIME_TICKS_PER_SECOND;
}

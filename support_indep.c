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
** support_indep.c
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "support_indep.h"

/******************************************************************
 Compares a string case sensitive. Accepts NULL pointers
*******************************************************************/
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

/******************************************************************
 Compares a string case insensitive. Accepts NULL pointers
*******************************************************************/
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

/******************************************************************
 Compares a string case insensitive and n characters.
 Accepts NULL pointers
*******************************************************************/
int mystrnicmp(const char *str1, const char *str2, int n)
{
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

/**************************************************************************
 Checks if a string is inside a string (not case sensitive)
**************************************************************************/
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

/******************************************************************
 returns the length of a string. Accepts NULL pointer (returns 0
 then)
*******************************************************************/
#if 0
unsigned int mystrlen(const char *str)
{
	if (!str) return 0;
	return strlen(str);
}
#endif

/******************************************************************
 Duplicates a string. NULL is accepted (will return NULL).
 A null byte string will also return NULL.
*******************************************************************/
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

/**************************************************************************
 Like mystrdup() but you can limit the chars. A 0 byte is guaranted.
 The string is allocated via malloc().
**************************************************************************/
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

/**************************************************************************
 Like strncpy() but ensures that the string is always 0 terminted
**************************************************************************/
size_t mystrlcpy(char *dest, const char *src, size_t n)
{
	size_t len = strlen(src);
	size_t num_to_copy = (len >= n) ? n-1 : len;
	if (num_to_copy>0)
		memcpy(dest, src, num_to_copy);
	dest[num_to_copy] = '\0';
	return len;
}

/**************************************************************************
 Returns 1 if a given array contains a givem string (case insensitive)
**************************************************************************/
int array_contains(char **strings, char *str)
{
	int i;
	if (!strings) return 0;
	for (i=0;strings[i];i++)
	{
		if (!mystricmp(strings[i],str)) return 1;
	}
	return 0;
}

/**************************************************************************
 Add the string str to an array. Returns the new array which must be used
 then. Use only rarly because its slow! Indented for easier creation of
 small arrays. strings might be NULL.
**************************************************************************/
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

/**************************************************************************
 Returns the length of a string array (safe to call with NULL pointer)
**************************************************************************/
int array_length(char **strings)
{
	int i;
	if (!strings) return 0;
	i = 0;
	for (;strings[i];i++);
	return i;
}

/**************************************************************************
 Duplicates an array of strings. Safe to call it with a NULL pointer
 (returns NULL then)
**************************************************************************/
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

/**************************************************************************
 Frees an array of strings. Safe to call this with NULL pointer.
**************************************************************************/
void array_free(char **string_array)
{
	char *string;
	int i = 0;

	if (!string_array) return;

	while ((string = string_array[i++]))
		free(string);
	free(string_array);
}


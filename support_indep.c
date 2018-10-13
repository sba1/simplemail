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
#include <sys/stat.h>

#include "codesets.h"
#include "debug.h"
#include "support_indep.h"

#include "support.h"
#include "timesupport.h"

/*****************************************************************************/

void sm_get_current_time(unsigned int *seconds, unsigned int *mics)
{
	do
	{
		*seconds = sm_get_current_seconds();
		*mics = sm_get_current_micros();
	} while (sm_get_current_seconds() != *seconds);
}

/*****************************************************************************/

int sm_get_addr_start_pos(char *contents, int pos)
{
	int start_pos = pos;

	if (start_pos && contents[start_pos] == ',')
		start_pos--;

	while (start_pos)
	{
		if (contents[start_pos] == ',')
		{
			start_pos++;
			break;
		}
		start_pos--;
	}
	return start_pos;
}

/*****************************************************************************/

char *sm_get_to_be_completed_address_from_line(char *contents, int pos)
{
	char *buf;
	int start_pos;

	start_pos = sm_get_addr_start_pos(contents, pos);

	while (start_pos < pos && contents[start_pos]==' ')
		start_pos++;

	buf = (char *)malloc(pos - start_pos + 1);
	if (!buf) return NULL;
	strncpy(buf,&contents[start_pos],pos - start_pos);
	buf[pos-start_pos]=0;

	return buf;
}

/*****************************************************************************/

int has_spaces(const char *str)
{
	char c;

	while ((c = *str++))
		if (isspace((unsigned char)c))
			return 1;
	return 0;
}

/*****************************************************************************/

char *get_key_value(char *str, const char *key)
{
	int len = strlen(key);
	if (!mystrnicmp(str,key,len))
	{
		unsigned char c;
		str += len;

		/* skip spaces */
		while ((c = *str) && isspace(c)) str++;
		if (*str != '=') return NULL;
		str++;
		/* skip spaces */
		while ((c = *str) && isspace(c)) str++;

		return str;
	}
	return NULL;
}

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

size_t mystrlcpy(char *dest, const char *src, size_t n)
{
	size_t len = strlen(src);
	size_t num_to_copy = (len >= n) ? n-1 : len;
	if (num_to_copy>0)
		memcpy(dest, src, num_to_copy);
	dest[num_to_copy] = '\0';
	return len;
}

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

unsigned int myfsize(FILE *file)
{
	unsigned int size;
	fseek(file, 0L, SEEK_END);
	size = ftell(file);
	fseek(file, 0L, SEEK_SET);
	return size;
}

/*****************************************************************************/

int myreadline(FILE *fh, char *buf)
{
	int len;

	if (!fgets(buf,512,fh)) return 0;

	len = strlen(buf);
	if (len)
	{
		if (buf[len-1] == '\n') buf[len-1] = 0;
		else if (buf[len-2] == '\r') buf[len-2] = 0;
	}
	return 1;
}

/*****************************************************************************/

char *mystrcat(char *str1, char *str2)
{
	int len = strlen(str1) + strlen(str2) + 1;
	char *rc = (char *)malloc(len);

	if(rc != NULL)
	{
		strcpy(rc, str1);
		strcat(rc, str2);
	}

	free(str1);

	return rc;
}


/**************************************************************************
 Support function: Appends a string to another, the result string is
 malloced. Should be placed otherwhere
**************************************************************************/
char *strdupcat(const char *string1, const char *string2)
{
	char *buf = (char*)malloc(strlen(string1) + strlen(string2) + 1);
	if (buf)
	{
		strcpy(buf,string1);
		strcat(buf,string2);
	}
	return buf;
}

/*****************************************************************************/

#ifndef __USE_XOPEN2K8
char *strndup(const char *str1, int n)
{
	char *dest = (char*)malloc(n+1);
	if (dest)
	{
		strncpy(dest,str1,n);
		dest[n]=0;
	}
	return dest;
}
#endif

/*****************************************************************************/

char *stradd(char *src, const char *str1)
{
	int len = mystrlen(src);
	char *dest;

	if ((dest = (char*)realloc(src,len+mystrlen(str1)+1)))
	{
		if (str1) strcpy(&dest[len],str1);
		else dest[len]=0;
	}
	return dest;
}

/*****************************************************************************/

char *strnadd(char *src, const char *str1, int n)
{
	int len = mystrlen(src);
	char *dest;

	if ((dest = (char*)realloc(src,len+n+1)))
	{
		if (str1)
		{
			strncpy(&dest[len],str1,n);
			dest[len+n]=0;
		}
		else dest[len]=0;
	}
	return dest;
}

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

int mydeletedir(const char *path)
{
	DIR *dfd; /* directory descriptor */
	struct dirent *dptr; /* dir entry */
	struct stat *st;
	char *buf;

	if (!(buf = (char *)malloc(512))) return 0;
	if (!(st = (struct stat *)malloc(sizeof(struct stat))))
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

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

int array_contains(char **strings, const char *str)
{
	return array_index(strings,str)!=-1;
}

/*****************************************************************************/

int array_contains_utf8(char **strings, const char *str)
{
	return array_index_utf8(strings,str)!=-1;
}

/*****************************************************************************/

int array_index(char **strings, const char *str)
{
	int i;
	if (!strings) return -1;
	for (i=0;strings[i];i++)
	{
		if (!mystricmp(strings[i],str)) return i;
	}
	return -1;
}

/*****************************************************************************/

int array_index_utf8(char **strings, const char *str)
{
	int i;
	if (!strings) return -1;
	for (i=0;strings[i];i++)
	{
		if (!utf8stricmp(strings[i],str)) return i;
	}
	return -1;
}

/*****************************************************************************/

char **array_replace_idx(char **strings, int idx, const char *str)
{
	char *dup = mystrdup(str);
	if (!dup) return NULL;

	if (strings[idx]) free(strings[idx]);
	strings[idx] = dup;
	return strings;
}

/*****************************************************************************/

char **array_remove_idx(char **strings, int idx)
{
	int len = array_length(strings);
	free(strings[idx]);
	memmove(&strings[idx],&strings[idx+1],(len - idx)*sizeof(char*));
	return strings;
}

/*****************************************************************************/

char **array_add_string(char **strings, const char *str)
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

/*****************************************************************************/

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

/*****************************************************************************/

int array_length(char **strings)
{
	int i;
	if (!strings) return 0;
	i = 0;
	for (;strings[i];i++);
	return i;
}

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

void array_sort_uft8(char **string_array)
{
	int len;

	len = array_length(string_array);

	if (len == 0)
		return;

	qsort(string_array, len, sizeof(char *), qsort_utf8stricmp_callback);
}

/*****************************************************************************/

void array_free(char **string_array)
{
	char *string;
	int i = 0;

	if (!string_array) return;

	while ((string = string_array[i++]))
		free(string);
	free(string_array);
}

/*****************************************************************************/

int string_initialize(string *string, unsigned int size)
{
	if (!size) size = 1;
	string->str = (char *)malloc(size);
	if (!string->str) return 0;
	string->str[0] = 0;
	string->allocated = size;
	string->len = 0;
	return 1;
}

/*****************************************************************************/

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
		newstr = (char *)realloc(string->str,alloclen);
		if (!newstr) return 0;
		string->allocated = alloclen;
		string->str = newstr;
	}

	memcpy(&string->str[string->len],appstr,bytes);
	string->len += bytes;
	string->str[string->len] = 0;
	return 1;

}

/*****************************************************************************/

int string_append(string *string, const char *appstr)
{
	if (!appstr) return 1;
	return string_append_part(string,appstr,strlen(appstr));
}

/*****************************************************************************/

int string_append_char(string *string, char c)
{
	return string_append_part(string,&c,1);
}

/*****************************************************************************/

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

/*****************************************************************************/

char *mycombinepath(const char *drawer, const char *file)
{
	int len;
	char *dest;
	len = strlen(drawer)+strlen(file)+4;
	if ((dest = (char *)malloc(len)))
	{
		strcpy(dest,drawer);
		sm_add_part(dest,file,len);
	}
	return dest;
}

/*****************************************************************************/

unsigned int time_reference_ticks(void)
{
	unsigned int micros = sm_get_current_micros();
	unsigned int seconds = sm_get_current_seconds();

	return seconds * TIME_TICKS_PER_SECOND + micros / (1000000/TIME_TICKS_PER_SECOND);
}

/*****************************************************************************/

unsigned int time_ticks_passed(unsigned int reference)
{
	unsigned int now = time_reference_ticks();
	if (now > reference) return now - reference;
	return (unsigned int)(reference - now);
}

/*****************************************************************************/

unsigned int time_ms_passed(unsigned int ref)
{
	unsigned int passed = time_ticks_passed(ref);
	return passed * 1000 / TIME_TICKS_PER_SECOND;
}

/*****************************************************************************/

char *utf8strdup(char *str, int utf8)
{
	if (!str) return NULL;
	if (utf8) return mystrdup(str);
	return (char*)utf8create(str,NULL);
}

/*****************************************************************************/

char *identify_file(char *fname)
{
	char *ctype = "application/octet-stream";
	FILE *fh;

	if ((fh = fopen(fname, "r")))
	{
		int len;
		static char buffer[1024], *ext;

		len = fread(buffer, 1, 1023, fh);
		buffer[len] = 0;
		fclose(fh);

		if ((ext = strrchr(fname, '.'))) ++ext;
		else ext = "--"; /* for rx */

		if (!mystricmp(ext, "htm") || !mystricmp(ext, "html"))
			return "text/html";
		else if (!mystrnicmp(buffer, "@database", 9) || !mystricmp(ext, "guide"))
			return "text/x-aguide";
		else if (!mystricmp(ext, "ps") || !mystricmp(ext, "eps"))
			return "application/postscript";
		else if (!mystricmp(ext, "rtf"))
			return "application/rtf";
		else if (!mystricmp(ext, "lha") || !strncmp(&buffer[2], "-lh5-", 5))
			return "application/x-lha";
		else if (!mystricmp(ext, "lzx") || !strncmp(buffer, "LZX", 3))
			return "application/x-lzx";
		else if (!mystricmp(ext, "zip"))
			return "application/x-zip";
		else if (!mystricmp(ext, "rexx") || !mystricmp(ext+strlen(ext)-2, "rx"))
			return "application/x-rexx";
		else if (!strncmp(&buffer[6], "JFIF", 4))
			return "image/jpeg";
		else if (!strncmp(buffer, "GIF8", 4))
			return "image/gif";
		else if (!mystrnicmp(ext, "png",4) || !strncmp(&buffer[1], "PNG", 3))
			return "image/png";
		else if (!mystrnicmp(ext, "tif",4))
			return "image/tiff";
		else if (!strncmp(buffer, "FORM", 4) && !strncmp(&buffer[8], "ILBM", 4))
			return "image/x-ilbm";
		else if (!mystricmp(ext, "au") || !mystricmp(ext, "snd"))
			return "audio/basic";
		else if (!strncmp(buffer, "FORM", 4) && !strncmp(&buffer[8], "8SVX", 4))
			return "audio/x-8svx";
		else if (!mystricmp(ext, "wav"))
			return "audio/x-wav";
		else if (!mystricmp(ext, "mpg") || !mystricmp(ext, "mpeg"))
			return "video/mpeg";
		else if (!mystricmp(ext, "qt") || !mystricmp(ext, "mov"))
			return "video/quicktime";
		else if (!strncmp(buffer, "FORM", 4) && !strncmp(&buffer[8], "ANIM", 4))
			return "video/x-anim";
		else if (!mystricmp(ext, "avi"))
			return "video/x-msvideo";
		else if (mystristr(buffer, "\nFrom:"))
			return "message/rfc822";
	}
	return ctype;
}

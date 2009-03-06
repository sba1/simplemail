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

#ifndef SM__CODESETS_H
#define SM__CODESETS_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

typedef unsigned char	utf8;

struct single_convert
{
	unsigned char code; /* the code in this representation */
	char utf8[8]; /* the utf8 string, first byte is alway the length of the string */
	unsigned int ucs4; /* the full 32 bit unicode */
};

struct codeset
{
	struct node node;
	char *name;
	char *alt_name; /* alternative name */
	char *characterization;
	int read_only; /* 1 if this charset should never be used to write */
	struct single_convert table[256];
	struct single_convert table_sorted[256];
};

char **codesets_supported(void);
int codesets_init(void);
void codesets_cleanup(void);
struct codeset *codesets_find(char *name);
int codesets_unconvertable_chars(struct codeset *codeset, char *text, int text_len);
struct codeset *codesets_find_best(char *text, int text_len, int *error_ptr);

#define utf8size(s) ((s)?(strlen(s)):(0))
#define utf8cpy(dest,src) ((utf8*)strcpy(dest,src))
#define utf8cat(dest,src) ((utf8*)strcat(dest,src))

int isascii7(const char *str);
int utf8len(const utf8 *str);
int utf8realpos(const utf8 *str, int pos);
int utf8charpos(const utf8 *str, int pos);
utf8 *utf8ncpy(utf8 *to, const utf8 *from, int n);
utf8 *utf8create(void *from, char *charset);
utf8 *utf8create_len(void *from, char *charset, int from_len);
int utf8tostr(utf8 *str, char *dest, int dest_size, struct codeset *codeset);
char *utf8tostrcreate(utf8 *str, struct codeset *codeset);
int utf8stricmp(const char *str1, const char *str2);
int utf8stricmp_len(const char *str1, const char *str2, int len);
int utf8tolower(const char *str, char *dest);
char *utf8stristr(const char *str1, const char *str2);
int utf8tochar(utf8 *str, unsigned int *chr, struct codeset *codeset);
char *uft8toucs(char *chr, unsigned int *code);
char *utf7ntoutf8(char *source, int sourcelen);
char *iutf7ntoutf8(char *source, int sourcelen);
char *utf8toiutf7(char *utf8, int sourcelen);
char *utf8topunycode(const utf8 *str, int sourcelen);
utf8 *punycodetoutf8(const char *source, int sourcelen);

#endif

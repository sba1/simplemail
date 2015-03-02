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
** parse.h
*/

#ifndef SM__PARSE_H
#define SM__PARSE_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

#ifndef SM__CODESETS_H
#include "codesets.h"
#endif

/* Needed for __USE_XOPEN2K8 */
#include <string.h>

/* some string functions, should be in strings.c or simliar */
char *strdupcat(const char *string1, const char *string2);

#ifndef __USE_XOPEN2K8
char *strndup(const char *str1, int n);
#endif

char *stradd(char *src, const char *str1);

int needs_quotation(char *str);
int needs_quotation_len(char *str, int len);
int is_token(char *token);

char *parse_addr_spec(char *addr_spec, char **pbuf);

/* the argument for parse_address */
struct parse_address
{
	char *group_name; /* the name of the group, NULL if a single mailbox */
	struct list mailbox_list; /* the list of all mailboxes */
};

/* the nodes of the above mailbox_list */
struct mailbox
{
	struct node node;
	utf8 *phrase; /* ususally a real name */
	char *addr_spec; /* usually the e-mail address */
};

char *parse_mailbox(char *mailbox, struct mailbox *mb);
char *parse_address(char *address, struct parse_address *dest);
void free_address(struct parse_address *addr);

char *parse_token(char *token, char **pbuf);
char *parse_value(char *value, char **pbuf);
void parse_text_string(char *text, utf8 **pbuf);


struct parse_parameter
{
	char *attribute;
	char *value;
};

char *parse_parameter(char *parameter, struct parse_parameter *dest);
char *parse_date(char *buf, int *pday,int *pmonth,int *pyear,int *phour,int *pmin,int *psec, int *pgmt);

#endif

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
 * @file parse.h
 */

#ifndef SM__PARSE_H
#define SM__PARSE_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

#ifndef SM__CODESETS_H
#include "codesets.h"
#endif

/**
 * Check if the string, when send as part of a mail header, needs a quotation.
 *
 * @param str the string to check
 * @return 1, if str needs quotation marks.
 */
int needs_quotation(char *str);

/**
 * Check if the string, when send as part of a mail header, needs a quotation.
 *
 * @param str the string to check
 * @param len number of bytes to check
 * @return 1, if str needs quotation marks.
 */
int needs_quotation_len(const char *str, int len);

/**
 * Check if the given string is a RFC822 token.
 *
 * @param token the string to check
 * @return 1, if the string is a valid RFC822 token
 */
int is_token(char *token);

/**
 * Parse an address.
 *
 * @verbatim
 * addr-spec   =  local-part "@" domain
 * @endverbatim
 *
 * @param addr_spec
 * @param pbuf
 * @return NULL for an parse error or the pointer to the next unparsed character
 */
char *parse_addr_spec(char *addr_spec, char **pbuf);

/** Structure used in parse_address() */
struct parse_address
{
	char *group_name; /* the name of the group, NULL if a single mailbox */
	struct list mailbox_list; /* the list of all mailboxes */
};

/** Nodes of the mailbox_list of parse_address */
struct mailbox
{
	struct node node;
	utf8 *phrase; /* ususally a real name */
	char *addr_spec; /* usually the e-mail address */
};

/**
 * Parse the mailbox specification.
 *
 * @verbatim
 * mailbox = addr-spec | phrase route-addr
 * route_addr = "<" [route] addr-spec ">"
 * @endverbatim
 *
 * @param mailbox pointer to the characters to parse as mailbox
 * @param mb were the result is stored.
 * @return NULL for an parse error or the pointer to the next unparsed character
 */
char *parse_mailbox(char *mailbox, struct mailbox *mb);

/**
 * Parse an address.
 *
 * @verbatim
 * address     =  mailbox / group
 * (actually 1#mailbox / group which is not correct)
 * @endverbatim
 *
 * @param address
 * @param dest
 * @return NULL for an parse error or the pointer to the next unparsed character
 */
char *parse_address(char *address, struct parse_address *dest);

/**
 * Frees all memory for a given address that was allocated in parse_address
 *
 * @param addr
 */
void free_address(struct parse_address *addr);

/**
 * Parse a string as token.
 *
 * @verbatim
 * token  :=  1*<any (ASCII) CHAR except SPACE, CTLs, or tspecials>
 * @endverbatim
 *
 * @param token the pointer to the characters to parse as token
 * @param pbuf will hold the buffer that contains the token only. The string
 *  shall be freed via free() when no longer in use.
 * @return pointer to the first character that is not a token or NULL if the
 *  string was not token
 */
char *parse_token(char *token, char **pbuf);

/**
 * Parse a string as value.
 *
 * @verbatim
 * value := token / quoted-string
 * @endverbatim
 *
 * @param value
 * @param pbuf
 * @return
 */
char *parse_value(char *value, char **pbuf);

/**
 * Parse the the text as text string.
 *
 * @verbatim
 *
 * text        =  <any CHAR, including bare    ; => atoms, specials,
 *                CR & bare LF, but NOT       ;  comments and
 *                including CRLF>             ;  quoted-strings are NOT recognized
 *
 * text_string = *(encoded-word/text)
 *
 * @endverbatim
 *
 * @param text
 * @param pbuf
 */
void parse_text_string(char *text, utf8 **pbuf);


struct parse_parameter
{
	char *attribute;
	char *value;
};

/**
 * Parse a string as parameter.
 *
 * @verbatim
 * parameter := attribute "=" value
 * attribute := token
 * @endverbatim
 *
 * Modified a little bit such that spaces surrounding the "=" sign are
 * also accepted.
 *
 * @param parameter
 * @param dest
 * @return NULL for an parse error or the pointer to the next unparsed character
 */
char *parse_parameter(char *parameter, struct parse_parameter *dest);

/**
 * Parse a date string.
 *
 * @param buf the buffer that should contain the string to parse.
 * @param pday where the day is stored
 * @param pmonth where the month is stored
 * @param pyear where the year is stored
 * @param phour where the hour is stored
 * @param pmin where the minute is stored
 * @param psec where the second is stored
 * @param pgmt where the GMT offset is stored
 * @return NULL for an parse error or the pointer to the next unparsed character
 */
char *parse_date(char *buf, int *pday,int *pmonth,int *pyear,int *phour,int *pmin,int *psec, int *pgmt);

#endif

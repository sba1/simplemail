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
** $Id$
*/

#ifndef SM__MAIL_H
#define SM__MAIL_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

struct header
{
	struct node node; /* embedded node structure */
	char *name;
	char *contents;
};

struct content_parameter
{
	struct node node;
	char *attribute;
	char *value;
};

struct mail
{
	int status;
	char *author;
	char *reply;
	char *subject;
	unsigned int size; /* the e-mails size in bytes */
	unsigned int seconds; /* seconds since 1.1.1978 */

	struct list header_list; /* the mail's headers */

	/* following things are only relevant when reading the mail */
	int mime; /* 0 means is not a mime mail */
	char *content_type;
	char *content_subtype; /* the types of the whole mail */
	struct list content_parameter_list;
	char *content_transfer_encoding;

	unsigned int text_begin; /* the beginning of the mail's text */
	unsigned int text_len; /* the length of the mails text */
	char *text; /* the mails text, allocated only for mails with filename */

	struct mail **multipart_array; /* only used in multipart messages */
	int multipart_allocated;
	int num_multiparts;

	char *decoded_data; /* the decoded data */
	unsigned int decoded_len;

	char *filename; /* the email filename on disk, NULL if e-mail is not from disk */
};

struct mail *mail_find_content_type(struct mail *m, char *type, char *subtype);
struct mail *mail_create(void);
struct mail *mail_create_from_file(char *filename);
struct mail *mail_create_reply(struct mail *mail);
void mail_free(struct mail *mail);
int mail_set_stuff(struct mail *mail, char *filename, unsigned int size);
int mail_process_headers(struct mail *mail);
void mail_read_contents(char *folder, struct mail *mail);
void mail_decode(struct mail *mail);

char *mail_find_header_contents(struct mail *mail, char *name);
char *mail_get_new_name(void);

/* mail scan functions */

struct mail_scan /* don't not access this */
{
	struct mail *mail; /* the mail */

	int position; /* the current position inside the mail */

	char *line; /* temporary saved line */
	int name_size; /* the length of the header's name */
	int contents_size; /* the length of the contents's name */
	int mode;
};

void mail_scan_buffer_start(struct mail_scan *ms, struct mail *mail);
void mail_scan_buffer_end(struct mail_scan *ms);
int mail_scan_buffer(struct mail_scan *ms, char *mail_buf, int size);

int mail_strip_lf(char *fn);


/* for mail composing */
struct composed_mail
{
	struct node node; /* embedded node structure */

	char *to; /* maybe NULL */
	char *subject; /* maybe NULL */

	char *filename; /* filename, maybe NULL */
	char *temporary_filename; /* maybe NULL */
	char *text; /* maybe NULL */
	char *content_type; /* maybe NULL */
	struct list list; /* more entries */

	char *mail_filename; /* The name of the mail (mainly used for changeing) */
	char *mail_folder; /* the folder of the mail ( -- " -- ) */

	/* more will follow */
};

void composed_mail_init(struct composed_mail *mail);

void mail_compose_new(struct composed_mail *new_mail);

/* for nodes in an address list NOTE: misplaced in mail.h */
struct address
{
	struct node node;
	char *realname;
	char *email;
};

struct list *create_address_list(char *str);
void append_to_address_list(struct list *list, char *str);
void free_address_list(struct list *list);

#endif

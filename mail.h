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
** mail.h
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
	int status; /* see below */
	int flags; /* see below */
	char *from; /* decoded "From" field */
	char *to; /* decoded "To" field, only the first address */
	char *reply;
	char *subject;
	char *message_id;
	char *message_reply_id;
	unsigned int size; /* the e-mails size in bytes */
	unsigned int seconds; /* seconds since 1.1.1978 */

	struct list header_list; /* the mail's headers */

	/* following things are only relevant when reading the mail */
	int mime; /* 0 means is not a mime mail */
	char *content_type;
	char *content_subtype; /* the types of the whole mail */
	struct list content_parameter_list; /* additional parameters */
	char *content_transfer_encoding;
	char *content_id; /* id of the content */

	unsigned int text_begin; /* the beginning of the mail's text */
	unsigned int text_len; /* the length of the mails text */
	char *text; /* the mails text, allocated only for mails with filename */

	/* after mail_read_structure() */
	/* only used in multipart messages */
	struct mail **multipart_array;
	int multipart_allocated;
	int num_multiparts;

  /* for "childs" of multipart messages */
	struct mail *parent_mail; /* if NULL, mail is root */

	/* after mail_decode() */
	char *decoded_data; /* the decoded data */
	unsigned int decoded_len;

  /* where to find the mail */
	char *filename; /* the email filename on disk, NULL if e-mail is not from disk */
	/*struct folder *folder;*/ /* the mail's folder, not yet implemented */

	/* for mail threads */
	struct mail *sub_thread_mail; /* one more level */
	struct mail *next_thread_mail; /* the same level */
	int child_mail; /* is a child mail */
};

/* Mail status */
#define MAIL_STATUS_UNREAD   0 /* unread messages */
#define MAIL_STATUS_READ     1 /* read message */
#define MAIL_STATUS_WAITSEND 2 /* wait to be sendet, new composed mail */
#define MAIL_STATUS_SENT     3 /* sent the mail */

/* Mail flags */
#define MAIL_FLAGS_NEW       (1L << 0) /* it's a new mail */
#define MAIL_FLAGS_GROUP     (1L << 1) /* it has been sent to more persons */
#define MAIL_FLAGS_ATTACH    (1L << 2) /* it has attachments */
#define MAIL_FLAGS_IMPORTANT (1L << 3) /* mail is important */

struct mail *mail_find_compound_object(struct mail *m, char *id);
struct mail *mail_find_content_type(struct mail *m, char *type, char *subtype);
void mail_identify_status(struct mail *m);
struct mail *mail_create(void);
struct mail *mail_create_from_file(char *filename);
struct mail *mail_create_reply(struct mail *mail);
int mail_forward(struct mail *mail);
void mail_free(struct mail *mail);
int mail_set_stuff(struct mail *mail, char *filename, unsigned int size);
int mail_process_headers(struct mail *mail);
void mail_read_contents(char *folder, struct mail *mail);
void mail_decode(struct mail *mail);

int mail_add_header(struct mail *mail, char *name, int name_len, char *contents, int contents_len);
char *mail_find_header_contents(struct mail *mail, char *name);
char *mail_get_new_name(void);
char *mail_get_status_filename(char *oldfilename, int status_new);

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

	char *reply_message_id; /* only for the root mail */

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

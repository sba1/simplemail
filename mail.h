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

#ifndef SM__CODESETS_H
#include "codesets.h"
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

/* TODO: Create a "struct short_mail" or simliar because
   struct mail contains some stuff which isn't used for only
   displaying */

struct mail
{
	int status;			/* see below */
	int flags;				/* see below */
	char *from_phrase;/* decoded "From" field, might be NULL if no phrase was defined */
	char *from_addr;	/* the email address */
	char *to_phrase;	/* decoded "To" field, only the first address, might be NULL if no phrase was defined */
	char *to_addr;		/* the email address, only a single one */
	char *reply;
	char *subject;
	char *message_id;
	char *message_reply_id;
	unsigned int size; /* the e-mails size in bytes */
	unsigned int seconds; /* seconds since 1.1.1978 */

	struct list header_list; /* the mail's headers */
	char *html_header; /* the header in html format, created by mail_create_html_header */

	/* following things are only relevant when reading the mail */
	int mime; /* 0 means is not a mime mail */
	char *content_type;
	char *content_subtype; /* the types of the whole mail */
	char *content_charset; /* the contents charset (usually for text parts only) */
	char *content_description; /* the description field */
	struct list content_parameter_list; /* additional parameters */
	char *content_transfer_encoding;
	char *content_id; /* id of the content */

	unsigned int text_begin; /* the beginning of the mail's text */
	unsigned int text_len; /* the length of the mails text */
	char *text; /* the mails text, allocated only for mails with filename */
	char *extra_text; /* this is extra allocated text, e.g. for decrypted */
										/* e-Mails, will always be freed if set */

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

/* Mail status (uses a range from 0-15) */
#define MAIL_STATUS_UNREAD   0 /* unread message */
#define MAIL_STATUS_READ     1 /* read message */
#define MAIL_STATUS_WAITSEND 2 /* wait to be sendet, new composed mail */
#define MAIL_STATUS_SENT     3 /* sent the mail */
#define MAIL_STATUS_REPLIED  4 /* mail has been replied */
#define MAIL_STATUS_FORWARD  5 /* mail has been forwared */
#define MAIL_STATUS_REPLFORW 6 /* mail has been replied and forwarded */
#define MAIL_STATUS_HOLD		 7 /* mail should not be send */
#define MAIL_STATUS_ERROR    8 /* mail has an error */
#define MAIL_STATUS_MAX 	 15
#define MAIL_STATUS_MASK		 (0xf) /* the mask for the status types */
#define MAIL_STATUS_FLAG_MARKED (1 << 4) /* the mail is marked */

/* A macro to easly get the mails status type */
#define mail_get_status_type(x) ((x->status) & (MAIL_STATUS_MASK))

/* Additional mail flags, they don't need to be stored within the filename */
#define MAIL_FLAGS_NEW	     (1L << 0) /* it's a new mail */
#define MAIL_FLAGS_GROUP     (1L << 1) /* it has been sent to more persons */
#define MAIL_FLAGS_ATTACH    (1L << 2) /* it has attachments */
#define MAIL_FLAGS_IMPORTANT (1L << 3) /* mail is important */
#define MAIL_FLAGS_CRYPT     (1L << 4) /* mail is crypted */
#define MAIL_FLAGS_SIGNED    (1L << 5) /* mail has been signed */

#define mail_get_from(x) ((x)->from_phrase?((x)->from_phrase):((x)->from_addr))
#define mail_get_to(x) ((x)->to_phrase?((x)->to_phrase):((x)->to_addr))

struct mail *mail_find_compound_object(struct mail *m, char *id);
struct mail *mail_find_content_type(struct mail *m, char *type, char *subtype);
struct mail *mail_find_initial(struct mail *m);
struct mail *mail_get_root(struct mail *m);
struct mail *mail_get_next(struct mail *m);
char *mail_get_from_address(struct mail *mail);
char *mail_get_to_address(struct mail *mail);
char *mail_get_replyto_address(struct mail *mail);

void mail_identify_status(struct mail *m);
struct mail *mail_create(void);
struct mail *mail_create_for(char *to, char *subject);
struct mail *mail_create_from_file(char *filename);
struct mail *mail_create_reply(int num, struct mail **mail_array);
struct mail *mail_create_forward(int num, struct mail **mail_array);
void mail_free(struct mail *mail);
int mail_set_stuff(struct mail *mail, char *filename, unsigned int size);
int mail_process_headers(struct mail *mail);
void mail_read_contents(char *folder, struct mail *mail);
void mail_decode(struct mail *mail);
void *mail_decode_bytes(struct mail *mail, unsigned int *len_ptr);
void mail_decoded_data(struct mail *mail, void **decoded_data_ptr, int *decoded_data_len_ptr);
int mail_create_html_header(struct mail *mail);

int mail_add_header(struct mail *mail, char *name, int name_len,
									  char *contents, int contents_len, int avoid_duplicates);
char *mail_find_header_contents(struct mail *mail, char *name);
char *mail_get_new_name(int status);
char *mail_get_status_filename(char *oldfilename, int status_new);

/* was static */
struct header *mail_find_header(struct mail *mail, char *name);

/* mail scan functions */

struct mail_scan /* don't not access this */
{
	struct mail *mail; /* the mail */
	int avoid_duplicates;

	int position; /* the current position inside the mail */

	char *line; /* temporary saved line */
	int name_size; /* the length of the header's name */
	int contents_size; /* the length of the contents's name */
	int mode;
};

void mail_scan_buffer_start(struct mail_scan *ms, struct mail *mail, int avoid_duplicates);
void mail_scan_buffer_end(struct mail_scan *ms);
int mail_scan_buffer(struct mail_scan *ms, char *mail_buf, int size);

/* for mail composing */
struct composed_mail
{
	struct node node; /* embedded node structure */

	char *from; /* the mail's from account */
	char *replyto; /* reply address */
	char *to; /* maybe NULL */
	char *cc; /* maybe NULL */
	char *subject; /* maybe NULL */

	char *filename; /* filename, maybe NULL */
	char *temporary_filename; /* maybe NULL */
	char *text; /* maybe NULL */
	char *content_type; /* maybe NULL */
	char *content_description; /* maybe NULL */
	struct list list; /* more entries */

	char *mail_filename; /* The name of the mail (mainly used for changeing) */
	char *mail_folder; /* the folder of the mail ( -- " -- ) */

	char *reply_message_id; /* only for the root mail */
	int encrypt;
	int sign;

	/* more will follow */
};

void composed_mail_init(struct composed_mail *mail);

int mail_compose_new(struct composed_mail *new_mail, int hold);

/* for nodes in an address list NOTE: misplaced in mail.h */
struct address
{
	struct node node;
	char *realname;
	char *email;
};

struct list *create_address_list(char *str);
struct mailbox *find_addr_spec_in_address_list(struct list *list, char *addr_spec);
void append_to_address_list(struct list *list, char *str);
void append_mailbox_to_address_list(struct list *list, struct mailbox *mb);
void remove_from_address_list(struct list *list, char *email);
void free_address_list(struct list *list);

char *mail_create_string(char *format, struct mail *mail, char *realname,
												 char *addr_spec);

int mail_allowed_to_download(struct mail *mail);

#endif

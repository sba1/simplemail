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
 * @file mail.h
 */

#ifndef SM__MAIL_H
#define SM__MAIL_H

#include <stdio.h>

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

/**
 * Encodes the primary status of a mail. The primary status of the mail
 * is stored within the filename of the mail.
 */
typedef enum
{
	/** The mail has been received but is not read yet */
	MAIL_STATUS_UNREAD		= 0,

	/** The mail has been read */
	MAIL_STATUS_READ		= 1,

	/** The mail is waiting for being sent */
	MAIL_STATUS_WAITSEND	= 2,

	/** The mail has been successfully sent */
	MAIL_STATUS_SENT        = 3,

	/** The mail has been replied */
	MAIL_STATUS_REPLIED     = 4,

	/** The mail mail has been forwarded */
	MAIL_STATUS_FORWARD		= 5,

	/** The mail has been both replied and forwarded */
	MAIL_STATUS_REPLFORW	= 6,

	/** The mail is composed but should be skipped mails are being sent */
	MAIL_STATUS_HOLD		 = 7,

	/** There was an error when proceeding the mail (e.g., during sending) */
	MAIL_STATUS_ERROR		= 8,

	/** The mail has been inferred to be spam */
	MAIL_STATUS_SPAM		= 9,

	/** Maximum value of real enum-like status component */
	MAIL_STATUS_MAX			= 15,

	/** The mail is marked */
	MAIL_STATUS_FLAG_MARKED  = (1 << 4)
} mail_status_t;

/** NUmber of bits that are needed to encode the enum-like status component */
#define MAIL_STATUS_BITS			4

/** the mask for the status types */
#define MAIL_STATUS_MASK			(0xf)

/** number of or'able status flags */
#define MAIL_STATUS_FLAG_BITS 		1

/* Mail status (uses a range from 0-15) */

/* A macro to easly get the mails status type */
#define mail_get_status_type(x) (((x)->status) & (MAIL_STATUS_MASK))
#define mail_is_spam(x) (mail_get_status_type(x) == MAIL_STATUS_SPAM)

#define mail_info_get_status_type(x) (((x)->status) & (MAIL_STATUS_MASK))
#define mail_info_is_spam(x) (mail_info_get_status_type(x) == MAIL_STATUS_SPAM)

/**
 * @brief Describes some user presentable information of a mail
 */
struct mail_info
{
	/** The mail's primary status */
	mail_status_t status;

	int flags;						/* see below */
	utf8 *from_phrase;  	/* decoded "From" field, might be NULL if no phrase was defined */
	utf8 *from_addr;			/* the email address */
	utf8 *to_phrase;			/* decoded "To" field, only the first address, might be NULL if no phrase was defined */
	utf8 *to_addr;				/* the email address, only a single one */
	struct address_list *to_list; /* a list of all TO'ed receivers (if any) */
	struct address_list *cc_list; /* a list of all CC'ed receivers (if any) */
	char *pop3_server;		/* the name of the pop3 server where the mail has been downloaded */
	char *reply_addr;			/* the address where the mail should be replied */
	utf8 *subject;
	char *message_id;
	char *message_reply_id;
	unsigned int size;			/* the e-mails size in bytes */
	unsigned int seconds; 	/* seconds since 1.1.1978 */
	unsigned int received;	/* seconds since 1.1.1978 */

	utf8 *excerpt; /**< An small one liner of the mail's contents */

	char *filename;					/* the email filename on disk, NULL if info belongs from a mail not from disk */

	unsigned short reference_count; /* number of additional references to this object */
	unsigned short to_be_freed;

	/* for mail threads */
	struct mail_info *sub_thread_mail;	/* one more level */
	struct mail_info *next_thread_mail;	/* the same level */
	int child_mail;									/* is a child mail */

};

struct mail_complete
{
	struct mail_info *info;   /* can be NULL for multipart mails */

	struct list header_list; /* the mail's headers */
	char *html_header; /* the header in html format, created by mail_create_html_header */

	/* following things are only relevant when reading the mail */
	int mime; /* 0 means is not a mime mail */
	char *content_type;
	char *content_subtype; /* the types of the whole mail */
	char *content_charset; /* the contents charset (usually for text parts only) */
	int content_inline; /* inline the content */
	utf8 *content_description; /* the description field (might be NULL) */
	struct list content_parameter_list; /* additional parameters */
	char *content_transfer_encoding;
	char *content_id; /* id of the content */
	char *content_name;

	unsigned int text_begin; /* the beginning of the mail's text */
	unsigned int text_len; /* the length of the mails text */
	char *text; /* the mails text, allocated only for mails with filename */
	char *extra_text; /* this is extra allocated text, e.g. for decrypted */
										/* e-Mails, will always be freed if set */

	/* after mail_read_structure() */
	/* only used in multipart messages */
	struct mail_complete **multipart_array;
	int multipart_allocated;
	int num_multiparts;

  /* for "childs" of multipart messages */
	struct mail_complete *parent_mail; /* if NULL, mail is root */

	/* after mail_decode() */
	char *decoded_data; /* the decoded data */
	unsigned int decoded_len;
};

/* Additional mail flags, they don't need to be stored within the filename
 * as they can be somehow easily derived from the mails contents */
#define MAIL_FLAGS_NEW	     (1L << 0) /* it's a new mail */
#define MAIL_FLAGS_GROUP     (1L << 1) /* it has been sent to more persons */
#define MAIL_FLAGS_ATTACH    (1L << 2) /* it has attachments */
#define MAIL_FLAGS_IMPORTANT (1L << 3) /* mail is important */
#define MAIL_FLAGS_CRYPT     (1L << 4) /* mail is crypted */
#define MAIL_FLAGS_SIGNED    (1L << 5) /* mail has been signed */
#define MAIL_FLAGS_NORCPT    (1L << 6) /* mail has no recipient */
#define MAIL_FLAGS_PARTIAL   (1L << 7) /* mail is only partial available locally */
#define MAIL_FLAGS_AUTOSPAM  (1L << 8) /* mail has been marked as spam automatically */

/* The following stuff is for optimizing displaying on AmigaOS, as strings
** must be converted here */
#define MAIL_FLAGS_SUBJECT_ASCII7   (1L << 24) /* subject uses only 7 bit */
#define MAIL_FLAGS_FROM_ASCII7      (1L << 25) /* from uses only 7 bit */
#define MAIL_FLAGS_TO_ASCII7        (1L << 26) /* to uses only 7 bit */
#define MAIL_FLAGS_FROM_ADDR_ASCII7 (1L << 27) /* from addr uses only 7 bit */
#define MAIL_FLAGS_TO_ADDR_ASCII7   (1L << 28) /* to addr uses only 7 bit */
#define MAIL_FLAGS_REPLYTO_ADDR_ASCII7 (1L << 29) /* reply to addr " */

#define mail_get_from(x) ((x)->info->from_phrase?((x)->info->from_phrase):((x)->info->from_addr))
#define mail_get_to(x) ((x)->info->to_phrase?((x)->info->to_phrase):((x)->info->to_addr))

#define mail_info_get_from(x) ((x)->from_phrase?((x)->from_phrase):((x)->from_addr))
#define mail_info_get_to(x) ((x)->to_phrase?((x)->to_phrase):((x)->to_addr))

/**
 * Creates a mail info, initialize it to default values.
 *
 * @return the mail info.
 */
struct mail_info *mail_info_create(void);

/**
 * Frees all memory associated with a mail info.
 *
 * @param info the mail info to free or NULL in which case this is a no-op.
 */
void mail_info_free(struct mail_info *info);

/**
 * Find an compound object of a multipart/related mail (RFC2387)
 * (either by Content-ID or Content-Location). NULL if object. m is a mail
 * in the multipart/related mail.
 *
 * @param m
 * @param id
 * @return
 */
struct mail_complete *mail_find_compound_object(struct mail_complete *m, char *id);

/**
 * Returns the first mail with the given mime type/subtype
 * (recursive). Return NULL if it doesn't exists.
 *
 * @param m
 * @param type
 * @param subtype
 * @return
 */
struct mail_complete *mail_find_content_type(struct mail_complete *m, char *type, char *subtype);

/**
 * Finds the initial mail which should be displayed. This is always the
 * first non-multipart mail. For multipart/alternative mails it returns the
 * preferred one (depending on what the GUI prefers and how SimpleMail is
 * configured).
 *
 * @param m
 * @return
 */
struct mail_complete *mail_find_initial(struct mail_complete *m);

/**
 * Returns the root of the mail.
 *
 * @param m the complete mail of which to return the root
 * @return the root complete mail
 */
struct mail_complete *mail_get_root(struct mail_complete *m);

/**
 * Returns the next part of the mail (excluding multiparts). Calling this
 * iteratively results in a depth-first order.
 *
 * @param m the mail from which the next mail should be determined.
 * @return the next mail
 */
struct mail_complete *mail_get_next(struct mail_complete *m);

/**
 * Extract the name of a given address (and looks for matches in the
 * address book). If more than one e-mail address is specified, *more_prt
 * will be set to 1. addr maybe NULL.
 *
 * @param addr
 * @param dest_phrase
 * @param dest_addr
 * @param more_ptr
 * @return
 */
int extract_name_from_address(char *addr, char **dest_phrase, char **dest_addr, int *more_ptr);

/**
 * Returns the "from" name and address (name <address>) of the mail.
 * @param mail
 * @return
 */
char *mail_get_from_address(struct mail_info *mail);

/**
 * Returns the first "to" name and address (name <address>) of the mail.
 *
 * @param mail
 * @return
 */
char *mail_get_to_address(struct mail_info *mail);

/**
 * Returns an array of all email addresses of the recipients (to and cc).
 * Duplicates are filtered.
 *
 * @param mail
 * @return the array. Must be freed with array_free().
 */
char **mail_info_get_recipient_addresses(struct mail_info *mail);

/**
 * Returns the first "to" name and address (name <address>) of the mail.
 *
 * @param mail
 * @return the name and address that must be freed with free() when no longer
 *  needed.
 */
char *mail_get_replyto_address(struct mail_info *mail);

/**
 * @brief Sets the given excerpt for the given mail.
 *
 * The given excerpt should be allocated via malloc(). You shouldn't
 * touch it once you have given it as argument to this function.
 *
 * Frees any previously set excerpt.
 *
 * @param mail of which the excerpt should be set
 * @param excerpt an excerpt
 * @return
 */
void mail_info_set_excerpt(struct mail_info *mail, utf8 *excerpt);

/**
 * Returns whether a mail is marked as deleted (on IMAP folders).
 *
 * @param mail
 * @return whether deleted or not.
 */
int mail_is_marked_as_deleted(struct mail_info *mail);

/**
 * Recovers the status of the mail that is represented by the given mail info
 * on basis of the filename.
 *
 * @param m represents the mail whose status should be recovered.
 */
void mail_identify_status(struct mail_info *m);

/**
 * Creates a mail, initialize it to default values.
 *
 * @return the newly created mail.
 */
struct mail_complete *mail_complete_create(void);

/**
 * Creates a mail to be sent to a given address.
 * It fills out the given fields field and a body template.
 *
 * @param from specifies the sender of the mail.
 * @param to_str_unexpanded specifies possible unexpanded addresses of receients.
 * @param replyto specifies the reply to address.
 * @param subject specifies the initial subject of the mail.
 * @param body maybe NULL in which case the best phrase is used.
 * @return the mail or NULL on an error.
 */
struct mail_complete *mail_create_for(char *from, char *to_str_unexpanded, char *replyto, char *subject, char *body);

/**
 * Scans a mail file and returns a filled (malloc'ed) mail instance, NULL
 * if an error happened.
 *
 * @param filename that points to the file that represents the mail.
 * @return the mail or NULL.
 */
struct mail_complete *mail_complete_create_from_file(char *filename);

/**
 * Creates a mail that is a reply to the given mails. That means change the
 * contents of "From:" to "To:", change the subject, quote the first text
 * passage and remove the attachments. The mail is processed. The given mail
 * should be processed to.
 *
 * @param num number of entries of the given mail_array.
 * @param mail_array the mail array that contains the mails that should be
 *  replied to.
 * @return the created reply mail
 */
struct mail_complete *mail_create_reply(int num, struct mail_complete **mail_array);

/**
 * Creates a Forwarded mail to the given mails. Note that the input mails
 * can be changed! And should be be freed with mail_free() afterwards.
 *
 * @param num
 * @param filename_array
 * @return the created forward mail
 */
struct mail_complete *mail_create_forward(int num, char **filename_array);

/**
 * Scans a mail file and returns a filled (malloc'ed) mail_info instance, NULL
 * if an error happened.
 *
 * @param filename
 * @return the mail or NULL.
 */
struct mail_info *mail_info_create_from_file(char *filename);

/**
 * Frees all memory associated with a mail.
 *
 * @param mail
 */
void mail_complete_free(struct mail_complete *mail);

/**
 * This function fills in the header list if it is empty.
 * Return 1 on success.
 *
 * @param m the mail whose headers should be read.
 * @return 1 on success, 0 otherwise.
 */
int mail_read_header_list_if_empty(struct mail_complete *m);

/**
 * Interprets the the already read headers. A return value of 0 means error.
 * This function can be called from sub threads.
 *
 * @param mail
 * @return 0 for an error.
 * @todo Must use functions better since this function is really too big
 */
int mail_process_headers(struct mail_complete *mail);

/**
 * Locally, read the contents of the given mail that is situated in the given
 * folder.
 *
 * @param folder
 * @param mail
 */
void mail_read_contents(char *folder, struct mail_complete *mail);

/**
 * Decodes the given mail. A text mail is always converted to UTF8 and
 * it is ensured that it ends with a 0-byte, that is, however, not
 * counted in decoded_len.
 *
 * @param mail
 */
void mail_decode(struct mail_complete *mail);

/**
 * Decodes a limited number of bytes of the given mail.
 *
 * @param mail the mail whose contents should be decoded.
 * @param len_ptr points to an int variable which limits the decode buffer. Use
 *  a big number to decode everything. After return, the value will contained
 *  the number truly decoded bytes
 *
 * @todo decouple in/out parameter.
 */
void *mail_decode_bytes(struct mail_complete *mail, unsigned int *len_ptr);

/**
 * Decodes the given mail to the given buffers.
 *
 * @param mail
 * @param decoded_data_ptr
 * @param decoded_data_len_ptr
 */
void mail_decoded_data(struct mail_complete *mail, void **decoded_data_ptr, int *decoded_data_len_ptr);

/**
 * Create a HTML text with headers from the given mail
 * The actuakl text is stored in the html_header field of given mail.
 *
 * @param mail the mail for which the html text shall be created.
 * @param all_headers whether all headers shall be included.
 * @return 0 on failure, all other values indicate success.
 */
int mail_create_html_header(struct mail_complete *mail, int all_headers);

/**
 * Looks for a given header with the given name in the given mail and returns
 * the value of the file. If the header is not contained, NULL is returned.
 *
 * @param mail
 * @param name
 * @return
 */
char *mail_find_header_contents(struct mail_complete *mail, char *name);

/**
 * Returns a unique filename for a new mail that should have the
 * given status.
 *
 * The filename is unique with respect to the current working directory. No
 * actual file will be created by this call.
 *
 * @param status the new status.
 * @return the unique filename that is allocated with malloc().
 *
 * @note this function is not thread-safe for the same working directory.
 */
char *mail_get_new_name(int status);

/**
 * Returns a new filename for the mail which matches the given status.
 * String is allocated with malloc.
 *
 * @param oldfilename
 * @param status_new
 * @return the
 */
char *mail_get_status_filename(char *oldfilename, int status_new);

/**
 * Increase reference counter of the given mail.
 *
 * @param mail the mail whose ref counter should be incremented.
 * @note this, like most other functions, is not thread-safe.
 */
void mail_reference(struct mail_info *mail);

/**
 * Decreases the reference counter for the given mail and possibly frees the
 * mail if the counter reaches zero.
 *
 * @param mail the mail whose ref counter should be decremented.
 * @note this, like most other functions, is not thread-safe.
 */
void mail_dereference(struct mail_info *mail);

/**
 * Looks for a header field as specified by the given name and return it.
 * If the header field is not contained, NULL is returned.
 *
 * @param mail
 * @param name
 * @return
 */
struct header *mail_find_header(struct mail_complete *mail, char *name);

/* mail scan functions */

struct mail_scan /* don't not access this */
{
	struct mail_complete *mail; /* the mail */
	int avoid_duplicates;

	int position; /* the current position inside the mail */

	char *line; /* temporary saved line */
	int name_size; /* the length of the header's name */
	int contents_size; /* the length of the contents's name */
	int mode;
};

void mail_scan_buffer_start(struct mail_scan *ms, struct mail_complete *mail, int avoid_duplicates);
void mail_scan_buffer_end(struct mail_scan *ms);
int mail_scan_buffer(struct mail_scan *ms, char *mail_buf, int size);

/* for mail composing */
struct composed_mail
{
	struct node node; /* embedded node structure */

	char *from; /* the mail's from account, utf8 */
	char *replyto; /* reply address, utf8 */
	char *to; /* maybe NULL, utf8 */
	char *cc; /* maybe NULL, utf8 */
	char *bcc; /* maybe NULL, utf8 */
	char *subject; /* maybe NULL, utf8 */

	char *filename; /* filename, maybe NULL, used with fopen() */
	char *temporary_filename; /* maybe NULL */
	char *text; /* maybe NULL */
	char *content_type; /* maybe NULL */
	char *content_description; /* maybe NULL */
	char *content_filename; /* maybe NULL, utf8, used in content disposition */

	struct list list; /* more entries */

	char *mail_filename; /* The name of the mail (mainly used for changeing) */
	char *mail_folder; /* the folder of the mail ( -- " -- ) */

	char *reply_message_id; /* only for the root mail */
	int encrypt;
	int sign;
	int importance; /* 0=low, 1=normal, 2=high */

	/* more will follow */
};

/**
 * Initialize a composed mail instance.
 *
 * @param mail
 */
void composed_mail_init(struct composed_mail *mail);

/**
 * Create a new mail of disk for the given already composed mails.
 *
 * @param new_mail
 * @param hold
 * @return 0 on failure, otherwise the operation was successful.
 */
int mail_compose_new(struct composed_mail *new_mail, int hold);

/**
 * Create a string for a greeting/closing phrase.
 *
 * @param format the format template
 * @param orig_mail a mail to which is referred, e.g., the mail that should
 *  be replied.
 * @param realname the name of the recipient of the to-be-written mail.
 *  NULL is a valid value.
 * @param addr_spec the address of the recipient of the to-be-written mail
 *  NULL is a valid value.
 * @return
 */
char *mail_create_string(char *format, struct mail_info *mail, char *realname,
												 char *addr_spec);

/**
 * Test whether external resources can be downloaded from the given mail.
 *
 * @param mail the mail to test
 * @return 0 if no external resouces shall be downloaded, otherwise 1.
 */
int mail_allowed_to_download(struct mail_info *mail);

/**
 * Exported function for test. Do not use for anything else.
 *
 * @param fp
 * @param new_mail
 * @return 1 for success, 0 otherwise.
 * @note this is for testing only.
 */
int private_mail_compose_write(FILE *fp, struct composed_mail *new_mail);

#endif

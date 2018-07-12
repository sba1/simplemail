/**
 * @file
 */

#ifndef SM__IMAP_HELPER_H
#define SM__IMAP_HELPER_H

#include "tcp.h"

#define RM_FLAG_SEEN      (1L<<0)
#define RM_FLAG_ANSWERED  (1L<<1)
#define RM_FLAG_FLAGGED	(1L<<2)

struct remote_mail
{
	unsigned int uid;
	unsigned int flags;
	unsigned int size;

	/* only if headers are requested  */
	char *headers;
};

struct local_mail
{
	unsigned int uid;
	unsigned int todel;
};


/**
 * Writes the next word into the dest buffer but not more than dest_size.
 *
 * @param src
 * @param dest
 * @param dest_size
 * @return
 */
char *imap_get_result(char *src, char *dest, int dest_size);


/**
 * Handle the answer of imap_get_remote_mails().
 *
 * @param conn
 * @param tag
 * @param buf
 * @param buf_size
 * @param remote_mail_array
 * @param num_of_remote_mails
 * @return
 */
int imap_get_remote_mails_handle_answer(struct connection *conn, char *tag, char *buf, int buf_size, struct remote_mail *remote_mail_array, int num_of_remote_mails);

#endif

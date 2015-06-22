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
 * @file
 */

#ifndef SM__SMTP_H
#define SM__SMTP_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

/**
 * Minimal description for outgoing mails.
 */
struct outmail
{
	char *from; /* the from address */
	char **rcp; /* an array of all recipients (e-mails) */
	char *mailfile; /* the name of the file */
	int size; /* the size of the mail */
};

/**
 * Creates an array of outmails with amm entries.
 * The array entries point already to the struct outmail *.
 *
 * @param amm the number of outmails that should be allocated
 * @return the array of outmail pointers that can be used. Free with
 *  free_outmail_array().
 */
struct outmail **create_outmail_array(int amm);

/**
 * Duplicates an array of outmails.
 *
 * @param om
 * @return the duplicate or NULL for an error.
 */
struct outmail **duplicate_outmail_array(struct outmail **om);

/**
 * Frees an array of outmails completely
 *
 * @param om_array
 */
void free_outmail_array(struct outmail **om_array);

#define ESMTP_ENHACEDSTATUSCODES  (1<<0)
#define ESMTP_8BITMIME            (1<<1)
#define ESMTP_ONEX                (1<<2)
#define ESMTP_ETRN                (1<<3)
#define ESMTP_XUSR                (1<<4)
#define ESMTP_AUTH                (1<<5)
#define ESMTP_PIPELINING          (1<<6)
#define ESMTP_STARTTLS						(1<<7)

#define AUTH_PLAIN							 1
#define AUTH_LOGIN							 2
#define AUTH_DIGEST_MD5				 4
#define AUTH_CRAM_MD5					 8

/**
 * Description of an SMTP server.
 */
struct smtp_server
{
	char *name;
	unsigned int 	port;
	char *fingerprint; /**< @brief fingerprint of server certificate */
	char *domain;

	int ip_as_domain;
	int pop3_first;
	int secure;

	int auth;
	char *auth_login;
	char *auth_password;
};


struct smtp_send_callbacks
{
	void (*set_status_static)(const char *str);
	void (*set_connect_to_server)(const char *server);
	void (*set_head)(const char *head);
	void (*set_title_utf8)(const char *title);
	void (*set_title)(const char *title);
	void (*init_gauge_as_bytes)(int maximal);
	void (*set_gauge)(int value);
	void (*init_mail)(int maximal);
	void (*set_mail)(int current, int current_size);
	int (*skip_server)(void);
	void (*mail_has_not_been_sent)(char *filename);
	void (*mail_has_been_sent)(char *filename);
};

struct smtp_send_options
{
	/** List of all accounts from which to sent */
	struct list *account_list;

	/** Array of all mails to be send */
	struct outmail **outmail;

	/** callbacks called during some operations */
	struct smtp_send_callbacks callbacks;
};

/**
 * Send the mails in the context of the calling thread.
 *
 * @param options further options for the send operation.
 * @return 1 on success, 0 otherwise.
 */
int smtp_send_really(struct smtp_send_options *options);

/**
 * Creates a new smtp server description.
 *
 * @return the allocated smtp server description. Free it via smtp_free().
 */
struct smtp_server *smtp_malloc(void);

/**
 * Duplicates an existing smtp server
 *
 * @param smtp the server to be duplicated.
 *
 * @return the duplicate or NULL for an error.
 */
struct smtp_server *smtp_duplicate(struct smtp_server *smtp);

/**
 * Frees an smtp server
 *
 * @param the smtp server to be freed.
 */
void smtp_free(struct smtp_server *);

#endif

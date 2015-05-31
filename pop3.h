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

#ifndef POP3_H
#define POP3_H

#include "lists.h"

/**
 * @brief The settings for a POP3 server
 */
struct pop3_server
{
	struct node node; /**< @brief embedded node structure */

	char *name; /**< @brief the name of the server */
	unsigned int port; /**< @brief the port */
	char *fingerprint; /**< @brief fingerprint of server certificate */
	int ssl; /**< @brief use ssl to connect */
	int stls; /**< @brief with STLS command */
	int apop; /**< @brief use APOP and fail if APOP is not available */
	char *login; /**< @brief the user login name */
	char *passwd; /**< @brief the password */
	int del; /**< @brief 1 if downloaded mails should be deleted */
	int nodupl; /**< @brief 1 if duplicates should be avoided */
	int active; /**< @brief is this pop server actove */
	int ask; /**< @brief ask for login/password */
	char *title; /**< @brief normaly NULL, will hold a copy of account->account_name while fetching mails */
};

struct pop3_dl_callbacks
{
	void (*set_status_static)(const char *str);
	void (*set_status)(const char *);
	void (*set_connect_to_server)(const char *server);
	void (*set_head)(const char *head);
	void (*set_title_utf8)(const char *title);
	void (*set_title)(const char *title);
	void (*init_gauge_as_bytes)(int maximal);
	void (*set_gauge)(int value);
	void (*init_mail)(int maximal);
	void (*set_mail)(int current, int current_size);
	int (*request_login)(char *text, char *login, char *password, int len);
};

struct pop3_dl_options
{
	/** the list of pop3_server connections to check. */
	struct list *pop_list;

	/** the drawer where to put the downloaded files (incoming folder). */
	char *dest_dir;

	/** the receive preselection */
	int receive_preselection;

	/** the receive size */
	int receive_size;

	/** whether remote filters should be applied */
	int has_remote_filter;

	/** the root directories of the folders */
	char *folder_directory;

	/** whether spam identification should be applied. */
	int auto_spam;

	/** list of email addresses that shall be not considered as spam */
	char **white;

	/** list of email addresses that shall be considered as spam */
	char **black;

	/** callbacks called during some operations */
	struct pop3_dl_callbacks callbacks;
};

/**
 * Download the mails in the context of the current thread.
 *
 * @param dl_options options for downloading
 * @return success or not.
 */
int pop3_really_dl(struct pop3_dl_options *dl_options);

/**
 * @brief Log in and log out into a POP3 server as given by the @p server parameter.
 *
 * This function can be used to test POP3 settings but also some SMTP server
 * requires prior POP3 server logins.
 *
 * @param server defines the connection details of the POP3 server.
 * @return 1 on sucess, 0 on failure.
 * @note Assumes to be run in a sub thread (i.e., not SimpleMail's main thread)
 */
int pop3_login_only(struct pop3_server *);

/**
 * @brief Construct a new instance holding POP3 server settings.
 *
 * Allocates a new POP3 server setting instance and initializes it
 * with default values.
 *
 * @return the instance which must be freed with pop_free() when no longer in
 * use or NULL on failure.
 */
struct pop3_server *pop_malloc(void);

/**
 * @brief Duplicates the given POP3 server settings.
 *
 * @param pop defines the settings which should be duplicated.
 * @return the duplicated POP3 server settings.
 */

struct pop3_server *pop_duplicate(struct pop3_server *pop);

/**
 * @brief Frees the POP3 server settings completely.
 *
 * Deallocates all resources associated with the given
 * POP3 server settings.
 *
 * @param pop defines the instance which should be freed. If this is NULL,
 *  this is a noop.
 */
void pop_free(struct pop3_server *pop);

#endif

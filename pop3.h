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

/**
 * Download the mails in the context of the current thread.
 *
 * @param pop_list the list of pop3_server connections to check.
 * @param dest_dir the drawer where to put the downloaded files (incoming folder).
 * @param receive_preselection the receive preselection
 * @param receive_size the receive size
 * @param has_remote_filter whether remote filters should be applied
 * @param folder_directory the root directories of the folders
 * @param auto_spam whether spam identification should be applied.
 * @param white list of email addresses that shall be not considered as spam
 * @param black list of email addresses that shall be considered as spam
 * @return success or not.
 */
int pop3_really_dl(struct list *pop_list, char *dest_dir, int receive_preselection, int receive_size, int has_remote_filter, char *folder_directory, int auto_spam, char **white, char **black);

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

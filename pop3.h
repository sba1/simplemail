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
** pop3.h
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

int pop3_really_dl(struct list *pop_list, char *dest_dir, int receive_preselection, int receive_size, int has_remote_filter, char *folder_directory, int auto_spam, char **white, char **black);
int pop3_login_only(struct pop3_server *);

struct pop3_server *pop_malloc(void);
struct pop3_server *pop_duplicate(struct pop3_server *pop);
void pop_free(struct pop3_server *pop);

#endif

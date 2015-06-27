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

#ifndef SM__IMAP_H
#define SM__IMAP_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

struct folder;
struct mail_info;

struct imap_server
{
	struct node node;

	char *name;
	unsigned int port;
	char *fingerprint; /**< @brief fingerprint of server certificate */
	char *login;
	char *passwd;

	int active;
	int ssl;
	int starttls;
	int ask;

	char *title; /* normaly NULL, will hold a copy of account->account_name while fetching mails */
};

struct imap_synchronize_callbacks
{
	void (*set_status_static)(const char *str);
	void (*set_connect_to_server)(const char *server);
	void (*set_head)(const char *head);
	void (*set_title_utf8)(const char *title);
	void (*set_title)(const char *title);
};

struct imap_synchronize_options
{
	/** list of imap servers to which connections should be  established. */
	struct list *imap_list;

	/** No user interaction */
	int quiet;

	struct imap_synchronize_callbacks callbacks;
};

/**
 * Synchronize with all imap severs in the given list.
 *
 * This is a synchronous function, it should not be called directly within the
 * main thread.
 *
 * @param options the options for synching
 */
void imap_synchronize_really(struct imap_synchronize_options *options);

/**
 * Request the list of all folders of the imap server.
 *
 * This is an asynchronous call that is answered with calling the given callback on the
 * context of the main thread.
 *
 * @param server describes the server to which should be connected.
 * @param callback the callback that is invoked if the request completes. The server
 *  argument is an actual duplicate of the original server, both lists contain string
 *  nodes with all_list containing all_list and sub_list containing only subscribed
 *  folders.
 *
 * @return whether the request has been in principle submitted or not.
 */
int imap_get_folder_list(struct imap_server *server, void (*callback)(struct imap_server *, struct string_list *, struct string_list *));

/**
 * Submit the given list of string_nodes as subscribed to the given server.
 *
 * This is an asynchronous call.
 *
 * @param server contains all the connection details that describe the login
 *  that is the concern of this call.
 * @param list the list of string_nodes to identify the
 * @return whether the call has in principle be invoked.
 */
int imap_submit_folder_list(struct imap_server *server, struct string_list *list);

/**
 * Allocate an imap server structure and initialize it with some default values.
 *
 * @return the newly allocated description for an imap server
 */
struct imap_server *imap_malloc(void);

/**
 * Duplicates a given imap server.
 *
 * @param imap
 * @return the duplicated
 */
struct imap_server *imap_duplicate(struct imap_server *imap);

/**
 * Frees the given imap server instance completely.
 *
 * @param imap
 */
void imap_free(struct imap_server *imap);

/**
 * Let the imap thread connect to the imap server represented by the folder.
 *
 * @param folder the folder to which the imap thread should connect.
 */
void imap_thread_connect(struct folder *folder);

/**
 * Download the given mail from the imap server in a synchrony manner.
 *
 * @param f
 * @param m
 * @return
 */
int imap_download_mail(struct folder *f, struct mail_info *m);

/**
 * Moves the mail from a source folder to a destination folder.
 *
 * @param mail
 * @param src_folder
 * @param dest_folder
 * @return
 */
int imap_move_mail(struct mail_info *mail, struct folder *src_folder, struct folder *dest_folder);

/**
 * Deletes a mail from the server.
 *
 * @param filename
 * @param folder
 * @return
 */
int imap_delete_mail_by_filename(char *filename, struct folder *folder);

/**
 * Store the given mail in the given folder of an imap server.
 *
 * @param mail
 * @param source_dir
 * @param dest_folder
 * @return
 */
int imap_append_mail(struct mail_info *mail, char *source_dir, struct folder *dest_folder);

/**
 * Download the given mail in an asynchronous manner. The callback is called
 * when the download process has finished.
 *
 * @param f
 * @param m
 * @param callback
 * @param userdata
 * @return
 */
int imap_download_mail_async(struct folder *f, struct mail_info *m, void (*callback)(struct mail_info *m, void *userdata), void *userdata);

#endif

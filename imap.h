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

#ifndef SM__STRING_LISTS_H
#include "string_lists.h"
#endif

#ifndef SM__TCP_H
#include "tcp.h"
#endif

struct folder;
struct mail_info;
struct remote_folder;

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

	/** Keep mails that are in a local folder but not on a remote one */
	int keep_orphans;

	char *title; /* normally NULL, will hold a copy of account->account_name while fetching mails */
};


/**
 * Checks whether in principle a new connection would be needed when switching
 * from srv1 to srv2.
 *
 * @param srv1
 * @param srv2
 * @return
 */
int imap_new_connection_needed(struct imap_server *srv1, struct imap_server *srv2);

struct imap_connect_and_login_to_server_callbacks
{
	void (*set_status)(const char *str);
	int (*request_login)(char *text, char *login, char *password, int len);
};

/**
 * Connect and login to the given imap server.
 *
 * @param connection
 * @param imap_server
 * @return
 */
int imap_really_connect_and_login_to_server(struct connection **connection, struct imap_server *imap_server, struct imap_connect_and_login_to_server_callbacks *callbacks);

struct imap_download_mails_callbacks
{
	void (*new_mails_arrived)(int num_filenames, char **filenames, char *user, char *server, char *path);
	void (*new_uids)(unsigned int uid_validity, unsigned int uid_next, char *user, char *server, char *path);
	void (*set_status)(const char *str);
	void (*set_status_static)(const char *str);
	void (*delete_mail_by_uid)(char *user, char *server, char *path, unsigned int uid);
};

/* TODO: Merge with the ones in folder.h */
struct imap_uid_options
{
	/**
	 * Set to 1, if IMAP UIDs should not be used.
	 */
	int imap_dont_use_uids;

	/**
	 * The UIDVALIDITY field as determined during the most recent
	 * access to this folder on its imap server.
	 */
	unsigned int imap_uid_validity;

	/**
	 * The UIDNEXT field as determined during the most recent
	 * access to this folder on its imap serve
	 */
	unsigned int imap_uid_next;
};

struct imap_download_mails_options
{
	char *imap_local_path;
	struct imap_server *imap_server;
	char *imap_folder;

	struct imap_uid_options uid_options;

	struct imap_download_mails_callbacks callbacks;
};

/**
 * Function to download mails.
 *
 * @return number of downloaded mails. A value < 0 indicates an error.
 */
int imap_really_download_mails(struct connection *imap_connection, struct imap_download_mails_options *options);

struct imap_delete_mail_by_filename_options
{
	char *filename;
	struct folder *folder;
};

struct imap_connect_to_server_callbacks
{
	void (*set_status)(const char *str);
	int (*request_login)(char *text, char *login, char *password, int len);
	void (*add_imap_folder)(char *user, char *server, char *path, char delim);
	void (*refresh_folders)(void);
};

struct imap_connect_to_server_options
{
	char *imap_local_path;
	struct imap_server *imap_server;
	char *imap_folder;
	struct imap_connect_to_server_callbacks callbacks;
	struct imap_download_mails_callbacks download_callbacks;
};

/**
 * Establishes a connection to the server and downloads mails.
 */
void imap_really_connect_to_server(struct connection **imap_connection, struct imap_connect_to_server_options *options);

/**
 * Delete a mail permanently from the server
 *
 * @param imap_connection already established imap connection, already logged in.
 * @param filename the (local) filename of the mail to be deleted.
 * @param folder the folder where the mail is located.
 * @return success or not.
 */
int imap_really_delete_mail_by_filename(struct connection *imap_connection, struct imap_delete_mail_by_filename_options *options);

/**
 * Store the mail represented by the mail located in the given source_dir
 * on the given server in the given dest_folder.
 *
 * @param connection already established connection for the imap server.
 * @param mail info about the mail that should be transfered
 * @param source_dir the folder in which the mail is stored.
 * @param server the target server
 * @param dest_folder folder for the target.
 * @return success or not.
 */
int imap_really_append_mail(struct connection *imap_connection, struct mail_info *mail, char *source_dir, struct imap_server *server, struct folder *dest_folder);

/**
 * Download the given mail. Usually called in the context of the imap thread.
 *
 * @param connection already established connection for the imap server.
 * @param local_path
 * @param m
 * @param callback called on the context of the parent task.
 * @param userdata user data supplied for the callback
 * @return
 */
int imap_really_download_mail(struct connection *imap_connection, char *local_path, struct mail_info *m, void (*callback)(struct mail_info *m, void *userdata), void *userdata);

/**
 * Move a given mail from one folder into another one of the given imap account.
 *
 * Usually called in the context of the imap thread.
 *
 * @param connection already established connection for the imap server.
 * @param mail
 * @param server
 * @param src_folder
 * @param dest_folder
 * @return success or failure.
 */
int imap_really_move_mail(struct connection *imap_connection, struct mail_info *mail, struct imap_server *server, struct folder *src_folder, struct folder *dest_folder);

struct imap_synchronize_callbacks
{
	void (*set_status)(const char *str);
	void (*set_status_static)(const char *str);
	void (*set_connect_to_server)(const char *server);
	void (*set_head)(const char *head);
	void (*set_title_utf8)(const char *title);
	void (*set_title)(const char *title);
	int (*request_login)(char *text, char *login, char *password, int len);
	void (*add_imap_folder)(char *user, char *server, char *path, char delim);
	void (*refresh_folders)(void);
	void (*init_gauge_as_bytes)(int maximal);
	void (*set_gauge)(int value);
	void (*new_mail_arrived)(char *filename, char *user, char *server, char *path);
	void (*delete_mail_by_uid)(char *user, char *server, char *path, unsigned int uid);
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

struct imap_get_folder_list_callbacks
{
	void (*set_status)(const char *str);
	void (*set_status_static)(const char *str);
	void (*set_connect_to_server)(const char *server);
	void (*set_head)(const char *head);
	void (*set_title_utf8)(const char *title);
	void (*set_title)(const char *title);
};

struct imap_get_folder_list_options
{
	struct imap_server *server;

	struct imap_get_folder_list_callbacks callbacks;
};

/**
 * Retrieve the folder list and call the given callback on the context of the
 * main thread.
 *
 * @param options
 * @param all_folders_out where the start pointer to the all folders array is stored.
 * @param num_all_folders_out where the length of the all folders array is stored.
 * @param sub_folders_out where the start pointer to subscribed all folders array is stored.
 * @param num_sub_folders_out where the length of the subscribed folders array is stored.
 * @return 1 on success, 0 on an error
 */
int imap_get_folder_list_really(struct imap_get_folder_list_options *options,
	struct remote_folder **all_folders_out, int *num_all_folders_out,
	struct remote_folder **sub_folders_out, int *num_sub_folders_out);

struct imap_submit_folder_list_callbacks
{
	void (*set_status)(const char *str);
	void (*set_status_static)(const char *str);
	void (*set_connect_to_server)(const char *server);
	void (*set_head)(const char *head);
	void (*set_title_utf8)(const char *title);
	void (*set_title)(const char *title);
};

struct imap_submit_folder_options
{
	struct imap_server *server;
	struct string_list *list;
	struct imap_submit_folder_list_callbacks callbacks;
};

/**
 * Submit the given list of string_nodes to the imap server in order to subscribe them.
 *
 * @param options
 */
void imap_submit_folder_list_really(struct imap_submit_folder_options *options);

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

/**
 * @file
 */

#ifndef SM__IMAP_HELPER_H
#define SM__IMAP_HELPER_H

#include "tcp.h"

/******************************************************************************/

struct imap_server;

/******************************************************************************/

extern int imap_val;

/**
 * Resets the imap command counter (e.g., imap_val).
 *
 * TODO: Get rid of this, use a struct imap_connection instead.
 */
void imap_reset_command_counter(void);

/******************************************************************************/

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

/** Describes a remote mailbox and its contents */
struct remote_mailbox
{
	struct remote_mail *remote_mail_array; /* may be NULL if remote_mail_num == 0 */
	int num_of_remote_mail;
	unsigned int uid_validity;
	unsigned int uid_next;
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
 * Waits for an OK after an connect, i.e., until login credentials are requested
 * Also secures the connection if starttls mode is active..
 *
 * @param conn
 * @param server
 * @return
 */
int imap_wait_login(struct connection *conn, struct imap_server *server);

/**
 * Perform a login for the given connection to the given imap server.
 *
 * @param conn
 * @param server
 * @return
 */
int imap_login(struct connection *conn, struct imap_server *server);

/**
 * Send a simple imap command only to check for success/failure.
 *
 * @param conn
 * @param cmd
 * @return
 */
int imap_send_simple_command(struct connection *conn, const char *cmd);

struct remote_folder
{
	char *name;
	char delim;
};

/**
 * Returns a list with string_node nodes which describes the folder names.
 * If you only want the subscribed folders set all to 0. Note that the
 * INBOX folder is always included if it does exist.
 *
 * @param conn the connection to write against
 * @param all whether all folders shall be returned or only the subscribed ones.
 * @param num_remote_folders length of the returned array
 * @return an array of all remote folders.
 */
struct remote_folder *imap_get_folders(struct connection *conn, int all, int *num_remote_folders);

/**
 * Free all memory associated with the given array of remote folders as
 * returned by imap_get_folders().
 *
 * @param rf array of remote folders as returned by imap_get_folders().
 * @param num_remote_folders length of the array.
 */
void imap_folders_free(struct remote_folder *rf, int num_remote_folders);

/**
 * @return whether the folder with the given name exists in the given remote
 *  folder array.
 */
int imap_remote_folder_exists(struct remote_folder *rf, int num_rf, const char *name);

struct imap_select_mailbox_args
{
	/** The already established connection */
	struct connection *conn;

	/** The utf8 encoded path of the mailbox to be selected */
	char *path;

	/** Whether mailbox should be selected in write mode */
	int writemode;

	void (*set_status_static)(const char *str);
	void (*set_status)(const char *str);
};

/**
 * Selects the given mailbox (as identified by args->path).
 *
 * @param args defines arguments and options.
 * @return a remote_mailbox for further processing. Field remote_mail_array will be NULL.
 */
struct remote_mailbox *imap_select_mailbox(struct imap_select_mailbox_args *args);

/**
 * Frees memory associated with the remote mailbox including the remote mailbox itself.
 * @param rm
 */
void imap_free_remote_mailbox(struct remote_mailbox *rm);

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

struct imap_get_remote_mails_args
{
	/** The already opened imap connection after successful login */
	struct connection *conn;

	/**
	 * Defines the remote path or mailbox from which the mail infos should be
	 * gathered
	 */
	char *path;

	/** Whether the mailbox should be opened in write mode */
	int writemode;

	/** Whether all of the mail headers should be downloaded */
	int headers;

	/**
	 * Defines the uid of first mail to be downloaded. Zero means that all mails
	 * should be downloaded (uid_end is ignored).
	 */
	unsigned int uid_start;

	/**
	 * Defines the uid of the final mail to be downloaded. Zero means that all mails
	 * should be downloaded (uid_end is ignored).
	 */
	unsigned int uid_end;

	void (*set_status_static)(const char *str);
	void (*set_status)(const char *str);
};

/**
 * Read information of all mails in the given path. Put this back into an array.
 *
 * @param args arguments to this function.
 *
 * @return returns information of the mailbox in form of a remote_mailbox object.
 *         NULL on failure (for any reasons). If not NULL, the elements in remote_mail_array
 *         are sorted according to their uids.
 *
 * @note the given path stays in the selected/examine state.
 * @note the returned structure must be free with imap_free_remote_mailbox()
 */
struct remote_mailbox *imap_get_remote_mails(struct imap_get_remote_mails_args *args);

#endif

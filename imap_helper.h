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

/**
 * Returns a list with string_node nodes which describes the folder names.
 * If you only want the subscribed folders set all to 0. Note that the
 * INBOX folder is always included if it does exist.
 *
 * @param conn the connection to write against
 * @param all whether all folders shall be returned or only the subscribed ones.
 * @return the list with string_nodes
 */
struct string_list *imap_get_folders(struct connection *conn, int all);

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
 * Selects the given mailbox (as identified by path).
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

#endif

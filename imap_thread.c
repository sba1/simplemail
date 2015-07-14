/**
 * @file
 *
 * The communications happens in a separate thread of name #IMAP_THREAD_NAME.
 */

#include "imap_thread.h"

#include <stdlib.h>
#include <string.h>

#include "account.h"
#include "debug.h"
#include "folder.h"
#include "imap.h"
#include "lists.h"
#include "mail.h"
#include "status.h"
#include "subthreads.h"
#include "support_indep.h"
#include "tcp.h"
#include "tcpip.h"

/*****************************************************************************/

/**
 * The name of the IMAP thread.
 */
#define IMAP_THREAD_NAME "SimpleMail - IMAP thread"


/*****************************************************************************/

/* Very similar to trans_set_status(). TODO: consolidate */
static void imap_set_status(const char *str)
{
	thread_call_parent_function_async_string(status_set_status, 1, str);
}

/*****************************************************************************/

/**
 * Contains the arguments submitted to the get folder list thread.
 */
struct imap_get_folder_list_entry_msg
{
	struct imap_server *server;
	void (*callback)(struct imap_server *, struct string_list *, struct string_list *);
};

/**
 * Entry point for the thread to get the folder list.
 *
 * @param msg
 * @return
 */
static int imap_get_folder_list_entry(struct imap_get_folder_list_entry_msg *msg)
{
	struct imap_server *server = imap_duplicate(msg->server);
	void (*callback)(struct imap_server *, struct string_list *, struct string_list *) = msg->callback;

	if (thread_parent_task_can_contiue())
	{
		thread_call_function_async(thread_get_main(),status_init,1,0);
		thread_call_function_async(thread_get_main(),status_open,0);

		imap_get_folder_list_really(server,callback);

		thread_call_function_async(thread_get_main(),status_close,0);
	}
	return 0;
}

/*****************************************************************************/

int imap_get_folder_list(struct imap_server *server, void (*callback)(struct imap_server *server, struct string_list *all_list, struct string_list *sub_list))
{
	struct imap_get_folder_list_entry_msg msg;
	msg.server = server;
	msg.callback = callback;
	return thread_start(THREAD_FUNCTION(&imap_get_folder_list_entry),&msg);
}


/**
 * Contains the arguments send to the submit folder list thread.
 */
struct imap_submit_folder_list_entry_msg
{
	struct imap_server *server;
	struct string_list *list;
};


/**
 * The entry point for the submit folder list thread.
 *
 * @param msg
 * @return
 */
static int imap_submit_folder_list_entry(struct imap_submit_folder_list_entry_msg *msg)
{
	struct imap_server *server = imap_duplicate(msg->server);
	struct string_list list;
	struct string_node *node;

	string_list_init(&list);
	node = string_list_first(msg->list);
	while (node)
	{
		string_list_insert_tail(&list,node->string);
		node = (struct string_node*)node_next(&node->node);
	}


	if (thread_parent_task_can_contiue())
	{
		thread_call_function_async(thread_get_main(),status_init,1,0);
		thread_call_function_async(thread_get_main(),status_open,0);

		imap_submit_folder_list_really(server,&list);

		thread_call_function_async(thread_get_main(),status_close,0);
	}
	string_list_clear(&list);
	return 0;
}

/*****************************************************************************/

int imap_submit_folder_list(struct imap_server *server, struct string_list *list)
{
	struct imap_submit_folder_list_entry_msg msg;
	msg.server = server;
	msg.list = list;
	return thread_start(THREAD_FUNCTION(&imap_submit_folder_list_entry),&msg);
}

/*****************************************************************************/

/***** IMAP Thread *****/

static thread_t imap_thread;
static struct connection *imap_connection;
static int imap_socket_lib_open;
static struct imap_server *imap_server;
static char *imap_folder; /** The path of the folder on the imap server */
static char *imap_local_path; /** The local path of the folder */

/**************************************************************************/

/**
 * The entry point for the imap thread. It just go into the wait state and
 * then frees all resources when finished
 *
 * @param test
 */
static void imap_thread_entry(void *test)
{
	if (thread_parent_task_can_contiue())
	{
		thread_wait(NULL,NULL,0);

		imap_free(imap_server);
		free(imap_folder);
		free(imap_local_path);

		if (imap_connection)
		{
			tcp_disconnect(imap_connection);
			imap_connection = NULL;
		}

		if (imap_socket_lib_open)
		{
			close_socket_lib();
			imap_socket_lib_open = 0;
		}
	}
}

/**
 * The function creates the IMAP thread. It doesn't start the thread
 * if it is already started.
 *
 * @return whether the thread has been started.
 */
static int imap_start_thread(void)
{
	if (imap_thread) return 1;
	imap_thread = thread_add(IMAP_THREAD_NAME, THREAD_FUNCTION(&imap_thread_entry),NULL);
	return !!imap_thread;
}

/**
 * Open the socket library, but make sure that this happns only once.
 *
 * @return socket is open (1) or not (0).
 */
static int imap_open_socket_lib(void)
{
	if (!imap_socket_lib_open)
	 imap_socket_lib_open = open_socket_lib();
	return imap_socket_lib_open;
}

/**
 * Disconnect the current imap connection if any is open.
 */
static void imap_disconnect(void)
{
	if (imap_connection)
	{
		tcp_disconnect(imap_connection);
		imap_connection = NULL;
	}
}

static int imap_thread_really_login_to_given_server(struct imap_server *server)
{
	if (!imap_open_socket_lib())
		return 0;

	if (!imap_connection || imap_new_connection_needed(imap_server,server))
	{
		free(imap_folder); imap_folder = NULL;
		free(imap_local_path); imap_local_path = NULL;

		if (imap_server) imap_free(imap_server);
		if ((imap_server = imap_duplicate(server)))
		{
			imap_disconnect();
			return imap_really_connect_and_login_to_server(&imap_connection, imap_server);
		}
		return 0;
	}
	return 1;
}

/**
 * Connect to the given imap server. Shall be invoked only in the context of the IMAP thread.
 *
 * @param server
 * @param folder
 * @param local_path
 * @return
 */
static int imap_thread_connect_to_server(struct imap_server *server, char *folder, char *local_path)
{
	static int connecting;

	int rc = 0;

	SM_ENTER;

	/* Ignore this request if we are already connecting. This can happen for instance, if a synchronous call
	 * to the parent task is issued.  */
	if (connecting)
	{
		SM_DEBUGF(5, ("Ignoring connect to server request for %s\n", local_path));

		if (server) imap_free(server);
		free(folder);
		free(local_path);
		goto bailout;
	}

	if (!imap_open_socket_lib())
		goto bailout;

	connecting = 1;

	if (!imap_connection || imap_new_connection_needed(imap_server,server))
	{
		struct imap_connect_to_server_options options = {0};

		imap_disconnect();

		if (imap_server) imap_free(imap_server);
		imap_server = server;

		free(imap_folder);
		imap_folder = folder;

		free(imap_local_path);
		imap_local_path = local_path;

		options.imap_local_path = imap_local_path;
		options.imap_server = imap_server;
		options.imap_folder = imap_folder;
		options.callbacks.set_status = imap_set_status;
		imap_really_connect_to_server(&imap_connection, &options);
		rc = 1;
	} else
	{
		struct imap_download_mails_options download_options = {0};

		imap_free(server);

		free(imap_folder);
		imap_folder = folder;

		free(imap_local_path);
		imap_local_path = local_path;

		download_options.imap_folder = imap_folder;
		download_options.imap_server = imap_server;
		download_options.imap_local_path = imap_local_path;
		imap_really_download_mails(imap_connection, &download_options);
		rc = 1;
	}

	connecting = 0;

bailout:
	SM_RETURN(rc,"%d");
	return rc;
}

/**
 * Download the given mail. Usually called in the context of the imap thread.
 *
 * @param server
 * @param local_path
 * @param m
 * @param callback called on the context of the parent task.
 * @param userdata user data supplied for the callback
 * @return
 */
static int imap_thread_download_mail(struct imap_server *server, char *local_path, struct mail_info *m, void (*callback)(struct mail_info *m, void *userdata), void *userdata)
{
	if (!imap_thread_really_login_to_given_server(server)) return 0;

	return imap_really_download_mail(imap_connection, server, local_path, m, callback, userdata);
}

/**
 * Move a given mail from one folder into another one of the given imap account.
 *
 * Usually called in the context of the imap thread.
 *
 * @param mail
 * @param server
 * @param src_folder
 * @param dest_folder
 * @return success or failure.
 */
static int imap_thread_move_mail(struct mail_info *mail, struct imap_server *server, struct folder *src_folder, struct folder *dest_folder)
{
	if (!imap_thread_really_login_to_given_server(server)) return 0;

	return imap_really_move_mail(imap_connection, mail, server, src_folder, dest_folder);
}

/**
 * Delete a mail permanently from the server
 *
 * @param filename
 * @param server
 * @param folder
 * @return success or not.
 * @note This function can only be called in the context of the imap thread.
 */
static int imap_thread_delete_mail_by_filename(char *filename, struct imap_server *server, struct folder *folder)
{
	struct imap_delete_mail_by_filename_options options = {0};

	if (!imap_thread_really_login_to_given_server(server)) return 0;

	options.filename = filename;
	options.folder = folder;

	return imap_really_delete_mail_by_filename(imap_connection, &options);
}

/**
 * Store the mail represented by the mail located in the given source_dir
 * on the given server in the given dest_folder.
 *
 * @param mail
 * @param source_dir
 * @param server
 * @param dest_folder
 * @return success or not.
 * @note This function can only be called in the context of the imap thread.
 */
static int imap_thread_append_mail(struct mail_info *mail, char *source_dir, struct imap_server *server, struct folder *dest_folder)
{
	if (!imap_thread_really_login_to_given_server(server)) return 0;

	return imap_really_append_mail(imap_connection, mail, source_dir, server, dest_folder);
}

/*****************************************************************************/

void imap_thread_connect(struct folder *folder)
{
	struct imap_server *server;
	char *imap_folder;
	char *imap_local_path;

	SM_ENTER;

	if (!(server = account_find_imap_server_by_folder(folder)))
	{
		SM_DEBUGF(5,("Server for folder %p (%s) not found\n",folder,folder?folder->name:"NONE"));
		goto bailout;
	}

	if (!imap_start_thread())
	{
		SM_DEBUGF(5,("Could not start IMAP thread\n"));
		goto bailout;
	}

	if (!(server = imap_duplicate(server)))
	{
		SM_DEBUGF(5,("Could not duplicate imap server\n"));
		goto bailout;
	}

	if ((!(imap_folder = mystrdup(folder->imap_path))) && folder->imap_path != NULL && strlen(folder->imap_path))
	{
		SM_DEBUGF(5,("Could not duplicate imap path\n"));
		goto bailout;
	}

	if (!(imap_local_path = mystrdup(folder->path)))
	{
		SM_DEBUGF(5,("Could not duplicate folder path\n"));
		goto bailout;
	}

	thread_call_function_async(imap_thread, imap_thread_connect_to_server, 4, server, imap_folder, imap_local_path, folder->imap_download);
bailout:
	SM_LEAVE;
}

/*****************************************************************************/

int imap_download_mail(struct folder *f, struct mail_info *m)
{
	struct imap_server *server;

	if (!(m->flags & MAIL_FLAGS_PARTIAL)) return 0;
	if (!(server = account_find_imap_server_by_folder(f))) return 0;
	if (!imap_start_thread()) return 0;

	if (thread_call_function_sync(imap_thread, imap_thread_download_mail, 5, server, f->path, m, NULL, NULL))
	{
		folder_set_mail_flags(f, m, (m->flags & (~MAIL_FLAGS_PARTIAL)));
		return 1;
	}
	return 0;
}

struct imap_download_data
{
	struct imap_server *server;
	char *local_path;
	char *remote_path;
	void (*callback)(struct mail_info *m, void *userdata);
	void *userdata;
};

/**
 * Function that is to be called when an email has been downloaded
 * asynchronously. Always called on the context of the parent task.
 *
 * @param m
 * @param userdata
 */
static void imap_download_mail_async_callback(struct mail_info *m, void *userdata)
{
	struct imap_download_data *d = (struct imap_download_data*)userdata;
	struct folder *local_folder;

	SM_ENTER;

	SM_DEBUGF(20,("m=%p, d=%p, local_path=%p\n",m,d,d->local_path));

	folders_lock();
	if ((local_folder = folder_find_by_path(d->local_path)))
		folder_lock(local_folder);
	folders_unlock();

	if (local_folder)
	{
		folder_set_mail_flags(local_folder, m, (m->flags & (~MAIL_FLAGS_PARTIAL)));
		folder_unlock(local_folder);
	}
	d->callback(m,d->userdata);
	mail_dereference(m);
	free(d->remote_path);
	free(d->local_path);
	imap_free(d->server);
	free(d);

	SM_LEAVE;
}

/*****************************************************************************/

int imap_download_mail_async(struct folder *f, struct mail_info *m, void (*callback)(struct mail_info *m, void *userdata), void *userdata)
{
	struct imap_download_data *d = NULL;

	SM_ENTER;

	if (!imap_start_thread()) goto bailout;
	if (!(d = malloc(sizeof(*d)))) goto bailout;
	memset(d,0,sizeof(*d));
	d->userdata = userdata;
	if (!(d->server = account_find_imap_server_by_folder(f))) goto bailout;
	if (!(d->server = imap_duplicate(d->server))) goto bailout;
	if (!(d->local_path = mystrdup(f->path))) goto bailout;
	if (!(d->remote_path = mystrdup(f->imap_path))) goto bailout;

	mail_reference(m);
	d->callback = callback;

	if (!thread_call_function_async(imap_thread, imap_thread_download_mail, 5, d->server, d->local_path, m, imap_download_mail_async_callback, d))
		goto bailout;
	SM_RETURN(1,"%ld");
	return 1;
bailout:
	if (d)
	{
		if (d->remote_path)
		{
			free(d->remote_path);
			mail_dereference(m);
		}
		free(d->local_path);
		imap_free(d->server);
		free(d);
	}
	SM_RETURN(0,"%ld");
	return 0;
}

/*****************************************************************************/

int imap_move_mail(struct mail_info *mail, struct folder *src_folder, struct folder *dest_folder)
{
	struct imap_server *server;

	if (!imap_start_thread()) return 0;
	if (!folder_on_same_imap_server(src_folder,dest_folder)) return 0;
	if (!(server = account_find_imap_server_by_folder(src_folder))) return 0;

	if (thread_call_function_sync(imap_thread, imap_thread_move_mail, 4, mail, server, src_folder, dest_folder))
	{
		return 1;
	}
	return 0;
}

/*****************************************************************************/

int imap_delete_mail_by_filename(char *filename, struct folder *folder)
{
	struct imap_server *server;

	if (!imap_start_thread()) return 0;
	if (!(server = account_find_imap_server_by_folder(folder))) return 0;

	if (thread_call_function_sync(imap_thread, imap_thread_delete_mail_by_filename, 3, filename, server, folder))
	{
		return 1;
	}
	return 0;
}

/*****************************************************************************/

int imap_append_mail(struct mail_info *mail, char *source_dir, struct folder *dest_folder)
{
	struct imap_server *server;

	if (!imap_start_thread()) return 0;
	if (!(server = account_find_imap_server_by_folder(dest_folder))) return 0;

	if (thread_call_function_sync(imap_thread, imap_thread_append_mail, 4, mail, source_dir, server, dest_folder))
	{
		return 1;
	}
	return 0;
}

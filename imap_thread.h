#ifndef IMAP_THREAD_H_
#define IMAP_THREAD_H_

#include "imap.h"

typedef void (*imap_get_folder_list_callback_t)(struct imap_server *server, struct remote_folder *all_folders, int num_all_folders, struct remote_folder *sub_folders, int num_sub_folders);

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
 *  folders. The callback is called on the main thread's context.
 *
 * @return whether the request has been in principle submitted or not.
 */
int imap_get_folder_list(struct imap_server *server, imap_get_folder_list_callback_t callback);

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

#endif /* IMAP_THREAD_H_ */

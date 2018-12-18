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
 *
 * @brief Provides IMAP4 support.
 *
 * Functions in this file are responsible for the communication with IMAP4 servers.
 *
 * @file imap.c
 */

#include "imap.h"

#include <ctype.h>
#include <dirent.h> /* unix dir stuff */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "account.h"
#include "codesets.h"
#include "debug.h"
#include "folder.h"
#include "imap_helper.h"
#include "logging.h"
#include "mail.h"
#include "progmon.h"
#include "qsort.h"
#include "smintl.h"
#include "support_indep.h"
#include "tcp.h"

#include "request.h"
#include "subthreads.h"
#include "support.h"
#include "tcpip.h"

#ifdef _AMIGA
#undef printf
#endif

#define MIN(a,b) ((a)<(b)?(a):(b))

/**
 * Maximum number of mails that can be hold before a refresh is invoked
 */
#define MAX_MAILS_PER_REFRESH 100

/**
 * Set the local mail array entry for the given entry.
 *
 * @param local_mail_array
 * @param i
 * @param fn
 * @return whether the mail is scheduled to be deleted or not.
 */
static int set_local_mail_array_entry(struct local_mail *local_mail_array, int i, const char *fn)
{
	int todel = mail_is_marked_as_deleted_by_filename(fn);

	if (fn[0] == 'u' || fn[0] == 'U' || todel)
	{
		local_mail_array[i].uid = atoi(fn + 1);
		local_mail_array[i].todel = todel;
	} else
	{
		local_mail_array[i].uid = 0;
		local_mail_array[i].todel = 0;
	}
	return todel;
}

/**
 * Returns the local mail array of a given folder. 0 for failure (does not
 * change the contents of the ptrs in that case). Free the array with free()
 * as soon as no longer needed. The mail array is sorted according to the uid
 * field.
 *
 * @param folder
 * @param local_mail_array_ptr
 * @param num_of_mails_ptr
 * @param num_of_todel_mails_ptr
 * @return 0 on failure, otherwise something different.
 */
static int get_local_mail_array(struct folder *folder, struct local_mail **local_mail_array_ptr, int *num_of_mails_ptr, int *num_of_todel_mails_ptr)
{
	struct local_mail *local_mail_array;
	int num_of_mails = 0, num_of_todel_mails = 0;
	int i,success = 0;

	/* Will house the names of mails in case there was no index of the folder loaded */
	struct string_list filename_list;
	int use_filename_list = 0;

	char logg_buf[80];

	SM_ENTER;

	string_list_init(&filename_list);

	folder_lock(folder);

	if (!folder->mail_infos_loaded)
	{
		/* No mail infos have been loaded, as we only need the filenames
		 * we just scan the directory rather than issuing a full rescan.
		 */
		DIR *dfd;
		struct dirent *dptr;

		SM_DEBUGF(10, ("Scanning folder \"%s\" for mails", folder->path));
		if (!(dfd = opendir(folder->path)))
		{
			sm_snprintf(logg_buf, sizeof(logg_buf), _("Couldn't get mails of locally saved IMAP folder \"%s\""), folder->name);
			SM_LOG_TEXT(ERROR, logg_buf);
			goto bailout;
		}

		use_filename_list = 1;
		while ((dptr = readdir(dfd)) != NULL)
		{
			char *name = dptr->d_name;

			if (!folder_is_filename_mail(name))
				continue;

			string_list_insert_tail(&filename_list, name);
			num_of_mails++;
		}
		closedir(dfd);
	} else
	{
		num_of_mails = folder->num_mails;
	}
	num_of_todel_mails = 0;

	SM_DEBUGF(10, ("%d mails in folder", num_of_mails));

	if ((local_mail_array = (struct local_mail *)malloc(sizeof(*local_mail_array) * num_of_mails)))
	{
		if (use_filename_list)
		{
			struct string_node *sn;

			i = 0;
			sn = string_list_first(&filename_list);
			while (sn)
			{
				const char *fn = sn->string;

				num_of_todel_mails += set_local_mail_array_entry(local_mail_array, i, fn);

				sn = string_node_next(sn);
				i++;
			}
			string_list_clear(&filename_list);
		} else
		{
			/* fill in the uids of the mails */
			for (i=0;i < num_of_mails;i++)
			{
				const char *fn = folder->mail_info_array[i]?folder->mail_info_array[i]->filename:"";

				num_of_todel_mails += set_local_mail_array_entry(local_mail_array, i, fn);
			}
		}

		/* now sort them */
		#define local_mail_lt(a,b) ((a->uid < b->uid)?1:0)
		QSORT(struct local_mail, local_mail_array, num_of_mails, local_mail_lt);

		success = 1;
		*local_mail_array_ptr = local_mail_array;
		*num_of_mails_ptr = num_of_mails;
		*num_of_todel_mails_ptr = num_of_todel_mails;
	}
bailout:
	folder_unlock(folder);

	sm_snprintf(logg_buf, sizeof(logg_buf), "Folder \"%s\" has %ld local mails, %d are scheduled for deletion", folder->name, num_of_mails, num_of_todel_mails);
	logg(INFO, 0, __FILE__, NULL, 0, logg_buf, LAST);

	SM_DEBUGF(20, ("num_of_mails=%d, num_of_todel_mails=%d\n", num_of_mails, num_of_todel_mails));
	SM_RETURN(success,"%d");
	return success;
}

struct imap_delete_orphan_messages_args
{
	struct local_mail *local_mail_array;
	int num_of_local_mails;

	struct remote_mail *remote_mail_array;
	int num_remote_mails;

	struct imap_server *imap_server;
	char *imap_folder;

	/** Callback that is invoked for each local uid that is not present in the remote mailbox */
	void (*delete_mail_by_uid)(char *user, char *server, char *path, unsigned int uid);
};

/**
 * Delete local mails that are not listed in the remote mail array (aka orphaned messages).
 *
 * @param options
 */
static void imap_delete_orphan_messages(struct imap_delete_orphan_messages_args *options)
{
	int i,j;

	struct local_mail *local_mail_array = options->local_mail_array;
	int num_of_local_mails = options->num_of_local_mails;

	struct remote_mail *remote_mail_array = options->remote_mail_array;
	int num_remote_mails = options->num_remote_mails;
	struct imap_server *imap_server = options->imap_server;
	char *imap_folder = options->imap_folder;

	SM_ENTER;
	SM_DEBUGF(20,("num_of_local_mails=%d, num_remote_mails=%d\n", num_of_local_mails, num_remote_mails));

	i = j = 0;
	while (i<num_of_local_mails && j<num_remote_mails)
	{
		unsigned int local_uid = local_mail_array[i].uid;
		unsigned int remote_uid = remote_mail_array[j].uid;

		if (local_uid < remote_uid)
		{
			if (local_uid) options->delete_mail_by_uid(imap_server->login,imap_server->name,imap_folder,local_uid);
			i++;
		}
		else if (local_uid > remote_uid) j++;
		else
		{
			i++;
			/* FIXME: If local mail list would not (possibly) contain ties, we
			 * could increment j as well */
		}
	}
	/* Delete the rest */
	for (;i<num_of_local_mails;i++)
	{
		unsigned int local_uid = local_mail_array[i].uid;
		if (local_uid) options->delete_mail_by_uid(imap_server->login,imap_server->name,imap_folder,local_uid);
	}

	SM_LEAVE;
}

/**
 * Synchronize the folder.
 *
 * @param conn
 * @param server
 * @param imap_path
 * @param callbacks
 * @return 1 on success, 0 for failure.
 */
static int imap_synchonize_folder(struct connection *conn, struct imap_server *server, char *imap_path, struct imap_synchronize_callbacks *callbacks)
{
	struct folder *folder;
	int success = 0;

	SM_DEBUGF(10,("Synchronizing folder \"%s\"\n",imap_path));

	folders_lock();
	if ((folder = folder_find_by_imap(server->login,server->name, imap_path)))
	{
		int num_of_local_mails;
		int num_of_todel_local_mails;
		struct local_mail *local_mail_array;
		char path[380];

		/* Get the local mails */
		if (!get_local_mail_array(folder, &local_mail_array, &num_of_local_mails, &num_of_todel_local_mails))
		{
			folders_unlock();
			return 0;
		}

		/* Change the current directory to the to be synchronized folder */
		folder_lock(folder);
		getcwd(path, sizeof(path));
		if (chdir(folder->path) == -1)
		{
			free(local_mail_array);
			folder_unlock(folder);
			folders_unlock();
			return 0;
		}
		folder_unlock(folder);

		folders_unlock();

		if (local_mail_array)
		{
			struct remote_mailbox *rm;

			/* get number of remote mails */
			char *line;
			char tag[20];
			char send[200];
			char buf[100];
			char status_buf[200];

			struct imap_get_remote_mails_args args = {0};

			args.conn = conn;
			args.path = folder->imap_path;
			args.set_status = callbacks->set_status;
			args.set_status_static = callbacks->set_status_static;

			if (num_of_todel_local_mails)
			{
				args.writemode = 1;

				if ((rm = imap_get_remote_mails(&args)))
				{
					struct remote_mail *remote_mail_array;
					int num_of_remote_mails;
					int i,j;

					remote_mail_array = rm->remote_mail_array;
					num_of_remote_mails = rm->num_of_remote_mail;

					sm_snprintf(status_buf,sizeof(status_buf),_("Removing %d mails from server"),num_of_todel_local_mails);
					callbacks->set_status(status_buf);

					for (i = 0 ; i < num_of_local_mails; i++)
					{
						if (mail_is_marked_as_deleted(folder->mail_info_array[i]))
						{
							for (j = 0; j < num_of_remote_mails; j++)
							{
								if (local_mail_array[i].uid == remote_mail_array[j].uid)
								{
									sprintf(tag,"%04x",imap_val++);
									sprintf(send,"%s STORE %d +FLAGS.SILENT (\\Deleted)\r\n",tag,j+1);
									tcp_write(conn,send,strlen(send));
									tcp_flush(conn);

									success = 0;
									while ((line = tcp_readln(conn)))
									{
										line = imap_get_result(line,buf,sizeof(buf));
										if (!mystricmp(buf,tag))
										{
											line = imap_get_result(line,buf,sizeof(buf));
											if (!mystricmp(buf,"OK"))
												success = 1;
											break;
										}
									}
									if (!success) break;
								}
							}
						}
					}

					if (success)
					{
						sprintf(tag,"%04x",imap_val++);
						sprintf(send,"%s EXPUNGE\r\n",tag);
						tcp_write(conn,send,strlen(send));
						tcp_flush(conn);

						success = 0;
						while ((line = tcp_readln(conn)))
						{
							line = imap_get_result(line,buf,sizeof(buf));
							if (!mystricmp(buf,tag))
							{
								line = imap_get_result(line,buf,sizeof(buf));
								if (!mystricmp(buf,"OK"))
									success = 1;
								break;
							}
						}
					}
					imap_free_remote_mailbox(rm);
				}
			}

			args.writemode = 0;

			/* Get information of all mails within the folder */
			if ((rm = imap_get_remote_mails(&args)))
			{
				int i,j;
				unsigned int max_todl_bytes = 0;
				unsigned int accu_todl_bytes = 0; /* this represents the exact todl bytes according to the RFC822.SIZE */
				unsigned int todl_bytes = 0;
				unsigned int num_msgs_to_dl = 0; /* number of mails that we really want to download */

				struct remote_mail *remote_mail_array;
				int num_of_remote_mails;

				remote_mail_array = rm->remote_mail_array;
				num_of_remote_mails = rm->num_of_remote_mail;

				if (!server->keep_orphans)
				{
					struct imap_delete_orphan_messages_args args = {0};

					args.local_mail_array = local_mail_array;
					args.num_of_local_mails = num_of_local_mails;
					args.remote_mail_array = remote_mail_array;
					args.num_remote_mails = num_of_remote_mails;
					args.imap_server = server;
					args.imap_folder = imap_path;
					args.delete_mail_by_uid = callbacks->delete_mail_by_uid;

					imap_delete_orphan_messages(&args);
				}

				/* Determine the number of bytes and mails which we are going to download
				 * Note that the contents of remote mail is partly destroyed. When we are
				 * ready, the top of the remote array contains the uids (as uids), the sizes
				 * (as size), and the indices (in field flags) that we want to download.
				 * The number of mails is kept in num_msgs_to_dl.
				 */
				i=j=0;
				while (i<num_of_remote_mails)
				{
					if (j < num_of_local_mails)
					{
						unsigned int remote_uid = remote_mail_array[i].uid;
						unsigned int local_uid = local_mail_array[j].uid;
						if (remote_uid >= local_uid)
						{
							/* Definitely skip local mail */
							j++;

							/* Skip also remote, if it matches the local uid (thus, the mail was present) */
							if (remote_uid == local_uid)
								i++;
							continue;
						}
					}
					max_todl_bytes += remote_mail_array[i].size;

					/* Note that num_msgs_to_dl <= i. Also note that harm will be done
					 * if num_msgs_to_dl == i. */
					remote_mail_array[num_msgs_to_dl].uid = remote_mail_array[i].uid;
					remote_mail_array[num_msgs_to_dl].size = remote_mail_array[i].size;
					remote_mail_array[num_msgs_to_dl].flags = i;
					num_msgs_to_dl++;
					i++;
				}

				/* Assume everything is fine */
				success = 1;
				callbacks->init_gauge_as_bytes(max_todl_bytes);

				for (i=0;i<num_msgs_to_dl;i++)
				{
					accu_todl_bytes += remote_mail_array[i].size;

					sm_snprintf(status_buf,sizeof(status_buf),_("Fetching mail %d from server"),remote_mail_array[i].flags + 1);
					callbacks->set_status(status_buf);

					sprintf(tag,"%04x",imap_val++);
					sprintf(send,"%s FETCH %d RFC822\r\n",tag,remote_mail_array[i].flags+1); /* We could also use the UID command */
					tcp_write(conn,send,strlen(send));
					tcp_flush(conn);

					success = 0;
					while ((line = tcp_readln(conn)))
					{
						line = imap_get_result(line,buf,sizeof(buf));
						if (!mystricmp(buf,tag))
						{
							line = imap_get_result(line,buf,sizeof(buf));
							if (!mystricmp(buf,"OK"))
								success = 1;
							break;
						} else
						{
							char msgno_buf[200];
							char *temp_ptr;
							int todownload;

							/* We only handle untagged lines here */
							if (buf[0] != '*')
								continue;

							line++;

							line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
							/*atoi(msgno_buf);*/ /* ignored */

							/* skip the fetch command */
							line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
							if ((temp_ptr = strchr(line,'{'))) /* } - avoid bracket checking problems */
							{
								temp_ptr++;
								todownload = atoi(temp_ptr);
							} else todownload = 0;

							if (todownload)
							{
								FILE *fh;
								char filename_buf[32];
								sprintf(filename_buf,"u%d",remote_mail_array[i].uid); /* u means unchanged */

								if ((fh = fopen(filename_buf,"w")))
								{
									while (todownload)
									{
										char buf[204];
										int dl;
										dl = tcp_read(conn,buf,MIN((sizeof(buf)-4),todownload));

										if (dl == -1 || !dl) break;
										todl_bytes = MIN(accu_todl_bytes,todl_bytes + dl);
										callbacks->set_gauge(todl_bytes);
										fwrite(buf,1,dl,fh);
										todownload -= dl;
									}
									fclose(fh);
									callbacks->new_mail_arrived(filename_buf, server->login, server->name, imap_path);
								}
							}
						}
					}

					if (!success) break;
					todl_bytes = accu_todl_bytes;
					callbacks->set_gauge(todl_bytes);
				}

				imap_free_remote_mailbox(rm);
			}
		}

		chdir(path);

		free(local_mail_array);
	} else folders_unlock();
	return success;
}

/*****************************************************************************/

/**
 * Synchronize a single imap account.
 *
 * @param imap_server
 * @param options
 * @return 0 for failure, 1 for success.
 * @note 0 currently means abort.
 */
static int imap_synchronize_really_single(struct imap_server *server, struct imap_synchronize_callbacks *callbacks)
{
	struct connection *conn;
	struct imap_connect_and_login_to_server_callbacks login_callbacks = {0};
	char head_buf[100];

	SM_DEBUGF(10,("Synchronizing with server \"%s\"\n",server->name));

	sm_snprintf(head_buf,sizeof(head_buf),_("Synchronizing mails with %s"),server->name);
	callbacks->set_head(head_buf);
	if (server->title)
		callbacks->set_title_utf8(server->title);
	else
		callbacks->set_title(server->name);
	callbacks->set_connect_to_server(server->name);

	login_callbacks.request_login = callbacks->request_login;
	login_callbacks.set_status = callbacks->set_status;

	if (imap_really_connect_and_login_to_server(&conn, server, &login_callbacks))
	{
		struct remote_folder *rf = NULL;
		int num_rf;

		callbacks->set_status_static(_("Login successful"));
		callbacks->set_status_static(_("Checking for folders"));

		SM_DEBUGF(10,("Get folders\n"));
		if ((rf = imap_get_folders(conn,0,&num_rf)))
		{
			int i;

			/* add the folders */
			for (i = 0; i < num_rf; i++)
			{
				callbacks->add_imap_folder(server->login, server->name, rf[i].name, rf[i].delim);
			}
			callbacks->refresh_folders();

			/* sync the folders */
			for (i = 0; i < num_rf; i++)
			{
				if (!(imap_synchonize_folder(conn, server, rf[i].name, callbacks)))
				{
					sm_snprintf(head_buf,sizeof(head_buf),_("Failed to sync folder \"%s\""), rf[i].name);
					callbacks->set_status(head_buf);
					break;
				}
			}
		}

		tcp_disconnect(conn);
	}

	if (thread_aborted()) return 0;

	/* We handle only the abort case as failure for now */
	return 1;
}

/*****************************************************************************/

void imap_synchronize_really(struct imap_synchronize_options *options)
{
	struct imap_synchronize_callbacks *callbacks = &options->callbacks;
	struct imap_server *server;

	SM_ENTER;

	if (!open_socket_lib())
	{
		tell_from_subtask(N_("Cannot open the bsdsocket.library!"));
		goto out;
	}

	server = (struct imap_server*)list_first(options->imap_list);
	while (server)
	{
		if (!imap_synchronize_really_single(server, callbacks))
			break;
		server = (struct imap_server*)node_next(&server->node);
	}
	close_socket_lib();

out:
	SM_LEAVE;
}

/*****************************************************************************/

int imap_get_folder_list_really(struct imap_get_folder_list_options *options,
	struct remote_folder **all_folders_out, int *num_all_folders_out,
	struct remote_folder **sub_folders_out, int *num_sub_folders_out)
{
	struct imap_get_folder_list_callbacks *callbacks = &options->callbacks;
	struct connection *conn = NULL;
	struct connect_options conn_opts = {0};
	struct string_list *all_folder_list = NULL;
	struct string_list *sub_folder_list = NULL;
	char head_buf[100];
	int error_code;
	int rc = 0;

	struct imap_server *server = options->server;

	if (!open_socket_lib())
		return 0;

	sm_snprintf(head_buf, sizeof(head_buf), _("Reading folders of %s"),server->name);
	callbacks->set_head(head_buf);
	if (server->title)
		callbacks->set_title_utf8(server->title);
	else
		callbacks->set_title(server->name);
	callbacks->set_connect_to_server(server->name);

	conn_opts.use_ssl = server->ssl;
	conn_opts.fingerprint = server->fingerprint;

	if (!(conn = tcp_connect(server->name, server->port, &conn_opts, &error_code)))
		goto bailout;

	callbacks->set_status_static(_("Waiting for login..."));
	if (!imap_wait_login(conn,server))
		goto bailout;

	callbacks->set_status_static(_("Login..."));
	if (!imap_login(conn,server))
	{
		callbacks->set_status_static(_("Login failed!"));
		tell_from_subtask(N_("Authentication failed!"));
		goto bailout;
	}

	callbacks->set_status_static(_("Reading folders..."));
	if (!(*all_folders_out = imap_get_folders(conn, 1, num_all_folders_out)))
		goto bailout;

	if (!(*sub_folders_out = imap_get_folders(conn, 0, num_sub_folders_out)))
		goto bailout;

	rc = 1;
bailout:
	if (!rc)
	{
		string_list_free(sub_folder_list);
		string_list_free(all_folder_list);
	}
	if (conn)  tcp_disconnect(conn);
	close_socket_lib();
	return rc;
}

/*****************************************************************************/

void imap_submit_folder_list_really(struct imap_submit_folder_options *options)
{
	struct imap_server *server = options->server;
	struct string_list *list = options->list;
	struct imap_submit_folder_list_callbacks *callbacks = &options->callbacks;

	struct connection *conn;
	struct connect_options conn_opts = {0};
	char head_buf[100];
	int error_code;
	struct remote_folder *all_folders = NULL;
	struct remote_folder *sub_folders = NULL;
	int i, num_all_folders = 0, num_sub_folders = 0;

	char *line;
	char tag[20];
	char send[200];
	char buf[100];
	struct string_node *node;

	if (!open_socket_lib())
		return;

	sm_snprintf(head_buf,sizeof(head_buf),_("Submitting subscribed folders to %s"),server->name);
	callbacks->set_head(head_buf);
	if (server->title)
		callbacks->set_title_utf8(server->title);
	else
		callbacks->set_title(server->name);
	callbacks->set_connect_to_server(server->name);

	conn_opts.use_ssl = server->ssl;
	conn_opts.fingerprint = server->fingerprint;

	if (!(conn = tcp_connect(server->name, server->port, &conn_opts, &error_code)))
		goto out;

	callbacks->set_status_static(_("Waiting for login..."));
	if (!imap_wait_login(conn,server))
		goto out;

	callbacks->set_status_static(_("Login..."));
	if (!imap_login(conn,server))
		goto out;

	callbacks->set_status_static(_("Reading folders..."));
	if (!(all_folders = imap_get_folders(conn, 1, &num_all_folders)))
		goto out;

	callbacks->set_status_static(_("Reading subscribed folders..."));
	if (!(sub_folders = imap_get_folders(conn, 0, &num_sub_folders)))
		goto out;

	for (node = string_list_first(list); node; node = string_node_next(node))
	{
		if (!imap_remote_folder_exists(sub_folders, num_sub_folders, node->string))
		{
			char *path = utf8toiutf7(node->string,strlen(node->string));
			if (path)
			{
				/* subscribe this folder */
				sprintf(tag,"%04x",imap_val++);
				sprintf(send,"%s SUBSCRIBE \"%s\"\r\n",tag,path);
				tcp_write(conn,send,strlen(send));
				tcp_flush(conn);

				while ((line = tcp_readln(conn)))
				{
					char *saved_line = line; /* for debugging reasons */

					line = imap_get_result(line,buf,sizeof(buf));
					if (!mystricmp(buf,tag))
					{
						line = imap_get_result(line,buf,sizeof(buf));
						if (mystricmp(buf,"OK"))
						{
							/* If it is not OK it is a failure */
							SM_DEBUGF(20,("%s",send));
							SM_DEBUGF(20,("%s",saved_line));

							tell_from_subtask(_("Subscribing folders failed!"));
						}
						break;
					}
				}
			}
			free(path);
		}
	}

	/* Unsubscribe from all folders that are not listed in the given list */
	for (i = 0; i < num_sub_folders; i++)
	{
		if (!string_list_find(list, sub_folders[i].name))
		{
			char *path = utf8toiutf7(node->string,strlen(node->string));
			if (path)
			{
				/* Unsubscribe this folder */
				sprintf(tag,"%04x",imap_val++);
				sprintf(send,"%s UNSUBSCRIBE \"%s\"\r\n",tag,path);
				tcp_write(conn,send,strlen(send));
				tcp_flush(conn);

				while ((line = tcp_readln(conn)))
				{
					line = imap_get_result(line,buf,sizeof(buf));
					if (!mystricmp(buf,tag))
					{
						line = imap_get_result(line,buf,sizeof(buf));
						if (mystricmp(buf,"OK"))
						{
							/* If it is not OK , it's a failure */
							tell_from_subtask(_("Unsubscribing folders failed!"));
						}
						break;
					}
				}
				free(path);
			}
		}
	}
out:
	imap_folders_free(all_folders, num_all_folders);
	imap_folders_free(sub_folders, num_sub_folders);
	tcp_disconnect(conn);
	close_socket_lib();
}

/*****************************************************************************/

int imap_really_connect_and_login_to_server(struct connection **connection, struct imap_server *imap_server, struct imap_connect_and_login_to_server_callbacks *callbacks)
{
	int success = 0;
	char status_buf[160];
	struct connect_options conn_opts = {0};
	int error_code;
	struct connection *imap_connection = NULL;

	SM_ENTER;

	if (!imap_server)
		goto bailout;

	if (imap_server->ask)
	{
		char *password = (char*)malloc(512);
		char *login = (char*)malloc(512);

		if (password && login)
		{
			if (imap_server->login) mystrlcpy(login,imap_server->login,512);
			password[0] = 0;

			if (callbacks->request_login(imap_server->name,login,password,512))
			{
				imap_server->login = mystrdup(login);
				imap_server->passwd = mystrdup(password);
			} else
			{
				free(password);
				free(login);
				SM_RETURN(0,"%ld");
				return 0;
			}
		}

		free(password);
		free(login);
	}

	/* Display "Connecting" - status message */
	sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Connecting..."));
	callbacks->set_status(status_buf);

	conn_opts.use_ssl = imap_server->ssl;
	conn_opts.fingerprint = imap_server->fingerprint;

	if ((imap_connection = tcp_connect(imap_server->name, imap_server->port, &conn_opts, &error_code)))
	{
		SM_DEBUGF(20,("Connected to %s\n",imap_server->name));

		if (imap_wait_login(imap_connection,imap_server))
		{
			/* Display "Logging in" - status message */
			sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Logging in..."));
			callbacks->set_status(status_buf);

			if (imap_login(imap_connection,imap_server))
			{
				/* Display "Connected" - status message */
				sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Connected"));
				callbacks->set_status(status_buf);

				success = 1;
			} else
			{
				SM_DEBUGF(10,("Login failed\n"));
				sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Log in failed. Check user name and password for this account."));
				callbacks->set_status(status_buf);
				tcp_disconnect(imap_connection);
				imap_connection = NULL;
				tell_from_subtask(N_("Authentication failed!"));
			}
		} else
		{
			sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Unexpected server answer. Connection failed."));
			callbacks->set_status(status_buf);
			tcp_disconnect(imap_connection);
			imap_connection = NULL;
		}
	} else
	{
		/* Display "Failed to connect" - status message */
		sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Connecting to the server failed"));
		callbacks->set_status(status_buf);
	}

bailout:
	*connection = imap_connection;
	SM_RETURN(success,"%ld");
	return success;
}

/*****************************************************************************/

int imap_really_download_mails(struct connection *imap_connection, struct imap_download_mails_options *options)
{
	char path[380];
	struct folder *local_folder;
	struct remote_mailbox *rm;

	int do_download = 1;
	int downloaded_mails = 0;
	int dont_use_uids = options->uid_options.imap_dont_use_uids;

	unsigned int local_uid_validiy = options->uid_options.imap_uid_validity;
	unsigned int local_uid_next = options->uid_options.imap_uid_next;

	unsigned int uid_from = 0;
	unsigned int uid_to = 0;

	char *imap_local_path = options->imap_local_path;
	struct imap_server *imap_server = options->imap_server;
	char *imap_folder = options->imap_folder;
	struct imap_download_mails_callbacks *callbacks = &options->callbacks;

	struct progmon *pm;

	SM_ENTER;

	if (!imap_connection)
	{
		SM_RETURN(-1,"%d");
		return -1;
	}

	SM_DEBUGF(10,("Downloading mails of folder \"%s\"\n",imap_folder));

	/* Determine uid values from folder if this shall not be skipped and if
	 * the uids are not known.
	 */
	if (!dont_use_uids && !local_uid_next)
	{
		folders_lock();
		if ((local_folder = folder_find_by_imap(imap_server->login, imap_server->name, imap_folder)))
		{
			local_uid_validiy = local_folder->imap_uid_validity;
			local_uid_next = local_folder->imap_uid_next;
			dont_use_uids = local_folder->imap_dont_use_uids;
		}
		folders_unlock();
	}

	/* Uids are valid only if they are non-zero */
	if (local_uid_validiy != 0 && local_uid_next != 0 && !dont_use_uids)
	{
		struct imap_select_mailbox_args select_mailbox_args = {0};
		select_mailbox_args.conn = imap_connection;
		select_mailbox_args.path = imap_folder;
		select_mailbox_args.set_status = callbacks->set_status;
		select_mailbox_args.set_status_static = callbacks->set_status_static;

		if ((rm = imap_select_mailbox(&select_mailbox_args)))
		{
			if (rm->uid_validity == local_uid_validiy)
			{
				if (rm->uid_next == local_uid_next)
				{
					/* Now new mails since called last time */
					do_download = 0;

					SM_DEBUGF(10,("UIDs match. Therefore no mails need to be downloaded.\n"));
				} else
				{
					uid_from = local_uid_next;
					uid_to = rm->uid_next;

					SM_DEBUGF(10,("Downloading mails from uid %ld to %ld\n",uid_from,uid_to));
				}
			}
			imap_free_remote_mailbox(rm);
			rm = NULL;
		}
	}

	if (do_download)
	{
		folders_lock();
		if ((local_folder = folder_find_by_imap(imap_server->login, imap_server->name, imap_folder)))
		{
			int num_of_local_mails;
			int num_of_todel_local_mails;
			struct local_mail *local_mail_array;

			if (get_local_mail_array(local_folder, &local_mail_array, &num_of_local_mails, &num_of_todel_local_mails))
			{
				struct imap_get_remote_mails_args args = {0};

				utf8 msg[80];

				folders_unlock();

				pm = progmon_create();
				if (pm)
				{
					utf8fromstr(_("Downloading mail headers"),NULL,msg,sizeof(msg));
					pm->begin(pm,1001,msg);

					utf8fromstr(_("Determining which mail headers to download"),NULL,msg,sizeof(msg));
					pm->working_on(pm,msg);
				}

				args.conn = imap_connection;
				args.path = imap_folder;
				args.writemode = 0;
				args.headers = 1;
				args.uid_start = uid_from;
				args.uid_end = uid_to;
				args.set_status = options->callbacks.set_status;
				args.set_status_static = options->callbacks.set_status_static;

				if ((rm = imap_get_remote_mails(&args)))
				{
					int i,j;

					struct remote_mail *remote_mail_array;
					int num_remote_mails;

					char *filename_ptrs[MAX_MAILS_PER_REFRESH];
					int filename_current = 0;
					unsigned int total_download_ticks;
					unsigned int ticks;
					unsigned int current_work = 1;

					if (pm)
					{
						pm->work(pm,1);
						utf8fromstr(_("Downloading new mails"),NULL,msg,sizeof(msg));
						pm->working_on(pm,msg);
					}

					num_remote_mails = rm->num_of_remote_mail;
					remote_mail_array = rm->remote_mail_array;

					/* Change dir to given local path (TODO: This is the same as f->path) */
					getcwd(path, sizeof(path));
					if (chdir(imap_local_path) == -1)
					{
						char logg_buf[80];
						sm_snprintf(logg_buf, sizeof(logg_buf), _("Failed to change directory to \"%s\""), imap_local_path);
						SM_LOG_TEXT(ERROR, logg_buf);
						goto dl_done;
					}

					total_download_ticks = ticks = time_reference_ticks();

					/* Check each remote mail whether it is already stored locally. If
					 * not store the header. Note that we exploit the sorting order
					 * of both the remote_mail_array and local_mail_array.
					 */
					i=j=0;
					while (i<num_remote_mails)
					{
						char filename_buf[32];
						char *filename;
						FILE *fh;
						int status = 0;

						/* Skip 0 uids */
						if (!remote_mail_array[i].uid)
						{
							i++;
							continue;
						}
						if (j < num_of_local_mails)
						{
							unsigned int remote_uid = remote_mail_array[i].uid;
							unsigned int local_uid = local_mail_array[j].uid;
							if (remote_uid >= local_uid)
							{
								/* Definitely skip local mail */
								j++;

								/* Skip also remote, if it matches the local uid (thus, the mail was present) */
								if (remote_uid == local_uid)
									i++;
								continue;
							}
						}

						downloaded_mails++;

						if (remote_mail_array[i].flags & RM_FLAG_ANSWERED) status = MAIL_STATUS_REPLIED;
						else if (remote_mail_array[i].flags & RM_FLAG_SEEN) status = MAIL_STATUS_READ;

						if (remote_mail_array[i].flags & RM_FLAG_FLAGGED) status |= MAIL_STATUS_FLAG_MARKED;

						sprintf(filename_buf,"u%d",remote_mail_array[i].uid); /* u means unchanged */
						if ((filename = mail_get_status_filename(filename_buf, status)))
						{
							/* Store as a partial mail */
							if ((fh = fopen(filename,"w")))
							{
								fprintf(fh,"X-SimpleMail-Partial: yes\n");
								fprintf(fh,"X-SimpleMail-Size: %d\n",remote_mail_array[i].size);
								if (remote_mail_array[i].headers) fputs(remote_mail_array[i].headers,fh);
								fclose(fh);

								filename_ptrs[filename_current++] = filename;

								if (filename_current == MAX_MAILS_PER_REFRESH || time_ticks_passed(ticks) > TIME_TICKS_PER_SECOND / 2)
								{
									callbacks->new_mails_arrived(filename_current, filename_ptrs, imap_server->login, imap_server->name, imap_folder);
									while (filename_current)
										free(filename_ptrs[--filename_current]);

									ticks = time_reference_ticks();

									if (pm)
									{
										unsigned int new_work = 1 + (i * 1000 / num_remote_mails);
										if (new_work != current_work)
										{
											pm->work(pm, new_work - current_work);
											current_work = new_work;
										}
									}
								}
							}
						}
						i++;
					}

					/* Add the rest */
					if (filename_current)
					{
						callbacks->new_mails_arrived(filename_current, filename_ptrs, imap_server->login, imap_server->name, imap_folder);
						while (filename_current)
							free(filename_ptrs[--filename_current]);
					}

					{
						char buf[80];
						sm_snprintf(buf, sizeof(buf), "%d mails downloaded after %d ms to folder \"%s\"", downloaded_mails, time_ms_passed(total_download_ticks), local_folder->path);
						logg(INFO, 0, __FILE__, NULL, 0, buf, LAST);
					}

					SM_DEBUGF(10,("%d mails downloaded after %d ms\n",downloaded_mails,time_ms_passed(total_download_ticks)));

					/* Finally, inform controller about new uids */
					callbacks->new_uids(rm->uid_validity, rm->uid_next, imap_server->login, imap_server->name, imap_folder);

					if (uid_from && uid_to)
					{
						/* connection and folder stays */
						args.uid_start = 0;
						args.uid_end = 0;
						args.writemode = 0;
						args.headers = 0;

						imap_free_remote_mailbox(rm);
						/* Rescan folder in order to delete orphaned messages */
						if (!imap_server->keep_orphans)
							rm = imap_get_remote_mails(&args);
						else
							rm = NULL;
					}

					if (rm)
					{
						if (!imap_server->keep_orphans)
						{
							/* Now delete orphaned messages */

							struct imap_delete_orphan_messages_args args = {0};

							args.local_mail_array = local_mail_array;
							args.num_of_local_mails = num_of_local_mails;
							if (rm)
							{
								args.remote_mail_array = rm->remote_mail_array;
								args.num_remote_mails = rm->num_of_remote_mail;
							}
							args.imap_server = imap_server;
							args.imap_folder = imap_folder;
							args.delete_mail_by_uid = callbacks->delete_mail_by_uid;

							imap_delete_orphan_messages(&args);
						}
						if (rm)
						{
							imap_free_remote_mailbox(rm);
						}
					}
				}
dl_done:
				chdir(path);
				if (pm)
				{
					pm->done(pm);
					progmon_delete(pm);
				}
				free(local_mail_array);
			} else folders_unlock();
		} else folders_unlock();
	}

	/* Display status message. We mis-use path here */
	{
		int l;
		const char *f = imap_folder?imap_folder:"Root";

		l = sm_snprintf(path,sizeof(path),"%s: ",imap_server->name);
		switch (downloaded_mails)
		{
			case 0: sm_snprintf(&path[l], sizeof(path) - l,_("No new mails in folder \"%s\""),f); break;
			case 1: sm_snprintf(&path[l], sizeof(path) - l,_("One new mail in folder \"%s\""),f); break;
			default: sm_snprintf(&path[l], sizeof(path) - l,_("%d new mails in folder \"%s\""),downloaded_mails,f); break;
		}

		callbacks->set_status(path);
	}

	SM_RETURN(downloaded_mails,"%d");
	return downloaded_mails;
}

/*****************************************************************************/

void imap_really_connect_to_server(struct connection **imap_connection, struct imap_connect_to_server_options *options)
{
	int i, num_rf;
	char status_buf[160];
	struct remote_folder *rf = NULL;
	struct progmon *pm = NULL;
	struct imap_connect_to_server_callbacks *callbacks = &options->callbacks;
	struct imap_download_mails_options download_options = {0};
	struct imap_connect_and_login_to_server_callbacks connect_and_login_callbacks = {0};

	SM_ENTER;

	if (!options->imap_server) goto bailout;

	pm = progmon_create();
	if (pm)
	{
		utf8 msg[80];

		utf8fromstr(_("Connect with IMAP server"),NULL,msg,sizeof(msg));
		pm->begin(pm,2,msg);

		utf8fromstr(_("Logging in"),NULL,msg,sizeof(msg));
		pm->working_on(pm,msg);
	}

	connect_and_login_callbacks.set_status = options->callbacks.set_status;
	connect_and_login_callbacks.request_login = options->callbacks.request_login;

	if (!imap_really_connect_and_login_to_server(imap_connection, options->imap_server, &connect_and_login_callbacks))
		goto bailout;

	/* Display "Retrieving mail folders" - status message */
	sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",options->imap_server->name, _("Retrieving mail folders..."));
	callbacks->set_status(status_buf);

	/* We have now connected to the server, check for the folders at first */
	if (!(rf = imap_get_folders(*imap_connection, 0, &num_rf)))
		goto bailout;

	/* add the folders */
	for (i = 0; i < num_rf; i++)
	{
		callbacks->add_imap_folder(options->imap_server->login, options->imap_server->name, rf[i].name, rf[i].delim);
	}
	callbacks->refresh_folders();

	download_options.imap_folder = options->imap_folder;
	download_options.imap_local_path = options->imap_local_path;
	download_options.imap_server = options->imap_server;
	download_options.callbacks = options->download_callbacks;
	imap_really_download_mails(*imap_connection, &download_options);

bailout:
	imap_folders_free(rf, num_rf);

	if (pm)
	{
		pm->done(pm);
		progmon_delete(pm);
	}

	SM_LEAVE;
}

/*****************************************************************************/

int imap_really_delete_mail_by_filename(struct connection *imap_connection, struct imap_delete_mail_by_filename_options *options)
{
	char send[200];
	char tag[20];
	char buf[380];
	char *line;
	int uid;
	int msgno = -1;
	int success;

	char *filename = options->filename;
	struct folder *folder = options->folder;
	sm_snprintf(send,sizeof(send),"SELECT \"%s\"\r\n",folder->imap_path);
	if (!imap_send_simple_command(imap_connection,send)) return 0;

	success = 0;
	uid = atoi(filename + 1);

	sprintf(tag,"%04x",imap_val++);
	sprintf(send,"%s SEARCH UID %d\r\n",tag,uid);
	tcp_write(imap_connection,send,strlen(send));
	tcp_flush(imap_connection);

	while ((line = tcp_readln(imap_connection)))
	{
		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,"OK"))
				success = 1;
			break;
		} else
		{
			/* untagged */
			char first[200];
			char second[200];

			line = imap_get_result(line,first,sizeof(first));
			line = imap_get_result(line,second,sizeof(second));

			msgno = atoi(second);
		}
	}

	if (success && msgno != -1)
	{
		sprintf(send,"STORE %d +FLAGS.SILENT (\\Deleted)\r\n",msgno);
		success = imap_send_simple_command(imap_connection,send);

		if (success)
		{
			success = imap_send_simple_command(imap_connection,"EXPUNGE");
		}
	}

	return success;
}

/*****************************************************************************/

int imap_really_move_mail(struct connection *imap_connection, struct mail_info *mail, struct imap_server *server, struct folder *src_folder, struct folder *dest_folder)
{
	char send[200];
	char tag[20];
	char buf[380];
	char *line;
	int uid;
	int msgno = -1;
	int success;

	sm_snprintf(send,sizeof(send),"SELECT \"%s\"\r\n",src_folder->imap_path);
	if (!imap_send_simple_command(imap_connection,send)) return 0;

	success = 0;
	uid = atoi(mail->filename + 1);

	sprintf(tag,"%04x",imap_val++);
	sprintf(send,"%s SEARCH UID %d\r\n",tag,uid);
	tcp_write(imap_connection,send,strlen(send));
	tcp_flush(imap_connection);

	while ((line = tcp_readln(imap_connection)))
	{
		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,"OK"))
				success = 1;
			break;
		} else
		{
			/* untagged */
			char first[200];
			char second[200];

			line = imap_get_result(line,first,sizeof(first));
			line = imap_get_result(line,second,sizeof(second));

			msgno = atoi(second);
		}
	}

	if (success && msgno != -1)
	{
		success = 0;

		/* Copy the message */
		sm_snprintf(send,sizeof(send),"COPY %d %s\r\n",msgno,dest_folder->imap_path);
		success = imap_send_simple_command(imap_connection,send);

		if (success)
		{
			sprintf(send,"STORE %d +FLAGS.SILENT (\\Deleted)\r\n",msgno);
			success = imap_send_simple_command(imap_connection,send);

			if (success)
			{
				success = imap_send_simple_command(imap_connection,"EXPUNGE");
			}
		}
	}

	return success;
}

/*****************************************************************************/

int imap_really_append_mail(struct connection *imap_connection, struct mail_info *mail, char *source_dir, struct imap_server *server, struct folder *dest_folder)
{
	const int line_buf_size = 2048;
	char send[200];
	char buf[380];
	char *line;
	int success;
	FILE *fh,*tfh;
	char tag[20],path[380];
	char *line_buf;
	int filesize;

	/* At first copy the mail to a temporary location because we may store the emails which only has a \n ending */
	if (!(tfh = tmpfile())) return 0;
	getcwd(path, sizeof(path));
	if (chdir(source_dir) == -1)
	{
		fclose(tfh);
		return 0;
	}

	if (!(fh = fopen(mail->filename,"r")))
	{
		chdir(path);
		fclose(tfh);
		return 0;
	}

	if (!(line_buf = (char *)malloc(line_buf_size)))
	{
		fclose(fh);
		fclose(tfh);
		chdir(path);
		return 0;
	}

	while (fgets(line_buf,line_buf_size - 2,fh))
	{
		int len = strlen(line_buf);
		if (len > 1 && line_buf[len-2] != '\r' && line_buf[len-1] == '\n')
		{
			line_buf[len-1] = '\r';
			line_buf[len] = '\n';
			line_buf[len+1]=0;
		} else if (len == 1 && line_buf[0] == '\n')
		{
			line_buf[0] = '\r';
			line_buf[1] = '\n';
			line_buf[2] = 0;
		}
		fputs(line_buf,tfh);
	}
	fclose(fh);
	chdir(path);

	/* Now upload the mail */
	filesize = ftell(tfh);
	fseek(tfh,0,SEEK_SET);
	sprintf(tag,"%04x",imap_val++);
	sm_snprintf(send,sizeof(send),"%s APPEND %s {%d}\r\n",tag,dest_folder->imap_path,filesize);
	tcp_write(imap_connection,send,strlen(send));
	tcp_flush(imap_connection);

	line = tcp_readln(imap_connection);
	if (line[0] != '+')
	{
		free(line_buf);
		fclose(tfh);
		return 0;
	}

	success = 1;
	while (filesize>0)
	{
		int read = fread(line_buf,1,line_buf_size,tfh);
		if (!read || read == -1)
		{
			success = 0;
			break;
		}
		tcp_write(imap_connection,line_buf,read);
		filesize -= read;
	}
	fclose(tfh);

	if (success)
	{
		tcp_write(imap_connection,"\r\n",2);
		success = 0;
		while ((line = tcp_readln(imap_connection)))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,tag))
			{
				line = imap_get_result(line,buf,sizeof(buf));
				if (!mystricmp(buf,"OK"))
					success = 1;
				break;
			}
		}
	}
	return success;
}

/*****************************************************************************/

int imap_really_download_mail(struct connection *imap_connection, char *local_path, struct mail_info *m, void (*callback)(struct mail_info *m, void *userdata), void *userdata)
{
	char send[200];
	char tag[20];
	char buf[380];
	char *line;
	int uid;
	int success;

	SM_ENTER;

	uid = atoi(m->filename + 1);

	sprintf(tag,"%04x",imap_val++);
	sprintf(send,"%s UID FETCH %d RFC822\r\n",tag,uid);
	tcp_write(imap_connection,send,strlen(send));
	tcp_flush(imap_connection);

	SM_DEBUGF(15,("Sending %s",send));
	success = 0;
	while ((line = tcp_readln(imap_connection)))
	{
		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,"OK"))
				success = 1;
			break;
		} else
		{
			/* untagged */
			if (buf[0] == '*')
			{
				char msgno_buf[200];
				char *temp_ptr;
				int todownload;
				line++;

				line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
				/*msgno = atoi(msgno_buf);*/ /* ignored */

				/* skip the fetch command */
				line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
				if ((temp_ptr = strchr(line,'{'))) /* } - avoid bracket checking problems */
				{
					temp_ptr++;
					todownload = atoi(temp_ptr);
				} else todownload = 0;

				if (todownload)
				{
					FILE *fh;

					mystrlcpy(buf,local_path,sizeof(buf));
					sm_add_part(buf,m->filename,sizeof(buf));

					if ((fh = fopen(buf,"w")))
					{
						while (todownload)
						{
							int dl;
							dl = tcp_read(imap_connection,buf,MIN((sizeof(buf)-4),todownload));

							if (dl == -1 || !dl) break;
							fwrite(buf,1,dl,fh);
							todownload -= dl;
						}
						fclose(fh);
					}
				}
			}
		}
	}

	if (callback)
	{
		callback(m, userdata);
	}

	SM_RETURN(success,"%ld");
	return success;
}

/*****************************************************************************/

struct imap_server *imap_malloc(void)
{
	struct imap_server *imap;
	if ((imap = (struct imap_server*)malloc(sizeof(*imap))))
	{
		memset(imap,0,sizeof(struct imap_server));
		imap->port = 143;
	}
	return imap;
}

/*****************************************************************************/

struct imap_server *imap_duplicate(struct imap_server *imap)
{
	struct imap_server *new_imap = imap_malloc();
	if (new_imap)
	{
		new_imap->name = mystrdup(imap->name);
		new_imap->fingerprint = mystrdup(imap->fingerprint);
		new_imap->login = mystrdup(imap->login);
		new_imap->passwd = mystrdup(imap->passwd);
		new_imap->title = mystrdup(imap->title);
		new_imap->port = imap->port;
		new_imap->active = imap->active;
		new_imap->ssl = imap->ssl;
		new_imap->starttls = imap->starttls;
		new_imap->ask = imap->ask;
	}
	return new_imap;
}

/*****************************************************************************/

void imap_free(struct imap_server *imap)
{
	if (imap->name) free(imap->name);
	if (imap->fingerprint) free(imap->fingerprint);
	if (imap->login) free(imap->login);
	if (imap->passwd) free(imap->passwd);
	if (imap->title) free(imap->title);
	free(imap);
}

/*****************************************************************************/

int imap_new_connection_needed(struct imap_server *srv1, struct imap_server *srv2)
{
	if (!srv1 && !srv2) return 0;
	if (!srv1) return 1;
	if (!srv2) return 1;

	return mystrcmp(srv1->name,srv2->name) || (srv1->port != srv2->port) || mystrcmp(srv1->login,srv2->login) ||
		(mystrcmp(srv1->passwd,srv2->passwd) && !srv1->ask && !srv2->ask)  || (srv1->ask != srv2->ask) ||
		(srv1->ssl != srv2->ssl) || (srv1->starttls != srv2->starttls);
}

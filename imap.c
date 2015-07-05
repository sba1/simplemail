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
 * The communications happens in a separate thread of name #IMAP_THREAD_NAME.
 *
 * @file imap.c
 */

#include "imap.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "account.h"
#include "codesets.h"
#include "debug.h"
#include "folder.h"
#include "mail.h"
#include "progmon.h"
#include "qsort.h"
#include "simplemail.h"
#include "smintl.h"
#include "status.h"
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
 * The name of the IMAP thread.
 */
#define IMAP_THREAD_NAME "SimpleMail - IMAP thread"

/**
 * Maximum number of mails that can be hold before a refresh is invoked
 */
#define MAX_MAILS_PER_REFRESH 100

/* TODO: Get rid of this one */
static int val;

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

struct local_mail
{
	unsigned int uid;
	unsigned int todel;
};

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
	int num_of_mails, num_of_todel_mails;
	void *handle = NULL;
	int i,success = 0;

	SM_ENTER;

	folder_lock(folder);
	folder_next_mail(folder,&handle);

	num_of_mails = folder->num_mails;
	num_of_todel_mails = 0;

	if ((local_mail_array = (struct local_mail *)malloc(sizeof(*local_mail_array) * num_of_mails)))
	{
		/* fill in the uids of the mails */
		for (i=0;i < num_of_mails;i++)
		{
			if (folder->mail_info_array[i] && folder->mail_info_array[i]->filename[0] == 'u')
			{
				local_mail_array[i].uid = atoi(folder->mail_info_array[i]->filename + 1);
				local_mail_array[i].todel = mail_is_marked_as_deleted(folder->mail_info_array[i]);
				num_of_todel_mails += !!local_mail_array[i].todel;
			} else
			{
				local_mail_array[i].uid = 0;
				local_mail_array[i].todel = 0;
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
	folder_unlock(folder);

	SM_DEBUGF(20, ("num_of_mails=%d, num_of_todel_mails=%d\n", num_of_mails, num_of_todel_mails));
	SM_RETURN(success,"%d");
	return success;
}

/**
 * Delete local mails that are not listed in the remote mail array (aka orphaned messages).
 *
 * @param local_mail_array
 * @param num_of_local_mails length of local_mail_array
 * @param remote_mail_array
 * @param num_remote_mails length of remote_mail_array
 * @param imap_server
 * @param imap_folder
 */
static void imap_delete_orphan_messages(struct local_mail *local_mail_array, int num_of_local_mails, struct remote_mail *remote_mail_array, int num_remote_mails, struct imap_server *imap_server, char *imap_folder)
{
	int i,j;

	SM_ENTER;
	SM_DEBUGF(20,("num_of_local_mails=%d, num_remote_mails=%d\n", num_of_local_mails, num_remote_mails));

	i = j = 0;
	while (i<num_of_local_mails && j<num_remote_mails)
	{
		unsigned int local_uid = local_mail_array[i].uid;
		unsigned int remote_uid = remote_mail_array[j].uid;

		if (local_uid < remote_uid)
		{
			if (local_uid) thread_call_parent_function_sync(NULL,callback_delete_mail_by_uid,4,imap_server->login,imap_server->name,imap_folder,local_uid);
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
		if (local_uid) thread_call_parent_function_sync(NULL,callback_delete_mail_by_uid,4,imap_server->login,imap_server->name,imap_folder,local_uid);
	}

	SM_LEAVE;
}



/**
 * Frees the given name list
 * @param list
 */
static void imap_free_name_list(struct string_list *list)
{
	if (!list) return;
	string_list_clear(list);
	free(list);
}

/**
 * Writes the next word into the dest buffer but not more than dest_size.
 *
 * @param src
 * @param dest
 * @param dest_size
 * @return
 */
static char *imap_get_result(char *src, char *dest, int dest_size)
{
	char c;
	char delim = 0;

  dest_size--;

	dest[0] = 0;
	if (!src) return NULL;

	while ((c = *src))
	{
		if (!isspace((unsigned char)c))
			break;
		src++;
	}

	if (c)
	{
		int i = 0;
		int b = 0;
		int incr_delim = 0;

		if (c == '(') { incr_delim = c; delim = ')';}
		else if (c== '"') delim = '"';
		else if (c== '[') delim = ']';

		if (delim)
		{
			src++;
			b++;
		}

		while ((c = *src))
		{
			if (c == incr_delim)
			{
				b++;
			} else
			if (c == delim && !(--b))
			{
				src++;
				break;
			}

			if (!delim)
			{
				if (isspace((unsigned char)c)) break;
			}

			if (i<dest_size)
				dest[i++] = c;
			src++;
		}
		dest[i] = 0;
		return src;
	}
	return NULL;
}

/**
 * Send a simple imap command only to check for success/failure.
 *
 * @param conn
 * @param cmd
 * @return
 */
static int imap_send_simple_command(struct connection *conn, const char *cmd)
{
	char send[200];
	char tag[20];
	char buf[380];
	char *line;
	int success;

	/* Now really remove the message */
	sprintf(tag,"%04x",val++);
	sm_snprintf(send,sizeof(send),"%s %s\r\n",tag,cmd);
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
	return success;
}

/**
 * Waits for an OK after an connect, i.e., until login credentials are requested.
 *
 * @param conn
 * @param server
 * @return
 */
static int imap_wait_login(struct connection *conn, struct imap_server *server)
{
	char *line;
	char buf[100];
	int ok = 0;

	if ((line = tcp_readln(conn)))
	{
		SM_DEBUGF(20,("recv: %s",line));

		line = imap_get_result(line,buf,sizeof(buf));
		line = imap_get_result(line,buf,sizeof(buf));
		if (mystricmp(buf,"OK"))
			goto bailout;
	}

	/* If starttls option is active, perform the starttls kick off */
	if (server->starttls)
	{
		if (!imap_send_simple_command(conn, "STARTTLS"))
		{
			SM_DEBUGF(10,("STARTTLS command failure\n"));
			goto bailout;
		}

		if (!tcp_make_secure(conn, server->name, server->fingerprint))
		{
			SM_DEBUGF(10,("Connection couldn't be made secure\n",buf));
			tell_from_subtask("Connection couldn't be made secure");
			goto bailout;
		}

		ok = 1;
		SM_DEBUGF(20,("STARTTLS success\n"));
	} else
	{
		ok = 1;
	}
bailout:
	return ok;
}

/**
 * Perform a login for the given connection to the given imap server.
 *
 * @param conn
 * @param server
 * @return
 */
static int imap_login(struct connection *conn, struct imap_server *server)
{
	char *line;
	char tag[16];
	char send[200];
	char buf[100];

	sprintf(tag,"%04x",val++);

	/* Logging */
	sm_snprintf(send,sizeof(send),"%s LOGIN %s %s", tag, server->login, "XXX");
	SM_DEBUGF(20,("send: %s\n",send));

	/* Build the IMAP command */
	if (has_spaces(server->passwd))
		sm_snprintf(buf,sizeof(buf),"\"%s\"", server->passwd);
	else
		mystrlcpy(buf,server->passwd,sizeof(buf));

	sm_snprintf(send,sizeof(send),"%s LOGIN %s %s\r\n", tag, server->login, buf);
	tcp_write(conn,send,strlen(send));
	tcp_flush(conn);

	while ((line = tcp_readln(conn)))
	{
		SM_DEBUGF(20,("recv: %s",line));

		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,"OK"))
			{
				return 1;
			}
			break;
		}
	}
	return 0;
}

/** Describes a remote mailbox and its contents */
struct remote_mailbox
{
	struct remote_mail *remote_mail_array; /* may be NULL if remote_mail_num == 0 */
	int num_of_remote_mail;
	unsigned int uid_validity;
	unsigned int uid_next;
};

/**
 * Frees memory associated with the remote mailbox including the remote mailbox itself.
 * @param rm
 */
static void imap_free_remote_mailbox(struct remote_mailbox *rm)
{
	int i;

	if (!rm) return;

	if (rm->remote_mail_array)
	{
		for (i=0; i < rm->num_of_remote_mail; i++)
			free(rm->remote_mail_array[i].headers);
	}
	free(rm->remote_mail_array);
	free(rm);
}

/**
 * Selects the given mailbox (as identified by path).
 *
 * @param conn defines the connection
 * @param path defines the utf8 encoded path
 * @param writemode whether mailbox should be selected in mailbox.
 * @return a rmemote_mailbox. Field remote_mail_array will be NULL.
 */
static struct remote_mailbox *imap_select_mailbox(struct connection *conn, char *path, int writemode)
{
	struct remote_mailbox *rm;

	char status_buf[200];
	char send[200];
	char buf[512];
	char tag[20];
	char *line;
	int success = 0;

	unsigned int uid_validity = 0; /* Note that valid uids are non-zero */
	unsigned int uid_next = 0;
	int num_of_remote_mails = 0;

	if (!path) return NULL;
	if (!(path = utf8toiutf7(path,strlen(path)))) return NULL;
	if (!(rm = (struct remote_mailbox*)malloc(sizeof(*rm)))) return NULL;
	memset(rm,0,sizeof(*rm));

	sm_snprintf(status_buf,sizeof(status_buf),_("Examining folder %s"),path);
	thread_call_parent_function_sync(NULL,status_set_status,1,status_buf);

	sprintf(tag,"%04x",val++);
	sm_snprintf(send,sizeof(send),"%s %s \"%s\"\r\n",tag,writemode?"SELECT":"EXAMINE",path);
	SM_DEBUGF(10,("Examining folder %s: %s",path,send));
	tcp_write(conn,send,strlen(send));
	tcp_flush(conn);

	while ((line = tcp_readln(conn)))
	{
		line = imap_get_result(line,buf,sizeof(buf));
		SM_DEBUGF(10,("Server: %s",line));
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

			if (!mystricmp("EXISTS",second))
			{
				num_of_remote_mails = atoi(first);
			} else if (!mystricmp("OK",first))
			{
				/* Store first identifier of valid untagged response in first */
				line = imap_get_result(second,first,sizeof(first));
				if (!mystricmp("UIDVALIDITY",first))
				{
					/* [UIDVALIDITY n] */
					imap_get_result(line,first,sizeof(first));
					uid_validity = strtoul(first,NULL,10);
				} else if (!mystricmp("UIDNEXT",first))
				{
					/* [UIDNEXT n] */
					imap_get_result(line,first,sizeof(first));
					uid_next = strtoul(first,NULL,10);
				}
			}
		}
	}

	if (success)
	{
		sm_snprintf(status_buf,sizeof(status_buf),_("Identified %d mails in %s"),num_of_remote_mails,path);
		thread_call_parent_function_async_string(status_set_status,1,status_buf);
		SM_DEBUGF(10,("Identified %d mails in %s (uid_validity=%u, uid_next=%u)\n",num_of_remote_mails,path,uid_validity,uid_next));

		rm->uid_next = uid_next;
		rm->uid_validity = uid_validity;
		rm->num_of_remote_mail = num_of_remote_mails;
	} else
	{
		thread_call_function_async(thread_get_main(),status_set_status,1,N_("Failed examining the folder"));
		SM_DEBUGF(10,("Failed examining the folder\n"));
		free(rm);
		rm = NULL;
	}

	free(path);
	return rm;
}

/**
 * Read information of all mails in the given path. Put
 * this back into an array.
 *
 * @param conn defines the connection
 * @param path defines the utf8 encoded path.
 * @param writemode
 * @param headers specifies whether headers are requested.
 * @param uid_start
 * @param uid_end
 *
 * @return returns information of the mailbox in form of a remote_mailbox object.
 *         NULL on failure (for any reasons). If not NULL, the elements in remote_mail_array
 *         are sorted according to their uids.
 *
 * @note the given path stays in the selected/examine state.
 * @note the returned structure must be free with imap_free_remote_mailbox()
 */
static struct remote_mailbox *imap_get_remote_mails(struct connection *conn, char *path, int writemode, int headers, unsigned int uid_start, unsigned int uid_end)
{
	/* get number of remote mails */
	char *line;
	char tag[20];
	char send[200];
	char buf[2048];
	int num_of_remote_mails = 0;
	int success = 0;
	struct remote_mail *remote_mail_array = NULL;
	struct remote_mailbox *rm;

	SM_ENTER;

	if (!(rm = imap_select_mailbox(conn,path,writemode)))
	{
		SM_RETURN(NULL,"%p");
		return NULL;
	}

	if (!uid_start) uid_end = 0;
	else if (!uid_end) uid_start = 0;

	if ((num_of_remote_mails = rm->num_of_remote_mail))
	{
		if ((remote_mail_array = (struct remote_mail*)malloc(sizeof(struct remote_mail)*num_of_remote_mails)))
		{
			unsigned int max_uid = 0; /* Max UID discovered so far */
			unsigned int fetch_time_ref;
			int needs_to_be_sorted = 0;

			fetch_time_ref = time_reference_ticks();

			memset(remote_mail_array,0,sizeof(struct remote_mail)*num_of_remote_mails);

			sprintf(tag,"%04x",val++);
			sm_snprintf(send,sizeof(send),"%s %sFETCH %d:%d (UID FLAGS RFC822.SIZE%s)\r\n",tag,uid_start?"UID ":"",uid_start?uid_start:1,uid_start?uid_end:num_of_remote_mails,headers?" BODY[HEADER.FIELDS (FROM DATE SUBJECT TO CC)]":"");
			SM_DEBUGF(10,("Fetching remote mail array: %s",send));
			tcp_write(conn,send,strlen(send));
			tcp_flush(conn);

			while ((line = tcp_readln(conn)))
			{
				SM_DEBUGF(10,("Server: %s",line));
				line = imap_get_result(line,buf,sizeof(buf));
				if (!mystricmp(buf,tag))
				{
					line = imap_get_result(line,buf,sizeof(buf));
					if (!mystricmp(buf,"OK")) success = 1;
					break;
				} else
				{
					/* untagged */
					unsigned int msgno;
					int is_mail = 0;
					unsigned int uid = 0;
					unsigned int flags = 0;
					unsigned int size = 0;
					char *headers = NULL;
					char msgno_buf[100];
					char stuff_buf[1024];
					char cmd_buf[1024];
					char *temp;
					int i;

					line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
					line = imap_get_result(line,cmd_buf,sizeof(cmd_buf));
					imap_get_result(line,stuff_buf,sizeof(stuff_buf)); /* don't update the line because BODY[HEADER.FIELDS] would be skipped and because it is parsed diffently */

					msgno = (unsigned int)atoi(msgno_buf);
					temp = stuff_buf;

					for (i=0;i<4;i++)
					{
						temp = imap_get_result(temp,cmd_buf,sizeof(cmd_buf));
						if (!mystricmp(cmd_buf,"UID"))
						{
							temp = imap_get_result(temp,cmd_buf,sizeof(cmd_buf));
							uid = atoi(cmd_buf);
							is_mail = 1;
						}
						else if (!mystricmp(cmd_buf,"FLAGS"))
						{
							temp = imap_get_result(temp,cmd_buf,sizeof(cmd_buf));
							if (strstr(cmd_buf,"\\Seen")) flags |= RM_FLAG_SEEN;
							if (strstr(cmd_buf,"\\Answered")) flags |= RM_FLAG_ANSWERED;
							if (strstr(cmd_buf,"\\Flagged")) flags |= RM_FLAG_FLAGGED;
						}
						else if (!mystricmp(cmd_buf,"RFC822.SIZE"))
						{
							temp = imap_get_result(temp,cmd_buf,sizeof(cmd_buf));
							size = atoi(cmd_buf);
						}
						else if (!mystrnicmp(cmd_buf,"BODY",4))
						{
							char *temp_ptr;
							int todownload;

							if ((temp_ptr = strchr(line,'{'))) /* } - avoid bracket checking problems */
							{
								temp_ptr++;
								todownload = atoi(temp_ptr);
							} else todownload = 0;

							if (todownload)
							{
								int pos = 0;

								if ((headers = (char*)malloc(todownload+1)))
								{
									headers[todownload]=0;

									while (todownload)
									{
										int dl;
										dl = tcp_read(conn,headers + pos,todownload);
										if (dl == -1 || !dl) break;
										todownload -= dl;
										pos += dl;
									}
								}
							}
						}
					}

					if (msgno <= num_of_remote_mails && msgno > 0 && is_mail)
					{
						remote_mail_array[msgno-1].uid = uid;
						remote_mail_array[msgno-1].flags = flags;
						remote_mail_array[msgno-1].size = size;
						remote_mail_array[msgno-1].headers = headers;

						if (uid < max_uid) needs_to_be_sorted = 1;
						else max_uid = uid;
					} else
					{
						free(headers);
					}
				}
			}

			if (needs_to_be_sorted)
			{
				SM_DEBUGF(10,("Sorting remote array\n"));

				#define remote_mail_lt(a,b) ((a->uid < b->uid)?1:0)
				QSORT(struct remote_mail, remote_mail_array, num_of_remote_mails, remote_mail_lt);
			}

			SM_DEBUGF(10,("Remote mail array fetched after %d ms\n",time_ms_passed(fetch_time_ref)));
		}
	}
	if (!success)
	{
		free(remote_mail_array);
		free(rm);
		rm = NULL;
	} else
	{
		rm->remote_mail_array = remote_mail_array;
		rm->num_of_remote_mail = num_of_remote_mails;
	}
	SM_RETURN(rm, "%p");
	return rm;
}

/**
 * Returns a list with string_node nodes which describes the folder names.
 * If you only want the subscribed folders set all to 0. Note that the
 * INBOX folder is always included if it does exist.
 *
 * @param conn the connection to write against
 * @param all whether all folders shall be returned or only the subscribed ones.
 * @return the list with string_nodes
 */
static struct string_list *imap_get_folders(struct connection *conn, int all)
{
	int ok;
	char *line;
	char tag[20];
	char send[200];
	char buf[100];

	struct string_list *list = (struct string_list*)malloc(sizeof(struct string_list));
	if (!list) return NULL;
	string_list_init(list);

	ok = 0;

	sprintf(tag,"%04x",val++);
	sprintf(send,"%s %s \"\" *\r\n",tag,all?"LIST":"LSUB");
	tcp_write(conn,send,strlen(send));
	tcp_flush(conn);

	SM_DEBUGF(20,("%s",send));

	while ((line = tcp_readln(conn)))
	{
		SM_DEBUGF(20,("%s\n",line));

		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,"OK")) ok = 1;
			break;
		} else
		{
			char *utf_name;

			/* command */
			line = imap_get_result(line,buf,sizeof(buf));

			/* read flags */
			line = imap_get_result(line,buf,sizeof(buf));

			/* read delim */
			line = imap_get_result(line,buf,sizeof(buf));

			/* read name */
			line = imap_get_result(line,buf,sizeof(buf));

			if ((utf_name = iutf7ntoutf8(buf, strlen(buf))))
			{
				string_list_insert_tail(list,utf_name);
				free(utf_name);
			}
		}
	}

	/* Some IMAP servers don't list the INBOX server on LSUB and don't allow subscribing of it,
   * so we add it manually in case it exists*/
	if (ok && !all && !string_list_find(list,"INBOX"))
	{
		sprintf(tag,"%04x",val++);
		sprintf(send,"%s STATUS INBOX (MESSAGES)\r\n",tag);
		tcp_write(conn,send,strlen(send));
		tcp_flush(conn);

		SM_DEBUGF(20,("%s",send));

		while ((line = tcp_readln(conn)))
		{
			SM_DEBUGF(20,("%s\n",line));
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,tag))
			{
				line = imap_get_result(line,buf,sizeof(buf));
				if (!mystricmp(buf,"OK"))
					string_list_insert_tail(list,"INBOX");
				break;
			}
		}
	}

	if (ok) return list;

	imap_free_name_list(list);
	return NULL;
}

/**
 * Synchronize the folder.
 *
 * @param conn
 * @param server
 * @param imap_path
 * @return
 */
static int imap_synchonize_folder(struct connection *conn, struct imap_server *server, char *imap_path)
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
			folder_unlock(folder);
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

			if (num_of_todel_local_mails)
			{
				if ((rm = imap_get_remote_mails(conn, folder->imap_path, 1, 0, 0, 0)))
				{
					struct remote_mail *remote_mail_array;
					int num_of_remote_mails;
					int i,j;

					remote_mail_array = rm->remote_mail_array;
					num_of_remote_mails = rm->num_of_remote_mail;

					sm_snprintf(status_buf,sizeof(status_buf),_("Removing %d mails from server"),num_of_todel_local_mails);
					thread_call_parent_function_async_string(status_set_status, 1, status_buf);

					for (i = 0 ; i < num_of_local_mails; i++)
					{
						if (mail_is_marked_as_deleted(folder->mail_info_array[i]))
						{
							for (j = 0; j < num_of_remote_mails; j++)
							{
								if (local_mail_array[i].uid == remote_mail_array[j].uid)
								{
									sprintf(tag,"%04x",val++);
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
						sprintf(tag,"%04x",val++);
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

			/* Get information of all mails within the folder */
			if ((rm = imap_get_remote_mails(conn, folder->imap_path, 0, 0, 0, 0)))
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

				imap_delete_orphan_messages(local_mail_array,num_of_local_mails,remote_mail_array,num_of_remote_mails, server, imap_path);

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
				thread_call_function_async(thread_get_main(),status_init_gauge_as_bytes,1,max_todl_bytes);

				for (i=0;i<num_msgs_to_dl;i++)
				{
					accu_todl_bytes += remote_mail_array[i].size;

					sm_snprintf(status_buf,sizeof(status_buf),_("Fetching mail %d from server"),remote_mail_array[i].flags + 1);
					thread_call_parent_function_async_string(status_set_status,1,status_buf);

					sprintf(tag,"%04x",val++);
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
							/* untagged */
							if (buf[0] == '*')
							{
								char msgno_buf[200];
								char *temp_ptr;
								int todownload;
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
									char filename_buf[60];
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
											thread_call_function_async(thread_get_main(),status_set_gauge, 1, todl_bytes);
											fwrite(buf,1,dl,fh);
											todownload -= dl;
										}
										fclose(fh);
										thread_call_parent_function_sync(NULL,callback_new_imap_mail_arrived, 4, filename_buf, server->login, server->name, imap_path);
									}
								}
							}
						}
					}

					if (!success) break;
					todl_bytes = accu_todl_bytes;
					/* TODO: should be enforced */
					thread_call_function_async(thread_get_main(),status_set_gauge, 1, todl_bytes);
				}

				imap_free_remote_mailbox(rm);
			}
		}

		chdir(path);

		free(local_mail_array);
	} else folders_unlock();
	return success;
}

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
	struct connect_options conn_opts = {0};
	char head_buf[100];
	int error_code;

	SM_DEBUGF(10,("Synchronizing with server \"%s\"\n",server->name));

	sm_snprintf(head_buf,sizeof(head_buf),_("Synchronizing mails with %s"),server->name);
	callbacks->set_head(head_buf);
	if (server->title)
		callbacks->set_title_utf8(server->title);
	else
		callbacks->set_title(server->name);
	callbacks->set_connect_to_server(server->name);

	/* Ask for the login/password */
	if (server->ask)
	{
		char *password = (char*)malloc(512);
		char *login = (char*)malloc(512);

		if (password && login)
		{
			int rc;

			if (server->login) mystrlcpy(login,server->login,512);
			password[0] = 0;

			if ((rc = callbacks->request_login(server->name,login,password,512)))
			{
				server->login = mystrdup(login);
				server->passwd = mystrdup(password);
			}
		}

		free(password);
		free(login);
	}

	conn_opts.use_ssl = server->ssl && !server->starttls;
	conn_opts.fingerprint = server->fingerprint;

	SM_DEBUGF(10,("Connecting\n"));
	if ((conn = tcp_connect(server->name, server->port, &conn_opts, &error_code)))
	{
		callbacks->set_status_static(_("Waiting for login..."));
		SM_DEBUGF(10,("Waiting for login\n"));
		if (imap_wait_login(conn,server))
		{
			callbacks->set_status_static(_("Login..."));
			SM_DEBUGF(10,("Login\n"));
			if (imap_login(conn,server))
			{
				struct string_list *folder_list;
				callbacks->set_status_static(_("Login successful"));
				callbacks->set_status_static(_("Checking for folders"));

				SM_DEBUGF(10,("Get folders\n"));
				if ((folder_list = imap_get_folders(conn,0)))
				{
					struct string_node *node;

					/* add the folders */
					node = string_list_first(folder_list);
					while (node)
					{
						callbacks->add_imap_folder(server->login,server->name,node->string);
						node = (struct string_node*)node_next(&node->node);
					}
					callbacks->refresh_folders();

					/* sync the folders */
					node = string_list_first(folder_list);
					while (node)
					{
						if (!(imap_synchonize_folder(conn, server, node->string)))
							break;
						node = (struct string_node*)node_next(&node->node);
					}

					imap_free_name_list(folder_list);
				}
			} else
			{
				callbacks->set_status_static(_("Login failed!"));
				tell_from_subtask(N_("Authentication failed!"));
			}
		}
		tcp_disconnect(conn);

		if (thread_aborted()) return 0;
	} else
	{
		SM_DEBUGF(10,("Unable to connect\n"));
		if (thread_aborted()) return 0;
		else tell_from_subtask((char*)tcp_strerror(error_code));
	}

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

/**
 * Retrieve the folder list and call the given callback on the context of the
 * main thread.
 *
 * @param server
 * @param callback
 */
static void imap_get_folder_list_really(struct imap_server *server, void (*callback)(struct imap_server *server, struct string_list *, struct string_list *))
{
	struct connection *conn = NULL;
	struct connect_options conn_opts = {0};
	struct string_list *all_folder_list = NULL;
	struct string_list *sub_folder_list = NULL;
	char head_buf[100];
	int error_code;

	if (!open_socket_lib())
		return;

	sm_snprintf(head_buf, sizeof(head_buf), _("Reading folders of %s"),server->name);
	thread_call_parent_function_async_string(status_set_head, 1, head_buf);
	if (server->title)
		thread_call_parent_function_async_string(status_set_title_utf8, 1, server->title);
	else
		thread_call_parent_function_async_string(status_set_title, 1, server->name);
	thread_call_parent_function_async_string(status_set_connect_to_server, 1, server->name);

	conn_opts.use_ssl = server->ssl;
	conn_opts.fingerprint = server->fingerprint;

	if (!(conn = tcp_connect(server->name, server->port, &conn_opts, &error_code)))
		goto bailout;

	thread_call_function_async(thread_get_main(),status_set_status,1,_("Waiting for login..."));
	if (!imap_wait_login(conn,server))
		goto bailout;

	thread_call_function_async(thread_get_main(),status_set_status,1,_("Login..."));
	if (!imap_login(conn,server))
	{
		thread_call_function_async(thread_get_main(),status_set_status,1,_("Login failed!"));
		tell_from_subtask(N_("Authentication failed!"));
		goto bailout;
	}

	thread_call_function_async(thread_get_main(),status_set_status,1,_("Reading folders..."));
	if (!(all_folder_list = imap_get_folders(conn,1)))
		goto bailout;

	if (!(sub_folder_list = imap_get_folders(conn,0)))
		goto bailout;

	thread_call_parent_function_sync(NULL,callback,3,server,all_folder_list,sub_folder_list);
bailout:
	if (sub_folder_list) imap_free_name_list(sub_folder_list);
	if (all_folder_list) imap_free_name_list(all_folder_list);
	if (conn)  tcp_disconnect(conn);
	close_socket_lib();
}

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

/*****************************************************************************/

/**
 * Submit the given list of string_nodes to the imap server in order to subscribe them.
 *
 * @param server
 * @param list
 */
static void imap_submit_folder_list_really(struct imap_server *server, struct string_list *list)
{
	struct connection *conn;
	struct connect_options conn_opts = {0};
	char head_buf[100];
	int error_code;
	struct string_list *all_folder_list = NULL;
	struct string_list *sub_folder_list = NULL;

	char *line;
	char tag[20];
	char send[200];
	char buf[100];
	struct string_node *node;

	if (!open_socket_lib())
		return;

	sm_snprintf(head_buf,sizeof(head_buf),_("Submitting subscribed folders to %s"),server->name);
	thread_call_parent_function_async_string(status_set_head, 1, head_buf);
	if (server->title)
		thread_call_parent_function_async_string(status_set_title_utf8, 1, server->title);
	else
		thread_call_parent_function_async_string(status_set_title, 1, server->name);
	thread_call_parent_function_async_string(status_set_connect_to_server, 1, server->name);

	conn_opts.use_ssl = server->ssl;
	conn_opts.fingerprint = server->fingerprint;

	if (!(conn = tcp_connect(server->name, server->port, &conn_opts, &error_code)))
		goto out;

	thread_call_function_async(thread_get_main(),status_set_status,1,_("Waiting for login..."));
	if (!imap_wait_login(conn,server))
		goto out;

	thread_call_function_async(thread_get_main(),status_set_status,1,_("Login..."));
	if (!imap_login(conn,server))
		goto out;

	thread_call_function_async(thread_get_main(),status_set_status,1,_("Reading folders..."));
	if (!(all_folder_list = imap_get_folders(conn,1)))
		goto out;

	thread_call_function_async(thread_get_main(),status_set_status,1,_("Reading subscribed folders..."));
	if (!(sub_folder_list = imap_get_folders(conn,0)))
		goto out;

	node = string_list_first(list);
	while (node)
	{
		if (!string_list_find(sub_folder_list,node->string))
		{
			char *path = utf8toiutf7(node->string,strlen(node->string));
			if (path)
			{
				/* subscribe this folder */
				sprintf(tag,"%04x",val++);
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
		node = (struct string_node*)node_next(&node->node);
	}

	node = string_list_first(sub_folder_list);
	while (node)
	{
		if (!string_list_find(list,node->string))
		{
			char *path = utf8toiutf7(node->string,strlen(node->string));
			if (path)
			{
				/* subscribe this folder */
				sprintf(tag,"%04x",val++);
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
		node = (struct string_node*)node_next(&node->node);
	}
out:
	imap_free_name_list(sub_folder_list);
	imap_free_name_list(all_folder_list);
	tcp_disconnect(conn);
	close_socket_lib();
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

/**
 * Connect and login to the given imap server.
 *
 * @param connection
 * @param imap_server
 * @return
 */
static int imap_really_connect_and_login_to_server(struct connection **connection, struct imap_server *imap_server)
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

			if (thread_call_parent_function_sync(NULL,sm_request_login,4,imap_server->name,login,password,512))
			{
				imap_server->login = mystrdup(login);
				imap_server->passwd = mystrdup(password);
			} else
			{
				free(password);
				free(login);
				return 0;
			}
		}

		free(password);
		free(login);
	}

	/* Display "Connecting" - status message */
	sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Connecting..."));
	thread_call_parent_function_async_string(status_set_status,1,status_buf);

	conn_opts.use_ssl = imap_server->ssl;
	conn_opts.fingerprint = imap_server->fingerprint;

	if ((imap_connection = tcp_connect(imap_server->name, imap_server->port, &conn_opts, &error_code)))
	{
		SM_DEBUGF(20,("Connected to %s\n",imap_server->name));

		if (imap_wait_login(imap_connection,imap_server))
		{
			/* Display "Logging in" - status message */
			sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Logging in..."));
			thread_call_parent_function_async_string(status_set_status,1,status_buf);

			if (imap_login(imap_connection,imap_server))
			{
				/* Display "Connected" - status message */
				sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Connected"));
				thread_call_parent_function_async_string(status_set_status,1,status_buf);

				success = 1;
			} else
			{
				SM_DEBUGF(10,("Login failed\n"));
				sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Log in failed. Check user name and password for this account."));
				thread_call_parent_function_async_string(status_set_status,1,status_buf);
				tcp_disconnect(imap_connection);
				imap_connection = NULL;
				tell_from_subtask(N_("Authentication failed!"));
			}
		} else
		{
			sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Unexpected server answer. Connection failed."));
			thread_call_parent_function_async_string(status_set_status,1,status_buf);
			tcp_disconnect(imap_connection);
			imap_connection = NULL;
		}
	} else
	{
		/* Display "Failed to connect" - status message */
		sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Connecting to the server failed"));
		thread_call_parent_function_async_string(status_set_status,1,status_buf);
	}

bailout:
	*connection = imap_connection;
	SM_RETURN(success,"%ld");
	return success;
}

/**
 * Function to download mails.
 *
 * @return number of downloaded mails. A value < 0 indicates an error.
 */
static int imap_really_download_mails(struct connection *imap_connection, char *imap_local_path, struct imap_server *imap_server, char *imap_folder)
{
	char path[380];
	struct folder *local_folder;
	struct remote_mailbox *rm;

	int do_download = 1;
	int downloaded_mails = 0;
	int dont_use_uids = 0;

	unsigned int local_uid_validiy = 0;
	unsigned int local_uid_next = 0;

	unsigned int uid_from = 0;
	unsigned int uid_to = 0;

	struct progmon *pm;

	SM_ENTER;

	if (!imap_connection)
	{
		SM_RETURN(-1,"%d");
		return -1;
	}

	getcwd(path, sizeof(path));
	if (chdir(imap_local_path) == -1)
	{
		SM_RETURN(-1,"%d");
		return -1;
	}

	SM_DEBUGF(10,("Downloading mails of folder \"%s\"\n",imap_folder));

	folders_lock();
	if ((local_folder = folder_find_by_imap(imap_server->login, imap_server->name, imap_folder)))
	{
		local_uid_validiy = local_folder->imap_uid_validity;
		local_uid_next = local_folder->imap_uid_next;
		dont_use_uids = local_folder->imap_dont_use_uids;
	}
	folders_unlock();

	/* Uids are valid only if they are non-zero */
	if (local_uid_validiy != 0 && local_uid_next != 0 && !dont_use_uids)
	{
		if ((rm = imap_select_mailbox(imap_connection, imap_folder, 0)))
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
				utf8 msg[80];

				folders_unlock();

				pm = progmon_create();
				if (pm)
				{
					utf8fromstr(_("Downloading mails"),NULL,msg,sizeof(msg));
					pm->begin(pm,1001,msg);

					utf8fromstr(_("Determining which mails to download"),NULL,msg,sizeof(msg));
					pm->working_on(pm,msg);
				}

				if ((rm = imap_get_remote_mails(imap_connection, imap_folder, 0, 1, uid_from, uid_to)))
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
									thread_call_parent_function_sync(NULL, callback_new_imap_mails_arrived, 5, filename_current, filename_ptrs, imap_server->login, imap_server->name, imap_folder);
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
						thread_call_parent_function_sync(NULL, callback_new_imap_mails_arrived, 5, filename_current, filename_ptrs, imap_server->login, imap_server->name, imap_folder);
						while (filename_current)
							free(filename_ptrs[--filename_current]);
					}

					SM_DEBUGF(10,("%d mails downloaded after %d ms\n",downloaded_mails,time_ms_passed(total_download_ticks)));

					/* Finally, inform controller about new uids */
					thread_call_parent_function_sync(NULL, callback_new_imap_uids, 5, rm->uid_validity, rm->uid_next, imap_server->login, imap_server->name, imap_folder);

					if (uid_from && uid_to)
					{
						imap_free_remote_mailbox(rm);
						/* Rescan folder in order to delete orphaned messages */
						rm = imap_get_remote_mails(imap_connection, imap_folder, 0, 0, 0, 0);
					}

					if (rm)
					{
						/* Now delete orphaned messages */
						imap_delete_orphan_messages(local_mail_array,num_of_local_mails,rm->remote_mail_array,rm->num_of_remote_mail, imap_server, imap_folder);
						imap_free_remote_mailbox(rm);
					}
				}

				if (pm)
				{
					pm->done(pm);
					progmon_delete(pm);
				}
				free(local_mail_array);
			} else folders_unlock();
		} else folders_unlock();
	}
	chdir(path);

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

		thread_call_parent_function_async_string(status_set_status,1,path);
	}

	SM_RETURN(downloaded_mails,"%d");
	return downloaded_mails;
}

/*****************************************************************************/

/**
 * Establishes a connection to the server and downloads mails.
 */
static void imap_really_connect_to_server(struct connection **imap_connection, char *imap_local_path, struct imap_server *imap_server, char *imap_folder)
{
	SM_ENTER;

	if (imap_server)
	{
		struct progmon *pm;

		pm = progmon_create();
		if (pm)
		{
			utf8 msg[80];

			utf8fromstr(_("Connect with IMAP server"),NULL,msg,sizeof(msg));
			pm->begin(pm,2,msg);

			utf8fromstr("Logging in",NULL,msg,sizeof(msg));
			pm->working_on(pm,msg);
		}

		if (imap_really_connect_and_login_to_server(imap_connection, imap_server))
		{
			char status_buf[160];
			struct string_list *folder_list;

			/* Display "Retrieving mail folders" - status message */
			sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Retrieving mail folders..."));
			thread_call_parent_function_async_string(status_set_status,1,status_buf);

			/* We have now connected to the server, check for the folders at first */
			folder_list = imap_get_folders(*imap_connection, 0);
			if (folder_list)
			{
				struct string_node *node;

				/* add the folders */
				node = string_list_first(folder_list);
				while (node)
				{
					thread_call_parent_function_sync(NULL,callback_add_imap_folder,3,imap_server->login,imap_server->name,node->string);
					node = (struct string_node*)node_next(&node->node);
				}
				thread_call_parent_function_sync(NULL,callback_refresh_folders,0);

				imap_free_name_list(folder_list);

				imap_really_download_mails(*imap_connection, imap_local_path, imap_server, imap_folder);
			}
		}

		if (pm)
		{
			pm->done(pm);
			progmon_delete(pm);
		}
	}
	SM_LEAVE;
}

/**
 * Delete a mail permanently from the server
 *
 * @param imap_connection already established imap connection, already logged in.
 * @param filename the (local) filename of the mail to be deleted.
 * @param folder the folder where the mail is located.
 * @return success or not.
 * @note This function can only be called in the context of the imap thread.
 */
static int imap_really_delete_mail_by_filename(struct connection *imap_connection, char *filename, struct folder *folder)
{
	char send[200];
	char tag[20];
	char buf[380];
	char *line;
	int uid;
	int msgno = -1;
	int success;

	sm_snprintf(send,sizeof(send),"SELECT \"%s\"\r\n",folder->imap_path);
	if (!imap_send_simple_command(imap_connection,send)) return 0;

	success = 0;
	uid = atoi(filename + 1);

	sprintf(tag,"%04x",val++);
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
static int imap_really_move_mail(struct connection *imap_connection, struct mail_info *mail, struct imap_server *server, struct folder *src_folder, struct folder *dest_folder)
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

	sprintf(tag,"%04x",val++);
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

/**
 * Checks whether in principle a new connection would be needed when swiching from srv1 to srv2.
 *
 * @param srv1
 * @param srv2
 * @return
 */
static int imap_new_connection_needed(struct imap_server *srv1, struct imap_server *srv2)
{
	if (!srv1 && !srv2) return 0;
	if (!srv1) return 1;
	if (!srv2) return 1;

	return mystrcmp(srv1->name,srv2->name) || (srv1->port != srv2->port) || mystrcmp(srv1->login,srv2->login) ||
		(mystrcmp(srv1->passwd,srv2->passwd) && !srv1->ask && !srv2->ask)  || (srv1->ask != srv2->ask) ||
		(srv1->ssl != srv2->ssl) || (srv1->starttls != srv2->starttls);
}


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
		imap_disconnect();

		if (imap_server) imap_free(imap_server);
		imap_server = server;

		free(imap_folder);
		imap_folder = folder;

		free(imap_local_path);
		imap_local_path = local_path;

		imap_really_connect_to_server(&imap_connection, imap_local_path, imap_server, imap_folder);
		rc = 1;
	} else
	{
		imap_free(server);

		free(imap_folder);
		imap_folder = folder;

		free(imap_local_path);
		imap_local_path = local_path;

		imap_really_download_mails(imap_connection, imap_local_path, imap_server, imap_folder);
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
	char send[200];
	char tag[20];
	char buf[380];
	char *line;
	int uid;
	int success;

	SM_ENTER;

	if (!imap_thread_really_login_to_given_server(server))
	{
		SM_RETURN(0,"%ld");
		return 0;
	}

	uid = atoi(m->filename + 1);

	sprintf(tag,"%04x",val++);
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
		thread_call_function_async(thread_get_main(), callback, 2, m, userdata);

	SM_RETURN(success,"%ld");
	return success;
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
	if (!imap_thread_really_login_to_given_server(server)) return 0;

	return imap_really_delete_mail_by_filename(imap_connection, filename, folder);
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
	char send[200];
	char buf[380];
	char *line;
	int success;
	FILE *fh,*tfh;
	char tag[20],path[380];
	char line_buf[1200];
	int filesize;

	if (!imap_open_socket_lib())
		return 0;

	/* Should be in a separate function */
	if (!imap_connection || imap_new_connection_needed(imap_server,server))
	{
		free(imap_folder); imap_folder = NULL;
		free(imap_local_path);imap_local_path = NULL;

		if (imap_server) imap_free(imap_server);
		if ((imap_server = imap_duplicate(server)))
		{
			imap_disconnect();
			imap_really_connect_and_login_to_server(&imap_connection, imap_server);
		}
	}

	if (!imap_server) return 0;
	if (!imap_connection) return 0;

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

	while (fgets(line_buf,sizeof(line_buf)-2,fh))
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
	sprintf(tag,"%04x",val++);
	sm_snprintf(send,sizeof(send),"%s APPEND %s {%d}\r\n",tag,dest_folder->imap_path,filesize);
	tcp_write(imap_connection,send,strlen(send));
	tcp_flush(imap_connection);

	line = tcp_readln(imap_connection);
	if (line[0] != '+')
	{
		fclose(tfh);
		return 0;
	}

	success = 1;
	while (filesize>0)
	{
		int read = fread(line_buf,1,sizeof(line_buf),tfh);
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

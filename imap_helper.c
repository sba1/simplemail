/**
 * @file imap_helper.c
 */

#include "imap_helper.h"

#include <ctype.h>
#include <stdlib.h>

#include "debug.h"
#include "imap.h"
#include "logging.h"
#include "qsort.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

/******************************************************************************/

int imap_val;

/******************************************************************************/

void imap_reset_command_counter(void)
{
	imap_val = 0;
}

/******************************************************************************/

char *imap_get_result(char *src, char *dest, int dest_size)
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

/******************************************************************************/

int imap_wait_login(struct connection *conn, struct imap_server *server)
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
	} else
	{
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

/******************************************************************************/

int imap_login(struct connection *conn, struct imap_server *server)
{
	char *line;
	char tag[16];
	char send[200];
	char buf[100];

	sprintf(tag,"%04x",imap_val++);

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

/******************************************************************************/

int imap_send_simple_command(struct connection *conn, const char *cmd)
{
	char send[200];
	char tag[20];
	char buf[380];
	char *line;
	int success;

	/* Now really remove the message */
	sprintf(tag,"%04x",imap_val++);
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

/******************************************************************************/

struct remote_folder *imap_get_folders(struct connection *conn, int all, int *num_remote_folders)
{
	int i, ok;
	int has_inbox = 0;
	char *line;
	char tag[20];
	char send[200];
	char buf[100];

	int num_rf = 0;
	struct remote_folder *rf = NULL;
	struct string_list list;
	struct string_node *sn;

	string_list_init(&list);

	ok = 0;

	sprintf(tag,"%04x",imap_val++);
	sprintf(send,"%s %s \"\" *\r\n",tag,all?"LIST":"LSUB");
	tcp_write(conn,send,strlen(send));
	tcp_flush(conn);

	SM_DEBUGF(20,("%s",send));

	/* Capture all lines expect ok */
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
			string_list_insert_tail(&list, line);
			num_rf++;
		}
	}

	if (!ok)
	{
		goto bailout;
	}

	/* Reserve memory for folders plus an additional for INBOX */
	if (!(rf = (struct remote_folder *)malloc(sizeof(*rf) * (num_rf + 1))))
	{
		goto bailout;
	}

	for (sn = string_list_first(&list), i=0; sn; sn = string_node_next(sn), i++)
	{
		unsigned char delim = 0;

		line = sn->string;

		/* command */
		line = imap_get_result(line,buf,sizeof(buf));

		/* read flags */
		line = imap_get_result(line,buf,sizeof(buf));

		/* read delim */
		line = imap_get_result(line,buf,sizeof(buf));
		if (strcmp("NIL", buf) && strlen(buf) == 1)
		{
			delim = buf[0];
		}

		/* read name */
		line = imap_get_result(line,buf,sizeof(buf));

		rf[i].delim = delim;
		if (!(rf[i].name = iutf7ntoutf8(buf, strlen(buf))))
		{
			goto bailout;
		}
		if (!strcmp(rf[i].name, "INBOX"))
		{
			has_inbox = 1;
		}
	}


	/* Some IMAP servers don't list the INBOX server on LSUB and don't allow subscribing of it,
	 * so we add it manually in case it exists*/
	if (!all && !has_inbox)
	{
		sprintf(tag,"%04x",imap_val++);
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
				{
					rf[i++].name = mystrdup("INBOX");
				}
				break;
			}
		}
	}

	*num_remote_folders = i;

bailout:
	string_list_clear(&list);
	return rf;
}

/******************************************************************************/

void imap_folders_free(struct remote_folder *rf, int num_remote_folders)
{
	int i;

	if (!rf)
	{
		return;
	}

	for (i = 0; i< num_remote_folders; i++)
	{
		free(rf[i].name);
	}
	free(rf);
}

/******************************************************************************/

int imap_remote_folder_exists(struct remote_folder *rf, int num_rf, const char *name)
{
	int i;

	for (i = 0; i < num_rf; i++)
	{
		if (!strcmp(rf[i].name, name))
		{
			return 1;
		}
	}
	return 0;
}

/******************************************************************************/

int imap_get_remote_mails_handle_answer(struct connection *conn, char *tag, char *buf, int buf_size, struct remote_mail *remote_mail_array, unsigned int num_of_remote_mails)
{
	char *line;
	int success = 0;
	int needs_to_be_sorted = 0;
	unsigned int max_uid = 0; /* Max UID discovered so far */

	while ((line = tcp_readln(conn)))
	{
		SM_DEBUGF(10,("Server: %s",line));
		line = imap_get_result(line,buf,buf_size);
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,buf_size);
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
			char msgno_buf[40];
			char stuff_buf[768];
			char cmd_buf[768];
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

	return success;
}

/******************************************************************************/

struct remote_mailbox *imap_select_mailbox(struct imap_select_mailbox_args *args)
{
	struct remote_mailbox *rm;

	char status_buf[200];
	char send[200];
	char buf[512];
	char tag[20];
	char *line;
	int success = 0;

	struct connection *conn = args->conn;
	char *path = args->path;
	int writemode = args->writemode;

	unsigned int uid_validity = 0; /* Note that valid uids are non-zero */
	unsigned int uid_next = 0;
	int num_of_remote_mails = 0;

	if (!path) return NULL;
	if (!(path = utf8toiutf7(path,strlen(path)))) return NULL;
	if (!(rm = (struct remote_mailbox*)malloc(sizeof(*rm)))) return NULL;
	memset(rm,0,sizeof(*rm));

	sm_snprintf(status_buf,sizeof(status_buf),_("Examining folder %s"),path);
	args->set_status(status_buf);

	sprintf(tag,"%04x",imap_val++);
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
		args->set_status(status_buf);
		SM_DEBUGF(10,("Identified %d mails in %s (uid_validity=%u, uid_next=%u)\n",num_of_remote_mails,path,uid_validity,uid_next));

		sm_snprintf(status_buf, sizeof(status_buf), "Folder with path \"%s\" has %d remote mails", args->path, num_of_remote_mails);
		logg(INFO, 0, __FILE__, NULL, 0, status_buf, LAST);

		rm->uid_next = uid_next;
		rm->uid_validity = uid_validity;
		rm->num_of_remote_mail = num_of_remote_mails;
	} else
	{
		args->set_status_static(N_("Failed examining the folder"));
		SM_DEBUGF(10,("Failed examining the folder\n"));
		free(rm);
		rm = NULL;
	}

	free(path);
	return rm;
}

/******************************************************************************/

void imap_free_remote_mailbox(struct remote_mailbox *rm)
{
	if (!rm) return;

	if (rm->remote_mail_array)
	{
		unsigned int i;

		for (i=0; i < rm->num_of_remote_mail; i++)
			free(rm->remote_mail_array[i].headers);
	}
	free(rm->remote_mail_array);
	free(rm);
}

/******************************************************************************/

struct remote_mailbox *imap_get_remote_mails(struct imap_get_remote_mails_args *args)
{
	/* get number of remote mails */
	char tag[16];
	char *buf = NULL; /* is used for sending and receiving */
	const int buf_size = 2048;
	int num_of_remote_mails = 0;
	int success = 0;
	struct remote_mail *remote_mail_array = NULL;
	struct remote_mailbox *rm;

	struct connection *conn = args->conn;
	char *path = args->path;
	int writemode = args->writemode;
	int headers = args->headers;
	unsigned int uid_start = args->uid_start;
	unsigned int uid_end = args->uid_end;
	unsigned int fetch_time_ref;

	struct imap_select_mailbox_args select_mailbox_args = {0};

	SM_ENTER;

	select_mailbox_args.conn = conn;
	select_mailbox_args.path = path;
	select_mailbox_args.writemode = writemode;
	select_mailbox_args.set_status = args->set_status;
	select_mailbox_args.set_status_static = args->set_status_static;

	if (!(rm = imap_select_mailbox(&select_mailbox_args)))
	{
		SM_RETURN(NULL,"%p");
		return NULL;
	}

	if (!(num_of_remote_mails = rm->num_of_remote_mail))
	{
		/* Assume success if the folder is empty */
		success = 1;

		goto bailout;
	}

	if (!(buf = (char *)malloc(buf_size)))
		goto bailout;

	if (!uid_start) uid_end = 0;
	else if (!uid_end) uid_start = 0;

	fetch_time_ref = time_reference_ticks();

	if ((remote_mail_array = (struct remote_mail*)malloc(sizeof(struct remote_mail)*num_of_remote_mails)))
	{
		memset(remote_mail_array,0,sizeof(struct remote_mail)*num_of_remote_mails);

		sprintf(tag,"%04x",imap_val++);
		sm_snprintf(buf,buf_size,"%s %sFETCH %d:%d (UID FLAGS RFC822.SIZE%s)\r\n",tag,uid_start?"UID ":"",uid_start?uid_start:1,uid_start?uid_end:num_of_remote_mails,headers?" BODY[HEADER.FIELDS (FROM DATE SUBJECT TO CC)]":"");
		SM_DEBUGF(10,("Fetching remote mail array: %s",buf));
		tcp_write(conn,buf,strlen(buf));
		tcp_flush(conn);

		success = imap_get_remote_mails_handle_answer(conn, tag, buf, buf_size, remote_mail_array, num_of_remote_mails);
	}

	SM_DEBUGF(10,("Remote mail array fetched after %d ms\n",time_ms_passed(fetch_time_ref)));
bailout:
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
	free(buf);
	SM_RETURN(rm, "%p");
	return rm;
}

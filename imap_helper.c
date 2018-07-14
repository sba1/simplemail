/**
 * @file imap_helper.c
 */

#include "imap_helper.h"

#include <ctype.h>
#include <stdlib.h>

#include "debug.h"
#include "imap.h"
#include "qsort.h"
#include "support.h"
#include "support_indep.h"

/******************************************************************************/

int imap_val;

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

int imap_get_remote_mails_handle_answer(struct connection *conn, char *tag, char *buf, int buf_size, struct remote_mail *remote_mail_array, int num_of_remote_mails)
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

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
 * @file pop3.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#ifdef __WIN32__
#include <windows.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#endif

#include "debug.h"
#include "filter.h"
#include "hash.h"
#include "mail.h"
#include "md5.h"
#include "pop3.h"
#include "tcp.h"
#include "simplemail.h"
#include "smintl.h"
#include "spam.h"
#include "status.h"
#include "support_indep.h"

#include "mainwnd.h"
#include "subthreads.h"
#include "support.h"
#include "tcpip.h"

/**************************************************************************
 Recieves a single line answer. Returns the line if this is positive
 (without the +OK) else 0. If silent is 1 simplemail doesn't notify the
 user about an error.
**************************************************************************/
static char *pop3_receive_answer(struct connection *conn, int silent)
{
	char *answer;
	if (!(answer = tcp_readln(conn)))
	{
		/* Don't put any message when only interrupted */
		if (tcp_error_code() != TCP_INTERRUPTED)
		{
			SM_DEBUGF(5,("Error receiving data from host!\n"));
			if (!silent) tell_from_subtask(N_("Error receiving data from host!"));
		}
		return NULL;
	}
	if (!strncmp(answer,"+OK",3)) return answer+3;
	if (!strncmp(answer,"-ERR",4)) if (!silent) tell_from_subtask(answer);
	return NULL;
}

/**************************************************************************
 Wait for the welcome message. If server delivers a timestamp it is placed
 into the timestamp_ptr argument (string must be freed when no longer used).
 timestamp is not touched if this call fails.
**************************************************************************/
static int pop3_wait_login(struct connection *conn, struct pop3_server *server, char **timestamp_ptr)
{
	char *answer;

	if ((answer = pop3_receive_answer(conn,0)))
	{
		char *ptr,*startptr,*endptr,*timestamp = NULL;
		char c;

		SM_DEBUGF(15,("POP3 Server answered: %s",answer));

		/* If server supports APOP it places a timestamp within its welcome message. Extracting it,
     * not, if apop shouldn't be tried (in which case apop is 2) */
		if (server->apop != 2)
		{
			ptr = answer;
			startptr = endptr = NULL;
			while ((c = *ptr))
			{
				if (c=='<') startptr = ptr;
				else if (c=='>')
				{
					endptr = ptr;
					break;
				}
				ptr++;
			}
			if (startptr && endptr && startptr<endptr && timestamp_ptr)
			{
				if ((timestamp = (char*)malloc(endptr-startptr+3)))
				{
					strncpy(timestamp,startptr,endptr-startptr+1);
					timestamp[endptr-startptr+1] = 0;
					SM_DEBUGF(15,("Found timestamp: %s\n",timestamp));
				}
			}
		}

		/* Make the connection secure if requested */
		if (server->ssl && server->stls)
		{
			if (tcp_write(conn,"STLS\r\n",6) <= 0) return 0;

			/* TODO: check if this call delivers a new timestamp */
			if (pop3_receive_answer(conn,0))
			{
				if (tcp_make_secure(conn))
				{
					if (timestamp_ptr) *timestamp_ptr = timestamp;
					return 1;
				}
			}
		} else
		{
			if (timestamp_ptr) *timestamp_ptr = timestamp;
			return 1;
		}
		free(timestamp);
	} else
	{
		SM_DEBUGF(5,("POP3 Server did not answer!\n"));
	}
	return 0;
}

/**************************************************************************
 Log into the pop3 server. timestamp is the thing the server sends
 within its welcome message. timestamp maybe NULL which means that
 no APOP is tried.
**************************************************************************/
static int pop3_login(struct connection *conn, struct pop3_server *server, char *timestamp)
{
	char buf[256];

	if (!timestamp && server->apop == 1)
	{
		tell_from_subtask(_("Failed to authentificate via APOP. Server hasn't delivered a timestamp."));
		return 0;
	}

	if (timestamp && !server->ssl)
	{
		SM_MD5_CTX context;
		unsigned char digest[16];
		char *ptr;
		int i;

		thread_call_parent_function_async(status_set_status,1,_("Authentificate via APOP..."));

		MD5Init(&context);
		MD5Update(&context, timestamp, strlen(timestamp));
		MD5Update(&context, server->passwd,mystrlen(server->passwd));
		MD5Final(digest, &context);

		sm_snprintf(buf,sizeof(buf)-64,"APOP %s ",server->login);
		ptr = buf + strlen(buf);

		for (i=0;i<16;i++)
		{
			sprintf(ptr,"%02x",digest[i]);
			ptr = ptr + strlen(ptr);
		}
		*ptr++ = '\r';
		*ptr++ = '\n';
		*ptr = 0;

		SM_DEBUGF(15,("Sending %s",buf));

		if (tcp_write(conn,buf,strlen(buf)) <= 0) return 0;
		if (!pop3_receive_answer(conn,1))
		{
			if (server->apop == 1)
			{
				tell_from_subtask(_("Failed to authentificate via APOP"));
				return 0;
			}
			SM_DEBUGF(15,("APOP authentification failed\n"));
		} else
		{
			thread_call_parent_function_async(status_set_status,1,_("Login successful!"));
			return 1;
		}
	}

	SM_DEBUGF(15,("Trying plain text method\n"));
	thread_call_parent_function_async(status_set_status,1,_("Sending username..."));

	sm_snprintf(buf, sizeof(buf), "USER %s\r\n",server->login);
	if (tcp_write(conn,buf,strlen(buf)) <= 0) return 0;
	if (!pop3_receive_answer(conn,!!timestamp)) /* be silent if timestamp has given */
	{
		if (tcp_error_code() != TCP_INTERRUPTED && !timestamp)
			tell_from_subtask(N_("Error while identifying the user"));
		SM_DEBUGF(15,("Sending the USER command failed\n"));
		return 0;
	}

	thread_call_parent_function_async(status_set_status,1,_("Sending password..."));
	sm_snprintf(buf,sizeof(buf),"PASS %s\r\n",server->passwd);
	if (tcp_write(conn,buf,strlen(buf)) <= 0) return 0;
	if (!pop3_receive_answer(conn,0))
	{
		if (tcp_error_code() != TCP_INTERRUPTED)
			tell_from_subtask(N_("Error while identifing the user"));
		return 0;
	}

	thread_call_parent_function_async(status_set_status,1,_("Login successful!"));
	return 1;
}

struct dl_mail
{
	unsigned int flags; /**< the download flags of this mail */
	int size;			/**< the mails size */
	char *uidl;			/**< the uidl string, might be NULL */
};

#define MAILF_DELETE     (1<<0) /**< mail should be deleted */
#define MAILF_DOWNLOAD   (1<<1) /**< mail should be downloaded */
#define MAILF_DUPLICATE  (1<<2) /**< mail is duplicated */
#define MAILF_ADDED      (1<<7) /**< mails has been added to the status window */


struct uidl_entry /* stored on the harddisk */
{
	int size; /**< size of the mail, unused yet, so it is -1 for now */
	char uidl[72]; /**< null terminated, 70 are enough according to RFC 1939 */
};

/**
 * @brief the uidl contents.
 *
 * @note TODO: Use a better data structure
 */
struct uidl
{
	char *filename; /**< the filename of the uidl file */
	int num_entries; /**< number of entries */
	struct uidl_entry *entries; /**< these are the entries */
};

/**
 * @brief Initialize a given uidl
 *
 * @param uidl the uidl instance to be initialized
 * @param server the server in question
 * @param folder_directory base directory of the folders
 */
static void uidl_init(struct uidl *uidl, struct pop3_server *server, char *folder_directory)
{
	char c;
	char *server_name = server->name;
	char *buf;
	int len = strlen(folder_directory)+strlen(server_name)+30;

	memset(uidl,0,sizeof(*uidl));

	/* Construct the file name */
	if (!(buf = uidl->filename = malloc(len)))
		return;

	strcpy(uidl->filename,folder_directory);
	sm_add_part(uidl->filename,".uidl.",len);

	/* Using a hash doesn't make the filename unique but it should work for now */
	sprintf(uidl->filename+strlen(uidl->filename),"%x",sdbm(server->login));
	buf += strlen(buf);
	while ((c=*server_name))
	{
		if (c!='.') *buf++=c;
		server_name++;
	}
	*buf = 0;
}

/**************************************************************************
 Opens the uidl if if it exists
**************************************************************************/
static int uidl_open(struct uidl *uidl, struct pop3_server *server)
{
	FILE *fh;
	int rc = 0;

	SM_ENTER;

	if (!uidl->filename) return 0;

	if ((fh = fopen(uidl->filename,"rb")))
	{
		unsigned char id[4];
		int fsize = myfsize(fh);
		int cnt = fread(id,1,4,fh);

		if (cnt == 4 && id[0] == 'S' && id[1] == 'M' && id[2] == 'U' && id[3] == 0 &&
		    ((fsize - 4)%sizeof(struct uidl_entry)) == 0)
		{
			uidl->num_entries = (fsize - 4)/sizeof(struct uidl_entry);
			if ((uidl->entries = malloc(fsize - 4)))
			{
				fread(uidl->entries,1,fsize - 4,fh);
				rc = 1;
			}
		}

		fclose(fh);
	}
	SM_RETURN(rc,"%ld");
}

/**************************************************************************
 Tests if a uidl is inside the uidl file
**************************************************************************/
static int uidl_test(struct uidl *uidl, char *to_check)
{
	int i;
	if (!uidl->entries) return 0;
	for (i=0;i<uidl->num_entries;i++)
	{
		if (!strcmp(to_check,uidl->entries[i].uidl)) return 1;
	}
	return 0;
}

/**************************************************************************
 Remove no longer used uidls in the uidl file (that means uidls which are
 not on the server)
**************************************************************************/
void uidl_remove_unused(struct uidl *uidl, struct dl_mail *mail_array)
{
	SM_ENTER;

	if (uidl->entries)
	{
		int i,amm=mail_array[0].flags;
		for (i=0; i<uidl->num_entries; i++)
		{
			int j,found=0;
			char *uidl_entry = uidl->entries[i].uidl;
			for (j=1;j<=amm;j++)
			{
				if (mail_array[j].uidl)
				{
					if (!strcmp(uidl_entry,mail_array[j].uidl))
					{
						found = 1;
						break;
					}
				}
			}

			if (!found)
				memset(uidl_entry,0,sizeof(uidl->entries[i].uidl));
		}
	}

	SM_LEAVE;
}

/**************************************************************************
 Add the uidl to the uidl file. Writes this into the file. Its directly
 written to the correct place. uidl->entries is not expanded.
**************************************************************************/
void uidl_add(struct uidl *uidl, struct dl_mail *m)
{
	int i=0;
	FILE *fh;

	SM_ENTER;

	if (!m->uidl || m->uidl[0] == 0) return;
	for (i=0;i<uidl->num_entries;i++)
	{
		if (!uidl->entries[i].uidl[0])
		{
			strcpy(uidl->entries[i].uidl,m->uidl);
			if ((fh = fopen(uidl->filename,"rb+")))
			{
				fseek(fh,4+i*sizeof(struct uidl_entry),SEEK_SET);
				uidl->entries[i].size = -1;
				fwrite(&uidl->entries[i],1,sizeof(uidl->entries[i]),fh);
				fclose(fh);
				SM_LEAVE;
				return;
			}
		}
	}

	SM_DEBUGF(15,("Appending to %s\n",uidl->filename));

	if ((fh = fopen(uidl->filename,"ab")))
	{
		struct uidl_entry entry;
		if (ftell(fh)==0)
		{
			/* we must have newly created the file */
			fwrite("SMU",1,4,fh);
		}
		entry.size = -1;
		strncpy(entry.uidl,m->uidl,sizeof(entry.uidl));
		fwrite(&entry,1,sizeof(entry),fh);
		fclose(fh);
	} else
	{
		SM_DEBUGF(5,("Failed to open %s\n",uidl->filename));
	}

	SM_LEAVE;
}

/**************************************************************************
 Returns the len of the uidl
**************************************************************************/
static int uidllen(char *buf)
{
	int len = 0;
	char c;

	for (;;)
	{
		c = *buf;
		if (c <= 0x20 || c >= 0x7f) /* signed anywhy */
			return len;
		len++;
		buf++;
	}
	return 0;
}

/**************************************************************************
 The uidl command. Sets the MAILF_DUPLICATE flag for mails which
 should not be downloaded because they have been downloaded already.
**************************************************************************/
static int pop3_uidl(struct connection *conn, struct pop3_server *server,
											struct dl_mail *mail_array, struct uidl *uidl)
{
	SM_ENTER;

	if (!server->nodupl) SM_RETURN(0,"%ld");

	thread_call_parent_function_async(status_set_status,1,_("Checking for mail duplicates..."));

	if (tcp_write(conn,"UIDL\r\n",6) == 6)
	{
		char *answer;
		if ((answer = pop3_receive_answer(conn,0)))
		{
			char status_buf[200];
			int num_duplicates = 0;

			while ((answer = tcp_readln(conn)))
			{
				int mno;

				if (answer[0] == '.' && answer[1] == '\n')
					break;

				mno = strtol(answer,&answer,10);
				if (mno >= 1 && mno <= mail_array[0].flags)
				{
					int len;
					answer++;
					len = uidllen(answer);
					if ((mail_array[mno].uidl = malloc(len+1)))
					{
						strncpy(mail_array[mno].uidl,answer,len);
						mail_array[mno].uidl[len] = 0;

						if (uidl_test(uidl,mail_array[mno].uidl))
						{
							mail_array[mno].flags |= MAILF_DUPLICATE;
							num_duplicates++;
							mail_array[mno].flags &= ~MAILF_DOWNLOAD;
						}
					}
				}
			}

			if (!answer)
			{
				if (tcp_error_code() == TCP_INTERRUPTED)
					SM_RETURN(0,"%ld");
			}

			sm_snprintf(status_buf,sizeof(status_buf),_("Found %d mail duplicates"),num_duplicates);
			thread_call_parent_function_async_string(status_set_status,1,status_buf);

			SM_RETURN(1,"%ld");
		}
	}
	SM_RETURN(0,"%ld");
}

/**************************************************************************
 Sends a noop to the given server
**************************************************************************/
static void pop3_noop(void *arg)
{
	struct connection *conn = (struct connection*)arg;

	if (tcp_write(conn,"STAT\r\n",6) == 6)
	{
		pop3_receive_answer(conn,0);
	}
}

/**************************************************************************
 Get statistics about pop3-folder contents. It returns the an array of
 dl_mail instances. The first (0) index gives the total amount of messages
 stored in the flags field.
 It is followed by the instances of the mail with the same index. See
 above for the dl_mail structure.
**************************************************************************/
static struct dl_mail *pop3_stat(struct connection *conn, struct pop3_server *server,
								 struct uidl *uidl,
                                 int receive_preselection, int receive_size, int has_remote_filter)
{
	char *answer;
	struct dl_mail *mail_array;
	int i,amm,size,mails_add = 0,cont = 0;
	int initial_mflags = MAILF_DOWNLOAD | (server->del?MAILF_DELETE:0);

	thread_call_parent_function_async(status_set_status,1,_("Getting statistics..."));

	if (tcp_write(conn,"STAT\r\n",6) <= 0) return 0;
	if (!(answer = pop3_receive_answer(conn,0)))
	{
		if (tcp_error_code() != TCP_INTERRUPTED)
			tell_from_subtask(N_("Could not get server statistics"));
		return 0;
	}

	if ((*answer++) != ' ') return NULL;
	if ((amm = strtol(answer,&answer,10))<=0) return NULL;
	if ((size = strtol(answer,NULL,10))<=0) return NULL;

	if (!(mail_array = malloc((amm+2)*sizeof(struct dl_mail)))) return NULL;
	mail_array[0].flags = amm;
	mail_array[0].size = size;

	/* Initial all mails should be downloaded */
	for (i=1;i<=amm;i++)
	{
		mail_array[i].flags = initial_mflags;
		mail_array[i].size = -1;
		mail_array[i].uidl = NULL;
	}

	if (server->nodupl)
	{
		/* open the uidl file and read in the string */
		uidl_open(uidl,server);

		/* call the POP3 UIDL command */
		if (pop3_uidl(conn,server,mail_array,uidl))
		{
			/* now check if there are uidls in the uidl file which are no longer on the server, remove them */
			uidl_remove_unused(uidl,mail_array);
		} else
		{
			if (tcp_error_code() == TCP_INTERRUPTED)
			{
				free(mail_array);
				return mail_array;
			}
		}
  }

	SM_DEBUGF(10,("Getting mail sizes...\n"));
	thread_call_parent_function_async(status_set_status,1,_("Getting mail sizes..."));

	/* List all mails with sizes */
	if (tcp_write(conn,"LIST\r\n",6) != 6)
	{
		SM_DEBUGF(5,("LIST command failed\n"));
		return mail_array;
	}

	/* Was the command succesful? */
	if (!(answer = pop3_receive_answer(conn,0)))
	{
		if (tcp_error_code() != TCP_INTERRUPTED)
		{
			SM_DEBUGF(5,("LIST command failed (%s)\n",answer));
			return mail_array;
		}
		free(mail_array);
		return NULL;
	}

	/* Freeze the list which displays the e-Mails */
	thread_call_parent_function_async(status_mail_list_freeze,0);

	/* Encounter the sizes of the mails, if we find a mail *
	 * with a bigger size notify the transfer window       */
	while ((answer = tcp_readln(conn)))
	{
		int mno,msize;
		if (answer[0] == '.' && answer[1] == '\n')
		{
			cont = 1; /* no errors occured */
			break;
		}
		mno = strtol(answer,&answer,10);
		msize = strtol(answer, NULL, 10);

		if (mno >= 1 && mno <= amm)
			mail_array[mno].size = msize;

		/* We only add the mails in mode 1 or 2 if they exceed the given limit */
		if ((receive_preselection == 2 || receive_preselection == 1) && msize > receive_size * 1024)
		{
			/* add this mail to the transfer window */
			mail_array[mno].flags |= MAILF_ADDED;
			thread_call_parent_function_async(status_mail_list_insert,3,mno,mail_array[mno].flags,msize);
			mails_add = 1;
		}
	}

	/* Thaw the list which displays the e-Mails */
	thread_call_parent_function_async(status_mail_list_thaw,0);

	if (!answer && tcp_error_code() == TCP_INTERRUPTED)
	{
		free(mail_array);
		return NULL;
	}

	/* No user interaction wanted */
	if (receive_preselection == 0 && !has_remote_filter) return mail_array;

	if (cont && ((mails_add && receive_preselection == 2) || has_remote_filter))
	{
		/* no errors and the user wants a more informative preselection or there are any remote filters */
		thread_call_parent_function_async(status_set_status,1,_("Getting mail infos..."));
		for (i=1;i<=amm;i++)
		{
			int issue_top = has_remote_filter || ((int)thread_call_parent_function_sync(NULL,status_mail_list_get_flags,1,i)!=-1);
			if (issue_top)
			{
				char buf[256];
				struct mail_scan ms;
				struct mail_complete *m;
				int more = 1; /* more lines needed */
				int showme = 0;

				if (!(m = mail_complete_create())) break;

				sprintf(buf, "TOP %d 1\r\n",i);
				if (tcp_write(conn,buf,strlen(buf)) != strlen(buf)) break;
				if (!(answer = pop3_receive_answer(conn,0)))
				{
					if (tcp_error_code() == TCP_INTERRUPTED)
					{
						mail_complete_free(m);
						free(mail_array);
						return NULL;
					}

					/* -ERR has been returned and nobody breaked the connection, what means that TOP is not supported */
					thread_call_parent_function_async(status_set_status,1,_("Couldn't receive more statistics"));
					break;
				}

				mail_scan_buffer_start(&ms, m,0);

				/* Read out the important infos */
				while ((answer = tcp_readln(conn)))
				{
					if (answer[0] == '.' && answer[1] == '\n') break;
					if (more)
						more = mail_scan_buffer(&ms, answer, strlen(answer));
				}

				mail_scan_buffer_end(&ms);

				if (!answer && tcp_error_code() == TCP_INTERRUPTED)
				{
					mail_complete_free(m);
					free(mail_array);
					return NULL;
				}

				/* Now check if mail should be filtered */
				if (has_remote_filter && (mail_array[i].flags & MAILF_DOWNLOAD))
				{
					/* process the headers as we require this now */
					if (mail_process_headers(m))
					{
						int ignore = (int)thread_call_parent_function_sync(NULL,callback_remote_filter_mail,1,m->info);
						if (ignore)
						{
							showme = 1;

							mail_array[i].flags &= ~MAILF_DOWNLOAD;

							if (!(mail_array[i].flags & MAILF_ADDED))
							{
								thread_call_parent_function_async(status_mail_list_insert,3,i,mail_array[i].flags,mail_array[i].size);
								mails_add = 1;
							} else
							{
								thread_call_parent_function_async(status_mail_list_set_flags,2,i,mail_array[i].flags);
							}
						}
					}
				}

				/* Tell the gui about the mail info (not asynchron!) */
				if (receive_preselection == 2 || showme)
				{
					thread_call_parent_function_sync(NULL,status_mail_list_set_info, 4,
						i, mail_find_header_contents(m,"from"), mail_find_header_contents(m,"subject"),mail_find_header_contents(m,"date"));
				}

				/* Check if we should receive more statitics (also not asynchron) */
				if (!(int)thread_call_parent_function_sync(NULL, status_more_statistics,0)) break;

				mail_complete_free(m);
			}
		}
	}

	/* if the application is iconified than only download mails < the selected size
	   and don't wait for user interaction  */
	if (thread_call_parent_function_sync(NULL,main_is_iconified,0))
	{
		for (i=1;i<=amm;i++)
		{
			if (mail_array[i].size > receive_size * 1024)
			{
				mail_array[i].flags &= ~(MAILF_DOWNLOAD | MAILF_DELETE);
			}
		}
		return mail_array;
	}

	if (mails_add && cont)
	{
		/* let the user select which mails (s)he wants */
		int start;

		thread_call_parent_function_async(status_set_status,1,_("Waiting for user interaction"));

		if (!(start = thread_call_parent_function_sync_timer_callback(pop3_noop, conn, 5000, status_wait,0)))
			return NULL;

		for (i=1;i<=amm;i++)
		{
			int fl = (int)thread_call_parent_function_sync(NULL,status_mail_list_get_flags,1,i);
			if (fl != -1) mail_array[i].flags = fl;
			else if (start & (1<<1)) mail_array[i].flags = 0; /* not listed mails should be ignored */
		}
	}

	return mail_array;
}

/**************************************************************************
 Regulary quit server.
**************************************************************************/
static int pop3_quit(struct connection *conn, struct pop3_server *server)
{
	thread_call_parent_function_sync(NULL,status_set_status,1,_("Logging out..."));
	if (tcp_write(conn,"QUIT\r\n",6) <= 0) return 0;
	return pop3_receive_answer(conn,1)?1:0;
}

/**************************************************************************
 Retrieve mail.
**************************************************************************/
static int pop3_get_mail(struct connection *conn, struct pop3_server *server,
												 int nr, int size, int already_dl, int auto_spam, char **white, char **black)
{
	char *fn,*answer;
	char buf[256];
	FILE *fp;
	int bytes_written;
	int delete_mail = 0;
	int headers = 1;

	if (!(fn = mail_get_new_name(MAIL_STATUS_UNREAD)))
	{
		tell_from_subtask(N_("Can\'t get new filename!"));
		return 0;
	}

	sprintf(buf, "RETR %d\r\n", nr);
	tcp_write(conn, buf, strlen(buf));

	if (!(fp = fopen(fn, "w")))
	{
		tell_from_subtask(N_("Can\'t open mail file!"));
		free(fn);
		return 0;
	}

	if (!(answer = pop3_receive_answer(conn,0)))
	{
		if (tcp_error_code() != TCP_INTERRUPTED)
			tell_from_subtask(N_("Couldn't receive the mail"));
		fclose(fp);
		remove(fn);
		free(fn);
		return 0;
	}

	bytes_written = 0;

	/* read the mail in now */
	while (1)
	{
		if (!(answer = tcp_readln(conn)))
		{
			if (tcp_error_code() != TCP_INTERRUPTED)
				tell_from_subtask(N_("Error while receiving the mail"));
			delete_mail = 1;
			break;
		}

		if (answer[0] == '.' && answer[1] == '\n')
			break;

		if (headers && answer[0] == '\n')
		{
			/* Write out the special SimpleMail header, to find out the pop server later */
			fprintf(fp,"X-SimpleMail-POP3: %s\n",server->name);
			headers = 0;
		}

		if (fputs(answer,fp) == EOF)
		{
			tell_from_subtask(N_("Error while writing the mail onto disk"));
			delete_mail = 1;
			break;
		}
		bytes_written += strlen(answer) + 1; /* tcp_readln() removes the \r */
		thread_call_parent_function_async(status_set_gauge, 1, already_dl + bytes_written);
	}

	thread_call_parent_function_async(status_set_gauge, 1, already_dl + bytes_written);

	fclose(fp);
	if (delete_mail) remove(fn);
	else
	{
		int is_spam = 0;

		if (auto_spam)
		{
			struct mail_info *mail = mail_info_create();
			if (mail)
			{
				mail->filename = fn;
				is_spam = spam_is_mail_spam(NULL,mail,white,black);
				mail->filename = NULL;
				mail_info_free(mail);
			}
		}

		thread_call_parent_function_async_string(callback_new_mail_arrived_filename, 2, fn, is_spam);
	}
	free(fn);

	return !delete_mail;
}

/**************************************************************************
 Mark the mail as deleted
**************************************************************************/
int pop3_del_mail(struct connection *conn, struct pop3_server *server, int nr)
{
	char buf[256];
	char *answer;
	sprintf(buf, "DELE %d\r\n",nr);
	if (tcp_write(conn,buf,strlen(buf))<=0) return 0;
	if (!(answer = pop3_receive_answer(conn,0))) return 0;
	return 1;
}

/**************************************************************************
 Download the mails
**************************************************************************/
int pop3_really_dl(struct list *pop_list, char *dest_dir, int receive_preselection, int receive_size, int has_remote_filter, char *folder_directory, int auto_spam, char **white, char **black)
{
	int rc = 0;

	/* If pop list is empty we of course succeed */
	if (!list_first(pop_list)) return 1;

	if (open_socket_lib())
	{
		struct pop3_server *server = (struct pop3_server*)list_first(pop_list);
		int nummails = 0; /* number of downloaded e-mails */

		for (;server; server = (struct pop3_server*)node_next(&server->node))
		{
			struct connection *conn;
			char head_buf[100];

			rc = 0;

			sprintf(head_buf,_("Fetching mails from %s"),server->name);
			thread_call_parent_function_async_string(status_set_head, 1, head_buf);
			if (server->title)
				thread_call_parent_function_async_string(status_set_title_utf8, 1, server->title);
			else
				thread_call_parent_function_async_string(status_set_title, 1, server->name);
			thread_call_parent_function_async_string(status_set_connect_to_server, 1, server->name);

			/* Ask for the login/password */
			if (server->ask)
			{
				char *password = malloc(512);
				char *login = malloc(512);

				if (password && login)
				{
					int rc;

					if (server->login) mystrlcpy(login,server->login,512);
					password[0] = 0;

					if ((rc = thread_call_parent_function_sync(NULL,sm_request_login,4,server->name,login,password,512)))
					{
						server->login = mystrdup(login);
						server->passwd = mystrdup(password);
					}
				}

				free(password);
				free(login);
			}

			if ((conn = tcp_connect(server->name, server->port, server->ssl && (!server->stls))))
			{
				char *timestamp;
				thread_call_parent_function_async(status_set_status,1,_("Waiting for login..."));

				if (pop3_wait_login(conn,server,&timestamp))
				{
					int goon = 1;

					if (!pop3_login(conn,server,timestamp))
					{
						SM_DEBUGF(15,("Logging in failed\n"));
						goon = 0;
						if (timestamp)
						{
							/* There seems to be POP3 Servers which don't like that APOP is tried first and the normal login procedure afterwards.
							   In such cases a reconnect should help. */
							pop3_quit(conn,server);
							tcp_disconnect(conn);
							SM_DEBUGF(15,("Trying to connect again to the server\n"));
							if ((conn = tcp_connect(server->name, server->port, server->ssl && (!server->stls))))
							{
								if (pop3_wait_login(conn,server,NULL))
								{
									goon = pop3_login(conn,server,NULL);
									if (!goon) SM_DEBUGF(15,("Login failed\n"));
								} else SM_DEBUGF(15,("Couldn't recevie a welcome message from the server\n"));
							} else SM_DEBUGF(15,("Couldn't connect again to the server\n"));
						}
					}

					if (goon)
					{
						struct uidl uidl;
						struct dl_mail *mail_array;

						SM_DEBUGF(15,("Logged in successfully\n"));

						uidl_init(&uidl,server,folder_directory);

						if ((mail_array = pop3_stat(conn,server,&uidl,receive_preselection,receive_size,has_remote_filter)))
						{
							int i;
							int mail_amm = mail_array[0].flags;
							char path[256];

							getcwd(path, 255);

							if(chdir(dest_dir) == -1)
							{
								tell_from_subtask(N_("Can\'t access income folder!"));
							} else
							{
								int max_mail_size_sum = 0;
								int mail_size_sum = 0;

								int max_dl_mails = 0;
								int cur_dl_mail = 0;

								int success = 1;

								/* determine the size of the mails which should be downloaded */
								for (i=1; i<=mail_amm; i++)
								{
									if (mail_array[i].flags & MAILF_DOWNLOAD)
									{
										max_mail_size_sum += mail_array[i].size;
										max_dl_mails++;
									} else
									{
										if (mail_array[i].flags & MAILF_DELETE)
										{
											max_dl_mails++;
										}
									}
								}

								thread_call_parent_function_async(status_init_gauge_as_bytes,1,max_mail_size_sum);
								thread_call_parent_function_async(status_init_mail,1,max_dl_mails);

								for (i=1; i<=mail_amm; i++)
								{
									int dl = (mail_array[i].flags & MAILF_DOWNLOAD)?1:0;
									int del = (mail_array[i].flags & MAILF_DELETE)?1:0;

									if (dl || del)
									{
										cur_dl_mail++;
										thread_call_parent_function_async(status_set_mail,2,cur_dl_mail,mail_array[i].size);
									}

									if (dl)
									{
										if (!pop3_get_mail(conn, server, i, mail_array[i].size, mail_size_sum, auto_spam, white, black))
										{
											if (tcp_error_code() != TCP_INTERRUPTED) tell_from_subtask(N_("Couldn't download the mail!\n"));
											success = 0;
											break;
										}

										/* add the mail to the uidl file if enabled */
										if (server->nodupl && mail_array[i].uidl)
										{
											uidl_add(&uidl,&mail_array[i]);
										}
										nummails++;
										mail_size_sum += mail_array[i].size;
									}

									if (del)
									{
										thread_call_parent_function_async(status_set_status,1,_("Marking mail as deleted..."));
										if (!pop3_del_mail(conn,server, i))
										{
											if (tcp_error_code() != TCP_INTERRUPTED) tell_from_subtask(N_("Can\'t mark mail as deleted!"));
											else
											{
												success = 0;
												break;
											}
										}
									}
								}

								rc = success;
								chdir(path);
							}
						}
						pop3_quit(conn,server);
						thread_call_parent_function_async(status_set_status,1,"");
					}
					free(timestamp);
				}
				tcp_disconnect(conn); /* NULL safe */
				if (thread_aborted())
				{
					if (!thread_call_parent_function_sync(NULL,status_skipped,0))
						break;
				}
			} else
			{
				if (thread_aborted())
				{
					if (!thread_call_parent_function_sync(NULL,status_skipped,0))
						break;
				} else
				{
					char message[380];

					sm_snprintf(message,sizeof(message),_("Unable to connect to server %s: %s"),server->name,tcp_strerror(tcp_error_code()));
					tell_from_subtask(message);
					rc = 0;
					break;
				}
			}
			/* Clear the preselection entries */
			thread_call_parent_function_sync(NULL,status_mail_list_clear,0);
		}
		close_socket_lib();

		thread_call_parent_function_async(callback_number_of_mails_downloaded,1,nummails);
	}
	else
	{
		tell_from_subtask(N_("Cannot open the bsdsocket.library!"));
	}

	return rc;
}


/**
 * @brief Log in and log out into a POP3 server as given by the @p server parameter.
 *
 * This function can be used to test POP3 settings but also some SMTP server
 * requires prior POP3 server logins.
 *
 * @param server defines the connection details of the POP3 server.
 * @return 1 on sucess, 0 on failure.
 * @note Assumes to be run in a sub thread (i.e., not SimpleMail's main thread)
 */
int pop3_login_only(struct pop3_server *server)
{
	int rc = 0;

	if (open_socket_lib())
	{
		struct connection *conn;

		if ((conn = tcp_connect(server->name, server->port,server->ssl && (!server->stls))))
		{
			char *timestamp;

			if (pop3_wait_login(conn,server,&timestamp))
			{
				int goon = 1;

				if (!pop3_login(conn,server,timestamp))
				{
					goon = 0;
					if (timestamp)
					{
						/* There seems to be POP3 Servers which don't like that APOP is tried first and the normal login procedure afterwards.
						   In such cases a reconnect should help. */
						pop3_quit(conn,server);
						tcp_disconnect(conn);
						if ((conn = tcp_connect(server->name, server->port, server->ssl && (!server->stls))))
						{
							if (pop3_wait_login(conn,server,NULL))
							{
								goon = pop3_login(conn,server,NULL);
							}
						}
					}
				}

				if (goon)
				{
					pop3_quit(conn,server);
					rc = 1;
				}
			}
			tcp_disconnect(conn); /* accepts NULL */
		}
		close_socket_lib();
	}

	/* Refresh the autocheck */
	thread_call_parent_function_sync(NULL,callback_autocheck_reset,0);
	return rc;
}

/**
 * @brief Construct a new instance holding POP3 server settings.
 *
 * Allocates a new POP3 server setting instance and initializes it
 * with default values.
 *
 * @return the instance which must be freed with pop_free() when no longer in
 * use or NULL on failure.
 */
struct pop3_server *pop_malloc(void)
{
	struct pop3_server *pop3;
	if ((pop3 = (struct pop3_server*)malloc(sizeof(*pop3))))
	{
		memset(pop3,0,sizeof(struct pop3_server));
		pop3->port = 110;
		pop3->active = 1;
		pop3->nodupl = 1;
	}
	return pop3;
}

/**
 * @brief Duplicates the given POP3 server settings.
 *
 * @param pop defines the settings which should be duplicated.
 * @return the duplicated POP3 server settings.
 */

struct pop3_server *pop_duplicate(struct pop3_server *pop)
{
	struct pop3_server *new_pop = pop_malloc();
	if (new_pop)
	{
		new_pop->name = mystrdup(pop->name);
		new_pop->login = mystrdup(pop->login);
		new_pop->passwd = mystrdup(pop->passwd);
		new_pop->title = mystrdup(pop->title);
		new_pop->del = pop->del;
		new_pop->port = pop->port;
		new_pop->apop = pop->apop;
		new_pop->ssl = pop->ssl;
		new_pop->stls = pop->stls;
		new_pop->active = pop->active;
		new_pop->nodupl = pop->nodupl;
		new_pop->ask = pop->ask;
	}
	return new_pop;
}

/**
 * @brief Frees the POP3 server settings completely.
 *
 * Deallocates all resources associated with the given
 * POP3 server settings.
 *
 * @param pop defines the instance which should be freed.
 */
void pop_free(struct pop3_server *pop)
{
	if (pop->name) free(pop->name);
	if (pop->login) free(pop->login);
	if (pop->passwd) free(pop->passwd);
	if (pop->title) free(pop->title);
	free(pop);
}

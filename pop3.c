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

#include "pop3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "hash.h"
#include "mail.h"
#include "md5.h"
#include "pop3_uidl.h"
#include "smintl.h"
#include "spam.h"
#include "support_indep.h"
#include "tcp.h"

#include "subthreads.h"
#include "support.h"
#include "tcpip.h"

/**
 * Receives a single line answer. Returns the line if this is positive
 * (without the +OK) else 0. If silent is 1 tell_from_subtask() is not called
 * in case of an error.
 *
 * @param conn the connection from which the pop3 answer should be read.
 * @param silent whether errors are propagated via tell_from_subtask().
 * @return the answer or NULL or an error.
 */
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

/**
 * Wait for the welcome message. If server delivers a timestamp it is placed
 * into the timestamp_ptr argument (string must be freed when no longer used).
 * The contents of timestamp is not touched if this call fails.
 *
 * @param conn the connection on which welcome message should be waited
 * @param server the server that was used to open the connection
 * @param timestamp_ptr if non-NULL, as pointer to a timestamp string is written
 *  to the contents. The timestamp must be freed via free() when no longer in
 *  use.
 * @return 0 on failure, otherwise something different.
 */
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
		if (server->stls)
		{
			if (tcp_write(conn,"STLS\r\n",6) <= 0) return 0;

			/* TODO: check if this call delivers a new timestamp */
			if (pop3_receive_answer(conn,0))
			{
				if (tcp_make_secure(conn, server->name, server->fingerprint))
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

/**
 * Log into the pop3 server.
 *
 * @param callbacks provides functions for user information
 * @param conn the previously established connection
 * @param server describes the server that was used to establish the connection
 *  and contains POP3-related options
 * @param timestamp as delivered by the server during the welcome phase. Used
 *  for APOP.
 * @return 0 on failure, otherwise something different than 0
 */
static int pop3_login(struct pop3_dl_callbacks *callbacks, struct connection *conn, struct pop3_server *server, char *timestamp)
{
	char buf[256];

	if (!timestamp && server->apop == 1)
	{
		tell_from_subtask(_("Failed to authenticate via APOP. Server hasn't delivered a timestamp."));
		return 0;
	}

	if (timestamp && !server->ssl)
	{
		SM_MD5_CTX context;
		unsigned char digest[16];
		char *ptr;
		int i;

		callbacks->set_status_static(_("Authenticate via APOP..."));

		MD5Init(&context);
		MD5Update(&context, (unsigned char*)timestamp, strlen(timestamp));
		MD5Update(&context, (unsigned char*)server->passwd,mystrlen(server->passwd));
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
				tell_from_subtask(_("Failed to authenticate via APOP"));
				return 0;
			}
			SM_DEBUGF(15,("APOP authentication failed\n"));
		} else
		{
			callbacks->set_status_static(_("Login successful!"));
			return 1;
		}
	}

	SM_DEBUGF(15,("Trying plain text method\n"));
	callbacks->set_status_static(_("Sending username..."));

	sm_snprintf(buf, sizeof(buf), "USER %s\r\n",server->login);
	if (tcp_write(conn,buf,strlen(buf)) <= 0) return 0;
	if (!pop3_receive_answer(conn,!!timestamp)) /* be silent if timestamp has given */
	{
		if (tcp_error_code() != TCP_INTERRUPTED && !timestamp)
			tell_from_subtask(N_("Error while identifying the user"));
		callbacks->set_status_static(_("Failed to identify the user"));
		SM_DEBUGF(15,("Sending the USER command failed\n"));
		return 0;
	}

	callbacks->set_status_static(_("Sending password..."));
	sm_snprintf(buf,sizeof(buf),"PASS %s\r\n",server->passwd);
	if (tcp_write(conn,buf,strlen(buf)) <= 0) return 0;
	if (!pop3_receive_answer(conn,0))
	{
		if (tcp_error_code() != TCP_INTERRUPTED)
			tell_from_subtask(N_("Error while identifying the user"));
		callbacks->set_status_static(_("Failed to identify the user"));
		return 0;
	}

	callbacks->set_status_static(_("Login successful!"));
	return 1;
}

struct dl_mail
{
	unsigned int flags; /**< the download flags of this mail */
	int size;			/**< the mails size */
};

#define MAILF_DELETE     (1<<0) /**< mail should be deleted */
#define MAILF_DOWNLOAD   (1<<1) /**< mail should be downloaded */
#define MAILF_DUPLICATE  (1<<2) /**< mail is duplicated */
#define MAILF_ADDED      (1<<7) /**< mails has been added to the status window */

struct pop3_mail_stats
{
	int num_dl_mails;
	struct dl_mail *dl_mails;
	char **uidls;					/**< vector of uidl strings */

	int total_size;
};

/**
 * Returns the len of the uidl.
 *
 * @param buf
 * @return the length.
 */
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

/**
 * Invokes the UIDL command. Sets the MAILF_DUPLICATE flag for mails which
 * should not be downloaded because they have been downloaded already.
 *
 * @param callbacks provides functions for user information
 * @param conn the connection to use.
 * @param server the server used to open the connection.
 * @param stats the filled stat information. The flags of items will
 *  possibly be changed by this call.
 * @param uidl the uild file.
 * @return success or not.
 */
static int pop3_uidl(struct pop3_dl_callbacks *callbacks,
					 struct connection *conn, struct pop3_server *server,
					 struct pop3_mail_stats *stats, struct uidl *uidl)
{
	char *answer;
	char status_buf[200];
	int num_duplicates = 0;

	SM_ENTER;

	if (!server->nodupl) SM_RETURN(0,"%ld");

	callbacks->set_status_static(_("Checking for mail duplicates..."));

	if (tcp_write(conn,"UIDL\r\n",6) != 6)
		SM_RETURN(0,"%ld");

	if (!(answer = pop3_receive_answer(conn,0)))
		SM_RETURN(0,"%ld");

	while ((answer = tcp_readln(conn)))
	{
		int mno, len;

		if (answer[0] == '.' && answer[1] == '\n')
			break;

		mno = strtol(answer,&answer,10) - 1;
		if (mno < 0 || mno >= stats->num_dl_mails)
			continue;

		/* Allocate memory for uidl vector, if not already done */
		if (!stats->uidls)
		{
			if ((stats->uidls = malloc(sizeof(stats->uidls[0])*stats->num_dl_mails)))
				memset(stats->uidls, 0, sizeof(stats->uidls[0])*stats->num_dl_mails);
		}

		if (!stats->uidls)
			continue;

		/* Extract the uidl from the answer */
		answer++;
		len = uidllen(answer);
		if (!(stats->uidls[mno] = malloc(len+1)))
			continue;
		strncpy(stats->uidls[mno],answer,len);
		stats->uidls[mno][len] = 0;

		/* Is this a know uidl? */
		if (!uidl_test(uidl,stats->uidls[mno]))
			continue;

		/* We have seem this uidl before, so the corresponding mail is
		 * a duplicate and we do not need to download it.
		 */
		stats->dl_mails[mno].flags |= MAILF_DUPLICATE;
		num_duplicates++;
		stats->dl_mails[mno].flags &= ~MAILF_DOWNLOAD;
	}

	if (!answer)
	{
		if (tcp_error_code() == TCP_INTERRUPTED)
			SM_RETURN(0,"%ld");
	}

	sm_snprintf(status_buf,sizeof(status_buf),_("Found %d mail duplicates"),num_duplicates);
	callbacks->set_status(status_buf);

	SM_RETURN(1,"%ld");
}

/**
 * Sends a noop/keep alive (via the STAT command) to the given server.
 *
 * @param arg
 */
static void pop3_timer_callback_noop(void *arg)
{
	struct connection *conn = (struct connection*)arg;

	if (tcp_write(conn,"STAT\r\n",6) == 6)
	{
		pop3_receive_answer(conn,0);
	}
}

/**
 * Free the resources in pop3_stat().
 *
 * @param mail_array
 */
static void pop3_free_mail_array(struct pop3_mail_stats *stats)
{
	int i;

	if (stats->uidls)
	{
		for (i=stats->num_dl_mails - 1; i>=0; i--)
			free(stats->uidls[i]);
		stats->uidls = NULL;
	}
	free(stats->dl_mails);
	stats->dl_mails = NULL;
}

/**
 * Get statistics about pop3 folder contents. It returns the an array of
 * dl_mail instances. The first (0) index gives the total amount of messages
 * stored in the flags field. It is followed by the instances of the mail with
 * the same index. See above for the dl_mail structure.
 *
 * This function may invoke several methods of the controller and interact with
 * the user.
 *
 * @param callbacks provides functions for user information
 * @param stats output parameter where to store the stats
 * @param conn the connection to use
 * @param server the server description used for opening the connection.
 * @param uidl an initialized uidl
 * @param receive_preselection a preselection value for the receive option.
 * @param receive_size a preselection value for the size of the mails.
 * @param has_remote_filter whether a remote filter is associated to the folder.
 *  In this case callback_remote_filter_mail() will be invoked on the context
 *  of the main thead for every mail to be downloaded.
 * @param quiet don't bother user about mail selections.
 * @return 0 on failure, else something other
 */
static int pop3_stat(struct pop3_dl_callbacks *callbacks,
					 struct pop3_mail_stats *stats,
					 struct connection *conn,
					 struct pop3_server *server,
					 struct uidl *uidl,
					 int receive_preselection,
					 int receive_size, int has_remote_filter, int quiet)
{
	char *answer;
	char *next_answer;
	struct dl_mail *mail_array;
	int i,amm,size,mails_add = 0,cont = 0;
	int initial_mflags = MAILF_DOWNLOAD | (server->del?MAILF_DELETE:0);

	callbacks->set_status_static(_("Getting statistics..."));

	if (tcp_write(conn,"STAT\r\n",6) <= 0) return 0;
	if (!(answer = pop3_receive_answer(conn,0)))
	{
		if (tcp_error_code() != TCP_INTERRUPTED)
			tell_from_subtask(N_("Could not get server statistics"));
		return 0;
	}

	if ((*answer++) != ' ') return 0;

	if ((amm = strtol(answer,&next_answer,10))<0) return 0;
	if (answer == next_answer) return 0;

	if ((size = strtol(next_answer,&answer,10))<0) return 0;
	if (next_answer == answer) return 0;

	if (!(mail_array = malloc((amm+1)*sizeof(struct dl_mail)))) return 0;
	stats->num_dl_mails = amm;
	stats->total_size = size;
	stats->dl_mails = mail_array;
	stats->uidls = NULL;

	/* Initial all mails should be downloaded */
	for (i=0;i<amm;i++)
	{
		mail_array[i].flags = initial_mflags;
		mail_array[i].size = -1;
	}

	if (server->nodupl)
	{
		/* open the uidl file and read in the string */
		uidl_open(uidl);

		/* call the POP3 UIDL command */
		if (pop3_uidl(callbacks,conn,server,stats,uidl))
		{
			/* now check if there are uidls in the uidl file which are no longer on the server, remove them */
			uidl_remove_unused(uidl, amm, stats->uidls);
		} else
		{
			if (tcp_error_code() == TCP_INTERRUPTED)
			{
				pop3_free_mail_array(stats);
				return 0;
			}
		}
  }

	SM_DEBUGF(10,("Getting mail sizes...\n"));
	callbacks->set_status_static(_("Getting mail sizes..."));

	/* List all mails with sizes */
	if (tcp_write(conn,"LIST\r\n",6) != 6)
	{
		SM_DEBUGF(5,("LIST command failed\n"));
		return 1;
	}

	/* Was the command successful? */
	if (!(answer = pop3_receive_answer(conn,0)))
	{
		if (tcp_error_code() != TCP_INTERRUPTED)
		{
			SM_DEBUGF(5,("LIST command failed (%s)\n",answer));
			return 1;
		}
		pop3_free_mail_array(stats);
		return 0;
	}

	/* Freeze the list which displays the e-Mails */
	if (callbacks->mail_list_freeze) callbacks->mail_list_freeze();

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
		mno = strtol(answer,&answer,10) - 1; /* We start counting at 0 */
		msize = strtol(answer, NULL, 10);

		if (mno >= 0 && mno < amm)
			mail_array[mno].size = msize;

		/* We only add the mails in mode 1 or 2 if they exceed the given limit */
		if ((receive_preselection == 2 || receive_preselection == 1) && msize > receive_size * 1024)
		{
			/* add this mail to the transfer window */
			mail_array[mno].flags |= MAILF_ADDED;
			callbacks->mail_list_insert(mno,mail_array[mno].flags,msize);
			mails_add = 1;
		}
	}

	/* Thaw the list which displays the e-Mails */
	if (callbacks->mail_list_thaw) callbacks->mail_list_thaw();

	if (!answer && tcp_error_code() == TCP_INTERRUPTED)
	{
		pop3_free_mail_array(stats);
		return 0;
	}

	/* No user interaction wanted */
	if (receive_preselection == 0 && !has_remote_filter) return 1;

	if (cont && ((mails_add && receive_preselection == 2) || has_remote_filter))
	{
		/* no errors and the user wants a more informative preselection or there are any remote filters */
		callbacks->set_status_static(_("Getting mail infos..."));
		for (i=0;i<amm;i++)
		{
			int issue_top = has_remote_filter || (callbacks->mail_list_get_flags(i) != -1);
			if (issue_top)
			{
				char buf[256];
				struct mail_scan ms;
				struct mail_complete *m;
				int more = 1; /* more lines needed */
				int showme = 0;

				if (!(m = mail_complete_create())) break;

				sprintf(buf, "TOP %d 1\r\n",i+1);
				if (tcp_write(conn,buf,strlen(buf)) != strlen(buf)) break;
				if (!(answer = pop3_receive_answer(conn,0)))
				{
					mail_complete_free(m);

					if (tcp_error_code() == TCP_INTERRUPTED)
					{
						pop3_free_mail_array(stats);
						return 0;
					}

					/* -ERR has been returned and nobody breaked the connection, what means that TOP is not supported */
					callbacks->set_status_static(_("Couldn't receive more statistics"));
					break;
				}

				mail_scan_buffer_start(&ms, m);

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
					pop3_free_mail_array(stats);
					return 0;
				}

				/* Now check if mail should be filtered */
				if (has_remote_filter && (mail_array[i].flags & MAILF_DOWNLOAD))
				{
					/* process the headers as we require this now */
					if (mail_process_headers(m))
					{
						if (callbacks->mail_ignore(m->info))
						{
							showme = 1;

							mail_array[i].flags &= ~MAILF_DOWNLOAD;

							if (!(mail_array[i].flags & MAILF_ADDED))
							{
								callbacks->mail_list_insert(i,mail_array[i].flags,mail_array[i].size);
								mails_add = 1;
							} else
							{
								callbacks->mail_list_set_flags(i,mail_array[i].flags);
							}
						}
					}
				}

				/* Tell the gui about the mail info */
				if (receive_preselection == 2 || showme)
				{
					char *from = mail_find_header_contents(m,"from");
					char *subject = mail_find_header_contents(m,"subject");
					char *date = mail_find_header_contents(m,"date");
					callbacks->mail_list_set_info(i, from, subject, date);
				}

				mail_complete_free(m);

				/* Check if we should receive more statistics */
				if (callbacks->more_statitics())
					break;
			}
		}
	}

	/* if the application is iconified than only download mails < the selected size
	   and don't wait for user interaction  */
	if (quiet)
	{
		for (i=1;i<=amm;i++)
		{
			if (mail_array[i].size > receive_size * 1024)
			{
				mail_array[i].flags &= ~(MAILF_DOWNLOAD | MAILF_DELETE);
			}
		}
		return 1;
	}

	if (mails_add && cont)
	{
		/* let the user select which mails (s)he wants */
		int start;

		callbacks->set_status_static(_("Waiting for user interaction"));
		if (!(start = callbacks->wait(pop3_timer_callback_noop, conn, 5000)))
		{
			pop3_free_mail_array(stats);
			return 0;
		}

		for (i=0;i<amm;i++)
		{
			int fl = callbacks->mail_list_get_flags(i);
			if (fl != -1) mail_array[i].flags = fl;
			else if (start & (1<<1)) mail_array[i].flags = 0; /* not listed mails should be ignored */
		}
	}

	return 1;
}

/**
 * Finish the connection.
 *
 * @param conn the connection to finish
 * @param server the server description used for opening the connection.
 * @return success or not.
 */
static int pop3_quit(struct pop3_dl_callbacks *callbacks, struct connection *conn, struct pop3_server *server)
{
	callbacks->set_status_static(_("Logging out..."));
	if (tcp_write(conn,"QUIT\r\n",6) <= 0) return 0;
	return pop3_receive_answer(conn,1)?1:0;
}

/**
 * Retrieve the complete mail.
 *
 * @param callbacks provides functions for user information
 * @param conn the connection to use
 * @param server the server description used for opening the connection.
 * @param nr
 * @param size
 * @param already_dl
 * @param auto_spam
 * @param white
 * @param black
 * @return
 */
static int pop3_get_mail(struct pop3_dl_callbacks *callbacks,
						 struct connection *conn,
						 struct pop3_server *server,
						 int nr, int size, int already_dl, int auto_spam,
						 char **white, char **black)
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
		callbacks->set_gauge(already_dl + bytes_written);
	}

	callbacks->set_gauge(already_dl + bytes_written);

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

		callbacks->new_mail_arrived_filename(fn, is_spam);
	}
	free(fn);

	return !delete_mail;
}

/**
 * Mark the mail as deleted on the POP3 server.
 *
 * @param conn the already opened connection.
 * @param nr the number of mail to be marked
 * @return 1 on success, 0 otherwise.
 */
static int pop3_del_mail(struct connection *conn, int nr)
{
	char buf[80];
	char *answer;
	sprintf(buf, "DELE %d\r\n",nr);
	if (tcp_write(conn,buf,strlen(buf))<=0) return 0;
	if (!(answer = pop3_receive_answer(conn,0))) return 0;
	return 1;
}

/*****************************************************************************/

/**
 * @brief Initialize the given uidl for the given server.
 *
 * @param uidl the uidl instance to be initialized
 * @param server the server in question
 * @param folder_directory base directory of the folders
 */
static void pop3_uidl_init(struct uidl *uidl, struct pop3_server *server, char *folder_directory)
{
	char c;
	char *buf;
	char *server_name = server->name;
	int len = strlen(folder_directory) + strlen(server_name) + 30;
	int n;

	memset(uidl,0,sizeof(*uidl));

	/* Construct the file name */
	if (!(uidl->filename = malloc(len)))
		return;

	strcpy(uidl->filename,folder_directory);
	sm_add_part(uidl->filename,".uidl.",len);
	buf = uidl->filename + strlen(uidl->filename);

	/* Using a hash doesn't make the filename unique but it should work for now */
	n = sprintf(buf,"%x",(unsigned int)sdbm((unsigned char*)server->login));

	buf += n;
	while ((c=*server_name))
	{
		if (c!='.') *buf++=c;
		server_name++;
	}
	*buf = 0;
}

/*****************************************************************************/

/**
 * Download mails from the given pop3 server.
 *
 * @param dl_options options for downloading. The pop list is ignored.
 * @param server the description defining the server from which to download
 * @param nummails_ptr
 * @return 0 for failure, 1 for success
 */
static int pop3_really_dl_single(struct pop3_dl_options *dl_options, struct pop3_server *server, int *nummails_ptr)
{
	char *dest_dir = dl_options->dest_dir;
	int receive_preselection = dl_options->receive_preselection;
	int receive_size = dl_options->receive_size;
	int has_remote_filter = dl_options->has_remote_filter;
	char *folder_directory = dl_options->folder_directory;
	int auto_spam = dl_options->auto_spam;
	char **white = dl_options->white;
	char **black = dl_options->black;

	struct pop3_dl_callbacks *callbacks = &dl_options->callbacks;

	struct connection *conn;
	struct connect_options connect_options = {0};
	char head_buf[100];
	int error_code;
	int nummails;
	int rc;

	rc = 0;
	nummails = 0;

	sm_snprintf(head_buf,sizeof(head_buf),_("Fetching mails from %s"),server->name);
	callbacks->set_head(head_buf);
	if (server->title)
		callbacks->set_title_utf8(server->title);
	else
		callbacks->set_title(server->name);
	callbacks->set_connect_to_server(server->name);

	/* Ask for the login/password */
	if (server->ask)
	{
		char *password = malloc(512);
		char *login = malloc(512);

		if (password && login)
		{
			if (server->login) mystrlcpy(login,server->login,512);
			password[0] = 0;

			if (callbacks->request_login(server->name, login, password, 512))
			{
				server->login = mystrdup(login);
				server->passwd = mystrdup(password);
			}
		}

		free(password);
		free(login);
	}

	connect_options.use_ssl = server->ssl && !server->stls;
	connect_options.fingerprint = server->fingerprint;

	if ((conn = tcp_connect(server->name, server->port, &connect_options, &error_code)))
	{
		char *timestamp;

		callbacks->set_status_static(_("Waiting for login..."));

		if (pop3_wait_login(conn,server,&timestamp))
		{
			int goon = 1;

			if (!pop3_login(callbacks, conn,server,timestamp))
			{
				SM_DEBUGF(15,("Logging in failed\n"));
				goon = 0;
				if (timestamp)
				{
					/* There seems to be POP3 Servers which don't like that APOP is tried first and the normal login procedure afterwards.
					   In such cases a reconnect should help. */
					pop3_quit(callbacks,conn,server);
					tcp_disconnect(conn);
					SM_DEBUGF(15,("Trying to connect again to the server\n"));
					if ((conn = tcp_connect(server->name, server->port, &connect_options, &error_code)))
					{
						if (pop3_wait_login(conn,server,NULL))
						{
							goon = pop3_login(callbacks, conn,server,NULL);
							if (!goon) SM_DEBUGF(15,("Login failed\n"));
						} else SM_DEBUGF(15,("Couldn't recevie a welcome message from the server\n"));
					} else SM_DEBUGF(15,("Couldn't connect again to the server\n"));
				}
			}

			if (goon)
			{
				struct uidl uidl;
				struct pop3_mail_stats stats;

				SM_DEBUGF(15,("Logged in successfully\n"));

				pop3_uidl_init(&uidl,server,folder_directory);

				if ((pop3_stat(callbacks, &stats, conn,server,&uidl,receive_preselection,receive_size,has_remote_filter,dl_options->quiet)))
				{
					struct dl_mail *mail_array = stats.dl_mails;
					int i;
					int mail_amm = stats.num_dl_mails;
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
						for (i=0; i<mail_amm; i++)
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

						callbacks->init_gauge_as_bytes(max_mail_size_sum);
						callbacks->init_mail(max_dl_mails);

						for (i=0; i<mail_amm; i++)
						{
							int dl = (mail_array[i].flags & MAILF_DOWNLOAD)?1:0;
							int del = (mail_array[i].flags & MAILF_DELETE)?1:0;

							if (dl || del)
							{
								cur_dl_mail++;
								callbacks->set_mail(cur_dl_mail,mail_array[i].size);
							}

							if (dl)
							{
								if (!pop3_get_mail(callbacks, conn, server, i + 1, mail_array[i].size, mail_size_sum, auto_spam, white, black))
								{
									if (tcp_error_code() != TCP_INTERRUPTED) tell_from_subtask(N_("Couldn't download the mail!\n"));
									success = 0;
									break;
								}

								/* add the mail to the uidl if available */
								if (stats.uidls && stats.uidls[i])
								{
									uidl_add(&uidl,stats.uidls[i]);
								}
								nummails++;
								mail_size_sum += mail_array[i].size;
							}

							if (del)
							{
								callbacks->set_status_static(_("Marking mail as deleted..."));

								if (!pop3_del_mail(conn, i + 1))
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

					pop3_free_mail_array(&stats);
				}
				pop3_quit(callbacks,conn,server);
				callbacks->set_status_static("");

				free(uidl.filename);
				free(uidl.entries);
			}
			free(timestamp);
		}
		tcp_disconnect(conn); /* NULL safe */
		if (thread_aborted())
		{
			if (callbacks->skip_server())
				goto out;
		}
	} else
	{
		if (thread_aborted())
		{
			if (callbacks->skip_server())
				goto out;
		} else
		{
			char message[380];

			sm_snprintf(message,sizeof(message),_("Unable to connect to server %s: %s"),server->name,tcp_strerror(tcp_error_code()));
			tell_from_subtask(message);
			rc = 0;
			goto out;
		}
	}
	/* Clear the preselection entries */
	if (callbacks->mail_list_clear) callbacks->mail_list_clear();
out:
	*nummails_ptr = nummails;
	return rc;
}

/*****************************************************************************/

int pop3_really_dl(struct pop3_dl_options *dl_options)
{
	int rc = 0;
	int socked_lib_opened = 0;
	int downloaded_mails = 0;

	struct list *pop_list = dl_options->pop_list;
	struct pop3_server *server = (struct pop3_server*)list_first(pop_list);

	/* If pop list is empty we of course succeed */
	if (!server) return 1;

	if (!open_socket_lib())
	{
		tell_from_subtask(N_("Cannot open the bsdsocket.library!"));
		goto out;
	}

	for (;server; server = (struct pop3_server*)node_next(&server->node))
	{
		int mails;

		if (!pop3_really_dl_single(dl_options, server, &mails))
			break;

		downloaded_mails += mails;
	}

	dl_options->callbacks.number_of_mails_downloaded(downloaded_mails);

out:
	if (socked_lib_opened) close_socket_lib();
	return rc;
}

/*****************************************************************************/

int pop3_login_only(struct pop3_server *server, struct pop3_dl_callbacks *callbacks)
{
	int rc = 0;
	int error_code;
	int goon = 1;
	struct connection *conn;
	struct connect_options conn_opts = {0};
	char *timestamp;

	if (!open_socket_lib())
		return rc;

	conn_opts.use_ssl = server->ssl && (!server->stls);
	conn_opts.fingerprint = server->fingerprint;

	if (!(conn = tcp_connect(server->name, server->port, &conn_opts, &error_code)))
		goto bailout;

	if (!pop3_wait_login(conn,server,&timestamp))
		goto bailout;

	if (!pop3_login(callbacks,conn,server,timestamp))
	{
		goon = 0;
		if (timestamp)
		{
			/* There seems to be POP3 Servers which don't like that APOP is tried first and the normal login procedure afterwards.
			   In such cases a reconnect should help. */
			pop3_quit(callbacks,conn,server);
			tcp_disconnect(conn);
			if ((conn = tcp_connect(server->name, server->port, &conn_opts, &error_code)))
			{
				if (pop3_wait_login(conn,server,NULL))
				{
					goon = pop3_login(callbacks,conn,server,NULL);
				}
			}
		}
	}

	if (goon)
	{
		pop3_quit(callbacks,conn,server);
		rc = 1;
	}

bailout:
	if (conn) tcp_disconnect(conn); /* accepts NULL */
	close_socket_lib();

	return rc;
}

/*****************************************************************************/

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

/*****************************************************************************/

struct pop3_server *pop_duplicate(struct pop3_server *pop)
{
	struct pop3_server *new_pop;

	if (!(new_pop = pop_malloc()))
		return NULL;

	new_pop->name = mystrdup(pop->name);
	new_pop->fingerprint = mystrdup(pop->fingerprint);
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
	return new_pop;
}

/*****************************************************************************/

void pop_free(struct pop3_server *pop)
{
	if (!pop) return;

	free(pop->name);
	free(pop->fingerprint);
	free(pop->login);
	free(pop->passwd);
	free(pop->title);
	free(pop);
}

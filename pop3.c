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

/*
** pop3.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#ifndef _AROS

#ifdef __WIN32__
#include <windows.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#endif

#include "configuration.h"
#include "mail.h"
#include "tcp.h"
#include "simplemail.h"
#include "smintl.h"
#include "status.h"
#include "support.h"
#include "support_indep.h"

#include "mainwnd.h"
#include "subthreads.h"
#include "tcpip.h"

#include "pop3.h"

#define REC_BUFFER_SIZE 512


/**************************************************************************
 Recieves a single line answer. Returns the line if this is positive
 (without the +OK) else 0
**************************************************************************/
static char *pop3_receive_answer(struct connection *conn)
{
	char *answer;
	if (!(answer = tcp_readln(conn)))
	{
		/* Don't put any message when only interrupted */
		if (tcp_error_code() != TCP_INTERRUPTED)
			tell_from_subtask(N_("Error receiving data from host!"));
		return NULL;
	}
	if (!strncmp(answer,"+OK",3)) return answer+3;
	if (!strncmp(answer,"-ERR",4)) tell_from_subtask(answer);
	return NULL;
}

/**************************************************************************
 Wait for the welcome message. Must be rewritten using tcp_readln()
**************************************************************************/
static int pop3_wait_login(struct connection *conn, struct pop3_server *server)
{
	if (pop3_receive_answer(conn))
	{
		/* Make the connection secure if requested */
		if (server->ssl && server->stls)
		{
			if (tcp_write(conn,"STLS\r\n",6) <= 0) return 0;

			if (pop3_receive_answer(conn))
			{
				return tcp_make_secure(conn);
			}
		} else return 1;
	}
	return 0;
}

/**************************************************************************
 Log into the pop3 server.
**************************************************************************/
int pop3_login(struct connection *conn, struct pop3_server *server)
{
	char buf[256];

	thread_call_parent_function_async(status_set_status,1,N_("Sending username..."));

	sprintf(buf, "USER %s\r\n",server->login);
	if (tcp_write(conn,buf,strlen(buf)) <= 0) return 0;
	if (!pop3_receive_answer(conn))
	{
		if (tcp_error_code() != TCP_INTERRUPTED)
			tell_from_subtask(N_("Error while identifing the user"));
		return 0;
	}

	thread_call_parent_function_async(status_set_status,1,N_("Sending password..."));
	sprintf(buf,"PASS %s\r\n",server->passwd);
	if (tcp_write(conn,buf,strlen(buf)) <= 0) return 0;
	if (!pop3_receive_answer(conn))
	{
		if (tcp_error_code() != TCP_INTERRUPTED)
			tell_from_subtask(N_("Error while identifing the user"));
		return 0;
	}

	thread_call_parent_function_async(status_set_status,1,N_("Login successful!"));
	return 1;
}

struct dl_mail
{
	unsigned int flags; /* the download flags of this mail */
	int size;					/* the mails size */
	char *uidl;				/* the uidl string, might be NULL */
};

#define MAILF_DELETE   (1<<0) /* mail should be deleted */
#define MAILF_DOWNLOAD (1<<1) /* mail should be downloaded */
#define MAILF_DUPLICATE (1<<2) /* mail is duplicated */


struct uidl_entry /* stored on the harddisk */
{
	int size; /* size of the mail, unused yet, so it is -1 for now */
	char uidl[72]; /* null terminated, 70 are enough according to RFC 1939 */
};

struct uidl
{
	char *filename; /* the filename of the uidl file */
	int num_entries; /* number of entries */
	struct uidl_entry *entries; /* this are the entries */
};

/**************************************************************************
 Init the uidl,
**************************************************************************/
static void uidl_init(struct uidl *uidl, struct pop3_server *server, char *folder_directory)
{
	char c;
	char *server_name = server->name;
	char *buf;
	int len = strlen(folder_directory)+strlen(server_name)+20;

	memset(uidl,0,sizeof(*uidl));
	if (!(buf = uidl->filename = malloc(len)))
		return;

	strcpy(uidl->filename,folder_directory);
	sm_add_part(uidl->filename,".uidl.",len);

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

	if (!uidl->filename) return 0;

	if ((fh = fopen(uidl->filename,"rb")))
	{
		unsigned char id[4];
		int fsize = myfsize(fh);
		fread(id,1,4,fh);

		if (id[0] == 'S' && id[1] == 'M' && id[2] == 'U' && id[3] == 0 && ((fsize - 4)%sizeof(struct uidl_entry))==0)
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
	return rc;
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
	if (uidl->entries)
	{
		int i,amm=mail_array[0].flags;
		for (i=0; i<uidl->num_entries; i++)
		{
			int j,found=0;
			char *uidl_entry = uidl->entries[i].uidl;
			for (j=1;j<=amm;j++)
			{
				if (!strcmp(uidl_entry,mail_array[j].uidl))
				{
					found = 1;
					break;
				}
			}

			if (!found)
				memset(uidl_entry,0,sizeof(uidl->entries[i].uidl));
		}
	}
}

/**************************************************************************
 Add the uidl to the uidl file. Writes this into the file. Its directly
 written to the correct place. uidl->entries is not expanded.
**************************************************************************/
void uidl_add(struct uidl *uidl, struct dl_mail *m)
{
	int i=0;
	FILE *fh;

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
				return;
			}
		}
	}
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
	}
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
	if (!server->nodupl) return 0;

	thread_call_parent_function_async(status_set_status,1,N_("Checking for mail duplicates..."));

	if (tcp_write(conn,"UIDL\r\n",6) == 6)
	{
		char *answer;
		if ((answer = pop3_receive_answer(conn)))
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
					return 0;
			}

			sprintf(status_buf,_("Found %d mail duplicates"),num_duplicates);
			thread_call_parent_function_async_string(status_set_status,1,status_buf);
			return 1;
		}
	}
	return 0;
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
                                 int receive_preselection, int receive_size)
{
	char *answer;
	struct dl_mail *mail_array;
	int i,amm,size,mails_add = 0,cont = 0;
	int initial_mflags = MAILF_DOWNLOAD | (server->del?MAILF_DELETE:0);

	thread_call_parent_function_async(status_set_status,1,N_("Getting statistics..."));

	if (tcp_write(conn,"STAT\r\n",6) <= 0) return 0;
	if (!(answer = pop3_receive_answer(conn)))
	{
		if (tcp_error_code() != TCP_INTERRUPTED)
			tell_from_subtask(N_("Could not get server statistics"));
		return 0;
	}

	if ((*answer++) != ' ') return NULL;
	if ((amm = strtol(answer,&answer,10))<=0) return NULL;
	if ((size = strtol(answer,NULL,10))<=0) return NULL;

//	thread_call_parent_function_async(dl_init_gauge_mail,1,amm);

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
		pop3_uidl(conn,server,mail_array,uidl);

		/* now check if there are uidls in the uidl file which are no longer on the server, remove them */
    uidl_remove_unused(uidl,mail_array);
  }

	thread_call_parent_function_async(status_set_status,1,N_("Getting mail sizes..."));

	/* List all mails with sizes */
	if (tcp_write(conn,"LIST\r\n",6) != 6)
	{
		return mail_array;
	}

  /* Was the command succesful? */
	if (!(answer = pop3_receive_answer(conn)))
	{
		if (tcp_error_code() != TCP_INTERRUPTED)
			return mail_array;
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

		if ((receive_preselection != 0) && (msize > receive_size * 1024))
		{
			/* add this mail to the transfer window */
			thread_call_parent_function_async(status_mail_list_insert,3,mno,mail_array[mno].flags,msize);
			mails_add = 1;
		}
	}

	if (!answer && tcp_error_code() == TCP_INTERRUPTED)
	{
		free(mail_array);
		return NULL;
	}

	/* Thaw the list which displays the e-Mails */
	thread_call_parent_function_async(status_mail_list_thaw,0);

	/* No user interaction wanted */
	if (receive_preselection == 0) return mail_array;

	if (cont && mails_add && receive_preselection == 2)
	{
		/* no errors and the user wants a more informative preselection */
		for (i=1;i<=amm;i++)
		{
			int has_added = ((int)thread_call_parent_function_sync(status_mail_list_get_flags,1,i)==-1)?0:1;
			if (has_added)
			{
				char buf[256];
				struct mail_scan ms;
				struct mail *m;
				int more = 1; /* more lines needed */

				if (!(m = mail_create())) break;

				sprintf(buf, "TOP %d 1\r\n",i);
				if (tcp_write(conn,buf,strlen(buf)) != strlen(buf)) break;
				if (!(answer = pop3_receive_answer(conn)))
				{
					if (tcp_error_code() == TCP_INTERRUPTED)
					{
						mail_free(m);
						free(mail_array);
						return NULL;
					}

					/* -ERR has been returned and nobody breaked the connection, what means that TOP is not supported */
					thread_call_parent_function_async(status_set_status,1,N_("Couldn't receive more statistics"));
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
					mail_free(m);
					free(mail_array);
					return NULL;
				}

				/* Tell the gui about the mail info (not asynchron!)*/
				thread_call_parent_function_sync(status_mail_list_set_info, 4,
					i, mail_find_header_contents(m,"from"), mail_find_header_contents(m,"subject"),mail_find_header_contents(m,"date"));

				/* Check if we should receive more statitics (also not asynchron) */
				if (!(int)thread_call_parent_function_sync(status_more_statistics,0)) break;

				mail_free(m);
			}
		}
	}

	if (mails_add && cont)
	{
		/* let the user select which mails (s)he wants */
		int start;

		thread_call_parent_function_async(status_set_status,1,N_("Waiting for user interaction"));
		if (!(start = thread_call_parent_function_sync(status_wait,0)))
			return NULL;

		for (i=1;i<=amm;i++)
		{
			int fl = (int)thread_call_parent_function_sync(status_mail_list_get_flags,1,i);
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
	thread_call_parent_function_sync(status_set_status,1,N_("Logging out..."));
	if (tcp_write(conn,"QUIT\r\n",6) <= 0) return 0;
	return pop3_receive_answer(conn)?1:0;
}

/**************************************************************************
 Retrieve mail.
**************************************************************************/
static int pop3_get_mail(struct connection *conn, struct pop3_server *server,
												 int nr, int size, int already_dl)
{
	char *fn,*answer;
	char buf[256];
	FILE *fp;
	int bytes_written;
	int delete_mail = 0;
	int headers = 1;
	unsigned int secs = sm_get_current_seconds(); /* used to reduce the amount of notifing the parent task */

//	thread_call_parent_function_sync(dl_init_gauge_byte,1,size);

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

	if (!(answer = pop3_receive_answer(conn)))
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

//  	thread_call_parent_function_async(dl_set_mail_size_sum,1,already_dl + bytes_written);
//		thread_call_parent_function_async(dl_set_gauge_byte,1,bytes_written);

		thread_call_parent_function_async(status_set_gauge, 1, already_dl + bytes_written);
	}

	thread_call_parent_function_async(status_set_gauge, 1, already_dl + bytes_written);

	fclose(fp);
	if (delete_mail) remove(fn);
	else
	{
		thread_call_parent_function_async_string(callback_new_mail_arrived_filename, 1, fn);
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
	if (!(answer = pop3_receive_answer(conn))) return 0;
	return 1;
}

/**************************************************************************
 Download the mails
**************************************************************************/
int pop3_really_dl(struct list *pop_list, char *dest_dir, int receive_preselection, int receive_size, char *folder_directory)
{
	int rc = 0;

	if (open_socket_lib())
	{
		struct pop3_server *server = (struct pop3_server*)list_first(pop_list);
		int nummails = 0; /* number of downloaded e-mails */

		for( ;server; server = (struct pop3_server*)node_next(&server->node))
		{
			struct connection *conn;
			char head_buf[100];

			sprintf(head_buf,_("Fetching mails from %s"),server->name);
			thread_call_parent_function_async_string(status_set_head, 1, head_buf);
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

					if ((rc = thread_call_parent_function_sync(sm_request_login,4,server->name,login,password,512)))
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
				thread_call_parent_function_async(status_set_status,1,N_("Waiting for login..."));
				if (pop3_wait_login(conn,server))
				{
					if (pop3_login(conn,server))
					{
						struct uidl uidl;
						struct dl_mail *mail_array;

						uidl_init(&uidl,server,folder_directory);

						if ((mail_array = pop3_stat(conn,server,&uidl,receive_preselection,receive_size)))
						{
							int i;
							int mail_amm = mail_array[0].flags;
							char path[256];
						
							getcwd(path, 255);

							if(chdir(dest_dir) == -1)
							{
								tell_from_subtask(N_("Can\'t access income-folder!"));
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
										if (!pop3_get_mail(conn, server, i, mail_array[i].size, mail_size_sum))
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
										thread_call_parent_function_async(status_set_status,1,N_("Marking mail as deleted..."));
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
					}
				}
				tcp_disconnect(conn);

				if (thread_aborted()) break;
			} else
			{
				if (thread_aborted()) break;
				else tell_from_subtask(tcp_strerror(tcp_error_code()));
				rc = 0;
				break;
			}

			/* Clear the preselection entries */
			thread_call_parent_function_sync(status_mail_list_clear,0);
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

/**************************************************************************
 Only log in the server (for smtp servers which need this)
**************************************************************************/
int pop3_login_only(struct pop3_server *server)
{
	int rc = 0;

	if (open_socket_lib())
	{
		struct connection *conn;

		if ((conn = tcp_connect(server->name, server->port,server->ssl && (!server->stls))))
		{
			if (pop3_wait_login(conn,server))
			{
				if (pop3_login(conn,server))
				{
					pop3_quit(conn,server);
					rc = 1;
				}
			}  
			tcp_disconnect(conn);
		}
		close_socket_lib();
	}

	/* Refresh the autocheck */
	thread_call_parent_function_sync(callback_autocheck_refresh,0);
	return rc;
}


struct pop_entry_msg
{
	struct list *pop_list;
	char *dest_dir;
	int receive_preselection;
	int receive_size;
	int called_by_auto;
	char *folder_directory;
};

/**************************************************************************
 Entrypoint for the fetch mail process
**************************************************************************/
static int pop3_entry(struct pop_entry_msg *msg)
{
	struct list pop_list;
	struct pop3_server *pop;
	char *dest_dir, *folder_directory;
	int receive_preselection = msg->receive_preselection;
	int receive_size = msg->receive_size;
	int called_by_auto = msg->called_by_auto;

	list_init(&pop_list);
	pop = (struct pop3_server*)list_first(msg->pop_list);

	while (pop)
	{
		if (pop->name)
		{
			struct pop3_server *new_pop = pop_duplicate(pop);
			if (new_pop)
				list_insert_tail(&pop_list,&new_pop->node);
		}
		pop = (struct pop3_server*)node_next(&pop->node);
	}

	dest_dir = mystrdup(msg->dest_dir);
	folder_directory = mystrdup(msg->folder_directory);

	if (thread_parent_task_can_contiue())
	{
		thread_call_parent_function_async(status_init,1,0);
		if (called_by_auto) thread_call_parent_function_async(status_open_notactivated,0);
		else thread_call_parent_function_async(status_open,0);

		pop3_really_dl(&pop_list,dest_dir,receive_preselection,receive_size,folder_directory);
		thread_call_parent_function_async(status_close,0);
	}
	return 0;
}

/**************************************************************************
 Fetch the mails. Starts a subthread.
**************************************************************************/
int pop3_dl(struct list *pop_list, char *dest_dir,
            int receive_preselection, int receive_size, int called_by_auto)
{
	struct pop_entry_msg msg;
	msg.pop_list = pop_list;
	msg.dest_dir = dest_dir;
	msg.receive_preselection = receive_preselection;
	msg.receive_size = receive_size;
	msg.called_by_auto = called_by_auto;
	msg.folder_directory = user.folder_directory;
	return thread_start(THREAD_FUNCTION(&pop3_entry),&msg);
}


#else

#include "configuration.h"
#include "mail.h"
#include "tcp.h"
#include "simplemail.h"
#include "smintl.h"
#include "status.h"
#include "support.h"
#include "support_indep.h"

int pop3_login_only(struct pop3_server *server)
{
	return 1;
}

int pop3_dl(struct list *pop_list, char *dest_dir,
            int receive_preselection, int receive_size, int called_by_auto)
{
	return 1;
}
#endif

/**************************************************************************
 malloc() a pop3_server and initializes it with default values.
 TODO: rename all pop3 identifiers to pop
**************************************************************************/
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

/**************************************************************************
 Duplicates a pop3_server
**************************************************************************/
struct pop3_server *pop_duplicate(struct pop3_server *pop)
{
	struct pop3_server *new_pop = pop_malloc();
	if (new_pop)
	{
		new_pop->name = mystrdup(pop->name);
		new_pop->login = mystrdup(pop->login);
		new_pop->passwd = mystrdup(pop->passwd);
		new_pop->del = pop->del;
		new_pop->port = pop->port;
		new_pop->ssl = pop->ssl;
		new_pop->stls = pop->stls;
		new_pop->active = pop->active;
		new_pop->nodupl = pop->nodupl;
		new_pop->ask = pop->ask;
	}
	return new_pop;
}

/**************************************************************************
 Frees a pop3_server completly
**************************************************************************/
void pop_free(struct pop3_server *pop)
{
	if (pop->name) free(pop->name);
	if (pop->login) free(pop->login);
	if (pop->passwd) free(pop->passwd);
	free(pop);
}

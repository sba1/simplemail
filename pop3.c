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
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "mail.h"
#include "tcp.h"
#include "simplemail.h"
#include "support.h"
#include "support_indep.h"

#include "dlwnd.h"
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
		tell_from_subtask("Error receiving data from host!");
		return NULL;
	}
	if (!strncmp(answer,"+OK",3)) return answer+3;
	tell_from_subtask(answer);
	return NULL;
}

/**************************************************************************
 Wait for the welcome message. Must be rewritten using tcp_readln()
**************************************************************************/
static int pop3_wait_login(struct connection *conn, struct pop3_server *server)
{
	return pop3_receive_answer(conn)?1:0;
}

/**************************************************************************
 Log into the pop3 server.
**************************************************************************/
int pop3_login(struct connection *conn, struct pop3_server *server)
{
	char buf[256];

	thread_call_parent_function_sync(dl_set_status,1,"Sending username...");

	sprintf(buf, "USER %s\r\n",server->login);
	if (tcp_write(conn,buf,strlen(buf)) <= 0) return 0;
	if (!pop3_receive_answer(conn))
	{
		tell_from_subtask("Error while identifing the user");
		return 0;
	}

	thread_call_parent_function_sync(dl_set_status,1,"Sending password...");
	sprintf(buf,"PASS %s\r\n",server->passwd);
	if (tcp_write(conn,buf,strlen(buf)) <= 0) return 0;
	if (!pop3_receive_answer(conn))
	{
		tell_from_subtask("Error while identifing the user");
		return 0;
	}

	thread_call_parent_function_sync(dl_set_status,1,"Login successful!");
	
	return 1;
}

#define MAILF_DELETE   (1<<0) /* mail should be deleted */
#define MAILF_DOWNLOAD (1<<1) /* mail should be downloaded */

/**************************************************************************
 Get statistics about pop3-folder contents. It returns an array of mail
 numbers which should be downloaded. The first (0) index gives the total
 amount of messages. It is followed by some flags which belong to the
 mail with the same index. See above for the flags.
**************************************************************************/
static int *pop3_stat(struct connection *conn, struct pop3_server *server,
                      int receive_preselection, int receive_size)
{
	char *answer;
	int *mail_array;
	int i,amm,mails_add = 0,cont = 0;
	int initial_mflags = MAILF_DOWNLOAD | (server->del?MAILF_DELETE:0);

	thread_call_parent_function_sync(dl_set_status,1,"Getting statistics...");
	if (tcp_write(conn,"STAT\r\n",6) <= 0) return 0;
	if (!(answer = pop3_receive_answer(conn)))
	{
		tell_from_subtask("Could not server statistics");
		return 0;
	}

	if ((*answer++) != ' ') return NULL;
	if ((amm = atoi(answer))<=0) return NULL;

	if (!(mail_array = malloc((amm+2)*sizeof(int)))) return NULL;
	mail_array[0] = amm;

	/* Initial all mails should be downloaded */
	for (i=1;i<=amm;i++)
		mail_array[i] = initial_mflags;

	/* Test if the user wished preselection at all */
	if (receive_preselection == 0) return mail_array;

  /* List all mails with sizes */
	if (tcp_write(conn,"LIST\r\n",6) != 6) return mail_array;

  /* Was the command succesful? */
	if (!(answer = pop3_receive_answer(conn))) return mail_array;

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

		if (msize > receive_size * 1024)
		{
			/* add this mail to the transfer window */
			thread_call_parent_function_sync(dl_insert_mail,3,mno,initial_mflags,msize);
			mails_add = 1;
		}
	}

	if (cont && mails_add && receive_preselection == 2)
	{
		/* no errors and the user wants a more informative preselection */
		for (i=1;i<=amm;i++)
		{
			int has_added = ((int)thread_call_parent_function_sync(dl_get_mail_flags,1,i)==-1)?0:1;
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
					/* -ERR has been returned, what means that TOP is not supported */
					thread_call_parent_function_sync(dl_set_status,1,"Couldn't receive more statistics");
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
				mail_process_headers(m);

				/* Tell the gui about the mail info */
				thread_call_parent_function_sync(dl_insert_mail_info, 4, i, m->from, m->subject, m->seconds);

				mail_free(m);
			}
		}
	}

	if (mails_add && cont)
	{
		/* let the user select which mails (s)he wants */
		if (!(thread_call_parent_function_sync(dl_wait,1)))
			return NULL;

		for (i=1;i<=amm;i++)
		{
			int fl = (int)thread_call_parent_function_sync(dl_get_mail_flags,1,i);
			if (fl != -1) mail_array[i] = fl;
		}
	}

	return mail_array;
}

/**************************************************************************
 Regulary quit server.
**************************************************************************/
static int pop3_quit(struct connection *conn, struct pop3_server *server)
{
	thread_call_parent_function_sync(dl_set_status,1,"Logging out...");
	if (tcp_write(conn,"QUIT\r\n",6) <= 0) return 0;
	return pop3_receive_answer(conn)?1:0;
}

/**************************************************************************
 Retrieve mail.
**************************************************************************/
static int pop3_get_mail(struct connection *conn, struct pop3_server *server, int nr)
{
	unsigned char c;
	char *fn,*answer;
	char buf[256];
	FILE *fp;
	int size; /* size of the mail */
	int bytes_written;
	int delete_mail = 0;

	sprintf(buf, "LIST %d\r\n", nr);
	tcp_write(conn, buf, strlen(buf));

	if (!(answer = pop3_receive_answer(conn)))
	{
		tell_from_subtask("Couldn't get the mails size!");
		return 0;
	}

	answer++; /* the space char */

	/* skip the mail number */
	while(1)
	{
		c = *answer++;
		if (!c) return 0;
		if (!isdigit(c)) break;
	}

	size = atoi(answer);
	thread_call_parent_function_sync(dl_init_gauge_byte,1,size);

	if (!(fn = mail_get_new_name()))
	{
		tell_from_subtask("Can\'t get new filename!");
		return 0;
	}

	sprintf(buf, "RETR %d\r\n", nr);
	tcp_write(conn, buf, strlen(buf));

	if (!(fp = fopen(fn, "w")))
	{
		tell_from_subtask("Can\'t open mail file!");
		free(fn);
		return 0;
	}

	if (!(answer = pop3_receive_answer(conn)))
	{
		tell_from_subtask("Couldn't receive the mail");
		fclose(fp);
		free(fn);
		return 0;
	}

	bytes_written = 0;
	thread_call_parent_function_sync(dl_set_gauge_byte,1,0);

	/* read the mail in now */
	while (1)
	{
		if (!(answer = tcp_readln(conn)))
		{
			tell_from_subtask("Error while receiving the mail");
			delete_mail = 1;
			break;
		}

		if (answer[0] == '.' && answer[1] == '\n')
			break;

		if (fputs(answer,fp) == EOF)
		{
			tell_from_subtask("Error while writing the mail onto disk");
			delete_mail = 1;
			break;
		}
		bytes_written += strlen(answer);
		thread_call_parent_function_sync(dl_set_gauge_byte,1,bytes_written);
	}

	fclose(fp);
	if (delete_mail) remove(fn);
	else
	{
		thread_call_parent_function_sync(callback_new_mail_arrived_filename, 1, fn);
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
static int pop3_really_dl(struct list *pop_list, char *dest_dir, int receive_preselection, int receive_size)
{
	int rc = 0;

	if (open_socket_lib())
	{
		struct pop3_server *server = (struct pop3_server*)list_first(pop_list);
		while (server)
		{
			struct connection *conn;
			char buf[512];

			sprintf(buf,"Connecting to server %s...",server->name);
			thread_call_parent_function_sync(dl_set_title,1,server->name);
			thread_call_parent_function_sync(dl_set_status,1,buf);

			if ((conn = tcp_connect(server->name, server->port, server->ssl)))
			{
				thread_call_parent_function_sync(dl_set_status,1,"Waiting for login...");
				if (pop3_wait_login(conn,server))
				{
					if (pop3_login(conn,server))
					{
						int *mail_array = pop3_stat(conn,server,receive_preselection,receive_size);
						if (mail_array)
						{
							int i;
							int mail_amm = mail_array[0];
							char path[256];
						
							thread_call_parent_function_sync(dl_init_gauge_mail,1,mail_amm);
							thread_call_parent_function_sync(dl_set_status,1,"Receiving mails...");

							getcwd(path, 255);

							if(chdir(dest_dir) == -1)
							{
								tell_from_subtask("Can\'t access income-folder!");
							} else
							{
								for(i = 1; i <= mail_amm; i++)
								{
									thread_call_parent_function_sync(dl_set_gauge_mail,1,i);

									if (mail_array[i] & MAILF_DOWNLOAD)
									{
										if (!pop3_get_mail(conn,server, i))
										{
											tell_from_subtask("Couldn't download the mail!\n");
											break;
										}
									}
									
									if (mail_array[i] & MAILF_DELETE)
									{
										thread_call_parent_function_sync(dl_set_status,1,"Marking mail as deleted...");
										if (!pop3_del_mail(conn,server, i))
										{
											tell_from_subtask("Can\'t mark mail as deleted!");
										}
									}
								}

								chdir(path);
							}
						}

						pop3_quit(conn,server);
					}
				}
				tcp_disconnect(conn);
			}

			/* Clear the preselection entries */
			thread_call_parent_function_sync(dl_clear,0);

			server = (struct pop3_server*)node_next(&server->node);
		}
		close_socket_lib();
	}
	else
	{
		tell_from_subtask("Cannot open the bsdsocket.library!");
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

		if ((conn = tcp_connect(server->name, server->port,server->ssl)))
		{
			if (pop3_wait_login(conn,server))
			{
				if (pop3_login(conn,server))
				{
					pop3_quit(conn,server);
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
};

/**************************************************************************
 Entrypoint for the fetch mail process
**************************************************************************/
static int pop3_entry(struct pop_entry_msg *msg)
{
	struct list pop_list;
	struct pop3_server *pop;
	char *dest_dir;
	int receive_preselection = msg->receive_preselection;
	int receive_size = msg->receive_size;

	list_init(&pop_list);
	pop = (struct pop3_server*)list_first(msg->pop_list);

	while (pop)
	{
		struct pop3_server *new_pop = pop_duplicate(pop);
		if (new_pop)
			list_insert_tail(&pop_list,&new_pop->node);
		pop = (struct pop3_server*)node_next(&pop->node);
	}

	dest_dir = mystrdup(msg->dest_dir);

	if (thread_parent_task_can_contiue())
	{
		thread_call_parent_function_sync(dl_window_open,0);
		pop3_really_dl(&pop_list,dest_dir,receive_preselection,receive_size);
		thread_call_parent_function_sync(dl_window_close,0);
	}
	return 0;
}

/**************************************************************************
 Fetch the mails. Starts a subthread.
**************************************************************************/
int pop3_dl(struct list *pop_list, char *dest_dir,
            int receive_preselection, int receive_size)
{
	struct pop_entry_msg msg;
	msg.pop_list = pop_list;
	msg.dest_dir = dest_dir;
	msg.receive_preselection = receive_preselection;
	msg.receive_size = receive_size;
	return thread_start(&pop3_entry,&msg);
}

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
		new_pop->active = pop->active;
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


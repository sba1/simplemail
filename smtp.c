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
** smtp.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "mail.h"
#include "tcp.h"
#include "simplemail.h"
#include "smtp.h"
#include "support.h"

#include "subthreads.h"
#include "tcpip.h"
#include "upwnd.h"

#include "configuration.h"

#include "md5.h"
#include "hmac_md5.h"
#include "codecs.h"

static int buf_flush(struct connection *conn, char *buf, long len)
{
	int rc;
	long sent;
	
	rc = 0;
	
	sent = tcp_write(conn, buf, len);
	if(sent == len)
	{
		rc = 1;
		if(!rc)
		{
			tell_from_subtask("Error while sending data.");
		}
		
	}
	
	return rc;
}

__inline static int buf_cat(struct connection *conn, char *buf, char c)
{
	int rc;
	static len = 0;
	static CR = 0;
	
	rc = 0;
	
	if(c == '\n')
	{
		if(!CR)
		{
			buf[len++] = '\r';
			CR = 0;
		}  
		buf[len++] = '\n';
		rc  = buf_flush(conn, buf, len);
		len = 0;
		buf[0]=0;
	}
	else
	{
		if(c == '\r')
		{
			CR = 1;
		}
		
		buf[len++] = c;
		buf[len] = 0;
		if(len == 1077)
		{
			len = 0;
			buf[0] = 0;
			tell_from_subtask("PANIC: Line with more than 1076 chars detected!");
		}
		else
		{
			rc = 1;
		}  
	}
	
	return rc;
}

static char *buf_init(void)
{
	char *rc;
	
	rc = malloc(1100);
	if(rc)
		rc[0] = 0;
	
	return rc;
}

static void buf_free(char *buf)
{
	free(buf);
}

static int smtp_send_cmd(struct connection *conn, char *cmd, char *args)
{
	int rc;
	char *buf;
	long count;
	int ready = 0;
	
	rc  = -1;
	buf = malloc(1024);

	if(cmd != NULL)
	{
		if(args != NULL)
		{
			sprintf(buf, "%s %s\r\n", cmd, args);
		}
		else
		{
			sprintf(buf, "%s\r\n", cmd);
		}
		count = tcp_write(conn, buf, strlen(buf));
		
		if(count != strlen(buf))
		{
			return(rc);
		}
	}

	free(buf);
	
	while (!ready && (buf = tcp_readln(conn)))
	{
/*		  puts(buf);*/

		if(buf[3] == ' ')
		{
			ready = 1;
			rc = atol(buf);
		}
	}
	
	return rc;
}

/**************************************************************************
 Send the HELO command
**************************************************************************/
static int smtp_helo(struct connection *conn, struct smtp_server *server)
{
	char dom[513];

	if (!server->ip_as_domain) mystrlcpy(dom,server->domain,sizeof(dom));
	else if(gethostname(dom, 512) != 0);

	if (smtp_send_cmd(conn, NULL, NULL) != SMTP_SERVICE_READY)
	{
		tell_from_subtask("Service not ready.");
		return 0;
	}

	if (smtp_send_cmd(conn,"HELO",dom) != SMTP_OK) return 0;

	return 1;
}

static int smtp_from(struct connection *conn, struct smtp_server *server, char *from)
{
	int rc;
	char *buf;

	rc = 0;
	buf = malloc(1024);

	if(buf != NULL)
	{
		sprintf(buf, "FROM:<%s>", from);

		if(smtp_send_cmd(conn, "MAIL", buf) == SMTP_OK)
		{
			rc = 1;
		}

		free(buf);
	}

	return rc;
}

static int smtp_rcpt(struct connection *conn, struct smtp_server *server, struct out_mail *om)
{
	int rc;
	long i;
	char *buf;

	rc = 0;
	
	buf = malloc(1024);
	if(buf != NULL)
	{
		rc = 1;
		
		for(i = 0; om->rcp[i] != NULL; i++)
		{
			sprintf(buf, "TO:<%s>", om->rcp[i]);
			if(smtp_send_cmd(conn, "RCPT", buf) != SMTP_OK)
			{
				rc = 0;
				break;
			}
		}
		
		free(buf);
	}

	return rc;

}

static int smtp_data(struct connection *conn, struct smtp_server *server, char *mailfile)
{
	int rc;
	long ret;
	char *buf;
	FILE *fp;
	int c;
	long size;

	rc = 0;
	
	buf = buf_init();
	if(buf != NULL)
	{
		fp = fopen(mailfile, "r");
		if(fp)
		{
			fseek(fp, 0L, SEEK_END);
			size = ftell(fp); /* what's that?? */ /* look into your ANSI-C manual :) */ /* now it's ok :-) */
			thread_call_parent_function_sync(up_init_gauge_byte,1,size);
			fseek(fp, 0L, SEEK_SET);
			
			ret = smtp_send_cmd(conn, "DATA", NULL);
			if((ret == SMTP_OK) || (ret == SMTP_SEND_MAIL))
			{
				long i;
				long z;
				
				i = 0;
				rc = 1;
				z = ((z = size / 100) >1)?z:1;
				
				while((c = fgetc(fp)) != EOF)
				{
					if(buf_cat(conn, buf, c) == 0)
					{
						rc = 0;
						break;
					}
					if((i++%z) == 0)
					{  
						thread_call_parent_function_sync(up_set_gauge_byte,1,i);
						if(thread_call_parent_function_sync(up_checkabort,0))
						{
							rc = 0;
							break;
						}
					}  
				}
				if(rc == 1)
				{
					buf_flush(conn, buf, strlen(buf));
					if(smtp_send_cmd(conn, "\r\n.", NULL) != SMTP_OK) /* \r\n is done by the function */
					{
						rc = 0;
					}
				}  
			}
			else
			{
				tell_from_subtask("DATA-Cmd failed.");
			}
			
			fclose(fp);
		}

		buf_free(buf);
	}  

	return rc;
}

/**************************************************************************
 Send the EHLO command (for ESMTP servers)
**************************************************************************/
int esmtp_ehlo(struct connection *conn, struct smtp_server *server)
{
	char dom[513];
	char *answer;

	server->esmtp.flags = 0;
	server->esmtp.auth_flags  = 0;

	if (!server->ip_as_domain) mystrlcpy(dom,server->domain,sizeof(dom));
	else if(gethostname(dom, 512) != 0);

	if (smtp_send_cmd(conn, NULL, NULL) != SMTP_SERVICE_READY)
	{
		tell_from_subtask("Service not ready.");
		return 0;
	}

	tcp_write(conn, "EHLO ",5);
	tcp_write(conn, dom, strlen(dom));
	tcp_write(conn, "\r\n", 2);
	tcp_flush(conn);

	do
	{
		answer = tcp_readln(conn);
		if (!answer) return 0;

		if (strstr(answer, "ENHANCEDSTATUSCODES")) server->esmtp.flags |= ESMTP_ENHACEDSTATUSCODES;
		else if (strstr(answer, "8BITMIME")) server->esmtp.flags |= ESMTP_8BITMIME;
		else if (strstr(answer, "ONEX")) server->esmtp.flags |= ESMTP_ONEX;
		else if (strstr(answer, "ETRN")) server->esmtp.flags |= ESMTP_ETRN;
		else if (strstr(answer, "XUSR")) server->esmtp.flags |= ESMTP_XUSR;
		else if (strstr(answer, "PIPELINING")) server->esmtp.flags |= ESMTP_PIPELINING;
		else if (strstr(answer, "AUTH"))
		{
			server->esmtp.flags |= ESMTP_AUTH;

			if (strstr(answer, "PLAIN")) server->esmtp.auth_flags |= AUTH_PLAIN;
			if (strstr(answer, "LOGIN")) server->esmtp.auth_flags |= AUTH_LOGIN;
			if (strstr(answer, "DIGEST-MD5")) server->esmtp.auth_flags |= AUTH_DIGEST_MD5;
			if (strstr(answer, "CRAM-MD5")) server->esmtp.auth_flags |= AUTH_CRAM_MD5;
		}
	} while (answer[3] != ' ');

	return atoi(answer)==SMTP_OK;
}

static int esmtp_auth_cram(struct connection *conn, struct smtp_server *server)
{
	static char cram_str[] = "AUTH CRAM-MD5\r\n";
	char *line;
	int rc;
	char *challenge;
	unsigned int challenge_len;
	unsigned long digest[4]; /* 16 chars */
	char buf[512];
	char *encoded_str;

	tcp_write(conn, cram_str,sizeof(cram_str)-1);

	if (!(line = tcp_readln(conn))) return 0;
	rc = atoi(line);
	if (rc != 334) return 0;

	if (!(challenge = decode_base64(line+4,strlen(line+4),&challenge_len)))
		return 0;

	hmac_md5(challenge,strlen(challenge),server->esmtp.auth_password,strlen(server->esmtp.auth_password),(char*)digest);
	free(challenge);
	sprintf(buf,"%s %08lx%08lx%08lx%08lx%c%c",server->esmtp.auth_login,
					digest[0],digest[1],digest[2],digest[3],0,0); /* I don't know if the two nullbytes should be counted as well */

	encoded_str = encode_base64(buf,strlen(buf));
	if (!encoded_str) return 0;
	tcp_write(conn,encoded_str,strlen(encoded_str)-1); /* -1 because of the linefeed */
	tcp_write(conn,"\r\n",2);
	free(encoded_str);

	if (!(line = tcp_readln(conn))) return 0;
	rc = atoi(line);

	if (rc != 235)
	{
		tell_from_subtask("SMTP AUTH CRAM failed");
	} else return 1;
	return 0;
}

static int esmtp_auth_digest_md5(struct connection *conn, struct smtp_server *server)
{
	static char digest_str[] = "AUTH DIGEST-MD5\r\n";
	char *line;
	int rc;
	char *challenge;
	unsigned int challenge_len;
	unsigned long digest[4]; /* 16 chars */
	char buf[512];
	char *encoded_str;
	MD5_CTX context;

	tcp_write(conn, digest_str,sizeof(digest_str)-1);

	if (!(line = tcp_readln(conn))) return 0;
	rc = atoi(line);
	if (rc != 334) return 0;

	if (!(challenge = decode_base64(line+4,strlen(line+4),&challenge_len)))
		return 0;

	strcpy(buf,challenge);
	strcpy(buf+challenge_len,server->esmtp.auth_password);

	free(challenge);
  
	MD5Init(&context);
	MD5Update(&context, buf, strlen(buf));
	MD5Final((char*)digest, &context);

	sprintf(buf,"%s %08lx%08lx%08lx%08lx%c%c",server->esmtp.auth_login,
					digest[0],digest[1],digest[2],digest[3],0,0); /* the same as above */

	encoded_str = encode_base64(buf,strlen(buf));
	if (!encoded_str) return 0;

	tcp_write(conn,encoded_str,strlen(encoded_str)-1); /* -1 because of the line feed */
	tcp_write(conn,"\r\n",2);
	free(encoded_str);

	if (!(line = tcp_readln(conn))) return 0;
	rc = atoi(line);
	if (rc != 235)
	{
		tell_from_subtask("SMTP AUTH DIGEST-MD5 failed");
	} else return 1;
	return 0;
}


int esmtp_auth(struct connection *conn, struct smtp_server *server)
{
	int rc;
	char *buf, prep[1024];

	rc = 0;

  if(server->esmtp.auth_flags & AUTH_CRAM_MD5)
	{
		rc = esmtp_auth_cram(conn, server);
	}
/*	else if(server->esmtp.auth_flags & AUTH_DIGEST_MD5)
	{
		rc = esmtp_auth_digest_md5(conn, server);
	}*/
	else if(server->esmtp.auth_flags & AUTH_LOGIN)
	{
		if(smtp_send_cmd(conn, "AUTH", "LOGIN") == 334)
		{
			strcpy(prep, server->esmtp.auth_login);

			buf = encode_base64(prep, strlen(prep));
			buf[strlen(buf) - 1] = 0;
			
			if(smtp_send_cmd(conn, buf, NULL) == 334)
			{
				free(buf);

				strcpy(prep, server->esmtp.auth_password);

				buf = encode_base64(prep, strlen(prep));
				buf[strlen(buf) - 1] = 0;

				if(smtp_send_cmd(conn, buf, NULL) == 235)
				{
					rc = 1;
				}
			}
		}
	}
	else if(server->esmtp.auth_flags & AUTH_PLAIN)
	{
		if(smtp_send_cmd(conn, "AUTH", "PLAIN") == 334)
		{
			prep[0]=0;
			strcpy(prep + 1, server->esmtp.auth_login);
			strcpy(prep + 1 + strlen(server->esmtp.auth_login) + 1, server->esmtp.auth_password);
			
			buf = encode_base64(prep, strlen(server->esmtp.auth_login) + strlen(server->esmtp.auth_password) + 2);
			buf[strlen(buf) - 1] = 0;
			if(smtp_send_cmd(conn, buf, NULL) == 235)
			{
				rc = 1;
			}
			free(buf);
		}
	}

	return rc;
}

/**************************************************************************
 Login into the (e)smtp server. After a succesfull call you
 can send the mails.
**************************************************************************/
static int smtp_login(struct connection *conn, struct smtp_server *server)
{
	if (server->esmtp.auth)
	{
		thread_call_parent_function_sync(up_set_status,1,"Sending EHLO...");
		if (!esmtp_ehlo(conn,server))
		{
			tell_from_subtask("EHLO failed");
			return 0;
		}
		thread_call_parent_function_sync(up_set_status,1,"Sending AUTH...");
		if (!esmtp_auth(conn,server))
		{
			tell_from_subtask("AUTH failed");
			return 0;
		}
	} else
	{
		thread_call_parent_function_sync(up_set_status,1,"Sending HELO...");
		if (!smtp_helo(conn,server))
		{
			tell_from_subtask("HELO failed");
			return 0;
		}
	}
	return 1;
}

/**************************************************************************
 Send all the mails in now
**************************************************************************/
static int smtp_send_mails(struct connection *conn, struct smtp_server *server)
{
	struct out_mail **om = server->out_mail;
	int i,amm=0;

	/* Count the number of mails */
	while (om[amm]) amm++;

	thread_call_parent_function_sync(up_init_gauge_mail,1,amm);
		
	for (i = 0; i < amm; i++)   
	{
		thread_call_parent_function_sync(up_set_gauge_mail,1,i+1);

		thread_call_parent_function_sync(up_set_status,1,"Sending FROM...");
		if (!smtp_from(conn,server, om[i]->from))
		{
			tell_from_subtask("FROM failed.");
			return 0;
		}

		thread_call_parent_function_sync(up_set_status,1,"Sending RCPT...");
		if (!smtp_rcpt(conn,server, om[i]))
		{
			tell_from_subtask("RCPT failed.");
			return 0;
		}

		thread_call_parent_function_sync(up_set_status,1,"Sending DATA...");
		if (!smtp_data(conn,server, om[i]->mailfile))
		{
			tell_from_subtask("DATA failed.");
			return 0;
		}

		/* no error while mail sending, so it can be moved to the "Sent" folder now */
		thread_call_parent_function_sync(callback_mail_has_been_sent,1,om[i]->mailfile);
	}
	return 1;
}

/**************************************************************************
 Send the QUIT command
**************************************************************************/
static int smtp_quit(struct connection *conn)
{
	return (smtp_send_cmd(conn, "QUIT", NULL) == SMTP_OK);
}

/**************************************************************************
 Send the mails now.
**************************************************************************/
static int smtp_send_really(struct smtp_server *server)
{
	int rc = 0;

	if (open_socket_lib())
	{
		struct connection *conn;
		thread_call_parent_function_sync(up_set_status,1,"Connecting...");

		if ((conn = tcp_connect(server->name, server->port)))
		{
			if (smtp_login(conn,server))
			{
				if (smtp_send_mails(conn,server))
				{
					thread_call_parent_function_sync(up_set_status,1,"Sending QUIT...");
					rc = smtp_quit(conn);
				}
			}

			thread_call_parent_function_sync(up_set_status,1,"Disconnecting...");
			tcp_disconnect(conn);
		}

		close_socket_lib();
	}
	else
	{
		tell_from_subtask("Cannot open bsdsocket.library. Please start a TCP/IP-Stack.");
	}

	return rc;
}

char **duplicate_string_array(char **rcp)
{
	char **newrcp;
	int rcps=0;
	while (rcp[rcps]) rcps++;

	if (newrcp = (char**)malloc((rcps+1)*sizeof(char*)))
	{
		int i;
		for (i=0;i<rcps;i++)
		{
			newrcp[i] = mystrdup(rcp[i]);
		}
		newrcp[i] = NULL;
	}
	return newrcp;
}

struct out_mail **create_outmail_array(int amm)
{
	int memsize;
	struct out_mail **newom;

	memsize = (amm+1)*sizeof(struct out_mail*) + sizeof(struct out_mail)*amm;
	if ((newom = (struct out_mail**)malloc(memsize)))
	{
		int i;
		struct out_mail *out = (struct out_mail*)(((char*)newom)+sizeof(struct out_mail*)*(amm+1));

		memset(newom,0,memsize);

		for (i=0;i<amm;i++)
			newom[i] = out++;
	}
	return newom;
}

struct out_mail **duplicate_outmail_array(struct out_mail **om)
{
	int amm = 0;
	struct out_mail **newom;

	while (om[amm]) amm++;

	if ((newom = create_outmail_array(amm)))
	{
		int i;

		for (i=0;i<amm;i++)
		{
			newom[i]->from = mystrdup(om[i]->from);
			newom[i]->mailfile = mystrdup(om[i]->mailfile);
			newom[i]->rcp = duplicate_string_array(om[i]->rcp);
		}
	}
	return newom;
}

/**************************************************************************
 Entrypoint for the send mail process
**************************************************************************/
static int smtp_entry(struct smtp_server *server)
{
	struct smtp_server copy_server;

	memset(&copy_server,0,sizeof(copy_server));

	copy_server.name          			= mystrdup(server->name);
	copy_server.domain					= mystrdup(server->domain);
	copy_server.port          			= server->port;
	copy_server.esmtp.auth          	= server->esmtp.auth;
	copy_server.esmtp.auth_login    	= mystrdup(server->esmtp.auth_login);
	copy_server.esmtp.auth_password 	= mystrdup(server->esmtp.auth_password);
	copy_server.ip_as_domain  			= server->ip_as_domain;
	copy_server.out_mail      			= duplicate_outmail_array(server->out_mail);

	if (thread_parent_task_can_contiue())
	{
		smtp_send_really(&copy_server);
		thread_call_parent_function_sync(up_window_close,0);
	}
	return 0;
}

/**************************************************************************
 Send the mails. Starts a subthread.
**************************************************************************/
int smtp_send(struct smtp_server *server)
{
	return thread_start(smtp_entry,server);
}

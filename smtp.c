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
#include <ctype.h>

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


int buf_flush(struct smtp_server *server, char *buf, long len)
{
	int rc;
	long sent;
	
	rc = 0;
	
	sent = tcp_write(server->socket, buf, len);
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

__inline static int buf_cat(struct smtp_server *server, char *buf, char c)
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
		rc  = buf_flush(server, buf, len);
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

char *buf_init(void)
{
	char *rc;
	
	rc = malloc(1100);
	if(rc)
		rc[0] = 0;
	
	return rc;
}

void buf_free(char *buf)
{
	free(buf);
}

int smtp_send_cmd(struct smtp_server *server, char *cmd, char *args)
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
		count = tcp_write(server->socket, buf, strlen(buf));
		
		if(count != strlen(buf))
		{
			return(rc);
		}
	}

	free(buf);
	
	while(!ready && (buf = tcp_readln(server->socket)))
	{
/*		  puts(buf);*/

		if(buf[3] == ' ')
		{
			ready = 1;
			rc = atol(buf);
		}
		free(buf);
	}
	
	return rc;
}

int smtp_helo(struct smtp_server *server)
{
	int rc;
	char dom[513];
	int ok;

	rc = 0;

	if(server->ip_as_domain)
	{
		ok = (gethostname(dom, 512) == 0);
	}
	else
	{
		strcpy(dom, server->domain);
		ok = 1;
	}

	if(ok)
	{
		if(smtp_send_cmd(server, NULL, NULL) == SMTP_SERVICE_READY)
		{
			if(smtp_send_cmd(server, "HELO", dom) == SMTP_OK)
			{
				rc = 1;
			}
		}
		else
		{
			tell_from_subtask("Service not ready.");
		}
	}

	return rc;
}

int smtp_from(struct smtp_server *server, char *from)
{
	int rc;
	char *buf;

	rc = 0;
	buf = malloc(1024);

	if(buf != NULL)
	{
		sprintf(buf, "FROM:<%s>", from);

		if(smtp_send_cmd(server, "MAIL", buf) == SMTP_OK)
		{
			rc = 1;
		}

		free(buf);
	}

	return rc;
}

int smtp_rcpt(struct smtp_server *server, struct out_mail *om)
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
			if(smtp_send_cmd(server, "RCPT", buf) != SMTP_OK)
			{
				rc = 0;
				break;
			}
		}
		
		free(buf);
	}

	return rc;

}

int smtp_data(struct smtp_server *server, char *mailfile)
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
			
			ret = smtp_send_cmd(server, "DATA", NULL);
			if((ret == SMTP_OK) || (ret == SMTP_SEND_MAIL))
			{
				long i;
				long z;
				
				i = 0;
				rc = 1;
				z = ((z = size / 100) >1)?z:1;
				
				while((c = fgetc(fp)) != EOF)
				{
					if(buf_cat(server, buf, c) == 0)
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
					buf_flush(server, buf, strlen(buf));
					if(smtp_send_cmd(server, "\r\n.\n", NULL) != SMTP_OK)
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

int smtp_quit(struct smtp_server *server)
{
	int rc;

	rc = (smtp_send_cmd(server, "QUIT", NULL) == SMTP_OK);

	return rc;
}

static long get_amm(struct out_mail **array)
{
	long rc;
	
	for(rc = 0; array[rc] != NULL; rc++);
	
	return rc;
}

int smtp_send_mail(struct smtp_server *server, struct out_mail **om)
{
	int rc;
	
	rc = 0;

	thread_call_parent_function_sync(up_set_status,1,"Sending HELO...");
	if(smtp_helo(server))
	{
		long i,amm;
		
		rc = 1;
		amm = get_amm(om);
		thread_call_parent_function_sync(up_init_gauge_mail,1,amm);
		
		for(i = 0; i < amm; i++)   
		{
			thread_call_parent_function_sync(up_set_gauge_mail,1,i+1);
			
			thread_call_parent_function_sync(up_set_status,1,"Sending FROM...");
			if(smtp_from(server, om[i]->from))
			{
				thread_call_parent_function_sync(up_set_status,1,"Sending RCP...");
				if(smtp_rcpt(server, om[i]))
				{
					thread_call_parent_function_sync(up_set_status,1,"Sending DATA...");
					if(!smtp_data(server, om[i]->mailfile))
					{
						tell_from_subtask("DATA failed.");
						rc = 0;
						break;
					}
				}
				else
				{
					tell_from_subtask("RCPT failed.");
					rc = 0;
					break;
				}
			}
			else
			{
				tell_from_subtask("FROM failed.");
				rc = 0;
				break;
			}

			if (rc)
			{
				/* no error while mail sending, so it can be moved to the
				 * "Sent" folder now */
				thread_call_parent_function_sync(callback_mail_has_been_sent,1,om[i]->mailfile);
			}
		}
		
		if(rc == 1)
		{
			thread_call_parent_function_sync(up_set_status,1,"Sending QUIT...");
			if(smtp_quit(server))
			{
				rc = 1;
			}
		}  
	}
	else
	{
		tell_from_subtask("HELO failed.");
	}

	return rc;
}

int esmtp_ehlo(struct smtp_server *server)
{
	int rc;
	char dom[513];
	int ok, running;
	char buf[1025], *rbuf;

	rc = 0;

	server->esmtp.flags = 0;
	server->esmtp.auth_flags  = 0;

	if(server->ip_as_domain)
	{
		ok = (gethostname(dom, 512) == 0);
	}
	else
	{
		strcpy(dom, server->domain);
		ok = 1;
	}

	if(ok)
	{
		if(smtp_send_cmd(server, NULL, NULL) == SMTP_SERVICE_READY)
		{
			sprintf(buf, "EHLO %s\r\n", dom);
			tcp_write(server->socket, buf, strlen(buf));

			do
			{
				rbuf = tcp_readln(server->socket);

				if(strstr(rbuf, "ENHANCEDSTATUSCODES"))
				{
					server->esmtp.flags |= ESMTP_ENHACEDSTATUSCODES;
				}
				else if(strstr(rbuf, "8BITMIME"))
				{
					server->esmtp.flags |= ESMTP_8BITMIME;
				}
				else if(strstr(rbuf, "ONEX"))
				{
					server->esmtp.flags |= ESMTP_ONEX;
				}
				else if(strstr(rbuf, "ETRN"))
				{
					server->esmtp.flags |= ESMTP_ETRN;
				}
				else if(strstr(rbuf, "XUSR"))
				{
					server->esmtp.flags |= ESMTP_XUSR;
				}
				else if(strstr(rbuf, "AUTH"))
				{
					server->esmtp.flags |= ESMTP_AUTH;

					if(strstr(rbuf, "PLAIN"))
					{
						server->esmtp.auth_flags |= AUTH_PLAIN;
					}
					if(strstr(rbuf, "LOGIN"))
					{
						server->esmtp.auth_flags |= AUTH_LOGIN;
					}
					if(strstr(rbuf, "DIGEST-MD5"))
					{
						server->esmtp.auth_flags |= AUTH_DIGEST_MD5;
					}
					if(strstr(rbuf, "CRAM-MD5"))
					{
						server->esmtp.auth_flags |= AUTH_CRAM_MD5;
					}
				}

				running = (rbuf[3] != ' ');

				if(running == 0)
				{
					rc = (atol(rbuf) == SMTP_OK);
				}

				free(rbuf);
			}
			while(running);
		}
		else
		{
			tell_from_subtask("Service not ready.");
		}
	}

	return rc;
}

static int esmtp_auth_cram(struct smtp_server *server)
{
	static char cram_str[] = "AUTH CRAM-MD5\r\n";
	char *line;
	int rc;
	char *challenge;
	unsigned int challenge_len;
	unsigned long digest[4]; /* 16 chars */
	char buf[512];
	char *encoded_str;

	tcp_write(server->socket, cram_str,sizeof(cram_str)-1);

	if (!(line = tcp_readln(server->socket))) return 0;
	rc = atoi(line);
	if (rc != 334)
	{
		free(line);
		return 0;
	}

	if (!(challenge = decode_base64(line+4,strlen(line+4),&challenge_len)))
	{
		free(line);
		return 0;
	}

	free(line);

	hmac_md5(challenge,strlen(challenge),server->esmtp.auth_password,strlen(server->esmtp.auth_password),(char*)digest);
	free(challenge);
	sprintf(buf,"%s %08lx%08lx%08lx%08lx%c%c",server->esmtp.auth_login,
					digest[0],digest[1],digest[2],digest[3],0,0); /* I don't know if the two nullbytes should be counted as well */

	encoded_str = encode_base64(buf,strlen(buf));
	if (!encoded_str) return 0;
	tcp_write(server->socket,encoded_str,strlen(encoded_str)-1); /* -1 because of the linefeed */
	tcp_write(server->socket,"\r\n",2);
	free(encoded_str);

	if (!(line = tcp_readln(server->socket))) return 0;
	rc = atoi(line);
	free(line);
	if (rc != 235)
	{
		tell_from_subtask("SMTP AUTH CRAM failed");
	} else return 1;
	return 0;
}

static int esmtp_auth_digest_md5(struct smtp_server *server)
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

	tcp_write(server->socket, digest_str,sizeof(digest_str)-1);

	if (!(line = tcp_readln(server->socket))) return 0;
	rc = atoi(line);
	if (rc != 334)
	{
		free(line);
		return 0;
	}

	if (!(challenge = decode_base64(line+4,strlen(line+4),&challenge_len)))
	{
		free(line);
		return 0;
	}

	strcpy(buf,challenge);
	strcpy(buf+challenge_len,server->esmtp.auth_password);

	free(line);
	free(challenge);
  
	MD5Init(&context);
	MD5Update(&context, buf, strlen(buf));
	MD5Final((char*)digest, &context);

	sprintf(buf,"%s %08lx%08lx%08lx%08lx%c%c",server->esmtp.auth_login,
					digest[0],digest[1],digest[2],digest[3],0,0); /* the same as above */

	encoded_str = encode_base64(buf,strlen(buf));
	if (!encoded_str) return 0;

	tcp_write(server->socket,encoded_str,strlen(encoded_str)-1); /* -1 because of the line feed */
	tcp_write(server->socket,"\r\n",2);
	free(encoded_str);

	if (!(line = tcp_readln(server->socket))) return 0;
	rc = atoi(line);
	if (rc != 235)
	{
		tell_from_subtask("SMTP AUTH DIGEST-MD5 failed");
	} else return 1;
	return 0;
}



int esmtp_auth(struct smtp_server *server)
{
	int rc;
	char *buf, prep[1024];

	rc = 0;

  if(server->esmtp.auth_flags & AUTH_CRAM_MD5)
	{
		rc = esmtp_auth_cram(server);
	}
/*	else if(server->esmtp.auth_flags & AUTH_DIGEST_MD5)
	{
		rc = esmtp_auth_digest_md5(server);
	}*/
	else if(server->esmtp.auth_flags & AUTH_LOGIN)
	{
		if(smtp_send_cmd(server, "AUTH", "LOGIN") == 334)
		{
			strcpy(prep, server->esmtp.auth_login);

			buf = encode_base64(prep, strlen(prep));
			buf[strlen(buf) - 1] = 0;
			
			if(smtp_send_cmd(server, buf, NULL) == 334)
			{
				free(buf);

				strcpy(prep, server->esmtp.auth_password);

				buf = encode_base64(prep, strlen(prep));
				buf[strlen(buf) - 1] = 0;

				if(smtp_send_cmd(server, buf, NULL) == 235)
				{
					rc = 1;
				}
			}
		}
	}
	else if(server->esmtp.auth_flags & AUTH_PLAIN)
	{
		if(smtp_send_cmd(server, "AUTH", "PLAIN") == 334)
		{
			prep[0]=0;
			strcpy(prep + 1, server->esmtp.auth_login);
			strcpy(prep + 1 + strlen(server->esmtp.auth_login) + 1, server->esmtp.auth_password);
			
			buf = encode_base64(prep, strlen(server->esmtp.auth_login) + strlen(server->esmtp.auth_password) + 2);
			buf[strlen(buf) - 1] = 0;
			if(smtp_send_cmd(server, buf, NULL) == 235)
			{
				rc = 1;
			}
			free(buf);
		}
	}

	return rc;
}

int esmtp_send_mail(struct smtp_server *server, struct out_mail **om)
{
	int rc;
	
	rc = 0;

	thread_call_parent_function_sync(up_set_status,1,"Sending EHLO...");
	if(esmtp_ehlo(server))
	{
		long i,amm;

		thread_call_parent_function_sync(up_set_status,1,"Sending AUTH...");
		if(esmtp_auth(server))
		{
			rc = 1;
			amm = get_amm(om);
			thread_call_parent_function_sync(up_init_gauge_mail,1,amm);
				
			for(i = 0; i < amm; i++)
			{
				thread_call_parent_function_sync(up_set_gauge_mail,1,i+1);

				thread_call_parent_function_sync(up_set_status,1,"Sending FROM...");
				if(smtp_from(server, om[i]->from))
				{
					thread_call_parent_function_sync(up_set_status,1,"Sending RCP...");
					if(smtp_rcpt(server, om[i]))
					{
						thread_call_parent_function_sync(up_set_status,1,"Sending DATA...");
						if(!smtp_data(server, om[i]->mailfile))
						{
							tell_from_subtask("DATA failed");
							rc = 0;
							break;
						}
					}
					else
					{
						tell_from_subtask("RCPT failed");
						rc = 0;
						break;
					}
				}
				else
				{
					tell_from_subtask("FROM failed.");
					rc = 0;
					break;
				}

				if (rc)
				{
					/* no error while mail sending, so it can be moved to the
					 * "Sent" folder now */
					thread_call_parent_function_sync(callback_mail_has_been_sent,1,om[i]->mailfile);
				}
			}

			if(rc == 1)
			{
				thread_call_parent_function_sync(up_set_status,1,"Sending QUIT...");
				if(smtp_quit(server))
				{
					rc = 1;
				}
			}
		}
		else
		{
			tell_from_subtask("AUTH failed.");
		}
	}
	else
	{
		tell_from_subtask("EHLO failed.");
	}

	return rc;
}


static int smtp_send_really(struct smtp_server *server)
{
	int rc = 0;
	struct out_mail **om = server->out_mail;

	if (open_socket_lib())
	{
		thread_call_parent_function_sync(up_set_status,1,"Connecting...");

		server->socket = tcp_connect(server->name, server->port);
		if (server->socket != SMTP_NO_SOCKET)
		{
				if(server->esmtp.auth)
				{
					rc = esmtp_send_mail(server, om);
				}
				else
				{
					rc = smtp_send_mail(server, om);
				}


			thread_call_parent_function_sync(up_set_status,1,"Disconnecting...");
			tcp_disconnect(server->socket);
			server->socket = SMTP_NO_SOCKET;
		}
		else
		{
			close_socket_lib();
		}
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
	int amm = get_amm(om);
	struct out_mail **newom = create_outmail_array(amm);
	if (newom)
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

static int smtp_entry(struct smtp_server *server)
{
	struct smtp_server copy_server;

	copy_server.name          			= mystrdup(server->name);
	copy_server.domain					= mystrdup(server->domain);
	copy_server.port          			= server->port;
	copy_server.socket        			= server->socket;
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

int smtp_send(struct smtp_server *server)
{
	return thread_start(smtp_entry,server);
}

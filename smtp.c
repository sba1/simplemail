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

#include <proto/socket.h>
#include <proto/exec.h>

#include "io.h"
#include "mail.h"
#include "tcp.h"
#include "simplemail.h"
#include "support.h"

#include "upwnd.h"

#include "smtp.h"

int buf_flush(long hsocket, char *buf, long len)
{
	int rc;
	long sent;
   
	rc = FALSE;
   
	sent = send(hsocket, buf, len, 0);
	if(sent == len)
	{
		rc = TRUE;
		if(!rc)
		{
			puts("flushing failed");
		}
		
	}
   
	return(rc);
}

__inline static int buf_cat(long hsocket, char *buf, char c)
{
	int rc;
	static len = 0;
	static CR = FALSE;
   
	rc = FALSE;
   
	if(c == '\n')
	{
		if(!CR)
		{
			buf[len++] = '\r';
			CR = FALSE;
		}	
		buf[len++] = '\n';
		rc  = buf_flush(hsocket, buf, len);
		len = 0;
		buf[0]=0;
	}
	else
	{
		if(c == '\r')
		{
			CR = TRUE;
		}
		
		buf[len++] = c;
		buf[len] = 0;
		if(len == 77)
		{
			len = 0;
			buf[0] = 0;
			puts("panic: line with more than 76 chars detected!");
		}
		else
		{
			rc = TRUE;
		}	
	}
   
	return(rc);
}

char *buf_init(void)
{
	char *rc;
   
	rc = malloc(100);
	rc[0] = 0;
   
	return(rc);
}

void buf_free(char *buf)
{
	free(buf);
}

int smtp_send_cmd(long hsocket, char *cmd, char *args)
{
	int rc;
	char *buf;
	long count;
   
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
		count = send(hsocket, buf, strlen(buf), 0);
		
		if(count != strlen(buf))
		{
			return(rc);
		}
	}
	
	count = recv(hsocket, buf, 1023, 0);
	if(count != -1)
	{
		buf[count] = 0;
		rc = atol(buf);
	}	
   
	return(rc);
}

int smtp_helo(long hsocket, char *domain)
{
	int rc;

	rc = FALSE;
	if(smtp_send_cmd(hsocket, NULL, NULL) == SMTP_SERVICE_READY)
	{
		if(smtp_send_cmd(hsocket, "HELO", domain) == SMTP_OK)
		{
			rc = TRUE;
		}
	}
	else
	{
		puts("service not ready");
	}

	return(rc);
}

int smtp_from(long hsocket, char *from)
{
	int rc;
	char *buf;

	rc = FALSE;
	buf = malloc(1024);

	if(buf != NULL)
	{
		sprintf(buf, "FROM:<%s>", from);

		if(smtp_send_cmd(hsocket, "MAIL", buf) == SMTP_OK)
		{
			rc = TRUE;
		}

		free(buf);
	}

	return(rc);
}

int smtp_rcpt(long hsocket, struct out_mail *om)
{
	int rc;
	long i;
	char *buf;

	rc = FALSE;
	
	buf = malloc(1024);
	if(buf != NULL)
	{
		rc = TRUE;	
		
		for(i = 0; om->rcp[i] != NULL; i++)
		{
			sprintf(buf, "TO:<%s>", om->rcp[i]);
			if(smtp_send_cmd(hsocket, "RCPT", buf) != SMTP_OK)
			{
				rc = FALSE;
				break;
			}
		}
		
		free(buf);
	}

   return(rc);

}

int smtp_data(long hsocket, char *mailfile)
{
	int rc;
	long ret;
	char *buf;
	FILE *fp;
	int c;
	long size;

	rc = FALSE;
	
	buf = buf_init();
	if(buf != NULL)
	{
		fp = fopen(mailfile, "r");
		if(fp)
		{
			
			fseek(fp, 0L, SEEK_END);
			size = ftell(fp); /* what's that?? */
			fseek(fp, 0L, SEEK_SET);
			
			ret = smtp_send_cmd(hsocket, "DATA", NULL);
			if((ret == SMTP_OK) || (ret == SMTP_SEND_MAIL))
			{
				rc = TRUE;
				
				while((c = fgetc(fp)) != EOF)
				{
					if(buf_cat(hsocket, buf, c) == FALSE)
					{
						rc = FALSE;
						break;
					}
				}
				buf_flush(hsocket, buf, strlen(buf));
				if(smtp_send_cmd(hsocket, "\r\n.\n", NULL) != SMTP_OK)
				{
					rc = FALSE;
				}	
			}
			else
			{
				puts("cmd failed");
			}
			
			fclose(fp);
		}

		buf_free(buf);
	}	

	return(rc);
}

int smtp_quit(long hsocket)
{
	int rc;

	rc = (smtp_send_cmd(hsocket, "QUIT", NULL) == SMTP_OK);

	return(rc);
}

int smtp_send_mail(long hsocket, struct out_mail *om)
{
	int rc;
   
	rc = FALSE;

	if(smtp_helo(hsocket, om->domain))
	{
		if(smtp_from(hsocket, om->from))
		{
			if(smtp_rcpt(hsocket, om))
			{
				if(smtp_data(hsocket, om->mailfile))
				{
					if(smtp_quit(hsocket))
					{
						rc = TRUE;
					}
				}
				else
				{
					puts("data failed");
				}
			}
			else
			{
				puts("rcpt failed");
			}
		}
		else
		{
			puts("from failed");
		}
	}
	else
	{
		puts("helo failed");
	}

	return(rc);
}

int smtp_send(char *server, struct out_mail **om)
{
	int rc;
	long hsocket;

	rc = FALSE;
   
	SocketBase = OpenLibrary("bsdsocket.library", 4);  
	if(SocketBase != NULL)  
	{
		hsocket = tcp_connect(server, 25);
		if(hsocket != SMTP_NO_SOCKET)
		{
			long i;
			
			for(i = 0; om[i] != NULL; i++)
			{
				smtp_send_mail(hsocket, om[i]);
			}
			
			rc = TRUE;
         
			CloseSocket(hsocket);
		}
		else
		{
			puts("cannot open server");
		}

		CloseLibrary(SocketBase);
	}  
	else
	{
		puts("cannot open lib");
	}
   
	return(rc);
}
/*
void
main(void)
{
	struct out_mail *om;

	om = malloc(sizeof(struct out_mail));
	om->domain = malloc(256);
	strcpy(om->domain, "t-online.de");
	om->from = malloc(256);
	strcpy(om->from, "hynek.schlawack@t-online.de");
	om->rcp = malloc(3 * sizeof(char *));
	om->rcp[0] = malloc(1024);
	strcpy(om->rcp[0], "test@hys.in-berlin.de");
	om->rcp[1] = NULL;
	om->mailfile = malloc(256);
	strcpy(om->mailfile, "test.mail");

	smtp_send("mailto.t-online.de", om);
}
*/
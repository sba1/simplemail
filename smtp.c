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

#include "io.h"
#include "mail.h"
#include "tcp.h"
#include "simplemail.h"
#include "smtp.h"
#include "support.h"

#include "subthreads.h"
#include "tcpip.h"
#include "upwnd.h"

int buf_flush(struct smtp_server *server, char *buf, long len)
{
   int rc;
   long sent;
   
   rc = FALSE;
   
   sent = send(server->socket, buf, len, 0);
   if(sent == len)
   {
      rc = TRUE;
      if(!rc)
      {
         tell("flushing failed");
      }
      
   }
   
   return(rc);
}

__inline static int buf_cat(struct smtp_server *server, char *buf, char c)
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
      rc  = buf_flush(server, buf, len);
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
         tell("panic: line with more than 76 chars detected!");
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

int smtp_send_cmd(struct smtp_server *server, char *cmd, char *args)
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
      count = send(server->socket, buf, strlen(buf), 0);
      
      if(count != strlen(buf))
      {
         return(rc);
      }
   }
   
   count = recv(server->socket, buf, 1023, 0);
   if(count != -1)
   {
      buf[count] = 0;
      rc = atol(buf);
   }  
   
   return(rc);
}

int smtp_helo(struct smtp_server *server, char *domain)
{
   int rc;

   rc = FALSE;
   if(smtp_send_cmd(server, NULL, NULL) == SMTP_SERVICE_READY)
   {
      if(smtp_send_cmd(server, "HELO", domain) == SMTP_OK)
      {
         rc = TRUE;
      }
   }
   else
   {
      tell("service not ready");
   }

   return(rc);
}

int smtp_from(struct smtp_server *server, char *from)
{
   int rc;
   char *buf;

   rc = FALSE;
   buf = malloc(1024);

   if(buf != NULL)
   {
      sprintf(buf, "FROM:<%s>", from);

      if(smtp_send_cmd(server, "MAIL", buf) == SMTP_OK)
      {
         rc = TRUE;
      }

      free(buf);
   }

   return(rc);
}

int smtp_rcpt(struct smtp_server *server, struct out_mail *om)
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
         if(smtp_send_cmd(server, "RCPT", buf) != SMTP_OK)
         {
            rc = FALSE;
            break;
         }
      }
      
      free(buf);
   }

   return(rc);

}

int smtp_data(struct smtp_server *server, char *mailfile)
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
         size = ftell(fp); /* what's that?? */ /* look into your ANSI-C manual :) */ /* now it's ok :-) */
         thread_call_parent_function_sync(up_init_gauge_byte,1,size);
         fseek(fp, 0L, SEEK_SET);
         
         ret = smtp_send_cmd(server, "DATA", NULL);
         if((ret == SMTP_OK) || (ret == SMTP_SEND_MAIL))
         {
            long i;
            long z;
            
            i = 0;
            rc = TRUE;
            z = ((z = size / 100) >1)?z:1;
            
            while((c = fgetc(fp)) != EOF)
            {
               if(buf_cat(server, buf, c) == FALSE)
               {
                  rc = FALSE;
                  break;
               }
               if((i++%z) == 0)
               {  
                  thread_call_parent_function_sync(up_set_gauge_byte,1,i);
                  if(thread_call_parent_function_sync(up_checkabort,0))
                  {
                     tell("aborted");
                     rc = FALSE;
                     break;
                  }
               }  
            }
            if(rc == TRUE)
            {
               buf_flush(server, buf, strlen(buf));
               if(smtp_send_cmd(server, "\r\n.\n", NULL) != SMTP_OK)
               {
                  rc = FALSE;
               }
            }  
         }
         else
         {
            tell("cmd failed");
         }
         
         fclose(fp);
      }

      buf_free(buf);
   }  

   return(rc);
}

int smtp_quit(struct smtp_server *server)
{
   int rc;

   rc = (smtp_send_cmd(server, "QUIT", NULL) == SMTP_OK);

   return(rc);
}

static long get_amm(struct out_mail **array)
{
   long rc;
   
   for(rc = 0; array[rc] != NULL; rc++);
   
   return(rc);
}

int smtp_send_mail(struct smtp_server *server, struct out_mail **om)
{
   int rc;
   
   rc = FALSE;

   thread_call_parent_function_sync(up_set_status,1,"Sending HELO...");
   if(smtp_helo(server, om[0]->domain))
   {
      long i,amm;
      
      rc = TRUE;
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
                  tell("data failed");
                  rc = FALSE;
                  break;
               }
            }
            else
            {
               tell("rcpt failed");
               rc = FALSE;
               break;
            }
         }
         else
         {
            tell("from failed");
            rc = FALSE;
            break;
         }

         if (rc)
         {
            /* no error while mail sending, so it can be moved to the
             * "Sent" folder now */
            thread_call_parent_function_sync(callback_mail_has_been_sent,1,om[i]->mailfile);
         }
      }
      
      if(rc == TRUE)
      {
         thread_call_parent_function_sync(up_set_status,1,"Sending QUIT...");
         if(smtp_quit(server))
         {
            rc = TRUE;
         }
      }  
   }
   else
   {
      tell("helo failed");
   }

   return(rc);
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
			rc = smtp_send_mail(server, om);

			thread_call_parent_function_sync(up_set_status,1,"Disconnecting...");
			CloseSocket(server->socket);
			server->socket = SMTP_NO_SOCKET;
		} else tell("cannot open server");
		close_socket_lib();
	} else tell("cannot open lib");
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
			newom[i]->domain = mystrdup(om[i]->domain);
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

	copy_server.name = mystrdup(server->name);
	copy_server.port = server->port;
	copy_server.socket = server->socket;
	copy_server.out_mail = duplicate_outmail_array(server->out_mail);

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

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

#include "tcpip.h"

#include "io.h"
#include "mail.h"
#include "tcp.h"
#include "simplemail.h"
#include "support.h"

#include "upwnd.h"

#include "smtp.h"

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
         up_init_gauge_byte(size);
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
                  up_set_gauge_byte(i);
                  if(up_checkabort())
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

long get_amm(long *array)
{
   long rc;
   
   for(rc = 0; array[rc] != NULL; rc++);
   
   return(rc);
}

int smtp_send_mail(struct smtp_server *server, struct out_mail **om)
{
   int rc;
   
   rc = FALSE;

   up_set_status("Sending HELO...");
   if(smtp_helo(server, om[0]->domain))
   {
      long i,amm;
      
      rc = TRUE;
      amm = get_amm((long *) om);
      up_init_gauge_mail(amm);
      
      for(i = 0; i < amm; i++)   
      {
         up_set_gauge_mail(i+1);
         
         up_set_status("Sending FROM...");
         if(smtp_from(server, om[i]->from))
         {
            up_set_status("Sending RCP...");
            if(smtp_rcpt(server, om[i]))
            {
               up_set_status("Sending DATA...");
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
            callback_mail_has_been_sent(om[i]->mailfile);
         }
      }
      
      if(rc == TRUE)
      {
         up_set_status("Sending QUIT...");
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

int smtp_send(struct smtp_server *server, struct out_mail **om)
{
   int rc;
   
   rc = FALSE;
   
   if(open_socket_lib())
   {
      up_window_open();
      up_set_title(server->name);
      up_set_status("Connecting...");
      
      server->socket = tcp_connect(server->name, server->port);
      if(server->socket != SMTP_NO_SOCKET)
      {
         rc = smtp_send_mail(server, om);
         
         up_set_status("Disconnecting...");
         CloseSocket(server->socket);
         server->socket = SMTP_NO_SOCKET;
      }
      else
      {
         tell("cannot open server");
      }
      
      up_window_close();

      close_socket_lib();
   }  
   else
   {
      tell("cannot open lib");
   }
   
   return(rc);
}

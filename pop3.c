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

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "io.h"
#include "mail.h"
#include "tcp.h"
#include "simplemail.h"
#include "support.h"

#include "dlwnd.h"
#include "mainwnd.h"
#include "tcpip.h"

#include "pop3.h"

#define REC_BUFFER_SIZE 512

/*
** Wait for the welcome message.
*/
int pop3_wait_login(struct pop3_server *server)
{
   int rc;
   char *buf;
   int got;
   
   rc = 0;
   if(server->socket != SMTP_NO_SOCKET)
   {
      buf = malloc(1024);
      if(buf != NULL)
      {
         got = recv(server->socket, buf, 1023, 0);
         if(got != 0)
         {
            buf[got] = 0;
         
            if(strncmp(buf, "+OK", 3) == 0)
            {
               /* Get rid of a eventually longer welcome message  */ 
               while(strstr(buf, "\r\n") == NULL)
               {
                  recv(server->socket, buf, 1023, 0);
               }
               
               rc = 1;
            }
            else
            {
               tell(buf);
            }
         }
         else
         {
            tell("Error receiving data from host!");
         }
      }
      else
      {
         tell("Not enough memory!");
      }
   }  
   else
   {
      tell("Not connected!");
   }
   
   return(rc);
}

/*
** Log into the pop3 server.
*/
int pop3_login(struct pop3_server *server)
{
   int rc;
   char *buf;
   int got;
   long len;
   
   rc = 0;
   
   len = 4096;
   buf = malloc(len);   
   if(buf != NULL)
   {
      dl_set_status("Sending username...");
   
      sprintf(buf, "USER %s\r\n", server->login);
      if(send(server->socket, buf, strlen(buf), 0) != -1)
      {
         got = recv(server->socket, buf, 4095, 0);
         if(got != 0)
         {
            buf[got] = 0;
            
            if(strncmp(buf, "+OK", 3) == 0)
            {
               dl_set_status("Sending password...");
               
               sprintf(buf, "PASS %s\r\n", server->passwd);
               if(send(server->socket, buf, strlen(buf), 0) != -1)
               {
                  got = recv(server->socket, buf, 4095, 0);
                  if(got != 0)
                  {
                     buf[got] = 0;
            
                     if(strncmp(buf, "+OK", 3) == 0)
                     {
                        dl_set_status("Login successful!");
                        rc = 1;
                     }
                     else
                     {
                        tell(buf);
                     }
                  }  
                  else
                  {
                     tell("Reveiving failed.");
                  }
               }  
               else
               {
                  tell("Sending failed.");
               }
            }
            else
            {
               tell(buf);
            }
         }
         else
         {
            tell("Receiving failed!");
         }
      }  
      else
      {
         tell("Sending failed!");
      }
      
      free(buf);
   }
   else
   {
      tell("Not enough memory!");
   }
   
   
   return(rc);
}

/*
** Get statistics about pop3-folder contents.
*/
int pop3_stat(struct pop3_server *server)
{
   int rc;
   char *buf;
   int got;

   rc = 0;
   
   buf = malloc(1024);
   if(buf != NULL)
   {
      dl_set_status("Getting statistics...");
      if(send(server->socket, "STAT\r\n", 6, 0) != -1)
      {
         got = recv(server->socket, buf, 1023, 0);
         if(got != 0)
         {
            buf[got] = 0;
            
            if(strncmp("+OK ", buf, 4) == 0)
            {
               char *str;
               
               str = buf;
               str += 4;
               rc = atol(str);
            }
            else
            {
               tell(buf);
            }
         }
         else
         {
            tell("Receiving failed!");
         }
      }
      else
      {
         tell("Sending failed!");
      }
      free(buf);
   }
   else
   {
      tell("Not enough memory!");
   }
   
   return(rc);
}

/*
** Regulary quit server.
*/
int pop3_quit(struct pop3_server *server)
{
   int rc;
   int got;
   char *buf;
   rc = 0;
   
   buf = malloc(1024);
   if(buf != NULL)
   {
      dl_set_status("Logging out...");
      if(send(server->socket, "QUIT\r\n", 6, 0) != -1)
      {
         got = recv(server->socket, buf, 1023, 0);
         if(got != 0)
         {
            buf[got] = 0;
            if(strncmp(buf, "+OK", 3) == 0)
            {
               rc = 1;
            }
            else
            {
               tell(buf);
            }
         }
         else
         {
            tell("Receiving failed!");
         }
      }
      else
      {
         tell("Sending failed!");
      }
      free(buf);
   }
   else
   {
      tell("Not enough memory!");
   }
   
   return(rc);
}

/*
** Retrieve mail.
*/
int pop3_get_mail(struct pop3_server *server, unsigned long nr)
{
   int rc;
   char *buf, *fn;
   int got;
   unsigned long size;
   FILE *fp;
   
   rc = FALSE;
   
   fn = mail_get_new_name();
   if(fn != NULL)
   {
      fp = fopen(fn, "w");
      if(fp != NULL)
      {
   
         buf = malloc(REC_BUFFER_SIZE+1);
         if(buf != NULL)
         {
            sprintf(buf, "LIST %ld\r\n", nr);
            send(server->socket, buf, strlen(buf), 0);
            got = recv(server->socket, buf, REC_BUFFER_SIZE, 0);
            if(got != 0)
            {
               if(buf[0] == '+')
               {
                  char *str;
            
                  str = buf;
                  str += 4;
                  
                  while(isdigit(*str++));
                  
                  size = atol(str);
                  dl_init_gauge_byte(size);

                  sprintf(buf, "RETR %ld\r\n", nr);
                  send(server->socket, buf, strlen(buf), 0);
                  got = recv(server->socket, buf, REC_BUFFER_SIZE, 0);
                  if(got != 0)
                  {
                     if(buf[0] == '+')
                     {
                        unsigned long i=0;
                        int running = TRUE;
                        struct mail *mail = mail_create();

                        if (mail)
                        {
                           if (mail_set_stuff(mail,fn,size))
                           {
                              struct mail_scan ms;
                              int scan_more = 1;

                              mail_scan_buffer_start(&ms,mail);
                              buf[got] = 0;
                        
                              str = strstr(buf, "\r\n");
                              str += 2;
                              if(strlen(str) > 0)
                              {
                                 fwrite(str, strlen(str), 1, fp);

                                 /* scan the headers */
                                 scan_more = mail_scan_buffer(&ms,buf,strlen(buf));
                              }  
                                 
                              rc = TRUE;

                              do
                              {
                                 if(dl_checkabort())
                                 {
                                    tell("Aborted");
                                    rc = FALSE;
                                    break;
                                 }
                                 
                                 got = recv(server->socket, buf, REC_BUFFER_SIZE, 0);
                                 if(got != 0)
                                 {
                                    i += got;
                                    dl_set_gauge_byte(i);

                                    buf[got] = 0;
                                    str = strstr(buf, "\r\n.\r\n");
                                    if(str != NULL)
                                    {
                                       str[2] = 0;
                                       running = FALSE;
                                    }

                                    fwrite(buf, strlen(buf), 1, fp);

                                    /* scan the headers now */
                                    if (scan_more)
                                    {
                                       scan_more = mail_scan_buffer(&ms,buf,strlen(buf));
                                    }
                                 }
                                 else
                                 {
                                    running = FALSE;
                                 }
                              }
                              while(running);
                              
                              if(rc != FALSE)
                              {
                                 mail_scan_buffer_end(&ms);
                                 mail_process_headers(mail);
                                 callback_new_mail_arrived(mail);
                              }  
                           }
                        }
                        
                        if(rc != FALSE)
                        {
                           dl_set_gauge_byte(size);
                           fclose(fp);
                           fp = NULL;
                           
                           if(Errno())
                           {
                              tell("Error retrieving mail!");
                              rc = FALSE;
                           }
                        }  
                     }
                     else
                     {
                        tell(buf);
                     }
                  }
                  else
                  {
                     tell("Receiving failed!");
                  }
               
               }
               else
               {
                  tell(buf);
               }
            }
            else
            {
               tell("Receiving failed!");
            }
   
            free(buf);
         }
         else
         {
            tell("Not enough memory!");
         }
         
         if(fp != NULL)
         {
            fclose(fp);
         }  
      }
      else
      {
         tell("Can\'t open mail file!");
      }

      free(fn);
   }
   else
   {
      tell("Can\'t get new filename!");
   }
   
   return(rc);
}

/*
** Output some infos (just a test)
*/
static void pop3_output_infos(struct mail *mail)
{
   printf("From: %s\n", mail_find_header_contents(mail, "from"));
   printf("To: %s\n", mail_find_header_contents(mail, "to"));
   printf("Date: %s\n", mail_find_header_contents(mail, "date"));
   printf("X-Mailer: %s\n", mail_find_header_contents(mail, "x-mailer"));
}

/*
** Get the headers of the specified file.
*/
int pop3_get_top(struct pop3_server *server, unsigned long nr)
{
   char *buf;
   int rc;

   rc = FALSE;

   if ((buf = malloc(REC_BUFFER_SIZE + 1)))
   {
      struct mail *mail;

      if ((mail = mail_create()))
      {
         int got;
         struct mail_scan ms;

         mail_scan_buffer_start(&ms, mail);

         sprintf(buf, "TOP %ld 1\r\n", nr); /* a 0 sends the whole mail :( */
         send(server->socket, buf, strlen(buf), 0);

         got = recv(server->socket, buf, REC_BUFFER_SIZE, 0);
         if(got > 0)
         {
            buf[got] = 0;

            if(strncmp(buf, "+OK", 3) == 0)
            {
               int running = TRUE, more = TRUE;
               char *str,*buf2;

               /* set buf2 to the last line */
               if ((buf2 = strstr(buf,"\r\n")))
               {
                  buf2 += 2;
                  while (running)
                  {
                     if ((str = strstr(buf2, "\r\n.\r\n")))
                     {
                        str[2] = 0;
                        running = FALSE;
                     }

                     if (more)   
                     {
                        /* scan the mail */
                        more = mail_scan_buffer(&ms, buf2, strlen(buf2));
                     }  

                     if (running) got = recv(server->socket, buf, REC_BUFFER_SIZE, 0);
                     if (got > 0) buf[got] = 0;
                     else running = FALSE;

                     /* now use the buf from beginning */
                     buf2 = buf;
                  }
               }
                  
               if(Errno() == 0)
               {
                  rc = TRUE;
               }  
               else
               {
                  tell("Error receiving!");
                  
                  mail_free(mail);
               }
            }
            else
            {
               tell(buf);
            }
         }
         else
         {
            tell("Receiving failed!");
         }
         pop3_output_infos(mail);
         mail_scan_buffer_end(&ms);
         mail_free(mail); /* never acces ms.mail, this structure is private! */
      }
      
      free(buf);
   }
   else
   {
      tell("Not enough memory!");
   }
   
   return(rc);
}

/*
**
*/
void pop3_get_tops(struct pop3_server *server, unsigned long amm)
{
   unsigned long i;
   
   for(i = 1; i <= amm; i++)
   {
      pop3_get_top(server, i);
   }
}

/*
** Mark mail as deleted.
*/
int pop3_del_mail(struct pop3_server *server, unsigned long nr)
{
   int rc;
   
   rc = FALSE;
   
   rc = TRUE;
   
   return(rc);
}

/*
** Download mails.
*/
int pop3_dl(struct pop3_server *server)
{
   int rc;
   int mail_amm;
   
   rc = 0;
   
   if(open_socket_lib())
   {
      dl_set_status("Connecting to server..."); 
      server->socket = tcp_connect(server->name, server->port);
      if(server->socket != SMTP_NO_SOCKET)
      {
         dl_set_status("Waiting for login...");
         if(pop3_wait_login(server))
         {
            if(pop3_login(server))
            {
               mail_amm = pop3_stat(server);
               if(mail_amm != 0)
               {
                  unsigned long i;
                  char path[2048];
                  
                  dl_init_gauge_mail(mail_amm);
                  
                  dl_set_status("Receiving mails...");

                  getcwd(path, 255);
                  sm_makedir("PROGDIR:.folders/income");
                  if(chdir("PROGDIR:.folders/income") == -1)
                  {
                     tell("Can\'t access income-folder!");
                     return(FALSE);
                  }
                  else
                  {
                     pop3_get_tops(server, mail_amm);
                  
                     for(i = 1; i <= mail_amm; i++)
                     {
                        dl_set_gauge_mail(i);
                        if(pop3_get_mail(server, i))
                        {
                           if(!pop3_del_mail(server, i))
                           {
                              tell("Can\'t mark mail as deleted!");
                           }  
                        }
                        else
                        {
                           tell("Can\'t download mail!");
                           break;
                        }
                     }
                     
                     chdir(path);
                  }
               }
            
               pop3_quit(server);
            }
         }  
      }
      close_socket_lib();
   }
   else
   {
      tell("You need a bsd-stack to download e-mails!");
   }
   
   return(rc);
}

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

#include "mail.h"
#include "tcp.h"
#include "simplemail.h"
#include "support.h"

#include "dlwnd.h"
#include "mainwnd.h"
#include "subthreads.h"
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
               tell_from_subtask(buf);
            }
         }
         else
         {
            tell_from_subtask("Error receiving data from host!");
         }
      }
      else
      {
         tell_from_subtask("Not enough memory!");
      }
   }  
   else
   {
      tell_from_subtask("Not connected!");
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
      thread_call_parent_function_sync(dl_set_status,1,"Sending username...");
   
      sprintf(buf, "USER %s\r\n", server->login);
      if(send(server->socket, buf, strlen(buf), 0) != -1)
      {
         got = recv(server->socket, buf, 4095, 0);
         if(got != 0)
         {
            buf[got] = 0;
            
            if(strncmp(buf, "+OK", 3) == 0)
            {
               thread_call_parent_function_sync(dl_set_status,1,"Sending password...");
               
               sprintf(buf, "PASS %s\r\n", server->passwd);
               if(send(server->socket, buf, strlen(buf), 0) != -1)
               {
                  got = recv(server->socket, buf, 4095, 0);
                  if(got != 0)
                  {
                     buf[got] = 0;
            
                     if(strncmp(buf, "+OK", 3) == 0)
                     {
                        thread_call_parent_function_sync(dl_set_status,1,"Login successful!");
                        rc = 1;
                     }
                     else
                     {
                        tell_from_subtask(buf);
                     }
                  }  
                  else
                  {
                     tell_from_subtask("Reveiving failed.");
                  }
               }  
               else
               {
                  tell_from_subtask("Sending failed.");
               }
            }
            else
            {
               tell_from_subtask(buf);
            }
         }
         else
         {
            tell_from_subtask("Receiving failed!");
         }
      }  
      else
      {
         tell_from_subtask("Sending failed!");
      }
      
      free(buf);
   }
   else
   {
      tell_from_subtask("Not enough memory!");
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
      thread_call_parent_function_sync(dl_set_status,1,"Getting statistics...");
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
               tell_from_subtask(buf);
            }
         }
         else
         {
            tell_from_subtask("Receiving failed!");
         }
      }
      else
      {
         tell_from_subtask("Sending failed!");
      }
      free(buf);
   }
   else
   {
      tell_from_subtask("Not enough memory!");
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
      thread_call_parent_function_sync(dl_set_status,1,"Logging out...");
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
               tell_from_subtask(buf);
            }
         }
         else
         {
            tell_from_subtask("Receiving failed!");
         }
      }
      else
      {
         tell_from_subtask("Sending failed!");
      }
      free(buf);
   }
   else
   {
      tell_from_subtask("Not enough memory!");
   }
   
   return(rc);
}

/*
** Retrieve mail.
*/
int pop3_get_mail(struct pop3_server *server, unsigned long nr)
{
	int rc = 0;
	char *buf, *fn;
	int got;
	FILE *fp;

	if (!(fn = mail_get_new_name()))
	{
    tell_from_subtask("Can\'t get new filename!");
    return 0;
	}

	if (!(buf = malloc(REC_BUFFER_SIZE+5)))
	{
		tell_from_subtask("Not enough memory!");
		free(fn);
		return 0;
	}

	if (!(fp = fopen(fn, "w")))
	{
		tell_from_subtask("Can\'t open mail file!");
		free(buf);
		free(fn);
		return 0;
	}

	sprintf(buf, "LIST %ld\r\n", nr);
	send(server->socket, buf, strlen(buf), 0);

	if ((got = recv(server->socket, buf, REC_BUFFER_SIZE, 0)) > 0)
	{
		if(buf[0] == '+')
		{
			char *str = buf + 4;
			int size;

			while(isdigit(*str++)); /* skip the mail number */
                  
			size = atoi(str); /* the size of the mail */
			thread_call_parent_function_sync(dl_init_gauge_byte,1,size);

			/* Now retrieve the mail data */
			sprintf(buf, "RETR %ld\r\n", nr);

			send(server->socket, buf, strlen(buf), 0);
			if ((got = recv(server->socket, buf, REC_BUFFER_SIZE, 0))>0)
			{
				if (buf[0] == '+')
				{
					char *buf2,*str;
					int running = 1;

					buf[got] = 0;

					if ((buf2 = strstr(buf,"\r\n")))
					{
						int bytes_written = 0;

						buf2 += 2;
						got = strlen(buf2);

						rc = 1;

						while (running)
						{
							/* Check if the downloading should be aborted */
							if(thread_call_parent_function_sync(dl_checkabort,0))
							{
								rc = 0;
								break;
							}

							/* possible problem: the following strstr could fail if the 5 chars are splitted */
							if ((str = strstr(buf, "\r\n.\r\n")))
							{
								str[2] = 0;
								running = 0;
							}

							fwrite(buf2, strlen(buf2), 1, fp);
							bytes_written += strlen(buf2);
							thread_call_parent_function_sync(dl_set_gauge_byte,1,bytes_written);

							if (running)
							{
								/* copy the last 4 characters to catch the end */
								buf[0] = buf2[got-4];
								buf[1] = buf2[got-3];
								buf[2] = buf2[got-2];
								buf[3] = buf2[got-1];

								got = recv(server->socket, buf+4, REC_BUFFER_SIZE, 0);
								if (got > 0)
								{
									/* now use the buf from beginning of the new data */
									buf2 = buf + 4;
									buf2[got] = 0;
								}
                else running = 0;
							}
						}
					}

					if (rc)
					{
						if (Errno())
						{
							tell_from_subtask("Error retrieving mail!");
							rc = 0;
						}

						thread_call_parent_function_sync(dl_set_gauge_byte,1,size);
						fclose(fp);
						fp = NULL;

						thread_call_parent_function_sync(callback_new_mail_arrived_filename, 1, fn);
					}  
				} else tell_from_subtask(buf);
			} else tell_from_subtask("Receiving failed!");
		} else tell_from_subtask(buf);
	} else tell_from_subtask("Receiving failed!");

	if (fp) fclose(fp);
	free(buf);
	free(fn);

	return rc;
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
                  tell_from_subtask("Error receiving!");
                  
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
            tell_from_subtask("Receiving failed!");
         }
         pop3_output_infos(mail);
         mail_scan_buffer_end(&ms);
         mail_free(mail); /* never acces ms.mail, this structure is private! */
      }
      
      free(buf);
   }
   else
   {
      tell_from_subtask("Not enough memory!");
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
	char *buf;
	unsigned long got;
   
	/* DELE nr */

	rc = FALSE;
   
   	buf = malloc(REC_BUFFER_SIZE);
   	
   	if(buf != NULL)
   	{
		sprintf(buf, "DELE %ld\r\n", nr);
		send(server->socket, buf, strlen(buf), 0);
		got = recv(server->socket, buf, REC_BUFFER_SIZE, 0);
	
		if(got != 0)
		{
			if(buf[0] == '+')
			{
				rc = TRUE;
			}
		}
		
		free(buf);
	}
   
	return(rc);
}

static int pop3_really_dl(struct pop3_server *server)
{
   int rc;
   int mail_amm;
   
   rc = 0;
   
   if(open_socket_lib())
   {
			thread_call_parent_function_sync(dl_set_status,1,"Connecting to server...");

      server->socket = tcp_connect(server->name, server->port);
      if(server->socket != SMTP_NO_SOCKET)
      {
         thread_call_parent_function_sync(dl_set_status,1,"Waiting for login...");
         if(pop3_wait_login(server))
         {
            if(pop3_login(server))
            {
               mail_amm = pop3_stat(server);
               if(mail_amm != 0)
               {
                  unsigned long i;
                  char path[2048];
                  
                  thread_call_parent_function_sync(dl_init_gauge_mail,1,mail_amm);
                  thread_call_parent_function_sync(dl_set_status,1,"Receiving mails...");

                  getcwd(path, 255);
                  sm_makedir("PROGDIR:.folders/income");
                  if(chdir("PROGDIR:.folders/income") == -1)
                  {
                     tell_from_subtask("Can\'t access income-folder!");
                     return(FALSE);
                  }
                  else
                  {
/*                     pop3_get_tops(server, mail_amm);*/
                  
                     for(i = 1; i <= mail_amm; i++)
                     {
                        thread_call_parent_function_sync(dl_set_gauge_mail,1,i);
                        if(pop3_get_mail(server, i))
                        {
                        	
                        	if(server->del)
                        	{
                        		thread_call_parent_function_sync(dl_set_status,1,"Marking mail as deleted...");
                           		if(!pop3_del_mail(server, i))
                           		{
                              		tell_from_subtask("Can\'t mark mail as deleted!");
                           		}
                           	}	
                        }
                        else
                        {
                           tell_from_subtask("Can\'t download mail!");
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
      tell_from_subtask("You need a bsd-stack to download e-mails!");
   }
   
   return(rc);
}

static int pop3_entry(struct pop3_server *server)
{
	struct pop3_server copy_server;

	copy_server.name = mystrdup(server->name);
	copy_server.port = server->port;
	copy_server.login = mystrdup(server->login);
	copy_server.passwd = mystrdup(server->passwd);
	copy_server.socket = server->socket;
	copy_server.del = server->del;

	if (thread_parent_task_can_contiue())
	{
		pop3_really_dl(&copy_server);
		thread_call_parent_function_sync(dl_window_close,0);
	}
	return 0;
}

/*
** Download mails.
*/
int pop3_dl(struct pop3_server *server)
{
	return thread_start(pop3_entry,server);
}

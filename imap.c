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
** imap.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include "folder.h"
#include "lists.h"
#include "simplemail.h"
#include "support_indep.h"
#include "status.h"
#include "tcp.h"

#include "support.h"
#include "tcpip.h"

#include "imap.h"

#ifdef _AMIGA
#undef printf
#endif

#define MIN(a,b) ((a)<(b)?(a):(b))

static int val;

/**************************************************************************
 Writes the next word into the dest buffer but not more than dest_size
**************************************************************************/
static char *imap_get_result(char *src, char *dest, int dest_size)
{
	char c;
	char delim = 0;


	dest[0] = 0;
	if (!src) return NULL;

	while ((c = *src))
	{
		if (!isspace((unsigned char)c))
			break;
		src++;
	}

	if (c)
	{
		int i = 0;

		if (c == '(') delim = ')';
		else if (c== '"') delim = '"';
		if (delim) src++;

		while ((c = *src))
		{
			if (c == delim)
			{
				src++;
				break;
			}

			if (!delim)
			{
				if (isspace((unsigned char)c)) break;
			}

			dest[i++] = c;
			src++;
		}
		dest[i] = 0;
		return src;
	}
	return NULL;
}

/**************************************************************************
 
**************************************************************************/
static void imap_really_dl(struct list *imap_list, int called_by_auto)
{
}

struct imap_entry_msg
{
	struct list *imap_list;
	int called_by_auto;
};

/**************************************************************************
 Entrypoint for the fetch mail process
**************************************************************************/
static int imap_entry(struct imap_entry_msg *msg)
{
	struct list imap_list;
	struct imap_server *imap;
	int called_by_auto = msg->called_by_auto;

	list_init(&imap_list);
	imap = (struct imap_server*)list_first(msg->imap_list);

	while (imap)
	{
		if (imap->name)
		{
			struct imap_server *new_imap = imap_duplicate(imap);
			if (new_imap)
				list_insert_tail(&imap_list,&new_imap->node);
		}
		imap = (struct imap_server*)node_next(&imap->node);
	}

	if (thread_parent_task_can_contiue())
	{
		thread_call_parent_function_async(status_init,1,0);
		if (called_by_auto) thread_call_parent_function_async(status_open_notactivated,0);
		else thread_call_parent_function_async(status_open,0);

		imap_really_dl(&imap_list, called_by_auto);

		thread_call_parent_function_async(status_close,0);
	}
	return 0;

}

/**************************************************************************
 
**************************************************************************/
int imap_dl_headers(struct list *imap_list)
{
	struct imap_server *serv;

	serv = (struct imap_server*)list_first(imap_list);
	while (serv)
	{
		struct folder *folder = folder_find_by_imap(serv->name,"");

		if (folder)
		{
			if (open_socket_lib())
			{
				struct connection *conn;
				if ((conn = tcp_connect(serv->name,serv->port,0)))
				{
					char tag[20];
					char send[200];
					char buf[200];

					char *line;
					int ok = 0;

					puts("connected to imap server\n");

					while ((line = tcp_readln(conn)))
					{
						puts(line);
						line = imap_get_result(line,buf,sizeof(buf));
						line = imap_get_result(line,buf,sizeof(buf));
						if (!mystricmp(buf,"OK"))
						{
							ok = 1;
							break;
						} else break;
					}

					if (ok)
					{
						puts("now sending login\n");
	
						sprintf(tag,"%04x",val++);
						sprintf(send,"%s LOGIN %s %s\r\n",tag,serv->login,serv->passwd);
	
						puts(send);
						tcp_write(conn,send,strlen(send));
						tcp_flush(conn);
	
						ok = 0;
	
						while ((line = tcp_readln(conn)))
						{
							puts(line);
							line = imap_get_result(line,buf,sizeof(buf));
							if (!mystricmp(buf,tag))
							{
								line = imap_get_result(line,buf,sizeof(buf));
								if (!mystricmp(buf,"OK"))
								{
									puts("Login successful\n");
									ok = 1;
								}
								break;
							}
						}

						if (ok)
						{
							sprintf(tag,"%04x",val++);
							sprintf(send,"%s LIST \"\" *\r\n",tag,serv->login,serv->passwd);
							puts(send);

							tcp_write(conn,send,strlen(send));
							tcp_flush(conn);

							while ((line = tcp_readln(conn)))
							{
								puts(line);
								line = imap_get_result(line,buf,sizeof(buf));
								if (!mystricmp(buf,tag))
								{
									line = imap_get_result(line,buf,sizeof(buf));
									if (!mystricmp(buf,"OK"))
									{
										puts("List successful\n");
										ok = 1;
									}
									break;
								} else
								{
									/* command */
									line = imap_get_result(line,buf,sizeof(buf));

									/* read flags */
									line = imap_get_result(line,buf,sizeof(buf));

									/* read delim */
									line = imap_get_result(line,buf,sizeof(buf));

									/* read name */
									line = imap_get_result(line,buf,sizeof(buf));

									if ((folder_add_imap(folder, buf)))
									{
									}
								}
							}
						}

						{
							int num_of_mails = 0;
							struct folder *inbox = folder_find_by_imap(serv->name,"INBOX");
							if (inbox)
							{
								char path[380];
								getcwd(path, sizeof(path));

								printf("Change path to %s\n",inbox->path);
								if (chdir(inbox->path) == -1)
								{
									printf("Path change to %s has failed\n",inbox->path);
								}

								sprintf(tag,"%04x",val++);
								sprintf(send,"%s EXAMINE %s\r\n",tag,inbox->imap_path);
								puts(send);
								tcp_write(conn,send,strlen(send));
								tcp_flush(conn);

								while ((line = tcp_readln(conn)))
								{
									puts(line);
									line = imap_get_result(line,buf,sizeof(buf));
									if (!mystricmp(buf,tag))
									{
										line = imap_get_result(line,buf,sizeof(buf));
										if (!mystricmp(buf,"OK"))
										{
											puts("Examine successful\n");
											ok = 1;
										}
										break;
									} else
									{
										/* untagged */
										char first[200];
										char second[200];

										line = imap_get_result(line,first,sizeof(first));
										line = imap_get_result(line,second,sizeof(second));

										if (!mystricmp("EXISTS",second))
										{
											num_of_mails = atoi(first);
										}
									}
								}

								if (num_of_mails)
								{
									struct {
										unsigned int uid;
										unsigned int size;
									} *mail_array;

									if ((mail_array = malloc(sizeof(*mail_array)*num_of_mails)))
									{
										sprintf(tag,"%04x",val++);
										sprintf(send,"%s FETCH %d:%d (UID RFC822.SIZE)\r\n",tag,1,num_of_mails);
								
										puts(send);
										tcp_write(conn,send,strlen(send));
										tcp_flush(conn);
		
										while ((line = tcp_readln(conn)))
										{
											puts(line);
											line = imap_get_result(line,buf,sizeof(buf));
											if (!mystricmp(buf,tag))
											{
												line = imap_get_result(line,buf,sizeof(buf));
												if (!mystricmp(buf,"OK"))
												{
													puts("Fetch successful\n");
													ok = 1;
												}
												break;
											} else
											{
												/* untagged */
												unsigned int msgno;
												unsigned int uid = 0;
												unsigned int size = 0;
												char msgno_buf[100];
												char cmd_buf[100];
												char stuff_buf[100];
												char *temp;
												int i;
		
												line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
												line = imap_get_result(line,cmd_buf,sizeof(cmd_buf));
												line = imap_get_result(line,stuff_buf,sizeof(stuff_buf));
	
												msgno = (unsigned int)atoi(msgno_buf);
												temp = stuff_buf;

												for (i=0;i<2;i++)
												{
													temp = imap_get_result(temp,cmd_buf,sizeof(cmd_buf));
													if (!mystricmp(cmd_buf,"UID"))
													{
														temp = imap_get_result(temp,cmd_buf,sizeof(cmd_buf));
														uid = atoi(cmd_buf);
													}
													else if (!mystricmp(cmd_buf,"RFC822.SIZE"))
													{
														temp = imap_get_result(temp,cmd_buf,sizeof(cmd_buf));
														size = atoi(cmd_buf);
													}
												}

												if (msgno <= num_of_mails)
												{
													mail_array[msgno-1].uid = uid;
													mail_array[msgno-1].size = size;
												}
											}
										}

										{
											int msgtodl;
											for (msgtodl=1;msgtodl <= num_of_mails;msgtodl++)
											{
												/* check if the mail already exists */
												if (folder_imap_find_mail_by_uid(inbox,mail_array[msgtodl-1].uid)) continue;

												sprintf(tag,"%04x",val++);
												sprintf(send,"%s FETCH %d RFC822\r\n",tag,msgtodl);
								
												puts(send);
												tcp_write(conn,send,strlen(send));
												tcp_flush(conn);

												while ((line = tcp_readln(conn)))
												{
													puts(line);
													line = imap_get_result(line,buf,sizeof(buf));
													if (!mystricmp(buf,tag))
													{
														line = imap_get_result(line,buf,sizeof(buf));
														if (!mystricmp(buf,"OK"))
														{
															puts("Fetch successful\n");
															ok = 1;
														}
														break;
													} else
													{
														/* untagged */
														if (buf[0] == '*')
														{
															char msgno_buf[200];
															char *temp_ptr;
															int msgno;
															int todownload;
															line++;

															line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
															msgno = atoi(msgno_buf); /* ignored */

															/* skip the fetch command */
															line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
															if ((temp_ptr = strchr(line,'{'))) /* } - avoid bracket checking problems */
															{
																temp_ptr++;
																todownload = atoi(temp_ptr);
															} else todownload = 0;

															if (todownload)
															{
																FILE *fh;
																char filename_buf[60];
																sprintf(filename_buf,"%d",mail_array[msgtodl-1].uid);

																if ((fh = fopen(filename_buf,"w")))
																{
																	while (todownload)
																	{
																		char buf[200];
																		int dl = tcp_read(conn,buf,MIN(sizeof(buf),todownload));
																		if (dl == -1 || !dl) break;
																		fwrite(buf,1,dl,fh);
																		todownload -= dl;
																	}
																	fclose(fh);
																	callback_new_imap_mail_arrived(filename_buf, inbox->imap_server, inbox->imap_path);
																}
															}
														}
													}
												}
											}
										}

										free(mail_array);
									}
								}
								chdir(path);
							}
						}
					} else
					{
						puts("error before logging in\n");
					}

					tcp_disconnect(conn);
				} else
				{
					puts("Couldn't connect to server ");
					puts(serv->name);
					puts("\n");
				}
				close_socket_lib();
			}
		}
		
		serv = (struct imap_server*)node_next(&serv->node);
	}
	callback_refresh_folders();
	return 1;
}

/**************************************************************************
 malloc() a imap_server and initializes it with default values.
 TODO: rename all imap identifiers to imap
**************************************************************************/
struct imap_server *imap_malloc(void)
{
	struct imap_server *imap;
	if ((imap = (struct imap_server*)malloc(sizeof(*imap))))
	{
		memset(imap,0,sizeof(struct imap_server));
		imap->port = 143;
	}
	return imap;
}

/**************************************************************************
 Duplicates a imap_server
**************************************************************************/
struct imap_server *imap_duplicate(struct imap_server *imap)
{
	struct imap_server *new_imap = imap_malloc();
	if (new_imap)
	{
		new_imap->name = mystrdup(imap->name);
		new_imap->login = mystrdup(imap->login);
		new_imap->passwd = mystrdup(imap->passwd);
		new_imap->port = imap->port;
		new_imap->active = imap->active;
	}
	return new_imap;
}

/**************************************************************************
 Frees a imap_server completly
**************************************************************************/
void imap_free(struct imap_server *imap)
{
	if (imap->name) free(imap->name);
	if (imap->login) free(imap->login);
	if (imap->passwd) free(imap->passwd);
	free(imap);
}


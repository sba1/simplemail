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
#include "support_indep.h"
#include "tcp.h"

#include "support.h"
#include "tcpip.h"

#include "imap.h"

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
										puts("Created ");puts(buf);puts("\n");
									}
								}
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


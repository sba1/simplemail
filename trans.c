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
** trans.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "configuration.h"
#include "dlwnd.h"
#include "folder.h"
#include "parse.h"
#include "pop3.h"
#include "simplemail.h"
#include "smtp.h"
#include "support.h"
#include "tcp.h"

#include "io.h" /* io.c should be removed after stuff has been moved to support.h */

int mails_dl(void)
{
	struct pop3_server server;

	memset(&server,0,sizeof(server));

	server.name = user.config.pop_server;
	server.port = 110;
	server.login = user.config.pop_login;
	server.passwd = user.config.pop_password;
	server.socket = SMTP_NO_SOCKET;

	if (!server.name)
	{
		tell("Please configure a pop3 server!");
		return(0);
	}

	dl_set_title(server.name);
	dl_window_open();

	if (!pop3_dl(&server))
	{
		dl_window_close();
	}

	return 0;
}

int mails_upload(void)
{
	char *domain;
	struct folder *out_folder = folder_outgoing();
	void *handle = NULL;
	int i;
	struct out_mail **out_array;
	struct out_mail *out;
	struct mail *m;
	int num_mails;
	struct smtp_server *server;

	char path[256];

	if (!out_folder)
	{
		tell("Couldn't find an outgoing folder!");
		return 0;
	}

	server = malloc(sizeof(struct smtp_server));
	server->name   = malloc(256);
	strcpy(server->name, user.config.smtp_server);
	server->port   = 25;
	server->socket = SMTP_NO_SOCKET;
	
	domain = user.config.smtp_domain;

	if (!server->name)
	{
		tell("Please specify a smtp server!");
		return 0;
	}

	if (!domain)
	{
		tell("Please configure a domain!");
		return 0;
	}

  /* folder_next_mail() is a little bit limited (not usable when mails are removed
   * from the folder, so we build an array of all mails first */
	num_mails = 0;
	while ((m = folder_next_mail(out_folder, &handle))) num_mails++;
	if (!num_mails) return 0;

	/* only one malloc() */
	i = sizeof(struct out_mail*)*(num_mails+1) + sizeof(struct out_mail)*num_mails;
	out_array = (struct out_mail**)malloc(i);
	if (!out_array) return 0;

	/* change into the outgoing folder directory */
	getcwd(path, sizeof(path));
	if(chdir(out_folder->path) == -1)
	{
		free(out_array);
		return 0;
	}

	/* clear the memory */
	memset(out_array,0,i);

	/* set the first out */
	out = (struct out_mail*)(((char*)out_array)+sizeof(struct out_mail*)*(num_mails+1));
	handle = NULL;
	i=0;

	/* initialize the arrays */
	while ((m = folder_next_mail(out_folder, &handle)))
	{
		char *from, *to;
		struct mailbox mb;
		struct list *list; /* "To" address list */

		out_array[i++] = out;

		to = mail_find_header_contents(m,"To");
		from = mail_find_header_contents(m,"From");

		memset(&mb,0,sizeof(struct mailbox));

		if (!to || !from ) break;
		if (!parse_mailbox(from,&mb)) break;

		out->domain = domain;
		out->mailfile = m->filename;
		out->from = mb.addr_spec; /* must be not freed here */

		/* fill in the recipients */
		if ((list = create_address_list(to)))
		{
			int length = list_length(list);
			if (length)
			{
				if ((out->rcp = (char**)malloc(sizeof(char*)*(length+1)))) /* not freed */
				{
					struct address *addr = (struct address*)list_first(list);
					int i=0;
					while (addr)
					{
						if (!(out->rcp[i++] = strdup(addr->email))) /* not freed */
							break;
						addr = (struct address*)node_next(&addr->node);
					}
					out->rcp[i] = NULL;
				}
			}
			free_address_list(list);
		}

		if (mb.phrase) free(mb.phrase); /* phrase is not necessary */
/*		if (mb.addr_spec) free(mb.addr_spec); */
		out++;
	}

	/* now send all mails */
	smtp_send(server, out_array);


	chdir(path);
	free(out_array);
	
	free(server->name);
	free(server);

	/* NOTE: A lot of memory leaks!! */
}


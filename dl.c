/*
** dl.c
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

#include "io.h" /* io.c should be removed after stuff has been moved to support.h */

int mails_dl(void)
{
	char *server, *login, *passwd;

	server = user.config.pop_server;
	login = user.config.pop_login;
	passwd = user.config.pop_password;

	if (!server)
	{
		tell("Please configure a pop3 server!");
		return 0;
	}

	dl_set_title(server);
	dl_window_open();

	/* Here we must create a new task which then downloads the mails */
	pop3_dl(server, 110, login, passwd);

	dl_window_close();

	return 0;
}

int mails_upload(void)
{
	char *server, *domain;
	struct folder *out_folder = folder_outgoing();
	struct folder *sent_folder = folder_sent();
	void *handle = NULL;
	int i;
	struct out_mail **out_array;
	struct out_mail *out;

	/* the following three fields should be obsolette */
	int num_mails;
	struct mail **mail_array;
	struct mail *m;

	char path[256];

	if (!out_folder)
	{
		tell("Couldn't find an outgoing folder!");
		return 0;
	}

	server = user.config.smtp_server;
	domain = user.config.smtp_domain;

	if (!server)
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

	mail_array = (struct mail**)malloc(sizeof(struct mail*)*num_mails);
	if (!mail_array) return 0;

	/* only one malloc() */
	i = sizeof(struct out_mail*)*(num_mails+1) + sizeof(struct out_mail)*num_mails;
	out_array = (struct out_mail**)malloc(i);
	if (!out_array)
	{
		free(mail_array);
		return 0;
	}

	/* change into the outgoing folder directory */
	getcwd(path, sizeof(path));
	if(chdir(out_folder->path) == -1)
	{
		free(out_array);
		free(mail_array);
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

		out_array[i] = out;
		mail_array[i++] = m; /* store the mail in the array, this should be obsoletted */

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

	/* this should be done in smtp_send() but we need some new functions
   * for that, until this the obsoletted marked stuff if necessary */
	if (sent_folder)
	{
		for (i=0;i<num_mails;i++)
		{
			callback_move_mail(mail_array[i],out_folder,sent_folder);
		}
	}

	chdir(path);
	free(out_array);
	free(mail_array);

	/* NOTE: A lot of memory leaks!! */
}


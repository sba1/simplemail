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

	set_dl_title(server);
	dl_window_open();

	/* Here we must create a new task which then downloads the mails */
	pop3_dl(server, 110, login, passwd);

	dl_window_close();

	return 0;
}


int smtp_send(char *server, struct out_mail *om);
/* bitte in smtp.h public definieren */

int mails_upload(void)
{
	char *server, *domain;
	struct folder *out_folder = folder_outgoing();
	struct folder *sent_folder = folder_sent();
	void *handle = NULL;
	int i,num_mails;
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

	i=0;
	handle = NULL;
	while ((m = folder_next_mail(out_folder, &handle))) mail_array[i++] = m;

	getcwd(path, sizeof(path));
	if(chdir(out_folder->path) == -1)
	{
		free(mail_array);
		return 0;
	}

	/* Now send the mails and move them to the sent folder */
	for (i=0; i<num_mails;i++)
	{
		char *from, *to;
		struct mailbox mb;
		struct out_mail out;
		struct list *list; /* "To" address list */

		m = mail_array[i];
		to = mail_find_header_contents(m,"To");
		from = mail_find_header_contents(m,"From");

		memset(&mb,0,sizeof(struct mailbox));
		memset(&out,0,sizeof(struct out_mail));

		if (!out_folder || !to || !from ) break;
		if (!parse_mailbox(from,&mb)) break;

		out.domain = domain;
		out.mailfile = m->filename;
		out.from = mb.addr_spec;

		if ((list = create_address_list(to)))
		{
			int length = list_length(list);
			if (length)
			{
				if ((out.rcp = (char**)malloc(sizeof(char*)*(length+1))))
				{
					struct address *addr = (struct address*)list_first(list);
					int i=0;
					while (addr)
					{
						out.rcp[i++] = addr->email;
						addr = (struct address*)node_next(&addr->node);
					}
					out.rcp[i] = NULL;

					smtp_send(server,&out);
				}
			}
			free_address_list(list);
		}

		if (mb.phrase) free(mb.phrase);
		if (mb.addr_spec) free(mb.addr_spec);

		if (sent_folder)
		{
			callback_move_mail(m,out_folder,sent_folder);
		}
	}

	chdir(path);
	free(mail_array);
}


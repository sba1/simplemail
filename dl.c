/*
** dl.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "dlwnd.h"
#include "folder.h"
#include "parse.h"
#include "pop3.h"
#include "smtp.h"

int mails_dl(void)
{
	int rc;
	char *server, *login, *passwd, *buf;
	
	rc = 0;
	
	buf = getenv("SIMPLEMAIL_POP3_SERVER");
	server = malloc(strlen(buf) + 1);
	strcpy(server, buf);
	buf = getenv("SIMPLEMAIL_LOGIN");
	login = malloc(strlen(buf) + 1);
	strcpy(login, buf);
	buf = getenv("SIMPLEMAIL_PASSWD");
	passwd = malloc(strlen(buf) + 1);
	strcpy(passwd, buf);
	
	set_dl_title(server);
	dl_window_open();
	
	
	pop3_dl(server, 110, login, passwd);
	
	dl_window_close();

	free(server);
	free(login);
	free(passwd);
	
	return(rc);
}


int smtp_send(char *server, struct out_mail *om);
/* bitte in smtp.h public definieren */

int mails_upload(void)
{
	char *server, *domain;
	struct folder *out_folder = folder_outgoing();
	void *handle = NULL;
	struct mail *m;

	char path[256];

	getcwd(path, sizeof(path));
	if(chdir(out_folder->path) == -1) return 0;

	if (!out_folder) return NULL;

	server = strdup(getenv("SIMPLEMAIL_SMTP_SERVER"));
	domain = strdup(getenv("SIMPLEMAIL_DOMAIN"));

	while ((m = folder_next_mail(out_folder, &handle)))
	{
		char *to = mail_find_header_contents(m,"To");
		char *from = mail_find_header_contents(m,"From");
		struct mailbox mb;
		struct out_mail out;
		struct list *list; /* to address lists */

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
	}

	free(server);
	free(domain);

	chdir(path);
}

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
** configuration.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "account.h"
#include "configuration.h"
#include "filter.h"
#include "pop3.h"
#include "support_indep.h"

#include "support.h"

int read_line(FILE *fh, char *buf); /* in addressbook.c */

char *get_config_item(char *buf, char *item)
{
	int len = strlen(item);
	if (!mystrnicmp(buf,item,len))
	{
		unsigned char c;
		buf += len;

		/* skip spaces */
		while ((c = *buf) && isspace(c)) buf++;
		if (*buf != '=') return NULL;
		buf++;
		/* skip spaces */
		while ((c = *buf) && isspace(c)) buf++;

		return buf;
	}
	return NULL;
}

struct user user;

void init_config(void)
{
	struct account *account;

	memset(&user,0,sizeof(struct user));
	user.directory = strdup("PROGDIR:");

	list_init(&user.config.account_list);
	list_init(&user.config.filter_list);

	if ((account = account_malloc()))
	{
		list_insert_tail(&user.config.account_list,&account->node);
	}
}

int load_config(void)
{
	char *buf;

	init_config();

	if ((buf = malloc(512)))
	{
		FILE *fh;

		if (user.directory) strcpy(buf,user.directory);
		else buf[0] = 0;

		sm_add_part(buf,".config",512);

		if ((user.config_filename = mystrdup(buf)))
		{
			if ((fh = fopen(user.config_filename,"r")))
			{
				read_line(fh,buf);
				if (!strncmp("SMCO",buf,4))
				{
					while (read_line(fh,buf))
					{
						char *result;

						if ((result = get_config_item(buf,"DST")))
							user.config.dst = ((*result == 'Y') || (*result == 'y'))?1:0;
						if ((result = get_config_item(buf, "Receive.Preselection")))
							user.config.receive_preselection = atoi(result);
						if ((result = get_config_item(buf, "Receive.Size")))
							user.config.receive_size = atoi(result);
						if ((result = get_config_item(buf,"Read.Wordwrap")))
							user.config.read_wordwrap = ((*result == 'Y') || (*result == 'y'))?1:0;

						if (!mystrnicmp(buf, "ACCOUNT",7))
						{
							/* it's a POP Server config line */
							unsigned char *account_buf = buf + 7;
							int account_no = atoi(account_buf);
							struct account *account;

							account = (struct account*)list_find(&user.config.account_list,account_no);
							if (!account)
							{
								if ((account = account_malloc()))
									list_insert_tail(&user.config.account_list,&account->node);
								account = (struct account*)list_find(&user.config.account_list,account_no);
							}

							if (account)
							{
								while (isdigit(*account_buf)) account_buf++;
								if (*account_buf++ == '.')
								{
									if ((result = get_config_item(account_buf,"User.Name")))
										account->name = mystrdup(result);
									if ((result = get_config_item(account_buf,"User.EMail")))
										account->email = mystrdup(result);
									if ((result = get_config_item(account_buf,"User.Reply")))
										account->reply = mystrdup(result);

									if ((result = get_config_item(account_buf,"SMTP.Server")))
										account->smtp->name = mystrdup(result);
									if ((result = get_config_item(account_buf,"SMTP.Port")))
										account->smtp->port = atoi(result);
									if ((result = get_config_item(account_buf,"SMTP.Login")))
										account->smtp->auth_login = mystrdup(result);
									if ((result = get_config_item(account_buf,"SMTP.Password")))
										account->smtp->auth_password = mystrdup(result);
									if ((result = get_config_item(account_buf,"SMTP.Auth")))
										account->smtp->auth = ((*result == 'Y') || (*result == 'y'))?1:0;
									if ((result = get_config_item(account_buf,"SMTP.IPasDomain")))
										account->smtp->ip_as_domain = ((*result == 'Y') || (*result == 'y'))?1:0;
									if ((result = get_config_item(account_buf,"SMTP.POP3first")))
										account->smtp->pop3_first = ((*result == 'Y') || (*result == 'y'))?1:0;

									if ((result = get_config_item(account_buf,"POP3.Server")))
										account->pop->name = mystrdup(result);
									if ((result = get_config_item(account_buf,"POP3.Port")))
										account->pop->port = atoi(result);
									if ((result = get_config_item(account_buf,"POP3.Login")))
										account->pop->login = mystrdup(result);
									if ((result = get_config_item(account_buf,"POP3.Port")))
										account->pop->passwd = mystrdup(result);
									if ((result = get_config_item(account_buf,"POP3.Delete")))
										account->pop->del = ((*result == 'Y') || (*result == 'y'))?1:0;
								}
							}
						}
					}
				}

				fclose(fh);
			}
		}

		if (user.directory) strcpy(buf,user.directory);
		else buf[0] = 0;

		sm_add_part(buf,".filters",512);

		if ((user.filter_filename = mystrdup(buf)))
		{
			if ((fh = fopen(user.filter_filename,"r")))
			{
				read_line(fh,buf);
				if (!strncmp("SMFI",buf,4))
				{
					filter_list_load(fh);
				}

				fclose(fh);
			}
		}

		free(buf);
	}

/*	if (!user.config.email) user.config.email = mystrdup("");
	if (!user.config.realname) user.config.realname = mystrdup("");*/

	return 1;
}

#define MAKESTR(x) ((x)?(char*)(x):"")

void save_config(void)
{
	if (user.config_filename)
	{
		FILE *fh = fopen(user.config_filename, "w");
		if (fh)
		{
			struct account *account;
			int i;

			fputs("SMCO\n\n",fh);

/*			fprintf(fh,"EmailAddress=%s\n",MAKESTR(user.config.email));
			fprintf(fh,"RealName=%s\n",MAKESTR(user.config.realname));*/
			fprintf(fh,"DST=%s\n",user.config.dst?"Y":"N");

			/* Write out receive stuff */
			fprintf(fh,"Receive.Preselection=%d\n",user.config.receive_preselection);
			fprintf(fh,"Receive.Size=%d\n",user.config.receive_size);

			/* Write the pop3 servers */
			i = 0;
			account = (struct account*)list_first(&user.config.account_list);
			while (account)
			{
				fprintf(fh,"ACCOUNT%d.User.Name=%s\n",i,MAKESTR(account->name));
				fprintf(fh,"ACCOUNT%d.User.EMail=%s\n",i,MAKESTR(account->email));
				fprintf(fh,"ACCOUNT%d.User.Reply=%s\n",i,MAKESTR(account->reply));

				fprintf(fh,"ACCOUNT%d.SMTP.Server=%s\n",i,MAKESTR(account->smtp->name));
				fprintf(fh,"ACCOUNT%d.SMTP.Port=%d\n",i,account->smtp->port);
				fprintf(fh,"ACCOUNT%d.SMTP.Login=%s\n",i,MAKESTR(account->smtp->auth_login));
				fprintf(fh,"ACCOUNT%d.SMTP.Password=%s\n",i,MAKESTR(account->smtp->auth_password));
				fprintf(fh,"ACCOUNT%d.SMTP.Auth=%s\n",i,account->smtp->auth?"Y":"N");
				fprintf(fh,"ACCOUNT%d.SMTP.IPasDomain=%s\n",i,account->smtp->ip_as_domain?"Y":"N");
				fprintf(fh,"ACCOUNT%d.SMTP.POP3first=%s\n",i,account->smtp->pop3_first?"Y":"N");

				fprintf(fh,"ACCOUNT%d.POP3.Server=%s\n",i,MAKESTR(account->pop->name));
				fprintf(fh,"ACCOUNT%d.POP3.Port=%d\n",i,account->pop->port);
				fprintf(fh,"ACCOUNT%d.POP3.Login=%s\n",i,MAKESTR(account->pop->login));
				fprintf(fh,"ACCOUNT%d.POP3.Password=%s\n",i,MAKESTR(account->pop->passwd));
				fprintf(fh,"ACCOUNT%d.POP3.Delete=%s\n",i,account->pop->del?"Y":"N");
				account = (struct account*)node_next(&account->node);
				i++;
			}

/*			fprintf(fh,"SMTP00.Server=%s\n",MAKESTR(user.config.smtp_server));
			fprintf(fh,"SMTP00.Port=%d\n",user.config.smtp_port);
			fprintf(fh,"SMTP00.Domain=%s\n",MAKESTR(user.config.smtp_domain));
			fprintf(fh,"SMTP00.IPAsDomain=%s\n",user.config.smtp_ip_as_domain?"Y":"N");
			fprintf(fh,"SMTP00.Auth=%s\n",user.config.smtp_auth?"Y":"N");
			fprintf(fh,"SMTP00.Login=%s\n",MAKESTR(user.config.smtp_login));
			fprintf(fh,"SMTP00.Password=%s\n",MAKESTR(user.config.smtp_password));*/
			fprintf(fh,"Read.Wordwrap=%s\n",user.config.read_wordwrap?"Y":"N");
			
			fclose(fh);
		}
	}
}

void save_filter(void)
{
	if (user.filter_filename)
	{
		FILE *fh = fopen(user.filter_filename, "w");
		if (fh)
		{
			int i;

			fputs("SMFI\n\n",fh);

			filter_list_save(fh);

			fclose(fh);
		}
	}
}

/* Clear all the accounts */
void clear_config_accounts(void)
{
	struct account *a;

	while ((a = (struct account*)list_remove_tail(&user.config.account_list)))
		account_free(a);
}

/* Insert a mew account into the configuration list */
void insert_config_account(struct account *account)
{
	struct account *new_account = account_duplicate(account);
	if (new_account)
		list_insert_tail(&user.config.account_list,&new_account->node);
}


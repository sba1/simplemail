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
	struct pop3_server *pop3;

	memset(&user,0,sizeof(struct user));
	user.directory = strdup("PROGDIR:");
	user.config.smtp_port = 25;

	list_init(&user.config.receive_list);
	list_init(&user.config.filter_list);

	if ((pop3 = pop_malloc()))
	{
		list_insert_tail(&user.config.receive_list,&pop3->node);
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

						if ((result = get_config_item(buf,"EmailAddress")))
							user.config.email = strdup(result);
						if ((result = get_config_item(buf,"RealName")))
							user.config.realname = strdup(result);
						if ((result = get_config_item(buf,"DST")))
							user.config.dst = ((*result == 'Y') || (*result == 'y'))?1:0;
						if ((result = get_config_item(buf, "Receive.Preselection")))
							user.config.receive_preselection = atoi(result);
						if ((result = get_config_item(buf, "Receive.Size")))
							user.config.receive_size = atoi(result);
						if ((result = get_config_item(buf,"SMTP00.Server")))
							user.config.smtp_server = strdup(result);
						if ((result = get_config_item(buf,"SMTP00.Port")))
							user.config.smtp_port = atoi(result);
						if ((result = get_config_item(buf,"SMTP00.Domain")))
							user.config.smtp_domain = strdup(result);
						if ((result = get_config_item(buf,"SMTP00.IPAsDomain")))
							user.config.smtp_ip_as_domain = ((*result == 'Y') || (*result == 'y'))?1:0;
						if ((result = get_config_item(buf,"SMTP00.Auth")))
							user.config.smtp_auth = ((*result == 'Y') || (*result == 'y'))?1:0;
						if ((result = get_config_item(buf,"SMTP00.Login")))
							user.config.smtp_login = strdup(result);
						if ((result = get_config_item(buf,"SMTP00.Password")))
							user.config.smtp_password = strdup(result);
						if ((result = get_config_item(buf,"Read.Wordwrap")))
							user.config.read_wordwrap = ((*result == 'Y') || (*result == 'y'))?1:0;

						if (!mystrnicmp(buf, "POP",3))
						{
							/* it's a POP Server config line */
							unsigned char *pop_buf = buf + 3;
							int pop_no = atoi(pop_buf);
							struct pop3_server *pop;

							pop = (struct pop3_server*)list_find(&user.config.receive_list,pop_no);
							if (!pop)
							{
								if ((pop = pop_malloc()))
									list_insert_tail(&user.config.receive_list,&pop->node);
								pop = (struct pop3_server*)list_find(&user.config.receive_list,pop_no);
							}

							if (pop)
							{
								while (isdigit(*pop_buf)) pop_buf++;
								if (*pop_buf++ == '.')
								{
									if ((result = get_config_item(pop_buf,"Login")))
										pop->login = mystrdup(result);
									if ((result = get_config_item(pop_buf,"Server")))
										pop->name = mystrdup(result);
									if ((result = get_config_item(pop_buf,"Port")))
										pop->port = atoi(result);
									if ((result = get_config_item(pop_buf,"Password")))
										pop->passwd = mystrdup(result);
									if ((result = get_config_item(pop_buf,"Delete")))
										pop->del = ((*result == 'Y') || (*result == 'y'))?1:0;
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
	if (!user.config.email) user.config.email = mystrdup("");
	if (!user.config.realname) user.config.realname = mystrdup("");

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
			struct pop3_server *pop;
			int i;

			fputs("SMCO\n\n",fh);

			fprintf(fh,"EmailAddress=%s\n",MAKESTR(user.config.email));
			fprintf(fh,"RealName=%s\n",MAKESTR(user.config.realname));
			fprintf(fh,"DST=%s\n",user.config.dst?"Y":"N");

			/* Write out receive stuff */
			fprintf(fh,"Receive.Preselection=%d\n",user.config.receive_preselection);
			fprintf(fh,"Receive.Size=%d\n",user.config.receive_size);

			/* Write the pop3 servers */
			i = 0;
			pop = (struct pop3_server*)list_first(&user.config.receive_list);
			while (pop)
			{
				fprintf(fh,"POP%d.Login=%s\n",i,MAKESTR(pop->login));
				fprintf(fh,"POP%d.Server=%s\n",i,MAKESTR(pop->name));
				fprintf(fh,"POP%d.Port=%d\n",i,pop->port);
				fprintf(fh,"POP%d.Password=%s\n",i,MAKESTR(pop->passwd));
				fprintf(fh,"POP%d.Delete=%s\n",i,pop->del?"Y":"N");
				pop = (struct pop3_server*)node_next(&pop->node);
				i++;
			}

			fprintf(fh,"SMTP00.Server=%s\n",MAKESTR(user.config.smtp_server));
			fprintf(fh,"SMTP00.Port=%d\n",user.config.smtp_port);
			fprintf(fh,"SMTP00.Domain=%s\n",MAKESTR(user.config.smtp_domain));
			fprintf(fh,"SMTP00.IPAsDomain=%s\n",user.config.smtp_ip_as_domain?"Y":"N");
			fprintf(fh,"SMTP00.Auth=%s\n",user.config.smtp_auth?"Y":"N");
			fprintf(fh,"SMTP00.Login=%s\n",MAKESTR(user.config.smtp_login));
			fprintf(fh,"SMTP00.Password=%s\n",MAKESTR(user.config.smtp_password));
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

/* Empties the config list */
void free_config_pop(void)
{
	struct pop3_server *pop;

	while ((pop = (struct pop3_server*)list_remove_tail(&user.config.receive_list)))
		pop_free(pop);
}

/* Insert a new pop server */
void insert_config_pop(struct pop3_server *pop)
{
	struct pop3_server *new_pop = pop_duplicate(pop);
	if (new_pop)
		list_insert_tail(&user.config.receive_list,&new_pop->node);
}





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
#include "signature.h"
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

	user.config.read_background = 0xb0b0b0;
	user.config.read_text = 0;
	user.config.read_quoted = 0xffffff;
	user.config.read_old_quoted = 0xffff00;
	user.config.read_link = 0x000098;
	user.config.read_link_underlined = 0;
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
						if ((result = get_config_item(buf,"Read.PropFont")))
							user.config.read_propfont = mystrdup(result);
						if ((result = get_config_item(buf,"Read.FixedFont")))
							user.config.read_fixedfont = mystrdup(result);
						if ((result = get_config_item(buf,"Signatures.Use")))
							user.config.signatures_use = ((*result == 'Y') || (*result == 'y'))?1:0;
						if ((result = get_config_item(buf,"Read.BackgroundColor")))
							sscanf(result,"%x",&user.config.read_background);
						if ((result = get_config_item(buf,"Read.TextColor")))
							sscanf(result,"%x",&user.config.read_text);
						if ((result = get_config_item(buf,"Read.QuotedColor")))
							sscanf(result,"%x",&user.config.read_quoted);
						if ((result = get_config_item(buf,"Read.OldQuotedColor")))
							sscanf(result,"%x",&user.config.read_old_quoted);
						if ((result = get_config_item(buf,"Read.LinkColor")))
							sscanf(result,"%x",&user.config.read_link);
						if ((result = get_config_item(buf,"Read.LinkUnderlined")))
							user.config.read_link_underlined = ((*result == 'Y') || (*result == 'y'))?1:0;

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
									if ((result = get_config_item(account_buf,"POP3.Password")))
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

		if (user.directory) strcpy(buf,user.directory);
		else buf[0] = 0;

		sm_add_part(buf,".signatures",512);

		if ((user.signature_filename = mystrdup(buf)))
		{
			if ((fh = fopen(user.signature_filename,"r")))
			{
				while ((read_line(fh,buf)))
				{
					if (!stricmp(buf,"begin signature"))
					{
						if (read_line(fh,buf))
						{
							char *name = mystrdup(buf);
							char *sign = NULL;
							struct signature *s;
							while (read_line(fh,buf))
							{
								int sign_len = sign?strlen(sign):0;
								if (!stricmp(buf,"end signature"))
									break;

								if ((sign = realloc(sign,sign_len + strlen(buf) + 2)))
								{
									strcpy(&sign[sign_len],buf + 1 /* skip space */);
									strcat(&sign[sign_len],"\n");
								}
							}

							if (sign && strlen(sign)) sign[strlen(sign)-1]=0; /* remove the additional new line */

							if ((s = signature_malloc()))
							{
								s->name = name;
								s->signature = sign;
								list_insert_tail(&user.config.signature_list,&s->node);
							}
						}
					}
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

			fprintf(fh,"Signatures.Use=%s",user.config.signatures_use?"Y":"N");
			fprintf(fh,"Read.Wordwrap=%s\n",user.config.read_wordwrap?"Y":"N");
			fprintf(fh,"Read.PropFont=%s\n",MAKESTR(user.config.read_propfont));
			fprintf(fh,"Read.FixedFont=%s\n",MAKESTR(user.config.read_fixedfont));
			fprintf(fh,"Read.BackgroundColor=0x%lx\n",user.config.read_background);
			fprintf(fh,"Read.TextColor=0x%lx\n",user.config.read_text);
			fprintf(fh,"Read.QuotedColor=0x%lx\n",user.config.read_quoted);
			fprintf(fh,"Read.OldQuotedColor=0x%lx\n",user.config.read_old_quoted);
			fprintf(fh,"Read.LinkColor=0x%lx\n",user.config.read_link);
			fprintf(fh,"Read.LinkUnderlined=%s\n",user.config.read_link_underlined?"Y":"N");

			fclose(fh);
		}
	}

	if (user.signature_filename)
	{
		int i;
		FILE *fh = fopen(user.signature_filename, "w");
		if (fh)
		{
			struct signature *signature;
			signature = (struct signature*)list_first(&user.config.signature_list);
			while (signature)
			{
				char *start = signature->signature;
				char *end;
				fputs("begin signature\n",fh);
				fputs(signature->name,fh);
				fputc(10,fh);

				while ((end = strchr(start,10)))
				{
					fputc(' ',fh);
					fwrite(start,end-start,1,fh);
					fputc(10,fh);
					start = end + 1;
				}
				fputc(' ',fh);
				fputs(start,fh);

				fputs("\nend signature\n",fh); /* the additional line will filtered out when loaded */

				signature = (struct signature*)node_next(&signature->node);
			}
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

/* Clear all the signatures  */
void clear_config_signatures(void)
{
	struct signature *s;

	while ((s = (struct signature*)list_remove_tail(&user.config.signature_list)))
		signature_free(s);
}

/* Insert a mew account into the configuration list */
void insert_config_signature(struct signature *signature)
{
	struct signature *new_signature = signature_duplicate(signature);
	if (new_signature)
		list_insert_tail(&user.config.signature_list,&new_signature->node);
}

/* Find a signature by name */
struct signature *find_config_signature_by_name(char *name)
{
	struct signature *s = (struct signature*)list_first(&user.config.signature_list);

	while (s)
	{
		if (!mystricmp(name,s->name)) return s;
		s = (struct signature*)node_next(&s->node);
	}
	return NULL;
}

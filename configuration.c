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
#include "phrase.h"
#include "pop3.h"
#include "signature.h"
#include "taglines.h"
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
	struct phrase *phrase;

	memset(&user,0,sizeof(struct user));

#ifdef _AMIGA
	user.directory = strdup("PROGDIR:");
	user.folder_directory = strdup("PROGDIR:.folders");
#else
	user.directory = strdup(".");
	user.folder_directory = strdup("./.folders");
#endif
	list_init(&user.config.account_list);
	list_init(&user.config.signature_list);
	list_init(&user.config.phrase_list);
	list_init(&user.config.filter_list);

	if ((account = account_malloc()))
	{
		list_insert_tail(&user.config.account_list,&account->node);
	}

	if ((phrase = phrase_malloc()))
	{
		list_insert_tail(&user.config.phrase_list,&phrase->node);

		phrase->addresses = NULL;
		phrase->write_welcome = mystrdup("Hello,\\n");
		phrase->write_welcome_repicient = mystrdup("Hello %v\\n");
		phrase->write_closing = mystrdup("Regards,");
		phrase->reply_welcome = mystrdup("Hello %f,\\n");
		phrase->reply_intro = mystrdup("On %d, you wrote:");
		phrase->reply_close = mystrdup("Regards");
		phrase->forward_initial = mystrdup("*** Begin of forwarded message ***\\n\\nDate: %d %t\\nFrom: %n <%e>\\nSubject: %s\\n\\n--- Forwarded message follows ---\\n\\n");
		phrase->forward_finish = mystrdup("*** End of forwarded message ***\\n");
	}

	if ((phrase = phrase_malloc()))
	{
		list_insert_tail(&user.config.phrase_list,&phrase->node);

		phrase->addresses = mystrdup(".de");
		phrase->write_welcome = mystrdup("Hallo,\\n");
		phrase->write_welcome_repicient = mystrdup("Hallo %v\\n");
		phrase->write_closing = mystrdup("Gruss,");
		phrase->reply_welcome = mystrdup("Hallo %f,\\n");
		phrase->reply_intro = mystrdup("Am %d schriebst Du:");
		phrase->reply_close = mystrdup("Gruss,");
		phrase->forward_initial = mystrdup("*** Start der weitergeleiteten Nachricht ***\\n\\nDatum: %d %t\\nVon: %n <%e>\\nBetreff: %s\\n\\n--- Weitergeleitete Nachricht folgt ---\\n\\n");
		phrase->forward_finish = mystrdup("*** Ende der weitergeleiteten Nachricht ***\\n");
	}

	user.config.header_flags = SHOW_HEADER_FROM | SHOW_HEADER_TO | SHOW_HEADER_CC | SHOW_HEADER_SUBJECT | SHOW_HEADER_DATE | SHOW_HEADER_REPLYTO;

	user.config.read_background = 0xb0b0b0;
	user.config.read_text = 0;
	user.config.read_quoted = 0xffffff;
	user.config.read_old_quoted = 0xffff00;
	user.config.read_link = 0x000098;
	user.config.read_link_underlined = 0;

	user.config.write_wrap = 76;
	user.config.write_wrap_type = 2;
	user.config.write_reply_quote = 1;
	user.config.write_reply_stripsig = 1;
	user.config.write_reply_citeemptyl = 0;
}

#define CONFIG_BOOL_VAL(x) (((*x == 'Y') || (*x == 'y'))?1:0)

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
					clear_config_phrases();
					while (read_line(fh,buf))
					{
						char *result;

						if ((result = get_config_item(buf,"FolderDirectory")))
							user.folder_directory = mystrdup(result);
						if ((result = get_config_item(buf,"DST")))
							user.config.dst = ((*result == 'Y') || (*result == 'y'))?1:0;
						if ((result = get_config_item(buf, "Receive.Preselection")))
							user.config.receive_preselection = atoi(result);
						if ((result = get_config_item(buf, "Receive.Size")))
							user.config.receive_size = atoi(result);
						if ((result = get_config_item(buf, "Receive.Autocheck")))
							user.config.receive_autocheck = atoi(result);
						if ((result = get_config_item(buf,"Receive.SoundPlay")))
							user.config.receive_sound = atoi(result);
						if ((result = get_config_item(buf,"Receive.SoundFile")))
							user.config.receive_sound_file = mystrdup(result);
						if ((result = get_config_item(buf,"Read.PropFont")))
							user.config.read_propfont = mystrdup(result);
						if ((result = get_config_item(buf,"Read.FixedFont")))
							user.config.read_fixedfont = mystrdup(result);
						if ((result = get_config_item(buf,"Signatures.Use")))
							user.config.signatures_use = CONFIG_BOOL_VAL(result);
						if ((result = get_config_item(buf,"Write.Wrap")))
							user.config.write_wrap = atoi(result);
						if ((result = get_config_item(buf,"Write.WrapType")))
							user.config.write_wrap_type = atoi(result);
						if ((result = get_config_item(buf,"Write.ReplyQuote")))
							user.config.write_reply_quote = CONFIG_BOOL_VAL(result);
						if ((result = get_config_item(buf,"Write.ReplyStripSig")))
							user.config.write_reply_stripsig = CONFIG_BOOL_VAL(result);
						if ((result = get_config_item(buf,"Write.ReplyCiteEmptyL")))
							user.config.write_reply_citeemptyl = CONFIG_BOOL_VAL(result);	
						if ((result = get_config_item(buf,"ReadHeader.Flags")))
							sscanf(result,"%x",&user.config.header_flags);
						if ((result = get_config_item(buf,"ReadHeader.HeaderName")))
							user.config.header_array = array_add_string(user.config.header_array,result);
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
						if ((result = get_config_item(buf,"Read.Wordwrap")))
							user.config.read_wordwrap = CONFIG_BOOL_VAL(result);
						if ((result = get_config_item(buf,"Read.LinkUnderlined")))
							user.config.read_link_underlined = CONFIG_BOOL_VAL(result);
						if ((result = get_config_item(buf,"Read.Smilies")))
							user.config.read_smilies = CONFIG_BOOL_VAL(result);
						if ((result = get_config_item(buf,"ReadHTML.AllowAddress")))
							user.config.internet_emails = array_add_string(user.config.internet_emails,result);

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
										account->smtp->auth = CONFIG_BOOL_VAL(result);
									if ((result = get_config_item(account_buf,"SMTP.IPasDomain")))
										account->smtp->ip_as_domain = CONFIG_BOOL_VAL(result);
									if ((result = get_config_item(account_buf,"SMTP.POP3first")))
										account->smtp->pop3_first = CONFIG_BOOL_VAL(result);
									if ((result = get_config_item(account_buf,"SMTP.Secure")))
										account->smtp->secure = CONFIG_BOOL_VAL(result);

									if ((result = get_config_item(account_buf,"POP3.Server")))
										account->pop->name = mystrdup(result);
									if ((result = get_config_item(account_buf,"POP3.Port")))
										account->pop->port = atoi(result);
									if ((result = get_config_item(account_buf,"POP3.Login")))
										account->pop->login = mystrdup(result);
									if ((result = get_config_item(account_buf,"POP3.Password")))
										account->pop->passwd = mystrdup(result);
									if ((result = get_config_item(account_buf,"POP3.Delete")))
										account->pop->del = CONFIG_BOOL_VAL(result);
									if ((result = get_config_item(account_buf,"POP3.SSL")))
										account->pop->ssl = CONFIG_BOOL_VAL(result);
									if ((result = get_config_item(account_buf,"POP3.STLS")))
										account->pop->stls = CONFIG_BOOL_VAL(result);
									if ((result = get_config_item(account_buf,"POP3.Active")))
										account->pop->active = CONFIG_BOOL_VAL(result);
									if ((result = get_config_item(account_buf,"POP3.AvoidDupl")))
										account->pop->nodupl = CONFIG_BOOL_VAL(result);
									if ((result = get_config_item(account_buf,"POP3.Ask")))
										account->pop->ask = CONFIG_BOOL_VAL(result);
								}
							}
						}

						if (!mystrnicmp(buf, "Phrase",6))
						{
							/* it's a POP Server config line */
							unsigned char *phrase_buf = buf + 6;
							int phrase_no = atoi(phrase_buf);
							struct phrase *phrase;

							phrase = (struct phrase*)list_find(&user.config.phrase_list,phrase_no);
							if (!phrase)
							{
								if ((phrase = phrase_malloc()))
									list_insert_tail(&user.config.phrase_list,&phrase->node);
								phrase = (struct phrase*)list_find(&user.config.phrase_list,phrase_no);
							}

							if (phrase)
							{
								while (isdigit(*phrase_buf)) phrase_buf++;
								if (*phrase_buf++ == '.')
								{
									if ((result = get_config_item(phrase_buf,"Addresses")))
										phrase->addresses = mystrdup(result);
									if ((result = get_config_item(phrase_buf,"Write.Welcome")))
										phrase->write_welcome = mystrdup(result);
									if ((result = get_config_item(phrase_buf,"Write.WelcomeRcp")))
										phrase->write_welcome_repicient = mystrdup(result);
									if ((result = get_config_item(phrase_buf,"Write.Close")))
										phrase->write_closing = mystrdup(result);
									if ((result = get_config_item(phrase_buf,"Reply.Welcome")))
										phrase->reply_welcome = mystrdup(result);
									if ((result = get_config_item(phrase_buf,"Reply.Intro")))
										phrase->reply_intro = mystrdup(result);
									if ((result = get_config_item(phrase_buf,"Reply.Close")))
										phrase->reply_close = mystrdup(result);
									if ((result = get_config_item(phrase_buf,"Forward.Initial")))
										phrase->forward_initial = mystrdup(result);
									if ((result = get_config_item(phrase_buf,"Forward.Finish")))
										phrase->forward_finish = mystrdup(result);
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
					if (!mystricmp(buf,"begin signature"))
					{
						if (read_line(fh,buf))
						{
							char *name = mystrdup(buf);
							char *sign = NULL;
							struct signature *s;
							while (read_line(fh,buf))
							{
								int sign_len = sign?strlen(sign):0;
								if (!mystricmp(buf,"end signature"))
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
		
		if (user.directory) strcpy(buf,user.directory);
		else buf[0] = 0;
		sm_add_part(buf,".taglines",512);

		if (user.taglines_filename = mystrdup(buf))
			taglines_init(user.taglines_filename);

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
			struct phrase *phrase;
			int i;

			fputs("SMCO\n\n",fh);

			if (user.new_folder_directory) fprintf(fh,"FolderDirectory=%s\n",user.new_folder_directory);
			else fprintf(fh,"FolderDirectory=%s\n",user.folder_directory);

			fprintf(fh,"DST=%s\n",user.config.dst?"Y":"N");

			/* Write out receive stuff */
			fprintf(fh,"Receive.Preselection=%d\n",user.config.receive_preselection);
			fprintf(fh,"Receive.Size=%d\n",user.config.receive_size);
			fprintf(fh,"Receive.Autocheck=%d\n",user.config.receive_autocheck);
			fprintf(fh,"Receive.SoundPlay=%d\n",user.config.receive_sound);
			fprintf(fh,"Receive.SoundFile=%s\n",MAKESTR(user.config.receive_sound_file));

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
				fprintf(fh,"ACCOUNT%d.SMTP.Secure=%s\n",i,account->smtp->secure?"Y":"N");

				fprintf(fh,"ACCOUNT%d.POP3.Server=%s\n",i,MAKESTR(account->pop->name));
				fprintf(fh,"ACCOUNT%d.POP3.Port=%d\n",i,account->pop->port);
				fprintf(fh,"ACCOUNT%d.POP3.Login=%s\n",i,MAKESTR(account->pop->login));
				if (!account->pop->ask) fprintf(fh,"ACCOUNT%d.POP3.Password=%s\n",i,MAKESTR(account->pop->passwd));
				fprintf(fh,"ACCOUNT%d.POP3.Delete=%s\n",i,account->pop->del?"Y":"N");
				fprintf(fh,"ACCOUNT%d.POP3.SSL=%s\n",i,account->pop->ssl?"Y":"N");
				fprintf(fh,"ACCOUNT%d.POP3.STLS=%s\n",i,account->pop->stls?"Y":"N");
				fprintf(fh,"ACCOUNT%d.POP3.Active=%s\n",i,account->pop->active?"Y":"N");
				fprintf(fh,"ACCOUNT%d.POP3.AvoidDupl=%s\n",i,account->pop->nodupl?"Y":"N");
				fprintf(fh,"ACCOUNT%d.POP3.Ask=%s\n",i,account->pop->ask?"Y":"N");
				account = (struct account*)node_next(&account->node);
				i++;
			}

			fprintf(fh,"Signatures.Use=%s\n",user.config.signatures_use?"Y":"N");
			fprintf(fh,"Write.Wrap=%d\n",user.config.write_wrap);
			fprintf(fh,"Write.WrapType=%d\n",user.config.write_wrap_type);
			fprintf(fh,"Write.ReplyQuote=%s\n",user.config.write_reply_quote?"Y":"N");
			fprintf(fh,"Write.ReplyStripSig=%s\n",user.config.write_reply_stripsig?"Y":"N");
			fprintf(fh,"Write.ReplyCiteEmptyL=%s\n",user.config.write_reply_citeemptyl?"Y":"N");

			fprintf(fh,"ReadHeader.Flags=%x\n",user.config.header_flags);
			if (user.config.header_array)
			{
				for (i=0;user.config.header_array[i];i++)
				{
					fprintf(fh,"ReadHeader.HeaderName=%s\n",user.config.header_array[i]);
				}
			}
			fprintf(fh,"Read.PropFont=%s\n",MAKESTR(user.config.read_propfont));
			fprintf(fh,"Read.FixedFont=%s\n",MAKESTR(user.config.read_fixedfont));
			fprintf(fh,"Read.BackgroundColor=0x%x\n",user.config.read_background);
			fprintf(fh,"Read.TextColor=0x%x\n",user.config.read_text);
			fprintf(fh,"Read.QuotedColor=0x%x\n",user.config.read_quoted);
			fprintf(fh,"Read.OldQuotedColor=0x%x\n",user.config.read_old_quoted);
			fprintf(fh,"Read.LinkColor=0x%x\n",user.config.read_link);
			fprintf(fh,"Read.Wordwrap=%s\n",user.config.read_wordwrap?"Y":"N");
			fprintf(fh,"Read.LinkUnderlined=%s\n",user.config.read_link_underlined?"Y":"N");
			fprintf(fh,"Read.Smilies=%s\n",user.config.read_smilies?"Y":"N");

			if (user.config.internet_emails)
			{
				for (i=0;user.config.internet_emails[i];i++)
				{
					fprintf(fh,"ReadHTML.AllowAddress=%s\n",user.config.internet_emails[i]);
				}
			}

			i = 0;
			phrase = (struct phrase*)list_first(&user.config.phrase_list);
			while (phrase)
			{
				fprintf(fh,"Phrase%d.Addresses=%s\n",i,MAKESTR(phrase->addresses));
				fprintf(fh,"Phrase%d.Write.Welcome=%s\n",i,MAKESTR(phrase->write_welcome));
				fprintf(fh,"Phrase%d.Write.WelcomeRcp=%s\n",i,MAKESTR(phrase->write_welcome_repicient));
				fprintf(fh,"Phrase%d.Write.Close=%s\n",i,MAKESTR(phrase->write_closing));
				fprintf(fh,"Phrase%d.Reply.Welcome=%s\n",i,MAKESTR(phrase->reply_welcome));
				fprintf(fh,"Phrase%d.Reply.Intro=%s\n",i,MAKESTR(phrase->reply_intro));
				fprintf(fh,"Phrase%d.Reply.Close=%s\n",i,MAKESTR(phrase->reply_close));
				fprintf(fh,"Phrase%d.Forward.Initial=%s\n",i,MAKESTR(phrase->forward_initial));
				fprintf(fh,"Phrase%d.Forward.Finish=%s\n",i,MAKESTR(phrase->forward_finish));
				phrase = (struct phrase*)node_next(&phrase->node);
				i++;
			}
			fclose(fh);
		}
	}

	if (user.signature_filename)
	{
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

/* Insert a new account into the configuration list */
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

/* Insert a new signature into the configuration list */
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

/* Clear all the phrases */
void clear_config_phrases(void)
{
	struct phrase *p;
	while ((p = (struct phrase*)list_remove_tail(&user.config.phrase_list)))
		phrase_free(p);
}

/* Insert a new phrase into the configuration list */
void insert_config_phrase(struct phrase *phrase)
{
	struct phrase *new_phrase = phrase_duplicate(phrase);
	if (new_phrase)
		list_insert_tail(&user.config.phrase_list,&new_phrase->node);
}

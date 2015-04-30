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
** print.c
*/

#include "print.h"

#include <stdio.h>
#include <stdlib.h>

#include "configuration.h"
#include "lists.h"
#include "mail.h"
#include "parse.h"
#include "smintl.h"

#include "sysprint.h"
#include "support.h"
#include "timesupport.h"

#define ANSI_NORMAL     "[0m"
#define ANSI_BOLD       "[1m"
#define ANSI_UNDER      "[4m"

static int create_ansi_header(FILE *fp, struct mail_complete *mail)
{
	char rc = 0;
	struct mail_complete *m = mail_get_root(mail);
	char *from = mail_find_header_contents(m,"from");
	char *to = mail_find_header_contents(m,"to");
	char *cc = mail_find_header_contents(m, "cc");
	char *replyto = mail_find_header_contents(m, "reply-to");


	if(from && (user.config.header_flags & (SHOW_HEADER_FROM | SHOW_HEADER_ALL)))
	{
		struct mailbox mb;
		parse_mailbox(from, &mb);
		fprintf(fp, ANSI_BOLD "%s:" ANSI_NORMAL " ", _("From"));

		if (mb.phrase)
		{
			fprintf(fp,"%s <%s>",mb.phrase,mb.addr_spec);
		} else
		{
			fprintf(fp, "%s", mb.addr_spec);
		}
		fprintf(fp, "\n");

		if (mb.phrase)  free(mb.phrase);
		if (mb.addr_spec) free(mb.addr_spec);
	}

	if(to && (user.config.header_flags & (SHOW_HEADER_TO | SHOW_HEADER_ALL)))
	{
		struct parse_address p_addr;
		fprintf(fp, ANSI_BOLD "%s:" ANSI_NORMAL " ",_("To"));

		if ((parse_address(to,&p_addr)))
		{
			struct mailbox *mb = (struct mailbox*)list_first(&p_addr.mailbox_list);
			while (mb)
				{
					if (mb->phrase) fprintf(fp,"%s <%s>",mb->phrase,mb->addr_spec);
					else fputs(mb->addr_spec,fp);
					if ((mb = (struct mailbox*) node_next(&mb->node)))
					{
						fputs(", ",fp);
					}
				}
				free_address(&p_addr);
			}
			fputs("\n",fp);
	}

	if(cc && (user.config.header_flags & (SHOW_HEADER_CC | SHOW_HEADER_ALL)))
	{
		struct parse_address p_addr;
		fprintf(fp, ANSI_BOLD "%s:" ANSI_NORMAL " ", _("Carbon Copy"));
		if ((parse_address(cc,&p_addr)))
		{
			struct mailbox *mb = (struct mailbox*)list_first(&p_addr.mailbox_list);
			while (mb)
			{
				if (mb->phrase)
				{
					fprintf(fp,"%s <%s>",mb->phrase,mb->addr_spec);
				}
				else
				{
					fputs(mb->addr_spec,fp);
				}
				if ((mb = (struct mailbox*)node_next(&mb->node)))
				{
					fputs(", ",fp);
				}
			}
			free_address(&p_addr);
		}
		fprintf(fp, "\n");
	}

	if(m->info->subject && (user.config.header_flags & (SHOW_HEADER_SUBJECT|SHOW_HEADER_ALL)))
	{
		fprintf(fp, ANSI_BOLD "%s:" ANSI_NORMAL " %s\n", _("Subject"), m->info->subject);
	}

	if((user.config.header_flags & (SHOW_HEADER_DATE | SHOW_HEADER_ALL)))
	{
		fprintf(fp, ANSI_BOLD "%s:" ANSI_NORMAL " %s\n", _("Date"),sm_get_date_long_str_utf8(m->info->seconds));
	}

	if(replyto && (user.config.header_flags & (SHOW_HEADER_REPLYTO | SHOW_HEADER_ALL)))
	{
		struct mailbox addr;
		parse_mailbox(replyto, &addr);
		fprintf(fp,ANSI_BOLD "%s:" ANSI_NORMAL " ",_("Replies To"));

		if (addr.phrase)
		{
			fprintf(fp,"%s <%s>",addr.phrase,addr.addr_spec);
		} else
		{
			fputs(addr.addr_spec,fp);
		}

		if (addr.phrase)  free(addr.phrase);
		if (addr.addr_spec) free(addr.addr_spec);

		fputs("\n",fp);

	}

	fputs("\n", fp);

	rc = 1;

	return rc;
}

/*
** print_mail - prints a given mail.
**
** These are the system-independent routines.
*/
int print_mail(struct mail_complete *m, int printhdr)
{
	int rc = 0;
	char *text, *buf;
	PrintHandle *ph;
	int txtlen, len;
	FILE *fp;

	mail_decode(m);
	mail_decoded_data(m,(void**)&text,(int*)&txtlen);

	fp = tmpfile();
	if(fp != NULL)
	{
		if(!printhdr || create_ansi_header(fp, m)) /* nasty, I know ;) */
		{
			fwrite(text, txtlen, 1, fp);

			len = ftell(fp);
			if(len != 0)
			{
				fseek(fp, 0, SEEK_SET);
				buf = malloc(len + 1);
				if(buf != NULL)
				{
					if(fread(buf, len, 1, fp) == 1)
					{
						buf[len] = 0;
						ph = sysprint_prepare();
						if(ph != NULL)
						{
							if(sysprint_print(ph, buf, len))
							{
								rc = 1; /* Mark success. */
							}
							sysprint_cleanup(ph);
						}
					}
					else
					{
						sm_request(NULL, _("I/O-Error."), _("Okay"));
					}
					free(buf);
				}
				else
				{
					sm_request(NULL, _("Not enough memory."), _("Okay"));
				}
			}
			else
			{
				sm_request(NULL, _("Internal error."), _("Okay"));
			}
		}
		fclose(fp);
	}
	else
	{
		sm_request(NULL, _("Can\'t create temporary file."), _("Okay"));
	}

	return rc;
}

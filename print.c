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

#include <stdlib.h>
#include <stdio.h>

#include "print.h"
#include "mail.h"
#include "configuration.h"
#include "support.h"
#include "smintl.h"

#include "sysprint.h"

#define ANSI_NORMAL     "[0m"
#define ANSI_BOLD       "[1m"
#define ANSI_UNDER      "[4m"

static int create_ansi_header(FILE *fp, struct mail *m)
{
	char rc = 0;
	char *from = mail_find_header_contents(m,"from");
	char *to = mail_find_header_contents(m,"to");
	char *cc = mail_find_header_contents(m, "cc");
	char *replyto = mail_find_header_contents(m, "reply-to");

	if(from && (user.config.header_flags & (SHOW_HEADER_FROM | SHOW_HEADER_ALL)))
	{
		fprintf(fp, ANSI_BOLD "%s:" ANSI_NORMAL " %s\n", _("From"), from);
	}

	if(to && (user.config.header_flags & (SHOW_HEADER_TO | SHOW_HEADER_ALL)))
	{
		fprintf(fp, ANSI_BOLD "%s:" ANSI_NORMAL " %s\n", _("To"), to);
	}

	if(cc && (user.config.header_flags & (SHOW_HEADER_CC | SHOW_HEADER_ALL)))
	{
		fprintf(fp, ANSI_BOLD "%s:" ANSI_NORMAL " %s\n", _("CC"), cc);
	}

	if(m->subject && (user.config.header_flags & (SHOW_HEADER_SUBJECT|SHOW_HEADER_ALL)))
	{
		fprintf(fp, ANSI_BOLD "%s:" ANSI_NORMAL" %s\n", _("Subject"), m->subject);
	}

	if((user.config.header_flags & (SHOW_HEADER_DATE | SHOW_HEADER_ALL)))
	{
		fprintf(fp, ANSI_BOLD "%s:" ANSI_NORMAL " %s\n", _("Date"),sm_get_date_long_str(m->seconds));
	}

	if(replyto && (user.config.header_flags & (SHOW_HEADER_REPLYTO | SHOW_HEADER_ALL)))
	{
		fprintf(fp, ANSI_BOLD "%s" ANSI_NORMAL "%s\n", _("ReplyTo"), replyto);
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
int print_mail(struct mail *m)
{
	int rc = 0;
	char *text, *buf;
	PrintHandle *ph;
	unsigned long txtlen, len;
	FILE *fp;

	mail_decode(m);

	if(m->decoded_data)
	{
		text = m->decoded_data;
		txtlen = m->decoded_len;
	}
	else
	{
		text = m->text+m->text_begin;
		txtlen = m->text_len;
	}

	fp = tmpfile();
	if(fp != NULL)
	{
		if(create_ansi_header(fp, m))
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

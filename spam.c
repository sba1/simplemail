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
** spam.c
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "hash.h"
#include "mail.h"
#include "folder.h"

#include "support_indep.h"

#define MYDEBUG 1
#include "debug.h"

static struct hash_table spam_table;
static struct hash_table ham_table;

/**************************************************************************
 ...
**************************************************************************/
int spam_init(void)
{
	if (hash_table_init(&spam_table, 13, "PROGDIR:.spam.stat"))
	{
		if (hash_table_init(&ham_table, 13, "PROGDIR:.ham.stat"))
		{
			return 1;
		}
	}
	return 0;
}

/**************************************************************************
 ...
**************************************************************************/
void spam_cleanup(void)
{
	hash_table_store(&spam_table);
	hash_table_store(&ham_table);
}

/**************************************************************************
 ...
**************************************************************************/
static int spam_feed_contents(struct hash_table *ht, const char *line, char *prefix)
{
	const char *buf_start, *buf;
	unsigned int c;
	buf_start = buf = line;

	while ((c = *buf))
	{
		if (c == ' ' || c == '\n' || c == '\r')
		{
			int len = buf - buf_start;
			if (len > 0)
			{
				char *token = (char*)malloc(len+1);
				if (token)
				{
					struct hash_entry *entry;

					strncpy(token,buf_start,len);
					token[len]=0;

					if (!(entry = hash_table_lookup(ht,token)))
					{
						entry = hash_table_insert(ht,token,0);
					} else free(token);

					if (entry)
					{
						entry->data++;
					}
				}
			}

			buf++;

			while ((c=*buf))
			{
				if (c != ' ' && c != '\n' && c != '\r') break;
				buf++;
			}
			buf_start = buf;
			continue;
		}

		buf++;
	}
	return 1;
}

/**************************************************************************
 
**************************************************************************/
static void spam_feed_parsed_mail(struct hash_table *ht, struct mail *mail)
{
	if (mail->num_multiparts == 0)
	{
		if (!mystricmp("text",mail->content_type))
		{
			void *cont;
			char *cont_dup;
			int cont_len;

			mail_decode(mail);
			mail_decoded_data(mail,&cont,&cont_len);

			if (cont)
			{
				cont_dup = mystrndup((char*)cont,cont_len); /* we duplicate this only because we need a null byte */
				spam_feed_contents(ht,cont_dup,NULL);
				free(cont_dup);
			}
		}
	} else
	{
		int i;
		for (i = 0;i<mail->num_multiparts;i++)
		{
			spam_feed_parsed_mail(ht,mail->multipart_array[i]);
		}
	}
}

/**************************************************************************
 ...
**************************************************************************/
static int spam_feed_mail(struct folder *folder, struct mail *to_parse_mail, struct hash_table *ht)
{
	char path[380];
	struct mail *mail;
	int rc = 0;

	if (getcwd(path, sizeof(path)) == NULL) return 0;

	chdir(folder->path);

	if ((mail = mail_create_from_file(to_parse_mail->filename)))
	{
		mail_read_contents("",mail);

		if (mail->subject) spam_feed_contents(ht,mail->subject,"*Subject:");
		spam_feed_parsed_mail(ht,mail);

		mail_free(mail);
	}
	chdir(path);

	return rc;
}

/**************************************************************************
 ...
**************************************************************************/
int spam_feed_mail_as_spam(struct folder *folder, struct mail *mail)
{
	return spam_feed_mail(folder,mail,&spam_table);
}

/**************************************************************************
 ...
**************************************************************************/
int spam_feed_mail_as_ham(struct folder *folder, struct mail *mail)
{
	return spam_feed_mail(folder,mail,&ham_table);
}

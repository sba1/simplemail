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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "hash.h"
#include "mail.h"
#include "folder.h"

#include "support_indep.h"

#define MYDEBUG 1
#include "debug.h"

static struct hash_table spam_table;
static struct hash_table ham_table;
/*static struct hash_table spam_prob_table;*/
static int rebuild_spam_prob_hash_table;

/**************************************************************************
 ...
**************************************************************************/
int spam_init(void)
{
	if (hash_table_init(&spam_table, 13, "PROGDIR:.spam.stat"))
	{
		if (hash_table_init(&ham_table, 13, "PROGDIR:.ham.stat"))
		{
/*			if (hash_table_init(&spam_prob_table, 13, NULL))*/
			{
				rebuild_spam_prob_hash_table = 1;
				return 1;
			}
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
 Checks if a given html tag is known
**************************************************************************/
static int spam_is_known_html_tag(const char *tag)
{
	/* TODO: Use a trie */
	if (!mystrnicmp(tag,"html",4)) return 1;
	else if (!mystrnicmp(tag,"/html",5)) return 1;
	else if (!mystrnicmp(tag,"font",4)) return 1;
	else if (!mystrnicmp(tag,"/font",5)) return 1;
	else if (!mystrnicmp(tag,"body",4)) return 1;
	else if (!mystrnicmp(tag,"/body",5)) return 1;
	else if (!mystrnicmp(tag,"a ",2)) return 1;
	else if (!mystrnicmp(tag,"/a",2)) return 1;
	else if (!mystrnicmp(tag,"br",2)) return 1;
	else if (!mystrnicmp(tag,"/br",3)) return 1;
	else if (!mystrnicmp(tag,"div",3)) return 1;
	else if (!mystrnicmp(tag,"/div",4)) return 1;
	else if (!mystrnicmp(tag,"p ",2)) return 1;
	else if (!mystrnicmp(tag,"/p",3)) return 1;
	else if (!mystrnicmp(tag,"table",5)) return 1;
	else if (!mystrnicmp(tag,"/table",6)) return 1;
	else if (!mystrnicmp(tag,"td",2)) return 1;
	else if (!mystrnicmp(tag,"/td",3)) return 1;
	else if (!mystrnicmp(tag,"tr",2)) return 1;
	else if (!mystrnicmp(tag,"/tr",3)) return 1;
	else if (!mystrnicmp(tag,"th",2)) return 1;
	else if (!mystrnicmp(tag,"/th",3)) return 1;

	return 0;
}

/**************************************************************************
 Extracts all token from the given text and calls the callback function 
 for every token. If callback function returns 0 the token will be freed
 and the function is immediatly aborted. It's save to call this function
 with a NULL text pointer (in which case the call will succeed)
**************************************************************************/
static int spam_tokenize(const char *text, const char *prefix, int (*callback)(char *token, void *data), void *data)
{
	const char *buf_start, *buf;
	char *filtered_text, *dest_buf;
	unsigned int c;
	int html = 0;

	int prefix_len;

	if (!text) return 1;

	if (prefix) prefix_len = strlen(prefix);
	else prefix_len = 0;

	if (!(dest_buf = filtered_text = (char*)malloc(strlen(text)+1))) return 0;
	buf_start = buf = text;
	/* Filter out unknown html tags */
	while ((c = *buf))
	{
		if (c == '<')
		{
			buf_start = buf;
			html = 1;
			buf++;
			continue;
		}
		else if (c== '>' && html)
		{
			const char *cmd_start = buf_start + 1;
			while (isspace((unsigned char)(*cmd_start)))
				cmd_start++;

			buf++;

			if (spam_is_known_html_tag(cmd_start))
			{
				strncpy(dest_buf, buf_start, buf - buf_start);
				dest_buf += buf - buf_start;
			}

			buf_start = buf;
			html = 0;
			continue;			
		}

		if (!html)
			*dest_buf++ = c;
		buf++;
	}
	*dest_buf = 0;

	buf_start = buf = filtered_text;

	while ((c = *buf))
	{
		if (c == '"' || c == '(' || c == ')' || c == '\\' || c == ',' || c < 33 || c == '<' || c == '>')
		{
			int len = buf - buf_start;
			if (c == '>') len++;

			if (len > 0)
			{
				char *token = (char*)malloc(len+prefix_len+1);
				if (token)
				{
					if (prefix_len) strcpy(token,prefix);
					strncpy(&token[prefix_len],buf_start,len);
					token[prefix_len + len]=0;

					if (!callback(token,data))
					{
						free(token);
						return 0;
					}
				}
			}

			/* so that we also get the beginning of the tag */
			if (c == '<') buf_start = buf++;
			else
			{
				buf++;
				while ((c=*buf))
				{
					if (c != '"' && c != '(' && c != ')' && c != '\\' && c != ',' &&  c > 32) break;
					buf++;
				}
				buf_start = buf;
			}
			continue;
		}

		buf++;
	}
	free(filtered_text);
	return 1;
}

/**************************************************************************
 Is called for every token
**************************************************************************/
static int spam_feed_hash_table_callback(char *token, void *data)
{
	struct hash_table *ht = (struct hash_table*)data;
	struct hash_entry *entry;

	if (!(entry = hash_table_lookup(ht,token)))
	{
		entry = hash_table_insert(ht,token,0);
	} else free(token);

	if (entry)
	{
		entry->data++;
		return 1;
	}
	return 0;
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
				if ((cont_dup = mystrndup((char*)cont,cont_len))) /* we duplicate this only because we need a null byte */
				{
					spam_tokenize(cont_dup,NULL,spam_feed_hash_table_callback,ht);
					free(cont_dup);
				}
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

		spam_tokenize(mail->subject,"*Subject:",spam_feed_hash_table_callback,ht);
		spam_feed_parsed_mail(ht,mail);

		rc = 1;

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
	int rc = spam_feed_mail(folder,mail,&spam_table);
	if (rc)
	{
		spam_table.data++;
		rebuild_spam_prob_hash_table = 1;
	}
	return rc;
}

/**************************************************************************
 ...
**************************************************************************/
int spam_feed_mail_as_ham(struct folder *folder, struct mail *mail)
{
	int rc = spam_feed_mail(folder,mail,&ham_table);
	if (rc)
	{
		ham_table.data++;
		rebuild_spam_prob_hash_table = 1;
	}
	return rc;
}

/* Holds the probability that a mail containing this word is indeed spam */
struct spam_token_probability
{
	const char *string;
	double prob;
};

#define NUM_OF_PROBABILITIES 20

/**************************************************************************
 
**************************************************************************/
int spam_extract_prob_callback(char *token, void *data)
{
	struct spam_token_probability *prob = (struct spam_token_probability*)data;
	struct hash_entry *entry;
	int i,taken = 0,spam, ham;
	double spamn, hamn;
	struct spam_token_probability sprob;

	int num_of_spam;
	int num_of_ham;
	
	if (!(num_of_spam = spam_table.data)) num_of_spam = 1;
	if (!(num_of_ham = ham_table.data)) num_of_ham = 1;

	if ((entry = hash_table_lookup(&spam_table,token))) spam = entry->data;
	else spam = 0;
	if (!spam) spamn = 0.01;
	else spamn = spam;

	if ((entry = hash_table_lookup(&ham_table,token))) ham = entry->data;
	else ham = 0;
	if (!ham) hamn = 0.01;
	else hamn = ham*2;

	sprob.string = token;

	if (ham || spam)
	{
		sprob.prob = (spamn/num_of_spam) / (spamn/num_of_spam + hamn/num_of_ham);
		if (sprob.prob > 0.99) sprob.prob = 0.99;
		else if (sprob.prob < 0.01) sprob.prob = 0.01;
	}
	else if (ham) sprob.prob = 0.01;
	else sprob.prob = 0.4;

	for (i=0;i<NUM_OF_PROBABILITIES;i++)
	{
		if (!mystrcmp(sprob.string,prob[i].string))
			break;

		if (!(prob[i].string != NULL) || (fabs(sprob.prob - 0.5) > fabs(prob[i].prob - 0.5)))
		{
			int j;
			free((char*)prob[NUM_OF_PROBABILITIES-1].string); /* does nothing if NULL */
			for (j=NUM_OF_PROBABILITIES-1; j>i;j--)
				prob[j]=prob[j-1];
			prob[i] = sprob;
			taken = 1;
			break;
		}
	}
	if (!taken) free(token);
	return 1;
}

/**************************************************************************
 
**************************************************************************/
static void spam_extract_parsed_mail(struct spam_token_probability *prob, struct mail *mail)
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
				if ((cont_dup = mystrndup((char*)cont,cont_len))) /* we duplicate this only because we need a null byte */
				{
					spam_tokenize(cont_dup,NULL,spam_extract_prob_callback,prob);
					free(cont_dup);
				}
			}
		}
	} else
	{
		int i;
		for (i = 0;i<mail->num_multiparts;i++)
		{
			spam_extract_parsed_mail(prob,mail->multipart_array[i]);
		}
	}
}

/**************************************************************************
 Determines wheater a mail is spam or not.
**************************************************************************/
int spam_is_mail_spam(struct folder *folder, struct mail *to_check_mail)
{
	char path[380];
	struct mail *mail;
	int i,rc = 0;
	struct spam_token_probability prob[NUM_OF_PROBABILITIES];

	for (i=0;i<NUM_OF_PROBABILITIES;i++)
	{
		prob[i].string = NULL;
		prob[i].prob = 0.0;
	}

/*
	if (rebuild_spam_prob_hash_table)
		hash_table_clear(&spam_prob_table);
*/

	if (getcwd(path, sizeof(path)) == NULL) return 0;
	chdir(folder->path);

	if ((mail = mail_create_from_file(to_check_mail->filename)))
	{
		double prod;
		double prod2;

		double p;

		mail_read_contents("",mail);
		spam_tokenize(mail->subject,"*Subject:",spam_extract_prob_callback,prob);
		spam_extract_parsed_mail(prob,mail);

		prod = prob[0].prob;
		for (i=1;i<NUM_OF_PROBABILITIES;i++)
			prod *= prob[i].prob;

		prod2 = 1 - prob[0].prob;
		for (i=1;i<NUM_OF_PROBABILITIES;i++)
			prod2 *= 1 - prob[i].prob;

		p = prod/(prod+prod2);
		if (p>0.99) p=0.99;

/*
		for (i=0;i<NUM_OF_PROBABILITIES;i++)
		{
			if (prob[i].string)
			{
				Printf("%s %ld\n",prob[i].string,(int)(prob[i].prob*1000.0));
			}
		}

		Printf("Prob: 0.%ld\n\n",(int)(p*100.0));
*/

		if (p > 0.9) rc = 1;
		mail_free(mail);
	}
	chdir(path);
	return rc;
}

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

/**
 * @file spam.c
 */

#include "spam.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "folder.h"
#include "hash.h"
#include "mail.h"
#include "support_indep.h"

#include "subthreads.h"

static struct hash_table spam_table;
static struct hash_table ham_table;
/*static struct hash_table spam_prob_table;*/
static int rebuild_spam_prob_hash_table;

static semaphore_t sem;

/*****************************************************************************/

int spam_init(void)
{
	if ((sem = thread_create_semaphore()))
	{
		if (hash_table_init(&spam_table, 13, "PROGDIR:.spam.stat"))
		{
			if (hash_table_init(&ham_table, 13, "PROGDIR:.ham.stat"))
			{

/*				if (hash_table_init(&spam_prob_table, 13, NULL))*/
				{
					rebuild_spam_prob_hash_table = 1;
					return 1;
				}
			}
		}
	}
	return 0;
}

/*****************************************************************************/

void spam_cleanup(void)
{
	hash_table_store(&spam_table);
	hash_table_store(&ham_table);

	hash_table_clean(&spam_table);
	hash_table_clean(&ham_table);

	thread_dispose_semaphore(sem);
}

/**
 * Checks if a given html tag is known.
 *
 * @param tag
 * @return 1 if the html tag is known
 */
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

/**
 * Extracts all token from the given text and calls the callback function
 * for every token. If callback function returns 0 the token will be freed
 * and the function is immediately aborted. It's save to call this function
 * with a NULL text pointer (in which case the call will succeed)
 *
 * @param text the text to be processed
 * @param prefix adds the given prefix
 * @param callback callback that is called for every token
 * @param data the user data that is supplied to the callback.
 * @return 0 on a error, else 1.
 */
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

/**
 * Callback to feed the given token into a hashtable.
 *
 * @param token the current token
 * @param data a hashtable
 * @return 1 on success, 0 on error
 */
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

/**
 * Process the given complete mail for counting its text token in the given
 * hash table.
 *
 * @param ht the hash table in which the tokens' occurrence are counted
 * @param mail the complete mail
 */
static void spam_feed_parsed_mail(struct hash_table *ht, struct mail_complete *mail)
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

/**
 * Feed the mail into the hash table.
 *
 * @param folder the folder where the mail is located
 * @param to_parse_mail the mail to be parsed.
 * @param ht the occurrences hash table
 * @return 1 on success, 0 otherwise.
 */
static int spam_feed_mail(struct folder *folder, struct mail_info *to_parse_mail, struct hash_table *ht)
{
	char path[380];
	struct mail_complete *mail;
	int rc = 0;

	if (getcwd(path, sizeof(path)) == NULL) return 0;

	chdir(folder->path);

	if ((mail = mail_complete_create_from_file(to_parse_mail->filename)))
	{
		mail_read_contents("",mail);

		spam_tokenize(mail->info->subject,"*Subject:",spam_feed_hash_table_callback,ht);
		spam_feed_parsed_mail(ht,mail);

		rc = 1;

		mail_complete_free(mail);
	}
	chdir(path);

	return rc;
}

/*****************************************************************************/

int spam_feed_mail_as_spam(struct folder *folder, struct mail_info *mail)
{
	int rc;
	thread_lock_semaphore(sem);
	if ((rc = spam_feed_mail(folder,mail,&spam_table)))
	{
		spam_table.data++;
		rebuild_spam_prob_hash_table = 1;
	}
	thread_unlock_semaphore(sem);
	return rc;
}

/*****************************************************************************/

int spam_feed_mail_as_ham(struct folder *folder, struct mail_info *mail)
{
	int rc;

	thread_lock_semaphore(sem);

	if ((rc = spam_feed_mail(folder,mail,&ham_table)))
	{
		ham_table.data++;
		rebuild_spam_prob_hash_table = 1;
	}

	thread_unlock_semaphore(sem);
	return rc;
}

/**
 * Holds the probability that a mail containing this word is indeed spam
 */
struct spam_token_probability
{
	const char *string;
	double prob;
};

#define NUM_OF_PROBABILITIES 20

/**
 * Calculate the token probability for being spam and update the given
 * spam_token_probability vector accordingly.
 *
 * @param token the token to e
 * @param data pointer to the first element of a vector of NUM_OF_PROBABILITIES
 *  spam_token_probability elements.
 * @return
 */
static int spam_extract_prob_callback(char *token, void *data)
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

/**
 * Calculate the probabilities of the given parsed mail.
 *
 * @param prob vector of NUM_OF_PROBABILITIES entries that holds the probabilities.
 * @param mail the mail for which to calculate the probabilities.
 */
static void spam_extract_parsed_mail(struct spam_token_probability *prob, struct mail_complete *mail)
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

/**
 * Determines whether a mail is spam or not via statistics.
 * folder_path maybe NULL.
 *
 * @param folder_path
 * @param to_check_mail
 * @return
 */
static int spam_is_mail_spam_using_statistics(char *folder_path, struct mail_info *to_check_mail)
{
	char path[380];
	struct mail_complete *mail;
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

	/* change the path */
	if (folder_path)
	{
		if (getcwd(path, sizeof(path)) == NULL) return 0;
		chdir(folder_path);
	}

	if ((mail = mail_complete_create_from_file(to_check_mail->filename)))
	{
		double prod;
		double prod2;

		double p;

		mail_read_contents("",mail);
		spam_tokenize(mail->info->subject,"*Subject:",spam_extract_prob_callback,prob);
		spam_extract_parsed_mail(prob,mail);

		prod = prob[0].prob;
		for (i=1;i<NUM_OF_PROBABILITIES;i++)
			prod *= prob[i].prob;

		prod2 = 1 - prob[0].prob;
		for (i=1;i<NUM_OF_PROBABILITIES;i++)
			prod2 *= 1 - prob[i].prob;

		p = prod/(prod+prod2);
		if (p>0.99) p=0.99;

		if (p > 0.9) rc = 1;
		mail_complete_free(mail);
	}

	/* change the path back */
	if (folder_path)
	{
		chdir(path);
	}

	return rc;
}

/*****************************************************************************/

int spam_is_mail_spam(char *folder_path, struct mail_info *to_check_mail, char **white, char **black)
{
	char *from_addr;
	int rc;

	thread_lock_semaphore(sem);
	if ((from_addr = to_check_mail->from_addr))
	{
		if (array_contains(white,from_addr)) {
			thread_unlock_semaphore(sem);
			return 0;
        }

		if (array_contains(black,from_addr)) {
			thread_unlock_semaphore(sem);
			return 1;
        }
	}

	rc = spam_is_mail_spam_using_statistics(folder_path,to_check_mail);
	thread_unlock_semaphore(sem);

	return rc;
}

/*****************************************************************************/

unsigned int spam_num_of_spam_classified_mails(void)
{
	return spam_table.data;
}

/*****************************************************************************/

unsigned int spam_num_of_ham_classified_mails(void)
{
	return ham_table.data;
}

/*****************************************************************************/

void spam_reset_ham(void)
{
	thread_lock_semaphore(sem);
	hash_table_clear(&ham_table);
	thread_unlock_semaphore(sem);
}

/*****************************************************************************/

void spam_reset_spam(void)
{
	thread_lock_semaphore(sem);
	hash_table_clear(&spam_table);
	thread_unlock_semaphore(sem);
}


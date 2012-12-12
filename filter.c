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
** filter.c
*/

#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "boyermoore.h"
#include "configuration.h"
#include "debug.h"
#include "lists.h"
#include "filter.h"
#include "support.h"
#include "support_indep.h"

int read_line(FILE *fh, char *buf); /* in addressbook.c */
char *get_config_item(char *buf, char *item); /* configuration.c */
/* Duplicates a config string and converts it to utf8 if not already done */
char *dupconfigstr(char *str, int utf8);

/**
 * Creates a new filter instance with default values.
 *
 * @return
 */
struct filter *filter_create(void)
{
	struct filter *f = malloc(sizeof(struct filter));
	if (f)
	{
		memset(f,0,sizeof(struct filter));
		f->name = mystrdup("Unnamed filter");
		list_init(&f->rules_list);
		list_init(&f->action_list);
	}
	return f;
}

/**
 * Deinitializes the parsed filter rule. Does not free the rule!
 *
 * @param p
 */
void filter_deinit_rule(struct filter_rule_parsed *p)
{
	boyermoore_delete_context(p->bm_context);
	p->bm_context = NULL;

	free(p->parsed);
	p->parsed = NULL;
}

/**
 * Initializes the given filter rule for the given str with flags.
 *
 * @param p
 * @param str
 * @param flags
 */
void filter_init_rule(struct filter_rule_parsed *p, char *str, int flags)
{
	int strl;

	filter_deinit_rule(p);

	strl = strlen(str);

	if (strl > 3 &&  (flags & SM_PATTERN_NOPATT) && (flags & SM_PATTERN_SUBSTR) && (flags & SM_PATTERN_NOCASE))
		p->bm_context = boyermoore_create_context(str,strl);
	else
		p->parsed = sm_parse_pattern(str, flags);
}

/**
 * Match the given str againt the parsed filter.
 *
 * @param p
 * @param str should be 0-byte terminated even if strl is given.
 * @param strl length of the string.
 * @param flags
 * @return
 */
int filter_match_rule_len(struct filter_rule_parsed *p, char *str, int strl, int flags)
{
	if (p->bm_context)
		return boyermoore(p->bm_context,str,strl,NULL,NULL) != -1;
	return sm_match_pattern(p->parsed,str,flags);
}

/**
 * Preprocesses the pattern of all rules of the given filter
 *
 * @param f the filter of which the rules should be processed
 */
void filter_parse_filter_rules(struct filter *f)
{
	struct filter_rule *rule;

	if (!f) return;

	rule = (struct filter_rule*)list_first(&f->rules_list);
	while (rule)
	{
		switch (rule->type)
		{
			case	RULE_FROM_MATCH:
						if (rule->u.from.from_pat) array_free(rule->u.from.from_pat);
						rule->u.from.from_pat = array_duplicate_parsed(rule->u.from.from, rule->flags);
						break;

			case	RULE_SUBJECT_MATCH:
						if (rule->u.subject.subject_pat) array_free(rule->u.subject.subject_pat);
						rule->u.subject.subject_pat = array_duplicate_parsed(rule->u.subject.subject, rule->flags);
						break;

			case	RULE_HEADER_MATCH:
						if (rule->u.header.name_pat) free(rule->u.header.name_pat);
						/* patternmatching for headerfields disabled for now */
						rule->u.header.name_pat = sm_parse_pattern(rule->u.header.name, SM_PATTERN_NOCASE|SM_PATTERN_NOPATT);
						if (rule->u.header.contents_pat) array_free(rule->u.header.contents_pat);
						rule->u.header.contents_pat = array_duplicate_parsed(rule->u.header.contents, rule->flags);
						break;

			case	RULE_RCPT_MATCH:
						if (rule->u.rcpt.rcpt_pat) array_free(rule->u.rcpt.rcpt_pat);
						rule->u.rcpt.rcpt_pat = array_duplicate_parsed(rule->u.rcpt.rcpt, rule->flags);
						break;

			case	RULE_BODY_MATCH:
						filter_deinit_rule(&rule->u.body.body_parsed);
						filter_init_rule(&rule->u.body.body_parsed,rule->u.body.body, rule->flags);
						break;
		}
		rule = (struct filter_rule*)node_next(&rule->node);
	}
}

/**
 * Preparse the pattern of all filters.
 */
void filter_parse_all_filters(void)
{
	struct filter *f = filter_list_first();
	while (f)
	{
		filter_parse_filter_rules(f);
		f = filter_list_next(f);
	}
}

/**
 * Duplicates a filter instance.
 *
 * @param filter
 * @return
 */
struct filter *filter_duplicate(struct filter *filter)
{
	struct filter *f = malloc(sizeof(struct filter));
	if (f)
	{
		struct filter_rule *rule;

		*f = *filter;
		f->name = mystrdup(f->name);
		f->dest_folder = mystrdup(f->dest_folder);

		list_init(&f->rules_list);

		rule = (struct filter_rule*)list_first(&filter->rules_list);
		while (rule)
		{
			struct filter_rule *new_rule = (struct filter_rule*)malloc(sizeof(struct filter_rule));
			if (new_rule)
			{
				memset(new_rule,0,sizeof(struct filter_rule));

				new_rule->type = rule->type;
				new_rule->flags = rule->flags;

				switch (new_rule->type)
				{
					case	RULE_FROM_MATCH:
								new_rule->u.from.from = array_duplicate(rule->u.from.from);
								break;

					case	RULE_SUBJECT_MATCH:
								new_rule->u.subject.subject = array_duplicate(rule->u.subject.subject);
								break;

					case	RULE_HEADER_MATCH:
								new_rule->u.header.name = mystrdup(rule->u.header.name);
								new_rule->u.header.contents = array_duplicate(rule->u.header.contents);
								break;

					case	RULE_STATUS_MATCH:
								new_rule->u.status.status = rule->u.status.status;
								break;

					case	RULE_RCPT_MATCH:
								new_rule->u.rcpt.rcpt = array_duplicate(rule->u.rcpt.rcpt);
								break;

					case	RULE_BODY_MATCH:
								new_rule->u.body.body = mystrdup(rule->u.body.body);
								break;
				}

				list_insert_tail(&f->rules_list, &new_rule->node);
			}

			rule = (struct filter_rule*)node_next(&rule->node);
		}
		filter_parse_filter_rules(f);
	}
	return f;
}

/**
 * Disposes the filter and all associated rules.
 *
 * @param f
 */
void filter_dispose(struct filter *f)
{
	struct filter_rule *rule;

	if (f->name) free(f->name);
	if (f->dest_folder) free(f->dest_folder);
	while ((rule = (struct filter_rule*)list_last(&f->rules_list)))
	{
		filter_remove_rule(rule);
	}
	free(f);
}

/**
 * Adds the given rule to the filter.
 *
 * @param filter
 * @param fr
 */
void filter_add_rule(struct filter *filter, struct filter_rule *fr)
{
	list_insert_tail(&filter->rules_list,&fr->node);
}

/**
 * Creates and adds a rule of the given type to the given filter.
 *
 * @param filter
 * @param type
 * @return
 */
struct filter_rule *filter_create_and_add_rule(struct filter *filter, int type)
{
	struct filter_rule *rule = (struct filter_rule*)malloc(sizeof(struct filter_rule));
	if (rule)
	{
		memset(rule, 0, sizeof(struct filter_rule));
		rule->type = type;
		rule->flags = SM_PATTERN_NOCASE|SM_PATTERN_NOPATT;
		filter_add_rule(filter,rule);
	}
	return rule;
}

/**
 * Adds the copy of the given text to the filter rule if rule
 * is a text rule.
 *
 * @param r
 * @param text
 */
void filter_rule_add_copy_of_string(struct filter_rule *fr, char *text)
{
	char ***dst;

	switch (fr->type)
	{
		case	RULE_FROM_MATCH: dst = &fr->u.from.from; break;
		case	RULE_RCPT_MATCH: dst = &fr->u.rcpt.rcpt; break;
		case	RULE_SUBJECT_MATCH: dst = &fr->u.subject.subject; break;
		case	RULE_HEADER_MATCH: dst = &fr->u.header.contents; break;

		default:
				dst = NULL;
				break;
	}

	if (dst)
	{
		*dst = array_add_string(*dst,text);
	} else
	{
		if (fr->type == RULE_BODY_MATCH)
		{
			if (fr->u.body.body) free(fr->u.body.body);
			fr->u.body.body = mystrdup(text);
		}
	}
}

/**
 * Create a filter rule that matches the given set of strings.
 *
 * @param strings
 * @param num_strings
 * @param flags
 * @return
 */
struct filter_rule *filter_rule_create_from_strings(char **strings, int num_strings, int type)
{
	int pos_in_a;
	int len;

	struct filter_rule *rule;

	char *common_text;

	switch (type)
	{
		case	RULE_FROM_MATCH:
		case	RULE_RCPT_MATCH:
		case	RULE_SUBJECT_MATCH:
		case	RULE_HEADER_MATCH:
		case	RULE_ATTACHMENT_MATCH:
		case	RULE_BODY_MATCH:
				break;

		default:
				/* Unsupported rule */
				SM_DEBUGF(10,("Unsupported type for string rule\n"));
				return NULL;
	}

	if (!(longest_common_substring((const char**)strings,num_strings,&pos_in_a,&len)))
		return NULL;

	if (!(rule = (struct filter_rule*)malloc(sizeof(struct filter_rule))))
		return NULL;

	memset(rule, 0, sizeof(struct filter_rule));
	rule->type = type;
	rule->flags = SM_PATTERN_NOPATT|SM_PATTERN_SUBSTR;

	if (!(common_text = mystrndup(&strings[0][pos_in_a],len)))
		return NULL;

	filter_rule_add_copy_of_string(rule,common_text);

	free(common_text);

	return rule;
}

/**
 * Create a filter rule from common sorted recpipients.
 *
 * @param addresses an array to an array of sorted recipients.
 * @param num_addresses length of addresses.
 * @param type
 * @return
 */
struct filter_rule *filter_rule_create_from_common_sorted_recipients(char ***addresses, unsigned int num_addresses)
{
	/* This is a very naive implementation, which could be optimized */
	int i,j;
	struct filter_rule *r;
	char **common_recipients;

	if (num_addresses < 1)
		return NULL;

	common_recipients = NULL;

	for (i=0;addresses[0][i];i++)
	{
		char *addr = addresses[0][i];

		int take = 1;

		for (j=1;j<num_addresses;j++)
		{
			if (!array_contains_utf8(addresses[j],addr))
			{
				take = 0;
				break;
			}
		}

		if (take)
			common_recipients = array_add_string(common_recipients,addr);
	}

	if (!common_recipients || array_length(common_recipients) == 0)
		return NULL;

	if (!(r = (struct filter_rule*)malloc(sizeof(struct filter_rule))))
	{
		free(common_recipients);
		return NULL;
	}

	memset(r, 0, sizeof(*r));
	r->type = RULE_RCPT_MATCH;
	r->flags = SM_PATTERN_NOPATT|SM_PATTERN_SUBSTR;

	filter_rule_add_copy_of_string(r,common_recipients[0]);
	array_free(common_recipients);

	return r;
}

/**
 * Remove the rule from its filter.
 *
 * @param fr
 */
void filter_remove_rule(struct filter_rule *fr)
{
	if (fr)
	{
		switch (fr->type)
		{
			case	RULE_FROM_MATCH:
						array_free(fr->u.from.from);
						array_free(fr->u.from.from_pat);
						break;

			case	RULE_RCPT_MATCH:
						array_free(fr->u.rcpt.rcpt);
						array_free(fr->u.rcpt.rcpt_pat);
						break;

			case	RULE_SUBJECT_MATCH:
						array_free(fr->u.subject.subject);
						array_free(fr->u.subject.subject_pat);
						break;

			case	RULE_HEADER_MATCH:
						free(fr->u.header.name);
						free(fr->u.header.name_pat);
						array_free(fr->u.header.contents);
						array_free(fr->u.header.contents_pat);
						break;

			case	RULE_BODY_MATCH:
						free(fr->u.body.body);
						filter_deinit_rule(&fr->u.body.body_parsed);
						break;
		}
		node_remove(&fr->node);
		free(fr);
	}
}

/**
 * Return the num-th rule of the given filter.
 *
 * @param filter
 * @param num
 * @return
 */
struct filter_rule *filter_find_rule(struct filter *filter, int num)
{
	return (struct filter_rule *)list_find(&filter->rules_list,num);
}


#if 0
/**************************************************************************
 Returns a string from a rule
**************************************************************************/
char *filter_get_rule_string(struct filter_rule *rule)
{
	static char buf[1024];
	switch(rule->type)
	{
		case	RULE_FROM_MATCH:
					if (rule->u.from.from && *rule->u.from.from) sprintf(buf,"From contains \"%s\"",*rule->u.from.from);
					else strcpy(buf,"From match");
					break;

		case	RULE_SUBJECT_MATCH:
					if (rule->u.subject.subject && *rule->u.subject.subject) sprintf(buf,"Subject contains \"%s\"",*rule->u.subject.subject);
					else strcpy(buf,"Subject match");
					break;

		case	RULE_HEADER_MATCH:
					if (rule->u.header.name && rule->u.header.contents && *rule->u.header.contents) sprintf(buf,"\"%s\"-field contains \"%s\"",rule->u.header.name,*rule->u.header.contents);
					else strcpy(buf,"Header match");
					break;

		case	RULE_ATTACHMENT_MATCH:
					strcpy(buf,"Has attachments");
					break;

		default:
					strcpy(buf,"Unknown");
					break;
	}
	return buf;
}
#endif

/**
 * Returns the num-th action of the given filter.
 *
 * @param filter
 * @param num
 * @return
 */
struct filter_action *filter_find_action(struct filter *filter, int num)
{
	return (struct filter_action *)list_find(&filter->action_list,num);
}

/**
 * Clear the global configuration filter list.
 */
void filter_list_clear(void)
{
	struct filter *f;
	while ((f = (struct filter*)list_remove_tail(&user.config.filter_list)))
	{
		filter_dispose(f);
	}
}

/**
 * Adds a duplicate of the filter to the global filter list
 *
 * @param f
 */
void filter_list_add_duplicate(struct filter *f)
{
	struct filter *df;

	if ((df = filter_duplicate(f)))
	{
		list_insert_tail(&user.config.filter_list,&df->node);
	}
}

/**
 * Get the first filter of the global filter list.
 *
 * @return
 */
struct filter *filter_list_first(void)
{
	return (struct filter*)list_first(&user.config.filter_list);
}

/**
 * Returns the next filter of the given filter.
 *
 * @param f
 * @return
 */
struct filter *filter_list_next(struct filter *f)
{
	return (struct filter*)node_next(&f->node);
}

/**
 * Returns whether the global filter list contains any remote filters.
 *
 * @return
 */
int filter_list_has_remote(void)
{
	struct filter *f = filter_list_first();
	while (f)
	{
		if (f->flags & FILTER_FLAG_REMOTE) return 1;
		f = filter_list_next(f);
	}
	return 0;
}

/**
 * Loads the filter list from the already fopen()'ed filehandle.
 *
 * @param fh
 */
void filter_list_load(FILE *fh)
{
	char *buf = (char*)malloc(512);
	int utf8 = 0;
	if (!buf) return;

	while (read_line(fh,buf))
	{
		if (!mystrnicmp(buf, "UTF8=1",6)) utf8 = 1;

		if (!mystrnicmp(buf, "FILTER",6))
		{
			unsigned char *filter_buf = buf + 6;
			int filter_no = atoi(filter_buf);
			struct filter *f;

			if (!(f = (struct filter*)list_find(&user.config.filter_list,filter_no)))
			{
				if ((f = malloc(sizeof(struct filter))))
				{
					memset(f,0,sizeof(struct filter));
					list_init(&f->rules_list);
					list_insert_tail(&user.config.filter_list, &f->node);
					f->flags = FILTER_FLAG_REQUEST;
					f = (struct filter*)list_find(&user.config.filter_list,filter_no);
				}
			}

			if (f)
			{
				char *result;

				while (isdigit(*filter_buf)) filter_buf++;
				if (*filter_buf++ == '.')
				{
					if ((result = get_config_item(filter_buf,"Name")))
						f->name = dupconfigstr(result,utf8);
					if ((result = get_config_item(filter_buf,"Flags")))
						f->flags = atoi(result);
					if ((result = get_config_item(filter_buf,"Mode")))
						f->mode = atoi(result);
					if ((result = get_config_item(filter_buf,"DestFolder")))
						f->dest_folder = dupconfigstr(result,utf8);
					if ((result = get_config_item(filter_buf,"UseDestFolder")))
						f->use_dest_folder = ((*result == 'Y') || (*result == 'y'))?1:0;
					if ((result = get_config_item(filter_buf,"SoundFile")))
						f->sound_file = dupconfigstr(result,utf8);
					if ((result = get_config_item(filter_buf,"UseSoundFile")))
						f->use_sound_file = ((*result == 'Y') || (*result == 'y'))?1:0;
					if ((result = get_config_item(filter_buf,"ARexxFile")))
						f->arexx_file = dupconfigstr(result,utf8);
					if ((result = get_config_item(filter_buf,"UseARexxFile")))
						f->use_arexx_file = ((*result == 'Y') || (*result == 'y'))?1:0;

					if (!mystrnicmp(filter_buf, "RULE",4))
					{
						unsigned char *rule_buf = filter_buf + 4;
						int rule_no = atoi(rule_buf);
						struct filter_rule *fr;

						if (!(fr = (struct filter_rule*)list_find(&f->rules_list, rule_no)))
						{
							if ((fr = malloc(sizeof(struct filter_rule))))
							{
								memset(fr,0,sizeof(struct filter_rule));
								/* setting old-flags to be downward compatible,
								   will be overwritten when saved in settingfile */
								fr->flags = (SM_PATTERN_NOCASE|SM_PATTERN_SUBSTR);
								list_insert_tail(&f->rules_list, &fr->node);
								fr = (struct filter_rule*)list_find(&f->rules_list,rule_no);
							}
						}
						if (fr)
						{
							char *result;

							while (isdigit(*rule_buf)) rule_buf++;
							if (*rule_buf++ == '.')
							{
								if ((result = get_config_item(rule_buf,"Type")))
								{
									if (!mystricmp(result,"FROM")) fr->type = RULE_FROM_MATCH;
									else if (!mystricmp(result,"SUBJECT")) fr->type = RULE_SUBJECT_MATCH;
									else if (!mystricmp(result,"HEADER")) fr->type = RULE_HEADER_MATCH;
									else if (!mystricmp(result,"ATTACHMENT")) fr->type = RULE_ATTACHMENT_MATCH;
									else if (!mystricmp(result,"STATUS")) fr->type = RULE_STATUS_MATCH;
									else if (!mystricmp(result,"RCPT")) fr->type = RULE_RCPT_MATCH;
									else if (!mystricmp(result,"BODY")) fr->type = RULE_BODY_MATCH;
								}

								if ((result = get_config_item(rule_buf,"Flags")))
									fr->flags = atoi(result);
								if ((result = get_config_item(rule_buf,"From.Address")))
									fr->u.from.from = array_add_string(fr->u.from.from,result);
								if ((result = get_config_item(rule_buf,"Subject.Subject")))
									fr->u.subject.subject = array_add_string(fr->u.subject.subject,result);
								if ((result = get_config_item(rule_buf,"Header.Name")))
									fr->u.header.name = mystrdup(result);
								if ((result = get_config_item(rule_buf,"Header.Contents")))
									fr->u.header.contents = array_add_string(fr->u.header.contents,result);
								if ((result = get_config_item(rule_buf,"Status.Status")))
									fr->u.status.status = atoi(result);
								if ((result = get_config_item(rule_buf,"Rcpt.Address")))
									fr->u.rcpt.rcpt = array_add_string(fr->u.rcpt.rcpt,result);
								if ((result = get_config_item(rule_buf,"Body.Contents")))
									fr->u.body.body = mystrdup(result);
							}
						}
					}
				}
			}
		}
	}
	free(buf);
	filter_parse_all_filters();
}

#define MAKESTR(x) ((x)?(char*)(x):"")

/**
 * Saves the filter list into the given handle that has been opened with
 * fopen() before.
 *
 * @param fh
 */
void filter_list_save(FILE *fh)
{
	struct filter *f = filter_list_first();
	int i=0;

	fputs("UTF8=1\n",fh);

	while (f)
	{
		struct filter_rule *rule;
		int j=0;

		fprintf(fh,"FILTER%d.Name=%s\n",i,MAKESTR(f->name));
		fprintf(fh,"FILTER%d.Flags=%d\n",i,f->flags);
		fprintf(fh,"FILTER%d.Mode=%d\n",i,f->mode);
		fprintf(fh,"FILTER%d.DestFolder=%s\n",i,MAKESTR(f->dest_folder));
		fprintf(fh,"FILTER%d.UseDestFolder=%s\n",i,f->use_dest_folder?"Y":"N");
		fprintf(fh,"FILTER%d.SoundFile=%s\n",i,MAKESTR(f->sound_file));
		fprintf(fh,"FILTER%d.UseSoundFile=%s\n",i,f->use_sound_file?"Y":"N");
		fprintf(fh,"FILTER%d.ARexxFile=%s\n",i,MAKESTR(f->arexx_file));
		fprintf(fh,"FILTER%d.UseARexxFile=%s\n",i,f->use_arexx_file?"Y":"N");

		rule = (struct filter_rule*)list_first(&f->rules_list);
		while (rule)
		{
			switch (rule->type)
			{
				case	RULE_FROM_MATCH:
							fprintf(fh,"FILTER%d.RULE%d.Type=From\n",i,j);
							fprintf(fh,"FILTER%d.RULE%d.Flags=%d\n",i,j,rule->flags);
							if (rule->u.from.from)
							{
								char *str;
								int k = 0;
								while ((str = rule->u.from.from[k]))
								{
									fprintf(fh,"FILTER%d.RULE%d.From.Address=%s\n",i,j,rule->u.from.from[k]);
									k++;
								}
							}
							break;
				case	RULE_RCPT_MATCH:
							fprintf(fh,"FILTER%d.RULE%d.Type=Rcpt\n",i,j);
							fprintf(fh,"FILTER%d.RULE%d.Flags=%d\n",i,j,rule->flags);
							if (rule->u.from.from)
							{
								char *str;
								int k = 0;
								while ((str = rule->u.rcpt.rcpt[k]))
								{
									fprintf(fh,"FILTER%d.RULE%d.Rcpt.Address=%s\n",i,j,rule->u.rcpt.rcpt[k]);
									k++;
								}
							}
							break;
				case	RULE_SUBJECT_MATCH:
							fprintf(fh,"FILTER%d.RULE%d.Type=Subject\n",i,j);
							fprintf(fh,"FILTER%d.RULE%d.Flags=%d\n",i,j,rule->flags);
							if (rule->u.subject.subject)
							{
								char *str;
								int k = 0;
								while ((str = rule->u.subject.subject[k]))
								{
									fprintf(fh,"FILTER%d.RULE%d.Subject.Subject=%s\n",i,j,rule->u.subject.subject[k]);
									k++;
								}
							}
							break;
				case	RULE_HEADER_MATCH:
							fprintf(fh,"FILTER%d.RULE%d.Type=Header\n",i,j);
							fprintf(fh,"FILTER%d.RULE%d.Flags=%d\n",i,j,rule->flags);
							fprintf(fh,"FILTER%d.RULE%d.Header.Name=%s\n",i,j,MAKESTR(rule->u.header.name));
							if (rule->u.header.contents)
							{
								char *str;
								int k = 0;
								while ((str = rule->u.header.contents[k]))
								{
									fprintf(fh,"FILTER%d.RULE%d.Header.Contents=%s\n",i,j,rule->u.header.contents[k]);
									k++;
								}
							}
							break;
				case	RULE_ATTACHMENT_MATCH:
							fprintf(fh,"FILTER%d.RULE%d.Type=Attachment\n",i,j);
							break;
				case	RULE_STATUS_MATCH:
							fprintf(fh,"FILTER%d.RULE%d.Type=Status\n",i,j);
							fprintf(fh,"FILTER%d.RULE%d.Status.Status=%d\n",i,j,rule->u.status.status);
							break;
			}
			rule = (struct filter_rule*)node_next(&rule->node);
			j++;
		}

		fputs("\n",fh);
		f = filter_list_next(f);
		i++;
	}
}

/**
 * Duplicates the given search option.
 *
 * @param so
 * @return
 */
struct search_options *search_options_duplicate(struct search_options *so)
{
	struct search_options *new_so = malloc(sizeof(*so));
	if (new_so)
	{
		*new_so = *so;
		new_so->folder = mystrdup(so->folder);
		new_so->from = mystrdup(so->from);
		new_so->to = mystrdup(so->to);
		new_so->subject = mystrdup(so->subject);
		new_so->body = mystrdup(so->body);
	}
	return new_so;
}

/**
 * Frees all memory associated with the given search options.
 * @param so
 */
void search_options_free(struct search_options *so)
{
	free(so->folder);
	free(so->from);
	free(so->to);
	free(so->subject);
	free(so->body);
	free(so);
}


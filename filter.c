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
 * @file filter.c
 */

#include "filter.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "configuration.h"
#include "debug.h"
#include "mail.h"
#include "support_indep.h"

#include "support.h"

/*****************************************************************************/

struct filter *filter_create(void)
{
	struct filter *f = (struct filter*)malloc(sizeof(struct filter));
	if (f)
	{
		memset(f,0,sizeof(struct filter));
		f->name = mystrdup("Unnamed filter");
		list_init(&f->rules_list);
		list_init(&f->action_list);
	}
	return f;
}

/*****************************************************************************/

void filter_deinit_rule(struct filter_rule_parsed *p)
{
	boyermoore_delete_context(p->bm_context);
	p->bm_context = NULL;

	free(p->parsed);
	p->parsed = NULL;
}

/*****************************************************************************/

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

/*****************************************************************************/

int filter_match_rule_len(struct filter_rule_parsed *p, char *str, int strl, int flags)
{
	if (p->bm_context)
		return boyermoore(p->bm_context,str,strl,NULL,NULL) != -1;
	return sm_match_pattern(p->parsed,str,flags);
}

/*****************************************************************************/

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

/*****************************************************************************/

void filter_parse_all_filters(void)
{
	struct filter *f = filter_list_first();
	while (f)
	{
		filter_parse_filter_rules(f);
		f = filter_list_next(f);
	}
}

/*****************************************************************************/

struct filter *filter_duplicate(struct filter *filter)
{
	struct filter *f = (struct filter*)malloc(sizeof(struct filter));
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

/*****************************************************************************/

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

/*****************************************************************************/

void filter_add_rule(struct filter *filter, struct filter_rule *fr)
{
	list_insert_tail(&filter->rules_list,&fr->node);
}

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

struct filter_rule *filter_rule_create_from_common_sorted_recipients(char ***addresses, unsigned int num_addresses)
{
	/* This is a very naive implementation, which could be optimized */
	unsigned int i,j;
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

/*****************************************************************************/

struct filter_rule *filter_rule_create_from_mail_iterator(enum filter_rule_create_type type, int num_mails,
												  struct mail_info * (*get_first_mail_info)(void *handle, void *userdata),
												  struct mail_info * (*get_next_mail_info)(void *handle, void *userdata),
												  void *userdata)
{
	struct mail_info *m;
	void *handle;
	void **data;
	struct filter_rule *fr;
	int i;

	if (num_mails < 0)
	{
		num_mails = 0;
		m = get_first_mail_info(&handle,userdata);
		while (m)
		{
			num_mails++;
			m = get_next_mail_info(&handle,userdata);
		}
	}

	if (num_mails == 0)
		return NULL;

	if (!(data = (void**)malloc(sizeof(*data)*num_mails)))
		return NULL;

	m = get_first_mail_info(&handle,userdata);
	for (i=0;i<num_mails;i++)
	{
		switch (type)
		{
			case	FRCT_FROM:
					data[i] = array_add_string(NULL,mail_info_get_from_addr(m));
					break;
			case	FRCT_RECEPIENTS:
					data[i] = mail_info_get_recipient_addresses(m); /* returns an array() */
					break;
			case	FRCT_SUBJECT:
					data[i] = m->subject;
					break;

		}
		m = get_next_mail_info(&handle,userdata);
	}

	switch (type)
	{
			case	FRCT_FROM:
			case	FRCT_RECEPIENTS:
					fr = filter_rule_create_from_common_sorted_recipients((char***)data,num_mails);
					break;
			case	FRCT_SUBJECT:
					fr = filter_rule_create_from_strings((char**)data,num_mails,RULE_SUBJECT_MATCH);
					break;
			default:
					fr = NULL;
					break;

	}

	m = get_first_mail_info(&handle,userdata);
	for (i=0;i<num_mails;i++)
	{
		switch (type)
		{
			case	FRCT_FROM:
			case	FRCT_RECEPIENTS:
					array_free((char**)data[i]);
					break;
			default: break;
		}
		m = get_next_mail_info(&handle,userdata);
	}

	free(data);
	return fr;
}

/*****************************************************************************/

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

/*****************************************************************************/

struct filter_rule *filter_find_rule(struct filter *filter, int num)
{
	return (struct filter_rule *)list_find(&filter->rules_list,num);
}

/*****************************************************************************/

struct filter_action *filter_find_action(struct filter *filter, int num)
{
	return (struct filter_action *)list_find(&filter->action_list,num);
}

/*****************************************************************************/

void filter_list_clear(void)
{
	struct filter *f;
	while ((f = (struct filter*)list_remove_tail(&user.config.filter_list)))
	{
		filter_dispose(f);
	}
}

/*****************************************************************************/

void filter_list_add_duplicate(struct filter *f)
{
	struct filter *df;

	if ((df = filter_duplicate(f)))
	{
		list_insert_tail(&user.config.filter_list,&df->node);
	}
}

/*****************************************************************************/

struct filter *filter_list_first(void)
{
	return (struct filter*)list_first(&user.config.filter_list);
}

/*****************************************************************************/

struct filter *filter_list_next(struct filter *f)
{
	return (struct filter*)node_next(&f->node);
}

/*****************************************************************************/

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

/*****************************************************************************/

void filter_list_load(FILE *fh)
{
	char *buf = (char*)malloc(512);
	int utf8 = 0;
	if (!buf) return;

	while (myreadline(fh,buf))
	{
		if (!mystrnicmp(buf, "UTF8=1",6)) utf8 = 1;

		if (!mystrnicmp(buf, "FILTER",6))
		{
			char *filter_buf = buf + 6;
			int filter_no = atoi(filter_buf);
			struct filter *f;

			if (!(f = (struct filter*)list_find(&user.config.filter_list,filter_no)))
			{
				if ((f = (struct filter*)malloc(sizeof(struct filter))))
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
					if ((result = get_key_value(filter_buf,"Name")))
						f->name = utf8strdup(result,utf8);
					if ((result = get_key_value(filter_buf,"Flags")))
						f->flags = atoi(result);
					if ((result = get_key_value(filter_buf,"Mode")))
						f->mode = atoi(result);
					if ((result = get_key_value(filter_buf,"DestFolder")))
						f->dest_folder = utf8strdup(result,utf8);
					if ((result = get_key_value(filter_buf,"UseDestFolder")))
						f->use_dest_folder = ((*result == 'Y') || (*result == 'y'))?1:0;
					if ((result = get_key_value(filter_buf,"SoundFile")))
						f->sound_file = utf8strdup(result,utf8);
					if ((result = get_key_value(filter_buf,"UseSoundFile")))
						f->use_sound_file = ((*result == 'Y') || (*result == 'y'))?1:0;
					if ((result = get_key_value(filter_buf,"ARexxFile")))
						f->arexx_file = utf8strdup(result,utf8);
					if ((result = get_key_value(filter_buf,"UseARexxFile")))
						f->use_arexx_file = ((*result == 'Y') || (*result == 'y'))?1:0;

					if (!mystrnicmp(filter_buf, "RULE",4))
					{
						char *rule_buf = filter_buf + 4;
						int rule_no = atoi(rule_buf);
						struct filter_rule *fr;

						if (!(fr = (struct filter_rule*)list_find(&f->rules_list, rule_no)))
						{
							if ((fr = (struct filter_rule*)malloc(sizeof(struct filter_rule))))
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
								if ((result = get_key_value(rule_buf,"Type")))
								{
									if (!mystricmp(result,"FROM")) fr->type = RULE_FROM_MATCH;
									else if (!mystricmp(result,"SUBJECT")) fr->type = RULE_SUBJECT_MATCH;
									else if (!mystricmp(result,"HEADER")) fr->type = RULE_HEADER_MATCH;
									else if (!mystricmp(result,"ATTACHMENT")) fr->type = RULE_ATTACHMENT_MATCH;
									else if (!mystricmp(result,"STATUS")) fr->type = RULE_STATUS_MATCH;
									else if (!mystricmp(result,"RCPT")) fr->type = RULE_RCPT_MATCH;
									else if (!mystricmp(result,"BODY")) fr->type = RULE_BODY_MATCH;
								}

								if ((result = get_key_value(rule_buf,"Flags")))
									fr->flags = atoi(result);
								if ((result = get_key_value(rule_buf,"From.Address")))
									fr->u.from.from = array_add_string(fr->u.from.from,result);
								if ((result = get_key_value(rule_buf,"Subject.Subject")))
									fr->u.subject.subject = array_add_string(fr->u.subject.subject,result);
								if ((result = get_key_value(rule_buf,"Header.Name")))
									fr->u.header.name = mystrdup(result);
								if ((result = get_key_value(rule_buf,"Header.Contents")))
									fr->u.header.contents = array_add_string(fr->u.header.contents,result);
								if ((result = get_key_value(rule_buf,"Status.Status")))
									fr->u.status.status = atoi(result);
								if ((result = get_key_value(rule_buf,"Rcpt.Address")))
									fr->u.rcpt.rcpt = array_add_string(fr->u.rcpt.rcpt,result);
								if ((result = get_key_value(rule_buf,"Body.Contents")))
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

/*****************************************************************************/

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

/*****************************************************************************/

struct search_options *search_options_duplicate(struct search_options *so)
{
	struct search_options *new_so = (struct search_options*)malloc(sizeof(*so));
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

/*****************************************************************************/

void search_options_free(struct search_options *so)
{
	free(so->folder);
	free(so->from);
	free(so->to);
	free(so->subject);
	free(so->body);
	free(so);
}


/*****************************************************************************/

struct filter *filter_create_from_search_options(struct search_options *sopt)
{
	struct filter *filter;
	struct filter_rule *rule;

	if (!(filter = filter_create()))
		goto bailout;

	if (sopt->from)
	{
		if ((rule = filter_create_and_add_rule(filter,RULE_FROM_MATCH)))
		{
			rule->flags = SM_PATTERN_SUBSTR|SM_PATTERN_NOCASE;
			rule->u.from.from = array_add_string(NULL,sopt->from);
		}
	}

	if (sopt->subject)
	{
		if ((rule = filter_create_and_add_rule(filter,RULE_SUBJECT_MATCH)))
		{
			rule->flags = SM_PATTERN_SUBSTR|SM_PATTERN_NOCASE;
			rule->u.subject.subject = array_add_string(NULL,sopt->subject);
		}
	}

	if (sopt->body)
	{
		if ((rule = filter_create_and_add_rule(filter,RULE_BODY_MATCH)))
		{
			rule->flags = SM_PATTERN_SUBSTR|SM_PATTERN_NOCASE;
			rule->u.body.body = mystrdup(sopt->body);
		}
	}

	if (sopt->to)
	{
		if ((rule = filter_create_and_add_rule(filter,RULE_RCPT_MATCH)))
		{
			rule->flags = SM_PATTERN_SUBSTR|SM_PATTERN_NOCASE;
			rule->u.rcpt.rcpt = array_add_string(NULL,sopt->to);
		}
	}

	filter_parse_filter_rules(filter);
	filter->search_filter = 1;

	bailout:
	return filter;
}

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

#include "configuration.h"
#include "lists.h"
#include "filter.h"
#include "support_indep.h"

int read_line(FILE *fh, char *buf); /* in addressbook.c */
char *get_config_item(char *buf, char *item); /* configuration.c */
/* Duplicates a config string and converts it to utf8 if not already done */
char *dupconfigstr(char *str, int utf8);

/**************************************************************************
 Creates a new filter instance with default values
**************************************************************************/
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

/**************************************************************************
 Duplicates a filter instance
**************************************************************************/
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
	}
	return f;
}

/**************************************************************************
 Dispose the filter
**************************************************************************/
void filter_dispose(struct filter *f)
{
	/* The rules must be freed */
	if (f->name) free(f->name);
	if (f->dest_folder) free(f->dest_folder);
	free(f);
}

/**************************************************************************
 
**************************************************************************/
struct filter_rule *filter_create_and_add_rule(struct filter *filter, int type)
{
	struct filter_rule *rule = (struct filter_rule*)malloc(sizeof(struct filter_rule));
	if (rule)
	{
		memset(rule, 0, sizeof(struct filter_rule));
		rule->type = type;
		list_insert_tail(&filter->rules_list,&rule->node);
	}
	return rule;
}

/**************************************************************************
 
**************************************************************************/
void filter_remove_rule(struct filter_rule *fr)
{
	if (fr)
	{
		node_remove(&fr->node);
		free(fr);
	}
}

/**************************************************************************
 Find a rule of the filter
**************************************************************************/
struct filter_rule *filter_find_fule(struct filter *filter, int num)
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

/**************************************************************************
 Find a action of the filter
**************************************************************************/
struct filter_action *filter_find_action(struct filter *filter, int num)
{
	return (struct filter_action *)list_find(&filter->action_list,num);
}

/**************************************************************************
 Clears the filter list in the configuration
**************************************************************************/
void filter_list_clear(void)
{
	struct filter *f;
	while ((f = (struct filter*)list_remove_tail(&user.config.filter_list)))
	{
		filter_dispose(f);
	}
}

/**************************************************************************
 Adds a duplicate of the filter to the filter list
**************************************************************************/
void filter_list_add_duplicate(struct filter *f)
{
	struct filter *df;

	if ((df = filter_duplicate(f)))
	{
		list_insert_tail(&user.config.filter_list,&df->node);
	}
}

/**************************************************************************
 Returs the first filter
**************************************************************************/
struct filter *filter_list_first(void)
{
	return (struct filter*)list_first(&user.config.filter_list);
}

/**************************************************************************
 Returs the first filter
**************************************************************************/
struct filter *filter_list_next(struct filter *f)
{
	return (struct filter*)node_next(&f->node);
}

/**************************************************************************
 Loads the filter list from the given FILE
**************************************************************************/
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
}

#define MAKESTR(x) ((x)?(char*)(x):"")

/**************************************************************************
 Saves the filter list into the given FILE
**************************************************************************/
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
							if (rule->u.from.from)
							{
								char *str;
								int k = 0;
								while ((str = rule->u.from.from[k]))
								{
									fprintf(fh,"FILTER%d.RULE%d.Rcpt.Address=%s\n",i,j,rule->u.rcpt.rcpt[k]);
									k++;
								}
							}
							break;
				case	RULE_SUBJECT_MATCH:
							fprintf(fh,"FILTER%d.RULE%d.Type=Subject\n",i,j);
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



/**************************************************************************
 Duplicates a given search option
**************************************************************************/
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

/**************************************************************************
 Saves the filter list into the given FILE
**************************************************************************/
void search_options_free(struct search_options *so)
{
	free(so->folder);
	free(so->from);
	free(so->to);
	free(so->subject);
	free(so->body);
	free(so);
}


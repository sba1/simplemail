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

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "configuration.h"
#include "lists.h"
#include "filter.h"
#include "support_indep.h"

/**************************************************************************
 Creates a new filter instance with default values
**************************************************************************/
struct filter *filter_create(void)
{
	struct filter *f = malloc(sizeof(struct filter));
	if (f)
	{
		memset(f,0,sizeof(struct filter));
		f->name = mystrdup("Unnamed");
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
		*f = *filter;
		f->name = mystrdup(f->name);
	}
	return f;
}

/**************************************************************************
 Dispose the filter
**************************************************************************/
void filter_dispose(struct filter *f)
{
	if (f->name) free(f->name);
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
 Find a rule of the filter
**************************************************************************/
struct filter_rule *filter_find_fule(struct filter *filter, int num)
{
	return (struct filter_rule *)list_find(&filter->rules_list,num);
}

/**************************************************************************
 Returns a string from a rule
**************************************************************************/
char *filter_get_rule_string(struct filter_rule *rule)
{
	static char buf[256];
	switch(rule->type)
	{
		case	RULE_FROM_MATCH:
					strcpy(buf,"From match");
					break;

		case	RULE_SUBJECT_MATCH:
					strcpy(buf,"Subject match");
					break;

		case	RULE_HEADER_MATCH:
					strcpy(buf,"Header match");
					break;

		default:
					strcpy(buf,"Unknown");
					break;
	}
	return buf;
}


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

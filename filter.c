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
 Dispose the filter
**************************************************************************/
struct filter_rule *filter_find_fule(struct filter *filter, int num)
{
	return (struct filter_rule *)list_find(&filter->rules_list,num);
}

/**************************************************************************
 Dispose the filter
**************************************************************************/
struct filter_action *filter_find_action(struct filter *filter, int num)
{
	return (struct filter_rule *)list_find(&filter->action_list,num);
}


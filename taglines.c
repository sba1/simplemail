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
** taglines.c
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "taglines.h"
#include "configuration.h"

int random(int max)
{
	static int x=0;

	if(!x)
	{
		srand(time(0));
		x = 1;
	}

	return (rand() % max);
}

char *get_tagline(void)
{
	char *rc;
	struct tagline *t;
	int nr, len = list_length(&user.config.tagline_list);
	struct node *n;

	if(!len) return NULL;

	n=list_first(&user.config.tagline_list);

	nr = random(len);

	t = (struct tagline *) list_find(&user.config.tagline_list, nr);


	rc = malloc(strlen(t->txt) + 1);
	if(rc != NULL)
	{
		strcpy(rc, t->txt);
	}

	return rc;
}

char * taglines_add_tagline(char *buf)
{
	char *rc = buf;
	char *tagline;
	long len;

	tagline = get_tagline();

	if(tagline != NULL)
	{
		len = strlen(buf) + strlen(tagline);

		rc = malloc(len+1);
		if(rc != NULL)
		{
			strcpy(rc, buf);
			strcat(rc, tagline);
			free(tagline);
			free(buf);
		}
	}
	else
	{
		rc = buf;
	}

	return rc;
}

struct tagline *taglines_create_tagline(char *txt)
{
	struct tagline *rc;

	rc = malloc(sizeof(struct tagline));
	if(rc != NULL)
	{
		rc->txt = malloc(strlen(txt) + 1);
		if(rc->txt != NULL)
		{
			strcpy(rc->txt, txt);
		}
		else
		{
			free(rc);
			rc = NULL;
		}
	}

	return rc;
}

void taglines_free_tagline(struct tagline *t)
{
	if(t != NULL)
	{
		if(t->txt != NULL)
		{
			free(t->txt);
		}
		free(t);
	}
}

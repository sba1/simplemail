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
** cookies.c
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cookies.h"
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

char *get_cookie(void)
{
	char *rc;
	struct cookie *c;
	int nr, len = list_length(&user.config.cookie_list);
	struct node *n;

	if(!len) return NULL;

	n=list_first(&user.config.cookie_list);

	nr = random(len);

	c = (struct cookie *) list_find(&user.config.cookie_list, nr);


	rc = malloc(strlen(c->txt) + 1);
	if(rc != NULL)
	{
		strcpy(rc, c->txt);
	}

	return rc;
}

char * cookies_add_cookie(char *buf)
{
	char *rc = buf;
	char *cookie;
	long len;

	cookie = get_cookie();

	if(cookie != NULL)
	{
		len = strlen(buf) + strlen(cookie);

		rc = malloc(len+1);
		if(rc != NULL)
		{
			strcpy(rc, buf);
			strcat(rc, cookie);
			free(cookie);
			free(buf);
		}
	}
	else
	{
		rc = buf;
	}

	return rc;
}

struct cookie *cookies_create_cookie(char *txt)
{
	struct cookie *rc;

	rc = malloc(sizeof(struct cookie));
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

void cookies_free_cookie(struct cookie *c)
{
	if(c != NULL)
	{
		if(c->txt != NULL)
		{
			free(c->txt);
		}
		free(c);
	}
}

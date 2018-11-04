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
 * @file pgp.c
 */

#include "pgp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "indep-include/support.h"
#include "support_indep.h"

static struct list pgp_list;

/**
 * Frees the internal pgp list.
 */
static void pgp_free_list(void)
{
	struct pgp_key *key;
	while ((key = (struct pgp_key*)list_remove_tail(&pgp_list)))
	{
		array_free(key->userids);
		free(key);
	}
}


/*****************************************************************************/

int pgp_update_key_list(void)
{
	int rc = 0;
	char *tmpname;
	pgp_free_list();

	tmpname = tmpnam(NULL);
	if (!pgp_operate("-kv",tmpname))
	{
		FILE *fh = fopen(tmpname,"rb");
		if (fh)
		{
			char buf[512];
			buf[9] = buf[23] = 0;
			while (fgets(buf,512,fh))
			{
				if (buf[9] == '/' && buf[23] == '/')
				{
					struct pgp_key *key = (struct pgp_key*)malloc(sizeof(struct pgp_key));
					if (key)
					{
						key->userids = NULL;
						key->keyid = strtoul(&buf[10],NULL,16);
						key->userids = array_add_string(key->userids,&buf[29]);
						list_insert_tail(&pgp_list,&key->node);
					}
					buf[9] = buf[23] = 0;
				}
			}
			fclose(fh);
		}
		remove(tmpname);
	}
	return rc;
}

/*****************************************************************************/

struct pgp_key *pgp_first(void)
{
	return (struct pgp_key *)list_first(&pgp_list);
}

/*****************************************************************************/

struct pgp_key *pgp_next(struct pgp_key *next)
{
	return (struct pgp_key *)node_next(&next->node);
}

/*****************************************************************************/

struct pgp_key *pgp_duplicate(struct pgp_key *key)
{
	struct pgp_key *new_key;

	if (!key) return NULL;

	if ((new_key = (struct pgp_key*)malloc(sizeof(struct pgp_key))))
	{
		*new_key = *key;
		new_key->userids = array_duplicate(key->userids);
	}
	return new_key;
}

/*****************************************************************************/

void pgp_dispose(struct pgp_key *key)
{
	if (key)
	{
		array_free(key->userids);
		free(key);
	}
}

/*****************************************************************************/

int pgp_operate(const char *options, char *output)
{
	char *path = sm_getenv("PGPPATH");
	int len = mystrlen(path)+10+mystrlen(options);
	int rc;
	char *buf = (char *)malloc(len);
	FILE *fh;

	if (!buf) return 0;

	if (path) strcpy(buf,path);
	else buf[0] = 0;

	sm_add_part(buf,"pgp",len);

	if ((fh = fopen(buf,"rb")))
	{
		fclose(fh);
		strcat(buf," ");
	} else strcpy(buf,"pgp ");

	strcat(buf,options);
	rc = sm_system(buf,output);
	free(buf);
	return rc;
}

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
** pgp.c
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "lists.h"
#include "pgp.h"
#include "support.h"
#include "support_indep.h"

static struct list pgp_list;

/******************************************************************
 Free's the pgp list
*******************************************************************/
static void pgp_free_list(void)
{
	struct pgp_key *key;
	while ((key = (struct pgp_key*)list_remove_tail(&pgp_list)))
	{
		array_free(key->userids);
		free(key);
	}
}


/******************************************************************
 Returns a list with struct key_node entries
*******************************************************************/
int pgp_update_key_list(void)
{
	int rc = 0;
	char *tmpname;
	pgp_free_list();

	tmpname = tmpnam(NULL);
	if (sm_system("pgp -kv",tmpname))
	{
	}
	return rc;
}

/******************************************************************
 Returns a list with struct key_node entries
*******************************************************************/
struct pgp_key *pgp_first(void)
{
	return (struct pgp_key *)list_first(&pgp_list);
}

/******************************************************************
 Returns a list with struct key_node entries
*******************************************************************/
struct pgp_key *pgp_next(struct pgp_key *next)
{
	return (struct pgp_key *)node_next(&next->node);
}


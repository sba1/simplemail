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
** imap.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include "support_indep.h"

#include "imap.h"

/**************************************************************************
 malloc() a imap_server and initializes it with default values.
 TODO: rename all imap identifiers to imap
**************************************************************************/
struct imap_server *imap_malloc(void)
{
	struct imap_server *imap;
	if ((imap = (struct imap_server*)malloc(sizeof(*imap))))
	{
		memset(imap,0,sizeof(struct imap_server));
		imap->port = 143;
	}
	return imap;
}

/**************************************************************************
 Duplicates a imap_server
**************************************************************************/
struct imap_server *imap_duplicate(struct imap_server *imap)
{
	struct imap_server *new_imap = imap_malloc();
	if (new_imap)
	{
		new_imap->name = mystrdup(imap->name);
		new_imap->login = mystrdup(imap->login);
		new_imap->passwd = mystrdup(imap->passwd);
		new_imap->port = imap->port;
		new_imap->active = imap->active;
	}
	return new_imap;
}

/**************************************************************************
 Frees a imap_server completly
**************************************************************************/
void imap_free(struct imap_server *imap)
{
	if (imap->name) free(imap->name);
	if (imap->login) free(imap->login);
	if (imap->passwd) free(imap->passwd);
	free(imap);
}


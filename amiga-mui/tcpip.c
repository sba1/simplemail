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
** $Id$
*/

#include <proto/exec.h>
#include <bsdsocket/socketbasetags.h>

#include "tcpip.h"

struct Library *SocketBase;

static long bsd_in_use;

int open_socket_lib(void)
{
	int rc;

	rc = FALSE;

	bsd_in_use++;
	if(SocketBase == NULL)
	{
		SocketBase = OpenLibrary("bsdsocket.library", 4);
		if(SocketBase != NULL)
		{
		 rc = TRUE;
		}
	}

	return rc;
}

void close_socket_lib(void)
{
	bsd_in_use--;

	if(bsd_in_use == 0)
	{
		if(SocketBase != NULL)
		{
			CloseLibrary(SocketBase);
			SocketBase = NULL;
		}
	}
}

long my_h_errno(void)
{
	long id;

	if(SocketBaseTags(
		SBTM_GETREF(SBTC_HERRNO), &id,
	TAG_DONE) != 0)
	{
		id = -1;
	}

	return id;
}

long read(long sd, void *buf, long nbytes)
{
	return recv(sd, buf, nbytes, 0);
}

long write(long sd, void *buf, long nbytes)
{
	return send(sd, buf, nbytes, 0);
}

void close(long sd)
{
	CloseSocket(sd);
}

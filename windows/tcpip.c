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
** tcpip.c
*/

#include <errno.h>
#include <unistd.h>

#ifndef NO_SSL
#include <openssl/ssl.h>
#endif

#include "tcpip.h"

int open_socket_lib(void)
{
	return 1;
}

void close_socket_lib(void)
{
}


#ifndef NO_SSL
static int ssl_in_use;
static SSL_CTX *ctx;
#endif

int open_ssl_lib(void)
{
#ifndef NO_SSL
	if (ssl_in_use)
	{
		ssl_in_use++;
		return 1;
	}

	SSLeay_add_ssl_algorithms();
	SSL_load_error_strings();

	if (ctx = SSL_CTX_new(SSLv23_client_method()))
	{
		/* Everything is ok */
		ssl_in_use = 1;
		return 1;
	}
#endif
	return 0;
}

void close_ssl_lib(void)
{
#ifndef NO_SSL
	if (!ssl_in_use) return;
	if (!(--ssl_in_use))
	{
		SSL_CTX_free(ctx);
		ctx = NULL;
		close_socket_lib();
	}
#endif
}

#ifndef NO_SSL
SSL_CTX *ssl_context(void)
{
	return ctx;
}
#endif

long tcp_herrno(void)
{
	return errno;
}

long tcp_errno(void)
{
	return errno;
}

void myclosesocket(int fd)
{
	close(fd);
}


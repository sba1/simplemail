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


#include "tcpip.h"

int open_socket_lib(void)
{
	return 1;
}

void close_socket_lib(void)
{
}

#if 0
struct Library *AmiSSLBase;
static int ssl_in_use;
#endif
static SSL_CTX *ctx;

int open_ssl_lib(void)
{
	if (!open_socket_lib()) return 0;

#if 0
	if (!AmiSSLBase)
	{
		if ((AmiSSLBase = OpenLibrary("amissl.library",1)))
		{
			if (!InitAmiSSL(AmiSSL_Version,
					AmiSSL_CurrentVersion,
					AmiSSL_Revision, AmiSSL_CurrentRevision,
					AmiSSL_SocketBase, SocketBase,
					/*	AmiSSL_VersionOverride, TRUE,*/ /* If you insist */
					TAG_DONE))
			{
				SSLeay_add_ssl_algorithms();
				SSL_load_error_strings();

				if (ctx = SSL_CTX_new(SSLv23_client_method()))
				{
					SSL_CTX_set_default_verify_paths(ctx);
					SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL/*SSL_verify_callback*/);

					/* Everything is ok */
					ssl_in_use++;
					return 1;
				}
				CleanupAmiSSL(TAG_DONE);
			}
			CloseLibrary(AmiSSLBase);
		}
	} else
	{
		ssl_in_use++;
		return 1;
	}
#endif

	close_socket_lib();
	return 0;
}

void close_ssl_lib(void)
{
#if 0
	if (!ssl_in_use) return;
	if (!(--ssl_in_use))
	{
		SSL_CTX_free(ctx);
		ctx = NULL;
		CleanupAmiSSL(TAG_DONE);
		CloseLibrary(AmiSSLBase);
		AmiSSLBase = NULL;
		close_socket_lib();
	}
#endif
}

SSL_CTX *ssl_context(void)
{
	return ctx;
}


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







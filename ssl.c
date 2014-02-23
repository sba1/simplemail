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
 * @file ssl.c
 */

#ifndef NO_SSL

#if defined(_AMIGA ) || defined(__AMIGAOS4__) || defined(__MORPHOS__)/* ugly */
#include <proto/amissl.h> /* not portable */
#else
#include <openssl/ssl.h>
#endif

#include "configuration.h"
#include "ssl.h"


/**
 * Initialize ssl and return the orimary context.
 *
 * @return
 */
SSL_CTX *ssl_init(void)
{
	SSL_CTX *ctx;

	SSLeay_add_ssl_algorithms();
	SSL_load_error_strings();

	if ((ctx = SSL_CTX_new(SSLv23_client_method())))
	{
		const char *cypher_list = user.config.cypher_list;

		if (!cypher_list)
			cypher_list = "HIGH:!aNULL:!eNULL:!EXPORT:!DES:!3DES:!MD5:!PSK@STRENGTH";

		SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
		SSL_CTX_set_cipher_list(ctx, cypher_list);

		if (SSL_CTX_set_default_verify_paths(ctx))
		{
			return ctx;
		}
	}
	return NULL;
}

#endif

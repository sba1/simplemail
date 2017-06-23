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

#if (defined(_AMIGA ) || defined(__AMIGAOS4__) || defined(__MORPHOS__)) && !defined(USE_OPENSSL) /* ugly */
#include <proto/amissl.h> /* not portable */
#else
#include <openssl/ssl.h>
#endif

#include "configuration.h"
#include "debug.h"
#include "ssl.h"
#include "subthreads.h"

#if OPENSSL_VERSION_NUMBER >= 0x10100000
#pragma message "OpenSSL may be too new. Check the code!"
#endif

#if defined(USE_OPENSSL) && OPENSSL_VERSION_NUMBER < 0x10100000
static semaphore_t lock_semaphores[CRYPTO_NUM_LOCKS];

static void ssl_get_thread_id(CRYPTO_THREADID *id)
{
	CRYPTO_THREADID_set_pointer(id, thread_get());
}

static void ssl_locking_callback(int mode,int type,const char *file,int line)
{
	if (mode & CRYPTO_LOCK)
	{
		thread_lock_semaphore(lock_semaphores[type]);
	} else
	{
		thread_unlock_semaphore(lock_semaphores[type]);
	}
}
#endif

/**
 * Cleans up the ssl subsystem.
 */
void ssl_cleanup(void)
{
#if defined(USE_OPENSSL) && OPENSSL_VERSION_NUMBER < 0x10100000
	int i;
	for (i = 0; i < CRYPTO_NUM_LOCKS; i++)
		thread_dispose_semaphore(lock_semaphores[i]);
#endif
}

/**
 * Initialize the ssl subsystem.
 */
int ssl_init(void)
{
#if defined(USE_OPENSSL) && OPENSSL_VERSION_NUMBER < 0x10100000
	/* see https://www.openssl.org/docs/crypto/threads.html */
	int i;
	for (i = 0; i < CRYPTO_NUM_LOCKS; i++)
	{
		if (!(lock_semaphores[i] = thread_create_semaphore()))
		{
			ssl_cleanup();
			return 0;
		}
	}

	CRYPTO_THREADID_set_callback(ssl_get_thread_id);
	CRYPTO_set_locking_callback(ssl_locking_callback);
#endif
	return 1;
}

/**
 * Initialize ssl and return the primary context.
 *
 * @return
 */
SSL_CTX *ssl_create_context(void)
{
	SSL_CTX *ctx;

	SSLeay_add_ssl_algorithms();
	SSL_load_error_strings();

	if ((ctx = SSL_CTX_new(SSLv23_client_method())))
	{
		const char *cypher_list = user.config.ssl_cypher_list;

		if (!cypher_list)
			cypher_list = "HIGH:!aNULL:!eNULL:!EXPORT:!DES:!3DES:!MD5:!PSK@STRENGTH";

		SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
		if (SSL_CTX_set_cipher_list(ctx, cypher_list))
		{
			if (SSL_CTX_set_default_verify_paths(ctx))
			{
				return ctx;
			}
		} else
		{
			SM_DEBUGF(5,("SSL_CTX_set_cipher_list() failed."));
		}
	}
	return NULL;
}

#endif

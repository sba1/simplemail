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
 * @file tcp.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#ifdef __WIN32__
#include <windows.h>
#else
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/tcp.h>
#endif

#ifndef NO_SSL

#if (defined(_AMIGA ) || defined(__AMIGAOS4__) || defined(__MORPHOS__)) && !defined(USE_OPENSSL) /* ugly */
#include <proto/amissl.h> /* not portable */
#else
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#endif

#include "tcpip.h"

#undef _
#include "debug.h"
#include "simplemail.h" /* callback_failed_ssl_verification() */
#include "smintl.h"
#include "support_indep.h"
#include "tcp.h"

#include "subthreads.h"
#include "support.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

/*#define DEBUG_OUTPUT*/
#undef printf

/* TODO: Get rid of this global error data */
static int error_code;

/**
 * Returns the error code of the last operation. Note that the
 * error code is only set on failure so you cannot use this function
 * to determine the success of a function
 *
 * @return the error code.
 */
int tcp_error_code(void)
{
	return error_code;
}

/**
 * Returns a descriptive string for the given error code.
 *
 * @param code defines the code of which the string should be returned.
 * @return the description, possibly translated.
 */
const char *tcp_strerror(int code)
{
	switch (code)
	{
		case	TCP_NOT_ENOUGH_MEMORY: return _("Not enough memory");
		case	TCP_NOT_SECURE: return _("Connection couldn't be made secure");
		case	TCP_ERRNO: return strerror(errno);
		case	TCP_HOST_NOT_FOUND: return _("Host wasn't found");
		case	TCP_TRY_AGAIN: return _("Cannot locate host. Try again later");
		case	TCP_NO_RECOVERY: return _("Unexpected server failure");
		case	TCP_NO_DATA: return _("No IP associated with name");
		case	TCP_ADDR_NOT_AVAILABLE: return _("The specified address is not available on this machine");
		case	TCP_TIMEOUT: return _("Connecting timed out");
		case	TCP_REFUSED: return _("Connection refused");
		case	TCP_UNREACHABLE: return _("Network unreachable");
		case	TCP_FAILED_CONNECT:return _("Failed to connect to the server");
		default: return _("Unknown error");
	}
}

/**
 * Establish the connection to the given server.
 *
 * @param server defines the name of the server to which the connection
 *        should be created.
 * @param port defines to which server port the connection should be established.
 * @param options defines additional options, such as if the connection should be made secure.
 * @param error_code_ptr specifies a location in which an additional error code is stored if the call returns NULL.
 * @return the connection or NULL on failure. Use tcp_error_code() for more information.
 *
 * @note TODO: Rework the error code handling.
 */
struct connection *tcp_connect(char *server, unsigned int port, struct connect_options *options, int *error_code_ptr)
{
	int i,sd;
	struct sockaddr_in sockaddr;
	struct hostent *hostent;
	struct connection *conn = NULL;
	int use_ssl = options?options->use_ssl:0;

	SM_ENTER;

	if (!server)
		goto out;
	if (!(conn = malloc(sizeof(struct connection))))
		goto out;

	memset(conn,0,sizeof(struct connection));

	conn->ssl_verify_failed = options->ssl_verify_failed;

	if ((hostent = gethostbyname(server)))
	{
		sd = socket(PF_INET, SOCK_STREAM, 0);

		SM_DEBUGF(20,("Got socket descriptor %ld\n",sd));
		if (sd != -1)
		{
			memset(&sockaddr,0,sizeof(struct sockaddr_in));

#if defined(_AMIGA) || defined(__MORPHOS__) /* ugly */
			sockaddr.sin_len = sizeof(struct sockaddr_in);
#endif
			sockaddr.sin_family = AF_INET;
			sockaddr.sin_port = htons(port);

			for (i=0; hostent->h_addr_list[i]; i++)
			{
				memcpy(&sockaddr.sin_addr, hostent->h_addr_list[i], hostent->h_length);
				if (connect(sd, (struct sockaddr *) &sockaddr, sizeof(struct sockaddr)) != -1)
				{
					SM_DEBUGF(20,("Connected to socket\n"));
					conn->socket = sd;

#ifndef NO_SSL
					if (use_ssl)
					{
						if (tcp_make_secure(conn, server, options->fingerprint))
						{
							SM_RETURN(conn,"0x%lx");
							return conn;
						}
						else
						{
							SM_DEBUGF(10,("Socket couldn't be made secure\n"));
							/* TODO: Reestablish only if unsecure connection can be tolerated.
							 * Use TCP_NOT_SECURE */
							myclosesocket(sd);
							sd = socket(PF_INET, SOCK_STREAM, 0);
							if (sd == -1)
							{
								error_code = TCP_ERRNO;
								goto out;
							}
							conn->socket = sd;
						}
					} else
					{
						SM_RETURN(conn,"0x%lx");
						return conn;
					}
#else
					SM_RETURN(conn,"0x%lx");
					return conn;
#endif
				}
			}

			if (!i)
			{
				error_code = TCP_HOST_NOT_FOUND;
			} else
			{
				switch(tcp_errno())
				{
					case EADDRNOTAVAIL: error_code = TCP_ADDR_NOT_AVAILABLE; break;
					case ETIMEDOUT: error_code = TCP_TIMEOUT; break;
					case ECONNREFUSED: error_code = TCP_REFUSED; break;
					case ENETUNREACH: error_code = TCP_UNREACHABLE; break;
					default: error_code = TCP_FAILED_CONNECT; break; /* Everything else seems too much low-level for the user to me. */
				}
			}

			myclosesocket(sd);
		} else
		{
			error_code = TCP_ERRNO;
			goto out;
		}
	} else
	{
		switch(tcp_herrno())
		{
			case HOST_NOT_FOUND: error_code = TCP_HOST_NOT_FOUND; break;
			case TRY_AGAIN: error_code = TCP_TRY_AGAIN; break;
			case NO_RECOVERY: error_code = TCP_NO_RECOVERY; break;
			case NO_DATA:error_code = TCP_NO_DATA; break;
			default: error_code = TCP_UNKNOWN; break;
		}
	}

out:
	free(conn);
	SM_RETURN(NULL,"0x%lx");
	return NULL;
}

/**
 * The certificate verify callback used in tcp_make_secure().
 *
 * @param preverify_ok
 * @param x509_ctx
 * @return
 */
#ifdef __SASC
static int __saveds tcp_make_secure_verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx)
#else
static int tcp_make_secure_verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx)
#endif
{
	if (!preverify_ok)
	{
		/* Get ssl object associated to the x509 context */
		SSL *ssl = X509_STORE_CTX_get_ex_data(x509_ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
		int *failed = SSL_get_app_data(ssl);
		*failed = 1;
		preverify_ok = 1;
	}
	return preverify_ok;
}

/**
 * Makes the given connection secure.
 *
 * @param conn defines the connection which should be made
 *             secure.
 * @param server_name the name of the server used to establish the connection
 * @param fingerprint specifies the servers fingerprint. If non-NULL and
 *  non-empty the fingerprint of the remote server must match the given
 *  fingerprint. Both SHA1 or SHA256 are tried. Otherwise, the trust is
 *  established according to the certificate chain.
 * @return 1 on success
 */
int tcp_make_secure(struct connection *conn, char *server_name, char *fingerprint)
{
#ifndef NO_SSL
	int rc;
	int failed = 0;

	if (!open_ssl_lib()) return 0;
	if (!(conn->ssl = SSL_new(ssl_context())))
	{
		close_ssl_lib();
		return 0;
	}
	SSL_set_app_data(conn->ssl, &failed);
	SSL_set_verify(conn->ssl, SSL_VERIFY_PEER, tcp_make_secure_verify_callback);

	/* Associate a socket with ssl structure */
	SSL_set_fd(conn->ssl, conn->socket);

	SM_DEBUGF(10,("Establishing SSL connection\n"));
	if ((rc = SSL_connect(conn->ssl)) >= 0)
	{
		X509 *server_cert;

		if ((server_cert = SSL_get_peer_certificate(conn->ssl)))
		{
			int i, rc;
			unsigned int sha1_size;
			unsigned char sha1[EVP_MAX_MD_SIZE];
			char sha1_ascii[EVP_MAX_MD_SIZE*3+1];
			unsigned int sha256_size;
			unsigned char sha256[EVP_MAX_MD_SIZE];
			char sha256_ascii[EVP_MAX_MD_SIZE*3+1];
			char issuer_common_name[40];
			char subject_common_name[40];
			char cert_summary[200];
			long verify_results;

			X509_digest(server_cert, EVP_sha1(), sha1, &sha1_size);
			for (i=0; i<sha1_size; i++)
				sm_snprintf(&sha1_ascii[i*3], 4, "%02X  ", sha1[i]);
			if (sha1_size>0) sha1_ascii[sha1_size*3-1] = 0;
			else sha1_ascii[0] = 0;

#ifdef USE_OPENSSL
			X509_digest(server_cert, EVP_sha256(), sha256, &sha256_size);
#else
			sha256_size = 0;
#endif
			for (i=0; i<sha256_size; i++)
				sm_snprintf(&sha256_ascii[i*3], 4, "%02X  ", sha256[i]);
			if (sha256_size>0) sha256_ascii[sha256_size*3-1]=0;
			else sha256_ascii[0] = 0;

			/* The server chain is secondary if a fingerprint was specified */
			if (mystrlen(fingerprint))
			{
				if (sha256_size)
				{
					if (!mystricmp(sha256_ascii, fingerprint))
					{
						SM_DEBUGF(5,("Connection is secure according to SHA256 fingerprint\n"));
						X509_free(server_cert);
						return 1;
					}
				}

				if (!mystricmp(sha1_ascii, fingerprint))
				{
					SM_DEBUGF(5,("Connection is secure according to SHA1 fingerprint (%s vs %s)\n", sha1_ascii, fingerprint));
					X509_free(server_cert);
					return 1;
				}
				SM_DEBUGF(5,("Connection could not be trusted as fingerprint didn't match (%s vs %s)!\n", sha1_ascii, fingerprint));
			} else if (!failed)
			{
				SM_DEBUGF(5,("Connection is secure according to cert chain\n"));
				X509_free(server_cert);
				return 1;
			}

			/* Issued to */
			if (X509_NAME_get_text_by_NID(X509_get_subject_name(server_cert), NID_commonName, subject_common_name, sizeof(subject_common_name)) < 0)
				mystrlcpy(subject_common_name, _("Not found"), sizeof(subject_common_name));

			/* Issued by */
			if (X509_NAME_get_text_by_NID(X509_get_issuer_name(server_cert), NID_commonName, issuer_common_name, sizeof(issuer_common_name)) < 0)
				mystrlcpy(issuer_common_name, _("Not found"), sizeof(issuer_common_name));

			sm_snprintf(cert_summary, sizeof(cert_summary),
					_("Issued to: %s (CN)\nIssues by: %s (CN)\nExpected fingerprint: %s"),
					subject_common_name, issuer_common_name, mystrlen(fingerprint)?fingerprint:_("None specified"));

			verify_results = SSL_get_verify_result(conn->ssl);

			if (conn->ssl_verify_failed)
			{
				rc = conn->ssl_verify_failed(server_name, X509_verify_cert_error_string(verify_results), cert_summary, sha1_ascii, sha256_ascii);
			} else
			{
				/* FIXME: Remove this */
				rc = thread_call_function_sync(thread_get_main(), callback_failed_ssl_verification, 5,
						server_name, X509_verify_cert_error_string(verify_results), cert_summary, sha1_ascii, sha256_ascii);
			}

			/* Add some checks here */
			X509_free(server_cert);

			if (rc == 1)
			{
				SM_DEBUGF(5,("Connection is assumed to be secure\n"));
				return 1;
			}
		}
	} else
	{
		int err = SSL_get_error(conn->ssl, rc);
		char buf[128];
		sm_snprintf(buf, sizeof(buf), "SSL_connect() failed with error %d", err);
		SM_DEBUGF(5,("%s\n", buf));
		/* TODO: Implement and use proper error API */
		tell_from_subtask(buf);

		switch (err)
		{
			case	SSL_ERROR_SSL:
					{
						long err_code;
						while ((err_code = ERR_get_error()))
						{
							char err_buf[120];

							ERR_error_string_n(err_code, err_buf, sizeof(err_buf));

							sm_snprintf(buf, sizeof(buf), "More specific error %d: %s", err_code, err_buf);
							tell_from_subtask(buf);
						}
					}
					break;
		}
	}

	SSL_shutdown(conn->ssl);
	SSL_free(conn->ssl);
	close_ssl_lib();
	conn->ssl = NULL;

	SM_DEBUGF(5,("Connection couldn't be made secure!\n"));

#endif

	return 0;
}

/**
 * Returns whether connection is secure.
 *
 * @param conn
 * @return 1 if connection is secure.
 */
int tcp_secure(struct connection *conn)
{
#ifndef NO_SSL
	return !!conn->ssl;
#else
	return 0;
#endif
}

/**
 * Disconnect from the server. Any further accesses
 * to the connection are forbidden afterwards.
 *
 * @param conn defines the connection to be disconnected.
 *        NULL is valid (it's a nop then).
 */
void tcp_disconnect(struct connection *conn)
{
	SM_ENTER;

	if (!conn) return;

	tcp_flush(conn); /* flush the write buffer */

#ifndef NO_SSL
	if (conn->ssl)
	{
		int rc;
		rc = SSL_shutdown(conn->ssl);
		if (rc == 0)
			SSL_shutdown(conn->ssl);
	}
#endif
#ifndef NO_SSL
	if (conn->ssl)
	{
		SSL_free(conn->ssl);
		close_ssl_lib(); /* FIXME: This may lead to crashes */
	}
	myclosesocket(conn->socket);
#endif

	if (conn->line) free(conn->line);
	free(conn);

	SM_LEAVE;
}

/**
 * Read a given amount of bytes from the connection.
 *
 * @param conn the connection from which data should be read.
 * @param buf the buffer in which the data should end up.
 * @param nbytes the number of bytes that should be read.
 * @return the number of bytes which have been read.
 * @note This method may read less bytes than requested.
 */
long tcp_read(struct connection *conn, void *buf, long nbytes)
{
	int didget;

	tcp_flush(conn); /* flush the write buffer */
	if (conn->read_pos < conn->read_size)
	{
		int len = MIN(conn->read_size - conn->read_pos,nbytes);
		memcpy(buf,&conn->read_buf[conn->read_pos],len);
		nbytes -= len;
		conn->read_pos += len;
		return len;
	}
#ifndef NO_SSL
	if (conn->ssl) didget = SSL_read(conn->ssl,buf,nbytes);
	else didget = recv(conn->socket,buf,nbytes,0);
#else
	didget = recv(conn->socket,buf,nbytes,0);
#endif
	if (didget < 0)
	{
		if (tcp_errno() == EINTR) error_code = TCP_INTERRUPTED;
		else error_code = TCP_ERRNO;
	}
	return didget;
}

/**
 * Reads a single char from the connection.
 *
 * @param conn the connection from which the character should be read.
 * @return the character or -1 in case something failed.
 * @note this function is buffered.
 */
static int tcp_read_char(struct connection *conn)
{
	if (conn->read_pos >= conn->read_size)
	{
		/* we must read a new chunk of bytes */
		int didget;
		conn->read_pos = 0;

#ifndef NO_SSL
		if (conn->ssl) didget = SSL_read(conn->ssl,conn->read_buf,sizeof(conn->read_buf));
		else didget = recv(conn->socket,conn->read_buf,sizeof(conn->read_buf),0);
#else
		didget = recv(conn->socket,conn->read_buf,sizeof(conn->read_buf),0);
#endif

		if (didget <= 0)
		{
			if (tcp_errno() == EINTR) error_code = TCP_INTERRUPTED;
			else error_code = TCP_ERRNO;

			conn->read_size = 0;
			return -1;
		}
		conn->read_size = didget;
	}
	return conn->read_buf[conn->read_pos++];
}


/**
 * Writes a given amount of bytes to the connection.
 *
 * @param conn defines the connection on which the data is written to.
 * @param buf the buffer which contains the data.
 * @param nbytes the number of bytes which should be written.
 * @return the number of bytes that actually have been written or -1 for an failure.
 * @note this function is buffered.
 */
int tcp_write(struct connection *conn, const void *buf, long nbytes)
{
	int rc = nbytes;

	conn->read_pos = conn->read_size = 0;

	while (conn->write_size + nbytes >= sizeof(conn->write_buf))
	{
		int size = sizeof(conn->write_buf) - conn->write_size;
		memcpy(&conn->write_buf[conn->write_size],buf,size);
		conn->write_size = sizeof(conn->write_buf);
		if (!tcp_flush(conn)) return -1;
		buf = ((char*)buf) + size;
		nbytes -= size;
	}

	memcpy(&conn->write_buf[conn->write_size],buf,nbytes);
	conn->write_size += nbytes;

	return rc;
}

/**
 * Flushes the write buffer.
 *
 * @param conn the connection whose write buffer should be flushed.
 * @return 1 on success, otherwise 0.
 */
int tcp_flush(struct connection *conn)
{
	int bytes;
	int rc = 1;

	if (conn->write_size)
	{
#ifdef DEBUG_OUTPUT
		printf("C: ");
		{
			static char buf[5000];
			memset(buf,0,sizeof(buf));
			memcpy(buf,conn->write_buf,MIN(5000-1,conn->write_size));
			printf("%s\n",buf);
		}

		if (conn->write_buf[conn->write_size-1]!='\n')
			printf("\n");
#endif

#ifndef NO_SSL
		if (conn->ssl) bytes = SSL_write(conn->ssl, conn->write_buf, conn->write_size);
		else bytes = send(conn->socket, conn->write_buf, conn->write_size, 0);
#else
		bytes = send(conn->socket, conn->write_buf, conn->write_size, 0);
#endif

		if (bytes < 0)
		{
			if (tcp_errno() == EINTR) error_code = TCP_INTERRUPTED;
			else error_code = TCP_ERRNO;
			rc = 0;
		}
		conn->write_size = 0;
	}
	return rc;
}

/**
 * Writes the given amount of bytes to the connection. Unbuffered.
 *
 * @param conn the connection on which the data should be written
 * @param buf the buffer from which the data is fetched
 * @param nbytes the number of sizes which should be written
 * @return number of bytes that actually have been written
 * @note this function is, as the name suggests, unbuffered. If there
 *       is already some data buffered it gets flushed before the actual
 *       data is sent.
 */
int tcp_write_unbuffered(struct connection *conn, void *buf, long nbytes)
{
	conn->read_pos = conn->read_size = 0;
	tcp_flush(conn); /* flush the write buffer */

#ifndef NO_SSL
	if (conn->ssl) return SSL_write(conn->ssl,buf,nbytes);
#endif
	return send(conn->socket, buf, nbytes, 0);
}

/**
 * Read a complete line from the given connection. Line will end
 * with a '\\n'. A '\\r' is removed. The returned buffer is allocated
 * per connection so it is only valid as long as the connection
 * exists and only until the next tcp_readln() is called.
 *
 * @param conn the connection from which the next line should be fetched.
 *
 * @return the line or NULL if something failed.
 */
char *tcp_readln(struct connection *conn)
{
	int line_pos = 0;

	tcp_flush(conn); /* flush the write buffer */
	while (1)
	{
		int c = tcp_read_char(conn);
		if (c < 0) return NULL;

		if (line_pos + 8 > conn->line_allocated)
		{
			conn->line_allocated += 1024;
			conn->line = realloc(conn->line,conn->line_allocated);
		}

		if (!conn->line)
		{
			conn->line_allocated = 0;
			return NULL;
		}

		conn->line[line_pos++] = c;

		if (c=='\n') break;
	}

	conn->line[line_pos]=0;

	if (line_pos > 1)
	{
		if (conn->line[line_pos-2]=='\r')
		{
			conn->line[line_pos-2]='\n';
			conn->line[line_pos-1]=0;
		}
	}

#ifdef DEBUG_OUTPUT
	printf("S: %s",conn->line);
#endif

	return conn->line;
}

/**
 * Wrapper for gethostname
 *
 * @param buf
 * @param buf_size
 * @return
 */
int tcp_gethostname(char *buf, int buf_size)
{
	return gethostname(buf,buf_size);
}


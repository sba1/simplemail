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
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/tcp.h>
#endif

#ifndef NO_SSL

#if defined(_AMIGA ) || defined(__AMIGAOS4__) || defined(__MORPHOS__)/* ugly */
#include <proto/amissl.h> /* not portable */
#else
#include <openssl/ssl.h>
#endif

#endif

#include "tcpip.h"

#undef _
#include "debug.h"
#include "smintl.h"
#include "tcp.h"

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
 * @param use_ssl defines whether connection should be made secure.
 * @return the connection or NULL on failure. Use tcp_error_code() for more information.
 *
 * @note TODO: Rework the error code handling.
 */
struct connection *tcp_connect(char *server, unsigned int port, int use_ssl)
{
	int i,sd;
	struct sockaddr_in sockaddr;
	struct hostent *hostent;
	struct connection *conn = NULL;

	SM_ENTER;

	if (!server)
		goto out;
	if (!(conn = malloc(sizeof(struct connection))))
		goto out;

	memset(conn,0,sizeof(struct connection));

	if ((hostent = gethostbyname(server)))
	{
		sd = socket(PF_INET, SOCK_STREAM, 0);

		SM_DEBUGF(20,("Got socket descriptor %ld\n",sd));
		if (sd != -1)
		{
#ifndef NO_SSL
			int security_error = 0;
#endif
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
						if (tcp_make_secure(conn))
						{
							SM_RETURN(conn,"0x%lx");
							return conn;
						}
						else
						{
							SM_DEBUGF(10,("Socket couldn't be made secure\n"));
							/* TODO: Reestablish only if unsecure connection can be tolerated.
							 * Use TCP_NOT_SECURE */
							security_error = 1;
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
 * Makes the given connection secure.
 *
 * @param conn defines the connection which should be made
 *             secure.
 * @return 1 on success
 */
int tcp_make_secure(struct connection *conn)
{
#ifndef NO_SSL
	int rc;

	if (!open_ssl_lib()) return 0;
	if (!(conn->ssl = SSL_new(ssl_context())))
	{
		close_ssl_lib();
		return 0;
	}

	SSL_set_verify(conn->ssl, SSL_VERIFY_PEER, NULL);

	/* Associate a socket with ssl structure */
	SSL_set_fd(conn->ssl, conn->socket);

	SM_DEBUGF(10,("Establishing SSL connection\n"));
	if ((rc = SSL_connect(conn->ssl)) >= 0)
	{
		X509 *server_cert;
		if ((server_cert = SSL_get_peer_certificate(conn->ssl)))
		{
			/* Add some checks here */
			X509_free(server_cert);

			SM_DEBUGF(5,("Connection is secure\n"));
			return 1;
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
int tcp_write(struct connection *conn, void *buf, long nbytes)
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


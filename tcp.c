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
** tcp.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/tcp.h>

#ifdef _AMIGA /* ugly */
#include <proto/amissl.h> /* not portable */
#else
#include <openssl/ssl.h>
#endif

#include "support.h"
#include "tcpip.h"

#include "tcp.h"

/******************************************************************
 Establish the connection to the given server.
 Return NULL on error.
 (TODO: Remove the error code handling)
*******************************************************************/
struct connection *tcp_connect(char *server, unsigned int port)
{
	long sd;
	struct sockaddr_in sockaddr;
	struct hostent *hostent;
	static char err[256];
	static long id;
	struct connection *conn;

	if (!(conn = malloc(sizeof(struct connection))))
		return NULL;

	memset(conn,0,sizeof(struct connection));

/*	conn->use_ssl = 1;*/

	if (conn->use_ssl)
	{
		if (!open_ssl_lib())
		{
			free(conn);
			return NULL;
		}
		if (!(conn->ssl = SSL_new(ssl_context())))
		{
			close_ssl_lib();
			free(conn);
			return NULL;
		}
	}

	hostent = gethostbyname(server);
	if(hostent != NULL)
	{
#ifdef _AMIGA /* ugly */
		sockaddr.sin_len = sizeof(struct sockaddr_in);
#endif
		sockaddr.sin_family = AF_INET;
		sockaddr.sin_port = htons(port);
		sockaddr.sin_addr = *(struct in_addr *) hostent->h_addr;
		bzero(&(sockaddr.sin_zero), 8);

		sd = socket(PF_INET, SOCK_STREAM, 0);
		if(sd != -1)
		{
			if(connect(sd, (struct sockaddr *) &sockaddr, sizeof(struct sockaddr)) != -1)
			{
				conn->socket = sd;
				if (conn->use_ssl)
				{
					/* Associate a socket with ssl structure */
					SSL_set_fd(conn->ssl, sd);

					if (SSL_connect(conn->ssl) >= 0)
					{
						if (conn->server_cert = SSL_get_peer_certificate(conn->ssl))
						{
							return conn;
						}
					}

					SSL_shutdown(conn->ssl);
				} else return conn;
			}
			else
			{
				switch(id=tcp_errno())
				{
					case EADDRNOTAVAIL:
						strcpy(err, "The specified address is not avaible on this machine.");
						break;

					case ETIMEDOUT:
						strcpy(err, "Connecting timed out.");
						break;

					case ECONNREFUSED:
						strcpy(err, "Connection refused.");
						break;

					case ENETUNREACH:
						strcpy(err, "Network unreachable.");
						break;

					default: /* Everything else seems too much low-level for the user to me. */
						strcpy(err, "Failed to connect to the server.");
						break;
				}

				tell_from_subtask(err);
			}
			myclosesocket(sd);
		}
		else
		{
			strcpy(err, "Failed to create a socketdescriptor.");

			tell_from_subtask(err);
		}
	}
	else
	{
		switch(id = tcp_herrno())
		{
			case HOST_NOT_FOUND:
				sprintf(err, "Host \"%s\" not found.", server);
				break;

			case TRY_AGAIN:
				sprintf(err, "Cannot locate %s. Try again later!", server);
				break;

			case NO_RECOVERY:
				strcpy(err, "Unexpected server failure.");
				break;

			case NO_DATA:
				sprintf(err, "No IP associated with %s!", server);
				break;

			case -1:
				strcpy(err, "Could not determinate a valid error code!");
				break;

			default:
				sprintf(err, "Unknown error %ld!", id);
				break;
		}
		
		tell_from_subtask(err);
	}  

	if (conn->use_ssl)
	{
		if (conn->ssl) SSL_free(conn->ssl);
		close_ssl_lib();
	}

	free(conn);
	return NULL;
}

/******************************************************************
 Disconnect from the server
*******************************************************************/
void tcp_disconnect(struct connection *conn)
{
	tcp_flush(conn); /* flush the write buffer */
	myclosesocket(conn->socket); /* not portable */

	if (conn->use_ssl)
	{
		X509_free(conn->server_cert);
		SSL_shutdown(conn->ssl);
		SSL_free(conn->ssl);
		close_ssl_lib();
	}
	if (conn->line) free(conn->line);
	free(conn);
}

/******************************************************************
 Read a given amount of bytes from the connection.
*******************************************************************/
long tcp_read(struct connection *conn, void *buf, long nbytes)
{
	tcp_flush(conn); /* flush the write buffer */
	if (conn->ssl) return SSL_read(conn->ssl,buf,nbytes);
	return recv(conn->socket,buf,nbytes,0);
}

/******************************************************************
 Read's a single char from the connection. Buffered.
 Returns -1 for an error.
*******************************************************************/
static int tcp_read_char(struct connection *conn)
{
	if (conn->read_pos >= conn->read_size)
	{
		/* we must read a new chunk of bytes */
		int didget;
		conn->read_pos = 0;

		didget = tcp_read(conn,conn->read_buf,sizeof(conn->read_buf));
		if (didget <= 0)
		{
			conn->read_size = 0;
			return -1;
		}
		conn->read_size = didget;
	}
	return conn->read_buf[conn->read_pos++];
}

/******************************************************************
 Writes a given amount of bytes to the connection. Buffered.
*******************************************************************/
int tcp_write(struct connection *conn, void *buf, long nbytes)
{
	int rc = nbytes;

	conn->read_pos = conn->read_size = 0;

	while (conn->write_size + nbytes >= sizeof(conn->write_buf))
	{
		int size = sizeof(conn->write_buf) - conn->write_size;
		memcpy(&conn->write_buf[conn->write_size],buf,size);
		tcp_flush(conn);
		buf = ((char*)buf) + size;
		nbytes -= size;
	}

	memcpy(&conn->write_buf[conn->write_size],buf,nbytes);
  conn->write_size += nbytes;

	return rc;
}

/******************************************************************
 Flushes the write buffer.
*******************************************************************/
int tcp_flush(struct connection *conn)
{
	if (conn->write_size)
	{
		if (conn->ssl) SSL_write(conn->ssl, conn->write_buf, conn->write_size);
		else send(conn->socket, conn->write_buf, conn->write_size, 0);
		conn->write_size = 0;
	}
	return 1;
}

/******************************************************************
 Writes a given amount of bytes to the connection. Unbuffered.
*******************************************************************/
int tcp_write_unbuffered(struct connection *conn, void *buf, long nbytes)
{
	conn->read_pos = conn->read_size = 0;
	tcp_flush(conn); /* flush the write buffer */
	if (conn->ssl) return SSL_write(conn->ssl,buf,nbytes);
	return send(conn->socket, buf, nbytes, 0);
}

/******************************************************************
 Read a complete line from the given connection. Line will end
 with a '\n'. A '\r' is removed. The returned buffer is allocated
 per connection so it is only valid as long as the connection
 exists and only until the next tcp_readln().
 Returns NULL for an error.
*******************************************************************/
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

	return conn->line;
}






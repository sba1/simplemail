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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "support.h"
#include "tcpip.h"

#include "tcp.h"

long tcp_connect(char *server, unsigned int port)
{
	long sd, rc;
	struct sockaddr_in sockaddr;
	struct hostent *hostent;
	static char err[256];
	static long id;
	
	rc = SMTP_NO_SOCKET;

	hostent = gethostbyname(server);
	if(hostent != NULL)
	{
		sockaddr.sin_len = sizeof(struct sockaddr_in);
		sockaddr.sin_family = AF_INET;
		sockaddr.sin_port = htons(port);
		sockaddr.sin_addr = *(struct in_addr *) hostent->h_addr;
		bzero(&(sockaddr.sin_zero), 8);

		sd = socket(PF_INET, SOCK_STREAM, 0);
		if(sd != -1)
		{
			if(connect(sd, (struct sockaddr *) &sockaddr, sizeof(struct sockaddr)) != -1)
			{
				rc = sd;
			}
			else
			{
				switch(id=Errno())
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
		}
		else
		{
			strcpy(err, "Failed to create a socketdescriptor.");

			tell_from_subtask(err);
		}
	}
	else
	{
		switch(id = h_errno)
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

	return rc;
}

void tcp_disconnect(long sd)
{
	if(sd != SMTP_NO_SOCKET)
	{
		shutdown(sd, 2);
	}
}

long tcp_read(long sd, void *buf, long nbytes)
{
	return recv(sd, buf, nbytes, 0);
}

long tcp_write(long sd, void *buf, long nbytes)
{
	return send(sd, buf, nbytes, 0);
}

/*
 * Returns a string if everything went okay,
 * an empty string if no data is present
 * or 0 on error.
 */
#define TCP_READLN_BUFSIZE 1500
/*
char *tcp_readln(long sd)
{
	char *rc = NULL;
	static char readbuf[TCP_READLN_BUFSIZE + 1], *rptr;
	char *pos;
	unsigned long len;

	if(rptr != NULL && *rptr != 0)
	{
		pos = strstr(rptr, "\r\n");
		if(pos != NULL)
		{
			len = (unsigned long) readbuf - rptr;
			rc = malloc(len + 1);
			strncpy(rc, rptr, len);
			rptr += len;
			memcpy(readbuf, rptr);
			rptr = readbuf;
			rptr += len;
		}
	}
	long stat;

	IoctlSocket(sd, FIONBIO, 1);

	len = 0;

	while(1)
	{
		stat = tcp_read(sd, rptr, TCP_READLN_BUFSIZE);
		if(stat == -1 && Errno() == EAGAIN)
		{
			break;
		}
		else
		{

		}
	}

	IoctlSocket(sd, FIOBBIO, 0);

	return rc;
}
*/

/*
char *tcp_readln(long sd)
{
	char *rc = NULL;
	static char readbuf[TCP_READLN_BUFSIZE + 1], *rptr;
	char *buf, *b1, b2;
	int stat;
	long got = 0;

	IoctlSocket(sd, FIONBIO, 1);

	buf = b1 = b2 = NULL;

	while(1)
	{
		stat = tcp_read(sd, readbuf, TCP_READLN_BUFSIZE);

		if(stat == -1 && (Errno() == EWOULDBLOCK || Errno() == EAGAIN))
		{
			rc = malloc(got+1);
			strcpy(rc, buf);
			break;
		}
		else if(stat == -1)
		{
			tell_from_subthread("Error while receiving.");
			break;
		}
		else
		{
			got += stat;
			readbuf[stat] = '\0';

			if(b1 == NULL && b2 == NULL && buf == NULL)
			{
				buf = b1 = malloc(stat + 1)
			}
			else
			{
				if(b1 == buf)
				{
					if(b2)
					{
						free(b2);
					}

					buf = b2 = malloc(stat + 1);

					free(b1);
					b1 = NULL;
				}
				else
				{
					if(b1)
					{
						free(b1);
					}

					buf = b1 = malloc(stat + 1);

					free(b2);
					b2 = NULL;

				}
			}

			strcpy(buf, stat);
			if(pos = strstr(buf, "\r\n"))
			{
				rptr = pos;
			}
		}
	}

	free(buf);

	IoctlSocket(sd, FIOBBIO, 0);

	return rc;
}*/

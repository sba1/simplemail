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
#include <sys/filio.h>
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

#define TCP_READLN_BUFSIZE 1500

/******************************************************************
 Read a complete line from the given socket. Not very well
 implemented (big overhead) but it works for now. Line
 will end with a '\n' an '\r' is removed. The returned
 buffer is allocated with malloc(). Returns NULL if end
 of stream is reached.
*******************************************************************/
char *tcp_readln(long sd)
{
	char *rc;
	static char readbuf[TCP_READLN_BUFSIZE + 2];
	int i=0;

	while (i<TCP_READLN_BUFSIZE)
	{
		if (tcp_read(sd, &readbuf[i],1)<=0)
			return NULL;

		if (readbuf[i]=='\n')
		{
			if (i && readbuf[i-1] == '\r')
			{
				readbuf[i-1] = '\n';
			} else i++;
			break;
		}
		i++;
	}

	/* add a nullbyte at the end */
	readbuf[i] = 0;

	if ((rc = malloc(strlen(readbuf) + 1)))
		strcpy(rc, readbuf);

	return rc;
}

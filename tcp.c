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

#include "tcpip.h"

#include "io.h"
#include "tcp.h"

long tcp_connect(char *server, unsigned int port)
{
   long hsocket, rc;
   struct sockaddr_in sockaddr;
   struct hostent *hostent;
   
   rc = SMTP_NO_SOCKET;

   hostent = gethostbyname(server);
   if(hostent != NULL)
   {
      sockaddr.sin_len = sizeof(struct sockaddr_in);
      sockaddr.sin_family = AF_INET;
      sockaddr.sin_port = port;
      sockaddr.sin_addr.s_addr = 0;

      memcpy(&sockaddr.sin_addr, hostent->h_addr, hostent->h_length);
      
      hsocket = socket(hostent->h_addrtype, SOCK_STREAM, 0);
      if(hsocket != -1)
      {
         if(connect(hsocket, (struct sockaddr *) &sockaddr, sizeof(struct sockaddr_in)) != -1)
         {
            rc = hsocket;
         }
         else
         {
            tell("Connect() failed!");
         }
      }
      else
      {
         tell("Socket() failed!");
      }
   }
   else
   {
      static char err[256];
      
      if(Errno() == TRY_AGAIN)
      {
         sprintf(err, "Can'\t locate %s. Try again later!", server);
      }
      else
      {
         sprintf(err, "%s is not a valid server!", server);
      }
      
      tell(err);
   }  

   return(rc);
}

void tcp_disconnect(long hsocket)
{
   if(hsocket != SMTP_NO_SOCKET)
   {
      CloseSocket(hsocket);
      shutdown(hsocket, 2);
   }
}


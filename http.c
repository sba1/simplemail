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
** pop3.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "tcp.h"
#include "tcpip.h"
#include "support_indep.h"

#include "http.h"

int http_download_photo(char *path, char *email)
{
	struct connection *conn;
	int rc;

	if (!open_socket_lib()) return 0;

  rc = 0;

	if ((conn = tcp_connect("simplemail.sourceforge.net",80)))
	{
		char *line;
		int download = 0;

		tcp_write(conn,"GET /galery_get_image.php?",sizeof("GET /galery_get_image.php?")-1);
		tcp_write(conn,email,strlen(email));
		tcp_write(conn," HTTP/1.0\r\nhost: simplemail.sourceforge.net\r\n\r\n",sizeof(" HTTP/1.0\r\nhost: simplemail.sourceforge.net\r\n\r\n")-1);

		while ((line = tcp_readln(conn)))
		{
			if (!mystrnicmp("Content-Type: image/",line,20)) download = 1;
			if (line[0] == 10 && line[1] == 0) break;
		}

		if (download)
		{
			FILE *fp = fopen(path,"wb");
			if (fp)
			{
				int got;
				char buf[1024];
				while ((got = tcp_read(conn,buf,1024))>0)
				{
					fwrite(buf,1,got,fp);
				}
				rc = 1;
				fclose(fp);
			}
		}

		tcp_disconnect(conn);
	}
	close_socket_lib();
	return rc;
}

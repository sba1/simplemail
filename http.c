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

#ifdef __WIN32__
#include <windows.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#endif

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

	if ((conn = tcp_connect("simplemail.sourceforge.net",80,0)))
	{
		char *line;
		int download = 0;

		tcp_write(conn,"GET /gallery_get_image.php?",sizeof("GET /gallery_get_image.php?")-1);
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

int http_download(char *uri, void **buf_ptr, int *buf_len_ptr)
{
	int rc = 0;

	if (!mystrnicmp(uri,"http://",7))
	{
		int port;
		char *path_buf;
		char *port_buf;
		struct connection *conn;
		char *server;

		uri += 7;

		if (!(server = mystrdup(uri))) return 0;
		if (!(path_buf = strchr(server,'/')))
		{
			free(server);
			return 0;
		}
		port_buf = strchr(uri,':');

		if (port_buf > path_buf) port_buf = NULL;
		if (port_buf)
		{
			*port_buf = 0;
			port = atoi(port_buf+1);
		} else
		{
			*path_buf = 0;
			port = 80;
		}

		if (open_socket_lib())
		{
			if ((conn = tcp_connect(server,port,0)))
			{
				FILE *fh;

				if ((fh = tmpfile()))
				{
					int download = 1;
					char *line;

					tcp_write(conn,"GET /", sizeof("GET /")-1);
					tcp_write(conn,path_buf+1,strlen(path_buf+1));
					tcp_write(conn," HTTP/1.0\r\nhost: ", sizeof(" HTTP/1.0\r\nhost: ")-1);
					tcp_write(conn,server,strlen(server));
					tcp_write(conn,"\r\n\r\n",sizeof("\r\n\r\n")-1);

					while ((line = tcp_readln(conn)))
					{
						if (line[0] == 10 && line[1] == 0) break;
					}

					if (download)
					{
						int got;
						int len = 0;
						char buf[1024];
						while ((got = tcp_read(conn,buf,1024))>0)
						{
							fwrite(buf,1,got,fh);
							len += got;
						}

						if ((*buf_ptr = malloc(len)))
						{
							fseek(fh,0,SEEK_SET);
							fread(*buf_ptr,1,len,fh);
							*buf_len_ptr = len;
							rc = 1;
						}
					}
					fclose(fh);
				}
				tcp_disconnect(conn);
			}
			close_socket_lib();
		}
		free(server);
	}
	return rc;
}


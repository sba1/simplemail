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
** tcp.h
*/

#ifndef SM__TCP_H
#define SM__TCP_H

#define CONN_BUF_READ_SIZE 1024
#define CONN_BUF_WRITE_SIZE 1024

#ifdef _AMIGA
#ifndef AMISSL_AMISSL_H
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct x509_st X509;
#endif
#else

#ifndef NO_SSL
#include <openssl/ssl.h>
#endif

#endif

struct connection
{
	long socket;
#ifndef NO_SSL
	SSL *ssl;
#endif
	/* for tcp_write() */
	unsigned char write_buf[CONN_BUF_WRITE_SIZE];
	int write_size;

	/* for tcp_readln() */
	unsigned char read_buf[CONN_BUF_READ_SIZE];
	int read_pos;
	int read_size;
	char *line; /* dynamically allocated, it's hold the line returned in tcp_readln() */
	int line_allocated; /* number of bytes which were allocated (including 0 byte) */
};

struct connection *tcp_connect(char *server, unsigned int port, int use_ssl);
void tcp_disconnect(struct connection *conn);
long tcp_read(struct connection *conn, void *, long);
int tcp_write(struct connection *conn, void *, long);
int tcp_write_unbuffered(struct connection *conn, void *,long);
int tcp_flush(struct connection *conn);
char *tcp_readln(struct connection *conn);


#define SMTP_NO_SOCKET        -1
#define SMTP_STATUS_REPLY     211
#define SMTP_HELP_MESSAGE     214
#define SMTP_SERVICE_READY    220
#define SMTP_SERVICE_CLOSE    221
#define SMTP_OK               250
#define SMTP_OK_FORWARD       251
#define SMTP_SEND_MAIL        354
#define SMTP_CRASH_CLOSE      421
#define SMTP_MBOX_BUSY        450
#define SMTP_ABORT_FAILURE    451
#define SMTP_STORAGE_FULL     452
#define SMTP_BAD_CMD_SYNTAX   500
#define SMTP_BAD_PARM_SYNTAX  501
#define SMTP_UNKNOWN_CMD      502
#define SMTP_BAD_CMD_SEQ      503
#define SMTP_UNKNOWN_PARM     504
#define SMTP_UNKNOWN_MBOX     550
#define SMTP_FORWARD_TO       551
#define SMTP_STORAGE_IS_FULL  552
#define SMTP_BAD_MBOX_SYNTAX  553
#define SMTP_TX_FAILURE       554

#endif


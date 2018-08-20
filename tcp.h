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

#ifndef NO_SSL

#if defined(_AMIGA) || defined(__AMIGAOS4__)
#if !defined(USE_OPENSSL) && !defined(AMISSL_AMISSL_H)
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct x509_st X509;
#endif
#endif

#include <openssl/ssl.h>

#endif

/**
 * Type for a callback that is invoked  when the ssl verification failed. The
 * callback must be callable as long as the connection is alive.
 *
 * @return 1 if the connection should still take place, 0 if the connection
 *  should be closed.
 */
typedef int (*tcp_ssl_verfiy_failed_t)(const char *server_name, const char *reason, const char *cert_summary, const char *sha1_ascii, const char *sha256_ascii);

struct connect_options
{
	int use_ssl;
	char *fingerprint;

	/**
	 * The ssl verification failed callback. This must be callable as long as
	 * the connection is kept alive
	 */
	tcp_ssl_verfiy_failed_t ssl_verify_failed;
};

struct connection
{
	long socket;
#ifndef NO_SSL
	SSL *ssl;

	/** The ssl verification failed callback. */
	tcp_ssl_verfiy_failed_t ssl_verify_failed;
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

	void *udata; /* Arbitrary user-data for  the following hooks */
	int (*read)(struct connection *c, void *buf, size_t len);
	int (*write)(struct connection *c, void *buf, size_t len);
};

int tcp_error_code(void);
const char *tcp_strerror(int code);

/**
 * Create and initialize a connection.
 *
 * @return the newly initialized connection.
 */
struct connection *tcp_create_connection(void);

struct connection *tcp_connect(char *server, unsigned int port, struct connect_options *options, int *error_code_ptr);
void tcp_disconnect(struct connection *conn);
int tcp_make_secure(struct connection *conn, char *server_name, char *fingerprint);
int tcp_secure(struct connection *conn);
long tcp_read(struct connection *conn, void *, long);
int tcp_write(struct connection *conn, const void *, long);
int tcp_write_unbuffered(struct connection *conn, void *,long);
int tcp_flush(struct connection *conn);
char *tcp_readln(struct connection *conn);
int tcp_gethostname(char *buf, int buf_size);

#define TCP_OK									  0
#define TCP_UNKNOWN						 -1 /* Unspecified error */
#define TCP_NOT_ENOUGH_MEMORY	 -2 /* Not enough memory */
#define TCP_NOT_SECURE					 -3 /* Connection couldn't made secure */
#define TCP_ERRNO							 -4 /* There is another error (errno) value, use tcp_errno() to get it */
#define TCP_HOST_NOT_FOUND			 -5 /* The host could not be found */
#define TCP_TRY_AGAIN					 -6
#define TCP_NO_RECOVERY				 -7
#define TCP_NO_DATA						 -8
#define TCP_ADDR_NOT_AVAILABLE	 -9
#define TCP_TIMEOUT						-10
#define TCP_REFUSED						-11
#define TCP_UNREACHABLE				-12
#define TCP_FAILED_CONNECT			-13
#define TCP_INTERRUPTED				-14 /* by tcp_read(), tcp_readln() */

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


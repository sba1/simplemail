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
** smtp.h
*/

#ifndef SM__SMTP_H
#define SM__SMTP_H

struct outmail
{
	char *from; /* the from address */
	char **rcp; /* an array of all recipients (e-mails) */
	char *mailfile; /* the name of the file */
	int size; /* the size of the mail */
};

/* functions for outmail */
struct outmail **create_outmail_array(int amm);
struct outmail **duplicate_outmail_array(struct outmail **om);
void free_outmail_array(struct outmail **om_array);

#define ESMTP_ENHACEDSTATUSCODES  (1<<0)
#define ESMTP_8BITMIME            (1<<1)
#define ESMTP_ONEX                (1<<2)
#define ESMTP_ETRN                (1<<3)
#define ESMTP_XUSR                (1<<4)
#define ESMTP_AUTH                (1<<5)
#define ESMTP_PIPELINING          (1<<6)
#define ESMTP_STARTTLS						(1<<7)

#define AUTH_PLAIN							 1
#define AUTH_LOGIN							 2
#define AUTH_DIGEST_MD5				 4
#define AUTH_CRAM_MD5					 8

struct smtp_connection
{
	struct connection *conn;
	int flags;			/* ESMTP flags */
	int auth_flags; /* Supported AUTH methods */
};

struct smtp_server
{
	char *name;
	unsigned int 	port;
	char *domain;

	int ip_as_domain;
	int pop3_first;
	int secure;

	int auth;
	char *auth_login;
	char *auth_password;
};

int smtp_send(struct list *account_list, struct outmail **outmail, char *folder_path);

struct smtp_server *smtp_malloc(void);
struct smtp_server *smtp_duplicate(struct smtp_server *smtp);
void smtp_free(struct smtp_server *);

#endif

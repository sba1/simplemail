/*
** smtp.h
*/

#ifndef SM__SMTP_H
#define SM__SMTP_H

struct out_mail
{
   char *domain;
   char *from;
   char **rcp;
   char *mailfile;
};

struct smtp_server
{
	char *name;
	unsigned int port;
	long socket;

	struct out_mail **out_mail;
};

int smtp_send(struct smtp_server *server);

#endif

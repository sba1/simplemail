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
	long port;
	long socket;
};

int smtp_send(struct smtp_server *server, struct out_mail **om);

#endif

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

#endif

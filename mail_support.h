/**
 * @file mail_support.h
 */

#ifndef SM__MAIL_SUPPORT_H
#define SM__MAIL_SUPPORT_H

int mailncpy(char *dest, const char *src, int n);
char *quote_text(char *src, int len);

#endif

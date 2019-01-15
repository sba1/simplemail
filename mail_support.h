/**
 * @file mail_support.h
 */

#ifndef SM__MAIL_SUPPORT_H
#define SM__MAIL_SUPPORT_H

int mailncpy(char *dest, const char *src, int n);
char *quote_text(char *src, int len);

/**
 * Given a subject line, return a subject line that can be used for comparing.
 *
 * @param subj the string that should be transformed
 * @return the transformed string
 */
const char *mail_get_compare_subject(const char *subj);

#endif

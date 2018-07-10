/**
 * @file
 */

#ifndef SM__IMAP_HELPER_H
#define SM__IMAP_HELPER_H

/**
 * Writes the next word into the dest buffer but not more than dest_size.
 *
 * @param src
 * @param dest
 * @param dest_size
 * @return
 */
char *imap_get_result(char *src, char *dest, int dest_size);

#endif

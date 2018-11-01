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

/**
 * @file codesets.h
 */

#ifndef SM__CODESETS_H
#define SM__CODESETS_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

#ifdef __SASC
#ifndef inline
#define inline __inline
#endif
#endif

typedef char utf8;

struct single_convert
{
	unsigned char code; /* the code in this representation */
	unsigned char utf8[8]; /* the utf8 string, first byte is always the length of the string */
	unsigned int ucs4; /* the full 32 bit unicode */
};

struct codeset
{
	struct node node;
	char *name;
	char *alt_name; /* alternative name */
	char *characterization;
	int read_only; /* 1 if this charset should never be used to write */
	struct single_convert table[256];
	struct single_convert table_sorted[256];
};

char **codesets_supported(void);


/**
 * Initialized and loads the codesets
 *
 * @return 1 on success, 0 on failure
 */
int codesets_init(void);

/**
 * Cleanup the memory for the codesets
 */
void codesets_cleanup(void);

/**
 * Finds the codeset by the given name. NULL returns ISO-8859-1 Latin 1
 *
 * @param name the name of
 * @return the codeset
 */
struct codeset *codesets_find(const char *name);

/**
 * Determines number of characters that cannot be converted when a given
 * utf8 text is transformed to a given codeset
 *
 * @param codeset the codetest
 * @param text the UTF8-encoded text to be converted
 * @param text_len number of bytes that should be converted
 * @return number of characters
 */
int codesets_unconvertable_chars(struct codeset *codeset, const char *text, int text_len);

/**
 * Returns the best codeset for the given text
 *
 * @param text the text for which the best codeset shall be determined
 * @param text_len length of the text (in bytes).
 * @param error_ptr here the number of conversion errors is stored
 * @return the best codesets
 */
struct codeset *codesets_find_best(const char *text, int text_len, int *error_ptr);

#define utf8size(s) ((s)?(strlen(s)):(0))
#define utf8cpy(dest,src) strcpy(dest,src)
#define utf8cat(dest,src) strcat(dest,src)

/**
 * Is the given string ASCII 7 bit only?
 *
 * @param str the text that should be checked
 * @return
 */
int isascii7(const char *str);

/**
 * Returns the number of characters a utf8 string has. This is not
 * identically with the size of memory is required to hold the string.
 * Please use utf8size() for this.
 *
 * @param str the string whose length shall be determined.
 * @return the length (in characters) of the string
 */
int utf8len(const utf8 *str);

/**
 * Duplicates an utf8 string.
 *
 * @param str the string to be duplicated
 * @return
 */
utf8 *utf8dup(const utf8 *str);

/**
 * Returns the number of bytes occupied by the utf8 character.
 *
 * @param str
 * @return
 */
int utf8bytes(const utf8 *str);

/**
 * Transforms a character position to the position in the char array
 *
 * @param str
 * @param pos
 * @return
 */
int utf8realpos(const utf8 *str, int pos);

/**
 * @brief Transform absolute byte position in the char array into a character position.
 *
 * @param str the utf8 string in question.
 * @param pos the byte position within the utf8 string.
 * @return the actual character position
 */
int utf8charpos(const utf8 *str, int pos);

/**
 * Copies a number of characters from "from" to "to".
 *
 * @param to
 * @param from
 * @param n
 * @return
 */
utf8 *utf8ncpy(utf8 *to, const utf8 *from, int n);

/**
 * Creates a uf8 string from a different one. from is the iso string and
 * charset the charset of from
 *
 * @param from
 * @param charset
 * @return
 */
utf8 *utf8create(void *from, const char *charset);

/**
 * Creates a uf8 string from a different one. from is the iso string and
 * charset the charset of from
 *
 * @param from
 * @param charset
 * @param from_len
 * @return
 */
utf8 *utf8create_len(void *from, const char *charset, int from_len);

/**
 * Converts a string with a given codeset to a utf8 representation.
 *
 * @param from
 * @param codeset NULL is okay (default codeset is assumed then)
 * @param dest
 * @param dest_size
 * @return number of bytes within the destination buffer
 */
int utf8fromstr(char *str, struct codeset *codeset, utf8 *dest, unsigned int dest_size);

/**
 * Converts a utf8 string to a given charset. Return the number of bytes
 * written to dest excluding the NULL byte (which is always ensured by this
 * function).
 *
 * @param str
 * @param dest
 * @param dest_size
 * @param codeset
 * @return
 */
int utf8tostr(const utf8 *str, char *dest, unsigned int dest_size, struct codeset *codeset);

/**
 *  Converts a UTF8 string to a representation as given by the charset.
 *  The returned string is allocated with malloc()
 *
 * @param str
 * @param codeset
 * @return
 */
char *utf8tostrcreate(const utf8 *str, struct codeset *codeset);

/**
 * Compares two utf8 string case-insensitive.
 *
 * @param str1
 * @param str2
 * @return
 * @note should be fixed for alias and endian issues
 */
int utf8stricmp(const char *str1, const char *str2);

/**
 * Compares two utf8 string case-insensitive (the args might be NULL).
 * Note: Changes for little endian
 *
 * @param str1
 * @param str2
 * @param len
 * @return
 */
int utf8stricmp_len(const char *str1, const char *str2, int len);

typedef unsigned int match_mask_t;

#define MATCH_MASK_T_BYTES sizeof(match_mask_t)
#define MATCH_MASK_T_BITS (MATCH_MASK_T_BYTES*8)

/**
 * Return the bitmask of the bit corresponding to the given pos.
 *
 * @param pos
 * @return
 */
static inline match_mask_t match_bitmask(unsigned int pos)
{
	return (match_mask_t)1 << (MATCH_MASK_T_BITS - 1 - pos % MATCH_MASK_T_BITS);
}

/**
 * Return the actual position of the bitmask for the given position.
 *
 * @param pos
 * @return
 */
static inline unsigned int match_bitmask_pos(unsigned pos)
{
	return pos / MATCH_MASK_T_BITS;
}

/**
 * Return the size occupied by a match bitmask representing len characters.
 *
 * @param len
 * @return
 */
static inline unsigned int match_bitmask_size(int len)
{
	return (len + MATCH_MASK_T_BITS - 1) / MATCH_MASK_T_BITS * MATCH_MASK_T_BYTES;
}

/**
 * @return whether there is a hit at the given pos.
 */
static inline int match_hit(match_mask_t *mask, int pos)
{
	unsigned int mp = match_bitmask_pos(pos);
	return !!(mask[mp] & match_bitmask(pos));
}

/**
 * Tries to match needle against haystack. A needle matches if it is a substring
 * of the haystack or, more generally, if the sequence of characters in the
 * substring can be found in the haystack with possible additional characters.
 * For instances, "158" matches "12345678".
 *
 * @param haystack
 * @param needle
 * @param case_insensitive
 * @param match_mask bitmask that must be at large as match_bitmask_size(utf8len(haystack)).
 *  Here the match mask is written, i.e., an 1 at bit position i, if position i matches, 0
 *  otherwise. Note that these are true utf8 character positions.
 *
 * @return 1, if matched
 */
int utf8match(const char *haystack, const char *needle, int case_insensitive, match_mask_t *match_mask);

/**
 * Converts a utf8 encoded character to its lower case equivalent.
 *
 * @param str defines the source character
 * @param dest note that dest should be at least 6 bytes in size.
 * @return the number of bytes written to dest. It is <= 0 for an error.
 *
 * @note should be fixed for alias and endian issues. Also doesn't respect
 *       locale settings.
 */
int utf8tolower(const char *str, char *dest);

/**
 * Returns the pointer where the given string starts or NULL
 *
 * @param str1
 * @param str2
 * @return
 */
char *utf8stristr(const char *str1, const char *str2);

/**
 * Converts a single UTF8 char to a given charset. Returns the number of
 * bytes to the next utf8 char. The resulting *chr might be 0 if it was
 * not in the codeset or could not be decoded for any other reasons.
 *
 * @param str
 * @param chr
 * @param codeset
 * @return
 */
int utf8tochar(const utf8 *str, unsigned int *chr, struct codeset *codeset);

/**
 * Converts a single UFT-8 Chracter to a�Unicode character very very
 * incomplete. Should return NULL if invalid (actualy not implemented)
 *
 * @param chr
 * @param code
 * @return
 */
const char *uft8toucs(const char *chr, unsigned int *code);

/**
 * Converts a UTF7 string into UTF8
 *
 * @param source
 * @param sourcelen
 * @return
 */
char *utf7ntoutf8(char *source, int sourcelen);

/**
 * Converts a IMAP "UTF7" string into UTF8. (see RFC2060)
 *
 * @param source
 * @param sourcelen
 * @return
 */
char *iutf7ntoutf8(char *source, int sourcelen);

/**
 * @brief Converts a utf8 string to utf7 that is used by IMAP4 protocol.
 *
 * @param utf8 defines the
 * @param sourcelen the number bytes (not characters!) of the utf8 string.
 * @return the null-terminated utf7 string allocated with malloc().
 */
char *utf8toiutf7(char *utf8, int sourcelen);

/**
 * Converts a utf8 string to puny code.
 *
 * @param str
 * @param sourcelen
 * @return
 */
char *utf8topunycode(const utf8 *str, int sourcelen);

/**
 * Converts a puny code sequence into a utf8 encoded string.
 *
 * @param source the source string
 * @param sourcelen number of converted bytes if the source string
 * @return
 */
utf8 *punycodetoutf8(const char *source, int sourcelen);

/**
 * Return wether the given sequence between source and source end (inclusive)
 * is a legal one.
 *
 * @return 1 or 0 depending wether it is legal or not.
 */
int utf8islegal(const char *source, const char *sourceend);

#endif

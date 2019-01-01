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
 * @file codecs.h
 */
#ifndef SM__CODECS_H
#define SM__CODECS_H

#ifndef SM__CODESETS_H
#include "codesets.h"
#endif

struct address_list;

/**
 * Decoding a given buffer using the base64 algorithm. *ret_len can be used
 * to cut the decoding, but the result might differ
 *
 * @param src the source base64 encoded buffer
 * @param len number of source bytes to decode
 * @param ret_len where to store the length of the decoded buffer.
 * @return the malloc'ed buffer or NULL on a failure. The actual buffer is
 * one byte larger than suggested by ret_len. The last byte is always a 0-byte.
 */
char *decode_base64(unsigned char *buf, unsigned int len, unsigned int *ret_len);

/**
 * Decoding a given buffer using the quoted_printable algorithm.
 * Set header to 1 if you want header decoding (underscore = space)
 *
 * @param buf the buffer to be decoded
 * @param len number of source bytes to be decoded
 * @param ret_len where to store the length of the decoded buffer. Also acts
 *  as an input parameter to specify the maximal number of bytes that should be
 *  decoded.
 * @param header whether this is a header that shall be decoded
 * @return the malloc'ed buffer or NULL on a failure. The actual buffer is
 *  one byte larger than suggested by ret_len. The last byte is always a 0-byte.
  */
char *decode_quoted_printable(unsigned char *buf, unsigned int len, unsigned int *ret_len, int header);

/**
 * Creates a unstructured encoded header field (includes all rules of the
 * RFC 821 and RFC 2047)
 * The string is allocated with malloc()
 *
 * @param field_name the field name to encode
 * @param field_contents the contents of the field to encoded
 * @return the encoded header.
 */
char *encode_header_field(const char *field_name, const char *field_contents);

/**
 * Creates a unstructured encoded header field (includes all rules of the
 * RFC 821 and RFC 2047)
 * The string is allocated with malloc()
 *
 * @param field_name the field name to encode
 * @param field_contents the field contents to encode
 * @return the encoded header
 */
char *encode_header_field_utf8(const char *field_name, const char *field_contents);

/**
 * Creates a structured address encoded header field (includes all rules of the
 * RFC 821 and RFC 2047). List is the list with all addresses.
 * The string is allocated with malloc().
 * This function is going to be replaced with the below one soon.
 *
 * @param field_name the field name to encode
 * @param address_list addresses to encoded
 * @return the encoded string
 */
char *encode_address_field(const char *field_name, struct address_list *address_list);

/**
 * Creates a structured address encoded header field (includes all rules of the
 * RFC 821, RFC 2047 and RFC3490). List is the list with all addresses.
 * The string is allocated with malloc()
 *
 * @param field_name the field name to encode
 * @param address_list addresses to encoded
 * @return the generated string. Must be freed with free().
 */
char *encode_address_field_utf8(const char *field_name, struct address_list *address_list);

/**
 * Encode the given email address puny (RFC 3490)

 * @param email the email to encoded
 * @return Puny encoding of the email. Needs to be freed with free() when no longer in use.
 */
char *encode_address_puny(utf8 *email);

/**
 * Encodes an given string to the base64 format. The string is 0 terminated.
 * No line feeds are inserted.
 *
 * @param buf the buffer to be encoded
 * @param len the number of valid bytes in buf-
 * @return the encoded buffer.
 */
char *encode_base64(unsigned char *buf, unsigned int len);

/**
 * Encodes the given body. The encoded buffer is allocated with malloc(),
 * the length is stored in *ret_len and the used transfer encoding in
 * *encoding (MIME Content-Transfer-Encoding). The returned buffer is 0 byte
 * terminated.
 *
 * @param buf the buffer to be encoded
 * @param len the number of valid bytes to be encoded
 * @param content_type the content type of the body
 * @param ret_len number of bytes in the encoded buffer
 * @param encoding where the encoding is stored that was actually used.
 * @return the encoded body.
 */
char *encode_body(unsigned char *buf, unsigned int len, char *content_type, unsigned int *ret_len, const char **encoding);

#endif


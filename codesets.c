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
** codesets.c
*/

#include <ctype.h>
#include <sys/types.h>
#include <dirent.h> /* dir stuff */
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "codesets.h"
#include "codesets_table.h"
#include "debug.h"
#include "lists.h"
#include "punycode.h"
#include "smintl.h"
#include "support_indep.h"

#include "arch.h"

/* from ConvertUTF.h */

/*
 * Copyright 2001 Unicode, Inc.
 *
 * Disclaimer
 * 
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 * 
 * Limitations on Rights to Redistribute This Code
 * 
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/* ---------------------------------------------------------------------

    Conversions between UTF32, UTF-16, and UTF-8.  Header file.

    Several funtions are included here, forming a complete set of
    conversions between the three formats.  UTF-7 is not included
    here, but is handled in a separate source file.

    Each of these routines takes pointers to input buffers and output
    buffers.  The input buffers are const.

    Each routine converts the text between *sourceStart and sourceEnd,
    putting the result into the buffer between *targetStart and
    targetEnd. Note: the end pointers are *after* the last item: e.g. 
    *(sourceEnd - 1) is the last item.

    The return result indicates whether the conversion was successful,
    and if not, whether the problem was in the source or target buffers.
    (Only the first encountered problem is indicated.)

    After the conversion, *sourceStart and *targetStart are both
    updated to point to the end of last text successfully converted in
    the respective buffers.

    Input parameters:
	sourceStart - pointer to a pointer to the source buffer.
		The contents of this are modified on return so that
		it points at the next thing to be converted.
	targetStart - similarly, pointer to pointer to the target buffer.
	sourceEnd, targetEnd - respectively pointers to the ends of the
		two buffers, for overflow checking only.

    These conversion functions take a ConversionFlags argument. When this
    flag is set to strict, both irregular sequences and isolated surrogates
    will cause an error.  When the flag is set to lenient, both irregular
    sequences and isolated surrogates are converted.

    Whether the flag is strict or lenient, all illegal sequences will cause
    an error return. This includes sequences such as: <F4 90 80 80>, <C0 80>,
    or <A0> in UTF-8, and values above 0x10FFFF in UTF-32. Conformant code
    must check for illegal sequences.

    When the flag is set to lenient, characters over 0x10FFFF are converted
    to the replacement character; otherwise (when the flag is set to strict)
    they constitute an error.

    Output parameters:
	The value "sourceIllegal" is returned from some routines if the input
	sequence is malformed.  When "sourceIllegal" is returned, the source
	value will point to the illegal value that caused the problem. E.g.,
	in UTF-8 when a sequence is malformed, it points to the start of the
	malformed sequence.  

    Author: Mark E. Davis, 1994.
    Rev History: Rick McGowan, fixes & updates May 2001.

------------------------------------------------------------------------ */

/* ---------------------------------------------------------------------
    The following 4 definitions are compiler-specific.
    The C standard does not guarantee that wchar_t has at least
    16 bits, so wchar_t is no less portable than unsigned short!
    All should be unsigned values to avoid sign extension during
    bit mask & shift operations.
------------------------------------------------------------------------ */

typedef unsigned long	UTF32;	/* at least 32 bits */
typedef unsigned short	UTF16;	/* at least 16 bits */
typedef unsigned char	UTF8;	/* typically 8 bits */
typedef unsigned char	Boolean; /* 0 or 1 */

/* Some fundamental constants */
#define UNI_REPLACEMENT_CHAR (UTF32)0x0000FFFD
#define UNI_MAX_BMP (UTF32)0x0000FFFF
#define UNI_MAX_UTF16 (UTF32)0x0010FFFF
#define UNI_MAX_UTF32 (UTF32)0x7FFFFFFF

typedef enum {
	conversionOK, 		/* conversion successful */
	sourceExhausted,	/* partial character in source, but hit end */
	targetExhausted,	/* insuff. room in target for conversion */
	sourceIllegal,		/* source sequence is illegal/malformed */
  sourceCorrupt,    /* source contains invalid UTF-7 */ /* addded */
} ConversionResult;

typedef enum {
	strictConversion = 0,
	lenientConversion
} ConversionFlags;

ConversionResult ConvertUTF32toUTF16 (
		UTF32** sourceStart, const UTF32* sourceEnd, 
		UTF16** targetStart, const UTF16* targetEnd, const ConversionFlags flags);

ConversionResult ConvertUTF16toUTF32 (
		UTF16** sourceStart, UTF16* sourceEnd, 
		UTF32** targetStart, const UTF32* targetEnd, const ConversionFlags flags);

ConversionResult ConvertUTF16toUTF8 (
		UTF16** sourceStart, const UTF16* sourceEnd, 
		UTF8** targetStart, const UTF8* targetEnd, ConversionFlags flags);
		
ConversionResult ConvertUTF8toUTF16 (
		UTF8** sourceStart, UTF8* sourceEnd, 
		UTF16** targetStart, const UTF16* targetEnd, const ConversionFlags flags);

ConversionResult ConvertUTF32toUTF8 (
		UTF32** sourceStart, const UTF32* sourceEnd,
		UTF8** targetStart, const UTF8* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF8toUTF32 (
		UTF8** sourceStart, UTF8* sourceEnd, 
		UTF32** targetStart, const UTF32* targetEnd, ConversionFlags flags);

Boolean isLegalUTF8Sequence(UTF8 *source, UTF8 *sourceEnd);

/* --------------------------------------------------------------------- */

/* ConvertUTF.c */

/*
 * Copyright 2001 Unicode, Inc.
 * 
 * Disclaimer
 * 
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 * 
 * Limitations on Rights to Redistribute This Code
 * 
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/* ---------------------------------------------------------------------

    Conversions between UTF32, UTF-16, and UTF-8. Source code file.
	Author: Mark E. Davis, 1994.
	Rev History: Rick McGowan, fixes & updates May 2001.

    See the header file "ConvertUTF.h" for complete documentation.

------------------------------------------------------------------------ */


/*#include "ConvertUTF.h"*/
/*#ifdef CVTUTF_DEBUG*/
#include <stdio.h>
/*#endif*/

static const int halfShift	= 10; /* used for shifting by 10 bits */

static const UTF32 halfBase	= 0x0010000UL;
static const UTF32 halfMask	= 0x3FFUL;

#define UNI_SUR_HIGH_START	(UTF32)0xD800
#define UNI_SUR_HIGH_END	(UTF32)0xDBFF
#define UNI_SUR_LOW_START	(UTF32)0xDC00
#define UNI_SUR_LOW_END		(UTF32)0xDFFF
#define false			0
#define true			1

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF32toUTF16 (
		UTF32** sourceStart, const UTF32* sourceEnd, 
		UTF16** targetStart, const UTF16* targetEnd, const ConversionFlags flags) {
	ConversionResult result = conversionOK;
	UTF32* source = *sourceStart;
	UTF16* target = *targetStart;
	while (source < sourceEnd) {
		UTF32 ch;
		if (target >= targetEnd) {
			result = targetExhausted; break;
		}
		ch = *source++;
		if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
			if ((flags == strictConversion) && (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END)) {
				--source; /* return to the illegal value itself */
				result = sourceIllegal;
				break;
			} else {
			    *target++ = ch;	/* normal case */
			}
		} else if (ch > UNI_MAX_UTF16) {
			if (flags == strictConversion) {
				result = sourceIllegal;
			} else {
				*target++ = UNI_REPLACEMENT_CHAR;
			}
		} else {
			/* target is a character in range 0xFFFF - 0x10FFFF. */
			if (target + 1 >= targetEnd) {
				result = targetExhausted; break;
			}
			ch -= halfBase;
			*target++ = (ch >> halfShift) + UNI_SUR_HIGH_START;
			*target++ = (ch & halfMask) + UNI_SUR_LOW_START;
		}
	}
	*sourceStart = source;
	*targetStart = target;
	return result;
}

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF16toUTF32 (
		UTF16** sourceStart, UTF16* sourceEnd, 
		UTF32** targetStart, const UTF32* targetEnd, const ConversionFlags flags) {
	ConversionResult result = conversionOK;
	UTF16* source = *sourceStart;
	UTF32* target = *targetStart;
	UTF32 ch, ch2;
	while (source < sourceEnd) {
		ch = *source++;
		if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END && source < sourceEnd) {
			ch2 = *source;
			if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
				ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
					+ (ch2 - UNI_SUR_LOW_START) + halfBase;
				++source;
			} else if (flags == strictConversion) { /* it's an unpaired high surrogate */
				--source; /* return to the illegal value itself */
				result = sourceIllegal;
				break;
			}
		} else if ((flags == strictConversion) && (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END)) {
			/* an unpaired low surrogate */
			--source; /* return to the illegal value itself */
			result = sourceIllegal;
			break;
		}
		if (target >= targetEnd) {
			result = targetExhausted; break;
		}
		*target++ = ch;
	}
	*sourceStart = source;
	*targetStart = target;
#ifdef CVTUTF_DEBUG
if (result == sourceIllegal) {
    fprintf(stderr, "ConvertUTF16toUTF32 illegal seq 0x%04x,%04x\n", ch, ch2);
    fflush(stderr);
}
#endif
	return result;
}

/* --------------------------------------------------------------------- */

/*
 * Index into the table below with the first byte of a UTF-8 sequence to
 * get the number of trailing bytes that are supposed to follow it.
 */

static const char trailingBytesForUTF8[256] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

/*
 * Magic values subtracted from a buffer value during UTF8 conversion.
 * This table contains as many values as there might be trailing bytes
 * in a UTF-8 sequence.
 */
static const UTF32 offsetsFromUTF8[6] = { 0x00000000UL, 0x00003080UL, 0x000E2080UL, 
					 0x03C82080UL, 0xFA082080UL, 0x82082080UL };

/*
 * Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
 * into the first byte, depending on how many bytes follow.  There are
 * as many entries in this table as there are UTF-8 sequence types.
 * (I.e., one byte sequence, two byte... six byte sequence.)
 */
static const UTF8 firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

/* --------------------------------------------------------------------- */

/* The interface converts a whole buffer to avoid function-call overhead.
 * Constants have been gathered. Loops & conditionals have been removed as
 * much as possible for efficiency, in favor of drop-through switches.
 * (See "Note A" at the bottom of the file for equivalent code.)
 * If your compiler supports it, the "isLegalUTF8" call can be turned
 * into an inline function.
 */

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF16toUTF8 (
		UTF16** sourceStart, const UTF16* sourceEnd, 
		UTF8** targetStart, const UTF8* targetEnd, ConversionFlags flags) {
	ConversionResult result = conversionOK;
	UTF16* source = *sourceStart;
	UTF8* target = *targetStart;
	while (source < sourceEnd) {
		UTF32 ch;
		unsigned short bytesToWrite = 0;
		const UTF32 byteMask = 0xBF;
		const UTF32 byteMark = 0x80; 
		ch = *source++;
		/* If we have a surrogate pair, convert to UTF32 first. */
		if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END && source < sourceEnd) {
			UTF32 ch2 = *source;
			if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
				ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
					+ (ch2 - UNI_SUR_LOW_START) + halfBase;
				++source;
			} else if (flags == strictConversion) { /* it's an unpaired high surrogate */
				--source; /* return to the illegal value itself */
				result = sourceIllegal;
				break;
			}
		} else if ((flags == strictConversion) && (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END)) {
			--source; /* return to the illegal value itself */
			result = sourceIllegal;
			break;
		}
		/* Figure out how many bytes the result will require */
		if (ch < (UTF32)0x80) {			bytesToWrite = 1;
		} else if (ch < (UTF32)0x800) {		bytesToWrite = 2;
		} else if (ch < (UTF32)0x10000) {	bytesToWrite = 3;
		} else if (ch < (UTF32)0x200000) {	bytesToWrite = 4;
		} else {				bytesToWrite = 2;
							ch = UNI_REPLACEMENT_CHAR;
		}

		target += bytesToWrite;
		if (target > targetEnd) {
			target -= bytesToWrite; result = targetExhausted; break;
		}
		switch (bytesToWrite) {	/* note: everything falls through. */
			case 4:	*--target = (ch | byteMark) & byteMask; ch >>= 6;
			case 3:	*--target = (ch | byteMark) & byteMask; ch >>= 6;
			case 2:	*--target = (ch | byteMark) & byteMask; ch >>= 6;
			case 1:	*--target =  ch | firstByteMark[bytesToWrite];
		}
		target += bytesToWrite;
	}
	*sourceStart = source;
	*targetStart = target;
	return result;
}

/* --------------------------------------------------------------------- */

/*
 * Utility routine to tell whether a sequence of bytes is legal UTF-8.
 * This must be called with the length pre-determined by the first byte.
 * If not calling this from ConvertUTF8to*, then the length can be set by:
 *	length = trailingBytesForUTF8[*source]+1;
 * and the sequence is illegal right away if there aren't that many bytes
 * available.
 * If presented with a length > 4, this returns false.  The Unicode
 * definition of UTF-8 goes up to 4-byte sequences.
 */

static Boolean isLegalUTF8(UTF8 *source, int length) {
	UTF8 a;
	UTF8 *srcptr = source+length;
	switch (length) {
	default: return false;
		/* Everything else falls through when "true"... */
	case 4: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
	case 3: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
	case 2: if ((a = (*--srcptr)) > 0xBF) return false;
		switch (*source) {
		    /* no fall-through in this inner switch */
		    case 0xE0: if (a < 0xA0) return false; break;
		    case 0xF0: if (a < 0x90) return false; break;
		    case 0xF4: if (a > 0x8F) return false; break;
		    default:  if (a < 0x80) return false;
		}
    	case 1: if (*source >= 0x80 && *source < 0xC2) return false;
		if (*source > 0xF4) return false;
	}
	return true;
}

/* --------------------------------------------------------------------- */

/*
 * Exported function to return whether a UTF-8 sequence is legal or not.
 * This is not used here; it's just exported.
 */
Boolean isLegalUTF8Sequence(UTF8 *source, UTF8 *sourceEnd) {
	int length = trailingBytesForUTF8[*source]+1;
	if (source+length > sourceEnd) {
	    return false;
	}
	return isLegalUTF8(source, length);
}

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF8toUTF16 (
		UTF8** sourceStart, UTF8* sourceEnd,
		UTF16** targetStart, const UTF16* targetEnd, const ConversionFlags flags) {
	ConversionResult result = conversionOK;
	UTF8* source = *sourceStart;
	UTF16* target = *targetStart;
	while (source < sourceEnd) {
		UTF32 ch = 0;
		unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
		if (source + extraBytesToRead >= sourceEnd) {
			result = sourceExhausted; break;
		}
		/* Do this check whether lenient or strict */
		if (! isLegalUTF8(source, extraBytesToRead+1)) {
			result = sourceIllegal;
			break;
		}
		/*
		 * The cases all fall through. See "Note A" below.
		 */
		switch (extraBytesToRead) {
			case 3:	ch += *source++; ch <<= 6;
			case 2:	ch += *source++; ch <<= 6;
			case 1:	ch += *source++; ch <<= 6;
			case 0:	ch += *source++;
		}
		ch -= offsetsFromUTF8[extraBytesToRead];

		if (target >= targetEnd) {
			result = targetExhausted; break;
		}
		if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
			if ((flags == strictConversion) && (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END)) {
				--source; /* return to the illegal value itself */
				result = sourceIllegal;
				break;
			} else {
			    *target++ = ch;	/* normal case */
			}
		} else if (ch > UNI_MAX_UTF16) {
			if (flags == strictConversion) {
				result = sourceIllegal;
				source -= extraBytesToRead; /* return to the start */
			} else {
				*target++ = UNI_REPLACEMENT_CHAR;
			}
		} else {
			/* target is a character in range 0xFFFF - 0x10FFFF. */
			if (target + 1 >= targetEnd) {
				result = targetExhausted; break;
			}
			ch -= halfBase;
			*target++ = (ch >> halfShift) + UNI_SUR_HIGH_START;
			*target++ = (ch & halfMask) + UNI_SUR_LOW_START;
		}
	}
	*sourceStart = source;
	*targetStart = target;
	return result;
}

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF32toUTF8 (
		UTF32** sourceStart, const UTF32* sourceEnd, 
		UTF8** targetStart, const UTF8* targetEnd, ConversionFlags flags) {
	ConversionResult result = conversionOK;
	UTF32* source = *sourceStart;
	UTF8* target = *targetStart;
	while (source < sourceEnd) {
		UTF32 ch;
		unsigned short bytesToWrite = 0;
		const UTF32 byteMask = 0xBF;
		const UTF32 byteMark = 0x80; 
		ch = *source++;
		/* surrogates of any stripe are not legal UTF32 characters */
		if (flags == strictConversion ) {
			if ((ch >= UNI_SUR_HIGH_START) && (ch <= UNI_SUR_LOW_END)) {
				--source; /* return to the illegal value itself */
				result = sourceIllegal;
				break;
			}
		}
		/* Figure out how many bytes the result will require */
		if (ch < (UTF32)0x80) {			bytesToWrite = 1;
		} else if (ch < (UTF32)0x800) {		bytesToWrite = 2;
		} else if (ch < (UTF32)0x10000) {	bytesToWrite = 3;
		} else if (ch < (UTF32)0x200000) {	bytesToWrite = 4;
		} else {				bytesToWrite = 2;
							ch = UNI_REPLACEMENT_CHAR;
		}
		
		target += bytesToWrite;
		if (target > targetEnd) {
			target -= bytesToWrite; result = targetExhausted; break;
		}
		switch (bytesToWrite) {	/* note: everything falls through. */
			case 4:	*--target = (ch | byteMark) & byteMask; ch >>= 6;
			case 3:	*--target = (ch | byteMark) & byteMask; ch >>= 6;
			case 2:	*--target = (ch | byteMark) & byteMask; ch >>= 6;
			case 1:	*--target =  ch | firstByteMark[bytesToWrite];
		}
		target += bytesToWrite;
	}
	*sourceStart = source;
	*targetStart = target;
	return result;
}

/* --------------------------------------------------------------------- */

ConversionResult ConvertUTF8toUTF32 (
		UTF8** sourceStart, UTF8* sourceEnd, 
		UTF32** targetStart, const UTF32* targetEnd, ConversionFlags flags) {
	ConversionResult result = conversionOK;
	UTF8* source = *sourceStart;
	UTF32* target = *targetStart;
	while (source < sourceEnd) {
		UTF32 ch = 0;
		unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
		if (source + extraBytesToRead >= sourceEnd) {
			result = sourceExhausted; break;
		}
		/* Do this check whether lenient or strict */
		if (! isLegalUTF8(source, extraBytesToRead+1)) {
			result = sourceIllegal;
			break;
		}
		/*
		 * The cases all fall through. See "Note A" below.
		 */
		switch (extraBytesToRead) {
			case 3:	ch += *source++; ch <<= 6;
			case 2:	ch += *source++; ch <<= 6;
			case 1:	ch += *source++; ch <<= 6;
			case 0:	ch += *source++;
		}
		ch -= offsetsFromUTF8[extraBytesToRead];

		if (target >= targetEnd) {
			result = targetExhausted; break;
		}
		if (ch <= UNI_MAX_UTF32) {
			*target++ = ch;
		} else if (ch > UNI_MAX_UTF32) {
			*target++ = UNI_REPLACEMENT_CHAR;
		} else {
			if (target + 1 >= targetEnd) {
				result = targetExhausted; break;
			}
			ch -= halfBase;
			*target++ = (ch >> halfShift) + UNI_SUR_HIGH_START;
			*target++ = (ch & halfMask) + UNI_SUR_LOW_START;
		}
	}
	*sourceStart = source;
	*targetStart = target;
	return result;
}

/* ---------------------------------------------------------------------

	Note A.
	The fall-through switches in UTF-8 reading code save a
	temp variable, some decrements & conditionals.  The switches
	are equivalent to the following loop:
		{
			int tmpBytesToRead = extraBytesToRead+1;
			do {
				ch += *source++;
				--tmpBytesToRead;
				if (tmpBytesToRead) ch <<= 6;
			} while (tmpBytesToRead > 0);
		}
	In UTF-8 writing code, the switches on "bytesToWrite" are
	similarly unrolled loops.

   --------------------------------------------------------------------- */

/* Some code has been taken from the ConvertUTF7.c file (the utf7 stuff below),
   this is the copyright notice */

/* ================================================================ */
/*
File:   ConvertUTF7.c
Author: David B. Goldsmith
Copyright (C) 1994, 1996 IBM Corporation All rights reserved.
Revisions: Header update only July, 2001.

This code is copyrighted. Under the copyright laws, this code may not
be copied, in whole or part, without prior written consent of IBM Corporation. 

IBM Corporation grants the right to use this code as long as this ENTIRE
copyright notice is reproduced in the code.  The code is provided
AS-IS, AND IBM CORPORATION DISCLAIMS ALL WARRANTIES, EITHER EXPRESS OR
IMPLIED, INCLUDING, BUT NOT LIMITED TO IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT
WILL IBM CORPORATION BE LIABLE FOR ANY DAMAGES WHATSOEVER (INCLUDING,
WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS
INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER PECUNIARY
LOSS) ARISING OUT OF THE USE OR INABILITY TO USE THIS CODE, EVEN
IF IBM CORPORATION HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
BECAUSE SOME STATES DO NOT ALLOW THE EXCLUSION OR LIMITATION OF
LIABILITY FOR CONSEQUENTIAL OR INCIDENTAL DAMAGES, THE ABOVE
LIMITATION MAY NOT APPLY TO YOU.

RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
government is subject to restrictions as set forth in subparagraph
(c)(l)(ii) of the Rights in Technical Data and Computer Software
clause at DFARS 252.227-7013 and FAR 52.227-19.

This code may be protected by one or more U.S. and International
Patents.

*/


/* ------------------------------------- */

int read_line(FILE *fh, char *buf); /* in addressbook.c */
char *get_config_item(char *buf, char *item); /* configuration.c */


struct list codesets_list;

/**************************************************************************
 Returns the supported codesets as an null terminated string array
**************************************************************************/
char **codesets_supported(void)
{
	static char **array;

	if (array) return array;

	if ((array = (char**)malloc(sizeof(char*)*(list_length(&codesets_list)+1))))
	{
		struct codeset *code;
		int i;

		SM_DEBUGF(15,("%ld supported Codesets:\n",list_length(&codesets_list)));

		code = (struct codeset*)list_first(&codesets_list);
		i = 0;

		while (code)
		{
			SM_DEBUGF(15,("  %p next=%p prev=%p list=%p name=%p %s alt=%p char=%p\n",code,code->node.next,code->node.prev,code->node.list,code->name,code->name,code->alt_name,code->characterization));
			array[i++] = code->name;
			code = (struct codeset*)node_next(&code->node);
		}
		array[i] = NULL;
	}
	return array;
}

/**************************************************************************
 The compare function
**************************************************************************/
static int codesets_cmp_unicode(const void *arg1, const void *arg2)
{
	return (int)(strcmp(((struct single_convert*)arg1)->utf8 + 1, ((struct single_convert*)arg2)->utf8+1));
}

/**************************************************************************
 Reads a coding table and adds it
**************************************************************************/
static int codesets_read_table(char *name)
{
	char buf[512];

	FILE *fh = fopen(name,"r");

	if (fh)
	{
		struct codeset *codeset;
		if ((codeset = (struct codeset*)malloc(sizeof(struct codeset))))
		{
			int i;
			memset(codeset,0,sizeof(struct codeset));

			for (i=0;i<256;i++)
				codeset->table[i].code = codeset->table[i].ucs4 = i;

			while (read_line(fh,buf))
			{
				char *result;
				if ((result = get_config_item(buf,"Standard"))) codeset->name = mystrdup(result);
				else if ((result = get_config_item(buf,"AltStandard"))) codeset->alt_name = mystrdup(result);
				else if ((result = get_config_item(buf,"ReadOnly"))) codeset->read_only = !!atoi(result);
				else if ((result = get_config_item(buf,"Characterization")))
				{
					if ((result[0] == '_') && (result[1] == '(') && (result[2] == '"'))
					{
						char *end = strchr(result+3,'"');
						if (end)
						{
							codeset->characterization = _(mystrndup(result+3,end-(result+3)));
						}
					}
				} else
				{
					char *p = buf;
					int fmt2 = 0;

					if ((*p == '=') || (fmt2 = ((*p == '0') || (*(p+1)=='x'))))
					{
						p++;
						p += fmt2;

						i = strtol(p,&p,16);
						if (i > 0 && i < 256)
						{
							while (isspace((unsigned char)*p)) p++;

							if (!mystrnicmp(p,"U+",2))
							{
								p += 2;
								codeset->table[i].ucs4 = strtol(p,&p,16);
							} else
							{
								if (*p!='#') codeset->table[i].ucs4 = strtol(p,&p,0);
							}
						}
					}
				}
			}

			for (i=0;i<256;i++)
			{
				UTF32 src = codeset->table[i].ucs4;
				UTF32 *src_ptr = &src;
				UTF8 *dest_ptr = &codeset->table[i].utf8[1];
				ConvertUTF32toUTF8(&src_ptr, src_ptr + 1, &dest_ptr, dest_ptr + 6, strictConversion);
				*dest_ptr = 0;
				codeset->table[i].utf8[0] = (char*)dest_ptr - (char*)&codeset->table[i].utf8[1];
			}

			memcpy(codeset->table_sorted,codeset->table,sizeof(codeset->table));
			qsort(codeset->table_sorted,256,sizeof(codeset->table[0]),(int (*)(const void *arg1, const void *arg2))codesets_cmp_unicode);
			list_insert_tail(&codesets_list,&codeset->node);
		}
		fclose(fh);
	}
	return 1;
}

/**************************************************************************
 Initialized and loads the codesets
**************************************************************************/
int codesets_init(void)
{
	int i;
	struct codeset *codeset;
	UTF32 src;

	SM_ENTER;

	list_init(&codesets_list);

	if (!(codeset = (struct codeset*)malloc(sizeof(struct codeset)))) return 0;
	codeset->name = mystrdup("ISO-8859-1 + Euro");
	codeset->alt_name = NULL;
	codeset->characterization = mystrdup(_("West European (with EURO)"));
	codeset->read_only = 1;
	for (i=0;i<256;i++)
	{
		UTF32 *src_ptr = &src;
		UTF8 *dest_ptr = &codeset->table[i].utf8[1];

		if (i==164) src = 0x20AC; /* the EURO sign */
		else src = i;
		codeset->table[i].code = i;
		codeset->table[i].ucs4 = src;
		ConvertUTF32toUTF8(&src_ptr, src_ptr + 1, &dest_ptr, dest_ptr + 6, strictConversion);
		*dest_ptr = 0;
		codeset->table[i].utf8[0] = (char*)dest_ptr - (char*)&codeset->table[i].utf8[1];
	}
	memcpy(codeset->table_sorted,codeset->table,sizeof(codeset->table));
	qsort(codeset->table_sorted,256,sizeof(codeset->table[0]),(int (*)(const void *arg1, const void *arg2))codesets_cmp_unicode);
	list_insert_tail(&codesets_list,&codeset->node);

	if (!(codeset = (struct codeset*)malloc(sizeof(struct codeset)))) return 1;
	codeset->name = mystrdup("ISO-8859-1");
	codeset->alt_name = NULL;
	codeset->characterization = mystrdup(_("West European"));
	codeset->read_only = 0;
	for (i=0;i<256;i++)
	{
		UTF32 *src_ptr = &src;
		UTF8 *dest_ptr = &codeset->table[i].utf8[1];

		src = i;
		codeset->table[i].code = i;
		codeset->table[i].ucs4 = src;
		ConvertUTF32toUTF8(&src_ptr, src_ptr + 1, &dest_ptr, dest_ptr + 6, strictConversion);
		*dest_ptr = 0;
		codeset->table[i].utf8[0] = (char*)dest_ptr - (char*)&codeset->table[i].utf8[1];
	}
	memcpy(codeset->table_sorted,codeset->table,sizeof(codeset->table));
	qsort(codeset->table_sorted,256,sizeof(codeset->table[0]),(int (*)(const void *arg1, const void *arg2))codesets_cmp_unicode);
	list_insert_tail(&codesets_list,&codeset->node);

	if (!(codeset = (struct codeset*)malloc(sizeof(struct codeset)))) return 1; /* One entry is enough */
	codeset->name = mystrdup("ISO-8859-2");
	codeset->alt_name = NULL;
	codeset->characterization = mystrdup(_("Central/East European"));
	codeset->read_only = 0;
	for (i=0;i<256;i++)
	{
		UTF32 *src_ptr = &src;
		UTF8 *dest_ptr = &codeset->table[i].utf8[1];

		if (i < 0xa0) src = i;
		else src = iso_8859_2_to_ucs4[i-0xa0];
		codeset->table[i].code = i;
		codeset->table[i].ucs4 = src;
		ConvertUTF32toUTF8(&src_ptr, src_ptr + 1, &dest_ptr, dest_ptr + 6, strictConversion);
		*dest_ptr = 0;
		codeset->table[i].utf8[0] = (char*)dest_ptr - (char*)&codeset->table[i].utf8[1];
	}
	memcpy(codeset->table_sorted,codeset->table,sizeof(codeset->table));
	qsort(codeset->table_sorted,256,sizeof(codeset->table[0]),(int (*)(const void *arg1, const void *arg2))codesets_cmp_unicode);
	list_insert_tail(&codesets_list,&codeset->node);

	if (!(codeset = (struct codeset*)malloc(sizeof(struct codeset)))) return 1; /* One entry is enough */
	codeset->name = mystrdup("ISO-8859-3");
	codeset->alt_name = NULL;
	codeset->characterization = mystrdup(_("South European"));
	codeset->read_only = 0;
	for (i=0;i<256;i++)
	{
		UTF32 *src_ptr = &src;
		UTF8 *dest_ptr = &codeset->table[i].utf8[1];

		if (i < 0xa0) src = i;
		else src = iso_8859_3_to_ucs4[i-0xa0];
		codeset->table[i].code = i;
		codeset->table[i].ucs4 = src;
		ConvertUTF32toUTF8(&src_ptr, src_ptr + 1, &dest_ptr, dest_ptr + 6, strictConversion);
		*dest_ptr = 0;
		codeset->table[i].utf8[0] = (char*)dest_ptr - (char*)&codeset->table[i].utf8[1];
	}
	memcpy(codeset->table_sorted,codeset->table,sizeof(codeset->table));
	qsort(codeset->table_sorted,256,sizeof(codeset->table[0]),(int (*)(const void *arg1, const void *arg2))codesets_cmp_unicode);
	list_insert_tail(&codesets_list,&codeset->node);

	if (!(codeset = (struct codeset*)malloc(sizeof(struct codeset)))) return 1; /* One entry is enough */
	codeset->name = mystrdup("ISO-8859-4");
	codeset->alt_name = NULL;
	codeset->characterization = mystrdup(_("North European"));
	codeset->read_only = 0;
	for (i=0;i<256;i++)
	{
		UTF32 *src_ptr = &src;
		UTF8 *dest_ptr = &codeset->table[i].utf8[1];

		if (i < 0xa0) src = i;
		else src = iso_8859_4_to_ucs4[i-0xa0];
		codeset->table[i].code = i;
		codeset->table[i].ucs4 = src;
		ConvertUTF32toUTF8(&src_ptr, src_ptr + 1, &dest_ptr, dest_ptr + 6, strictConversion);
		*dest_ptr = 0;
		codeset->table[i].utf8[0] = (char*)dest_ptr - (char*)&codeset->table[i].utf8[1];
	}
	memcpy(codeset->table_sorted,codeset->table,sizeof(codeset->table));
	qsort(codeset->table_sorted,256,sizeof(codeset->table[0]),(int (*)(const void *arg1, const void *arg2))codesets_cmp_unicode);
	list_insert_tail(&codesets_list,&codeset->node);

	if (!(codeset = (struct codeset*)malloc(sizeof(struct codeset)))) return 1; /* One entry is enough */
	codeset->name = mystrdup("KOI8-R");
	codeset->alt_name = NULL;
	codeset->characterization = mystrdup(_("Russian"));
	codeset->read_only = 0;
	for (i=0;i<256;i++)
	{
		UTF32 *src_ptr = &src;
		UTF8 *dest_ptr = &codeset->table[i].utf8[1];

		if (i < 0x80) src = i;
		else src = koi8r_to_ucs4[i-0x80];
		codeset->table[i].code = i;
		codeset->table[i].ucs4 = src;
		ConvertUTF32toUTF8(&src_ptr, src_ptr + 1, &dest_ptr, dest_ptr + 6, strictConversion);
		*dest_ptr = 0;
		codeset->table[i].utf8[0] = (char*)dest_ptr - (char*)&codeset->table[i].utf8[1];
	}
	memcpy(codeset->table_sorted,codeset->table,sizeof(codeset->table));
	qsort(codeset->table_sorted,256,sizeof(codeset->table[0]),(int (*)(const void *arg1, const void *arg2))codesets_cmp_unicode);
	list_insert_tail(&codesets_list,&codeset->node);

	if (!(codeset = (struct codeset*)malloc(sizeof(struct codeset)))) return 1; /* One entry is enough */
	codeset->name = mystrdup("ISO-8859-5");
	codeset->alt_name = NULL;
	codeset->characterization = mystrdup(_("Slavic languages"));
	codeset->read_only = 0;
	for (i=0;i<256;i++)
	{
		UTF32 *src_ptr = &src;
		UTF8 *dest_ptr = &codeset->table[i].utf8[1];

		if (i < 0xa0) src = i;
		else src = iso_8859_5_to_ucs4[i-0xa0];
		codeset->table[i].code = i;
		codeset->table[i].ucs4 = src;
		ConvertUTF32toUTF8(&src_ptr, src_ptr + 1, &dest_ptr, dest_ptr + 6, strictConversion);
		*dest_ptr = 0;
		codeset->table[i].utf8[0] = (char*)dest_ptr - (char*)&codeset->table[i].utf8[1];
	}
	memcpy(codeset->table_sorted,codeset->table,sizeof(codeset->table));
	qsort(codeset->table_sorted,256,sizeof(codeset->table[0]),(int (*)(const void *arg1, const void *arg2))codesets_cmp_unicode);
	list_insert_tail(&codesets_list,&codeset->node);

	if (!(codeset = (struct codeset*)malloc(sizeof(struct codeset)))) return 1; /* One entry is enough */
	codeset->name = mystrdup("ISO-8859-9");
	codeset->alt_name = NULL;
	codeset->characterization = mystrdup(_("Turkish"));
	codeset->read_only = 0;
	for (i=0;i<256;i++)
	{
		UTF32 *src_ptr = &src;
		UTF8 *dest_ptr = &codeset->table[i].utf8[1];

		if (i < 0xa0) src = i;
		else src = iso_8859_9_to_ucs4[i-0xa0];
		codeset->table[i].code = i;
		codeset->table[i].ucs4 = src;
		ConvertUTF32toUTF8(&src_ptr, src_ptr + 1, &dest_ptr, dest_ptr + 6, strictConversion);
		*dest_ptr = 0;
		codeset->table[i].utf8[0] = (char*)dest_ptr - (char*)&codeset->table[i].utf8[1];
	}
	memcpy(codeset->table_sorted,codeset->table,sizeof(codeset->table));
	qsort(codeset->table_sorted,256,sizeof(codeset->table[0]),(int (*)(const void *arg1, const void *arg2))codesets_cmp_unicode);
	list_insert_tail(&codesets_list,&codeset->node);

	if (!(codeset = (struct codeset*)malloc(sizeof(struct codeset)))) return 1; /* One entry is enough */
	codeset->name = mystrdup("ISO-8859-15");
	codeset->alt_name = NULL;
	codeset->characterization = mystrdup(_("West European II"));

	codeset->read_only = 0;
	for (i=0;i<256;i++)
	{
		UTF32 *src_ptr = &src;
		UTF8 *dest_ptr = &codeset->table[i].utf8[1];

		if (i < 0xa0) src = i;
		else src = iso_8859_15_to_ucs4[i-0xa0];
		codeset->table[i].code = i;
		codeset->table[i].ucs4 = src;
		ConvertUTF32toUTF8(&src_ptr, src_ptr + 1, &dest_ptr, dest_ptr + 6, strictConversion);
		*dest_ptr = 0;
		codeset->table[i].utf8[0] = (char*)dest_ptr - (char*)&codeset->table[i].utf8[1];
	}
	memcpy(codeset->table_sorted,codeset->table,sizeof(codeset->table));
	qsort(codeset->table_sorted,256,sizeof(codeset->table[0]),(int (*)(const void *arg1, const void *arg2))codesets_cmp_unicode);
	list_insert_tail(&codesets_list,&codeset->node);

	if (!(codeset = (struct codeset*)malloc(sizeof(struct codeset)))) return 1; /* One entry is enough */
	codeset->name = mystrdup("AmigaPL");
	codeset->alt_name = NULL;
	codeset->characterization = mystrdup("AmigaPL");
	codeset->read_only = 1;
	for (i=0;i<256;i++)
	{
		UTF32 *src_ptr = &src;
		UTF8 *dest_ptr = &codeset->table[i].utf8[1];

		if (i < 0xa0) src = i;
		else src = amigapl_to_ucs4[i-0xa0];
		codeset->table[i].code = i;
		codeset->table[i].ucs4 = src;
		ConvertUTF32toUTF8(&src_ptr, src_ptr + 1, &dest_ptr, dest_ptr + 6, strictConversion);
		*dest_ptr = 0;
		codeset->table[i].utf8[0] = (char*)dest_ptr - (char*)&codeset->table[i].utf8[1];
	}
	memcpy(codeset->table_sorted,codeset->table,sizeof(codeset->table));
	qsort(codeset->table_sorted,256,sizeof(codeset->table[0]),(int (*)(const void *arg1, const void *arg2))codesets_cmp_unicode);
	list_insert_tail(&codesets_list,&codeset->node);

	SM_DEBUGF(15,("%ld internal charsets\n",list_length(&codesets_list)));

	{
		/* dynamicaly loaded */

		DIR *dfd; /* directory descriptor */
		struct dirent *dptr; /* dir entry */
		char path[380];

		getcwd(path, sizeof(path));
		if (chdir(SM_CHARSET_DIR) != -1)
		{
			if ((dfd = opendir(SM_CURRENT_DIR)))
			{
				while ((dptr = readdir(dfd)) != NULL)
				{
					if (!strcmp(".",dptr->d_name) || !strcmp("..",dptr->d_name)) continue;
					SM_DEBUGF(15,("Loading \"%s\" charset\n",dptr->d_name,list_length(&codesets_list)));
					codesets_read_table(dptr->d_name);
				}
				closedir(dfd);
			}
			chdir(path);
		}
	}

	SM_RETURN(1,"%ld");
}

/**************************************************************************
 Cleanup the memory for the codeset
**************************************************************************/
void codesets_cleanup(void)
{
}

/**************************************************************************
 Returns the given codeset. NULL returns ISO-8859-1 Latin 1
**************************************************************************/
struct codeset *codesets_find(char *name)
{
	struct codeset *codeset = (struct codeset*)list_first(&codesets_list);

	/* Return ISO-8859-1 as default codeset */
	if (!name) return codeset;

	while (codeset)
	{
		if (!mystricmp(name,codeset->name) || !mystricmp(name,codeset->alt_name)) return codeset;
		codeset = (struct codeset*)node_next(&codeset->node);
	}
	return NULL;
}

/**************************************************************************
 Returns the best codeset for the given text
**************************************************************************/
struct codeset *codesets_find_best(char *text, int text_len, int *error_ptr)
{
	struct codeset *codeset = (struct codeset*)list_first(&codesets_list);
	struct codeset *best_codeset = NULL;
	int best_errors = text_len;

	while (codeset)
	{
		if (!codeset->read_only)
		{
			struct single_convert conv;
			char *text_ptr = text;
			int i;
			int errors = 0;
	
			for (i=0;i < text_len;i++)
			{
				unsigned char c = *text_ptr++;
				if (c)
				{
					int len = trailingBytesForUTF8[c];
					conv.utf8[1] = c;
					strncpy(&conv.utf8[2],text_ptr,len);
					conv.utf8[2+len] = 0;
					text_ptr += len;

					if (!bsearch(&conv,codeset->table_sorted,256,sizeof(codeset->table_sorted[0]),codesets_cmp_unicode))
						errors++;
				} else break;
			}

			if (errors < best_errors)
			{
				best_codeset = codeset;
				best_errors = errors;
			}
			if (!best_errors) break;
		}
		codeset = (struct codeset*)node_next(&codeset->node);
	}

	if (!best_codeset) best_codeset = (struct codeset*)list_first(&codesets_list);
	if (error_ptr) *error_ptr = best_errors;

	return best_codeset;
}

/**************************************************************************
 Returns the number of characters a utf8 string has. This is not
 identically with the size of memory is required to hold the string.
 Please use utfsize() for this.
**************************************************************************/
int utf8len(const utf8 *str)
{
	int len ;
	unsigned char c;

	if (!str) return 0;
	len = 0;

	while ((c = *str++))
	{
		len++;
		str += trailingBytesForUTF8[c];
	}

	return len;
}

/**************************************************************************
 Transforms a character position to the position in the char array
**************************************************************************/
int utf8realpos(const utf8 *str, int pos)
{
	const utf8 *str_save = str;
	unsigned char c;

	if (!str) return 0;

	while (pos && (c = *str))
	{
		pos--;
		str += trailingBytesForUTF8[c] + 1;
	}
	return str - str_save;
}

/**************************************************************************
 ...
**************************************************************************/
int utf8charpos(const utf8 *str, int pos)
{
	int cp = 0;
	unsigned char c;

	while (pos > 0 && (c = *str))
	{
		str += trailingBytesForUTF8[c] + 1;
		pos -= trailingBytesForUTF8[c] + 1;
		cp++;
	}
	return cp;
}

/**************************************************************************
 Copies a number of characters from "from" to "to". 
**************************************************************************/
utf8 *utf8ncpy(utf8 *to, const utf8 *from, int n)
{
	utf8 *saved_to = to;
	for (;n;n--)
	{
		unsigned char c = *from++;
		int len = trailingBytesForUTF8[c];

		*to++ = c;
		for (;len;len--)
		{
			*to++ = *from++;
		}
	}
	return saved_to;
}

/**************************************************************************
 Creates a uf8 string from a different one. from is the iso string and
 charset the charset of from
**************************************************************************/
utf8 *utf8create(void *from, char *charset)
{
  /* utf8create_len() will stop on a null byte */
	return utf8create_len(from,charset,0x7fffffff);
}

/**************************************************************************
 Creates a uf8 string from a different one. from is the iso string and
 charset the charset of from
**************************************************************************/
utf8 *utf8create_len(void *from, char *charset, int from_len)
{
	int dest_size = 0;
	char *dest;
	char *src = (char*)from;
	unsigned char c;
	int len;
	struct codeset *codeset = codesets_find(charset);

	if (!from) return NULL;

	if (!codeset)
	{
		if (!mystricmp(charset,"utf-7"))
		{
			return utf7ntoutf8((char *)from,from_len);
		}
		codeset = (struct codeset*)list_first(&codesets_list);
	}

	len = from_len;

	while (((c = *src++) && (len--)))
		dest_size += codeset->table[c].utf8[0];

	if ((dest = malloc(dest_size+1)))
	{
		char *dest_ptr = dest;

		for (src = (char*)from;from_len && (c = *src);src++,from_len--)
		{
			unsigned char *utf8_seq;

			for(utf8_seq = &codeset->table[c].utf8[1];(c = *utf8_seq);utf8_seq++)
				*dest_ptr++ = c;
		}

		*dest_ptr = 0;
		return dest;
	}
	return NULL;
}

/**************************************************************************
 Converts a UTF8 string to a given charset. Return the number of bytes
 written to dest excluding the NULL byte (which is always ensured by this
 function).
**************************************************************************/
int utf8tostr(utf8 *str, char *dest, int dest_size, struct codeset *codeset)
{
	int i;
	struct single_convert *f;
	char *dest_iter = dest;

	if (!codeset) codeset = (struct codeset*)list_first(&codesets_list);
	if (!codeset || !str)
	{
		if (dest_size) *dest = 0;
		return 0;
	}

	for (i=0;i < dest_size-1;i++)
	{
		unsigned char c = *str;
		if (c)
		{
			if (c > 127)
			{
				int len_add = trailingBytesForUTF8[c];
				int len_str = len_add + 1;

				BIN_SEARCH(codeset->table_sorted,0,255,mystrncmp(str,codeset->table_sorted[m].utf8+1,len_str),f);

				if (f) *dest_iter++ = f->code;
				else *dest_iter++ = '_';

				str += len_add;
			} else *dest_iter++ = c;

			str++;
		} else break;
	}
	*dest_iter = 0;
	return i;
}

/**************************************************************************
 Converts a UTF8 string to a given charset. The returned string is
 allocated with malloc();
**************************************************************************/
char *utf8tostrcreate(utf8 *str, struct codeset *codeset)
{
	char *dest;
	int len;
	if (!str) return NULL;
	len = strlen(str);
	if ((dest = malloc(len+1)))
		utf8tostr(str,dest,len+1,codeset);
	return dest;
}

/**************************************************************************
 Converts a single UTF8 char to a given charset. Returns the number of
 bytes to the next utf8 char. *chr might be 0 if it was not in the codeset
**************************************************************************/
int utf8tochar(utf8 *str, unsigned int *chr, struct codeset *codeset)
{
	struct single_convert conv;
	struct single_convert *f;
	unsigned char c;
	int len = 0;

	if (!codeset) codeset = (struct codeset*)list_first(&codesets_list);
	if (!codeset) return 0;

	if ((c = *str++))
	{
		len = trailingBytesForUTF8[c];
		conv.utf8[1] = c;
		strncpy(&conv.utf8[2],str,len);
		conv.utf8[2+len] = 0;

		if ((f = (struct single_convert*)bsearch(&conv,codeset->table_sorted,256,sizeof(codeset->table_sorted[0]),codesets_cmp_unicode)))
		{
			*chr = f->code;
		} else *chr = 0;
	} else *chr = 0;
	return len+1;
}

/**************************************************************************
 Compares two utf8 string case-insensitive (the args might be NULL).
 Note: Changes for little endian
**************************************************************************/
int utf8stricmp(char *str1, char *str2)
{
	unsigned char c1;
	unsigned char c2;

	if (!str1)
	{
		if (!str2) return 0;
		return -1;
	}

	if (!str2) return 1;

	while (1)
	{
		int d;
		char bytes1,bytes2;

		c1 = *str1++;
		c2 = *str2++;

		if (!c1)
		{
			if (!c2) return 0;
			return -1;
		}
		if (!c2) return 1;

		if (c1 < 0x80)
		{
			if (c2 < 0x80)
			{
				d = tolower(c1) - tolower(c2);
				if (d) return d;
				continue;
			} else
			{
				/* TODO: must use locale sensitive sorting */
				return -1;
			}
		}
		if (c2 < 0x80) return 1; /* TODO: must use locale sensitive sorting */

		bytes1 = trailingBytesForUTF8[c1];
		bytes2 = trailingBytesForUTF8[c2];

		/* case mapping only happens within same number of bytes (currently) */
		if ((d = bytes1 - bytes2)) return d;

		if (bytes1 > 3)
		{
			/* case mapping relevant characters are only withing 4 bytes */
			while (bytes1)
			{
				if ((d = *str1++ - *str2++)) return d;
				bytes1--;
			}
		} else
		{

			unsigned char ch1[4],ch2[4];
			struct uniconv *uc1;
			struct uniconv *uc2;
			int ch1l;
			int ch2l;

			*((unsigned int *)ch1) = 0;
			*((unsigned int *)ch2) = 0;

			ch1[3-bytes1] = c1;
			ch2[3-bytes1] = c2;

			while (bytes1)
			{
				bytes1--;
				ch1[3 - bytes1] = *str1++;
				ch2[3 - bytes1] = *str2++;
			}


			BIN_SEARCH(utf8_tolower_table,0,ARRAY_LEN(utf8_tolower_table),(*((unsigned int *)utf8_tolower_table[m].from) - *((unsigned int *)ch1)),uc1);
			BIN_SEARCH(utf8_tolower_table,0,ARRAY_LEN(utf8_tolower_table),(*((unsigned int *)utf8_tolower_table[m].from) - *((unsigned int *)ch2)),uc2);

			if (uc1) ch1l = *((unsigned int *)uc1->to);
			else ch1l = *((unsigned int *)ch1);

			if (uc2) ch2l = *((unsigned int *)uc2->to);
			else ch2l = *((unsigned int *)ch2);

			if (ch1l != ch2l)
			{
				if (ch1l < ch2l) return -1;
				return 1;
			}
		}
	}
	return 0;
}

/**************************************************************************
 Compares two utf8 string case-insensitive (the args might be NULL).
 Note: Changes for little endian
**************************************************************************/
int utf8stricmp_len(const char *str1, const char *str2, int len)
{
	unsigned char c1;
	unsigned char c2;

	if (!str1)
	{
		if (!str2) return 0;
		return -1;
	}

	if (!str2) return 1;

	while (len>0)
	{
		int d;
		char bytes1,bytes2;

		c1 = *str1++;
		c2 = *str2++;
		len--;

		if (!c1)
		{
			if (!c2) return 0;
			return -1;
		}
		if (!c2) return 1;

		if (c1 < 0x80)
		{
			if (c2 < 0x80)
			{
				d = tolower(c1) - tolower(c2);
				if (d) return d;
				continue;
			} else
			{
				/* TODO: must use locale sensitive sorting */
				return -1;
			}
		}
		if (c2 < 0x80) return 1; /* TODO: must use locale sensitive sorting */

		bytes1 = trailingBytesForUTF8[c1];
		bytes2 = trailingBytesForUTF8[c2];

		/* case mapping only happens within same number of bytes (currently) */
		if ((d = bytes1 - bytes2)) return d;

		if (bytes1 > 3)
		{
			/* case mapping relevant characters are only withing 4 bytes */
			while (bytes1)
			{
				if ((d = *str1++ - *str2++)) return d;
				bytes1--;
			}
		} else
		{

			unsigned char ch1[4],ch2[4];
			struct uniconv *uc1;
			struct uniconv *uc2;
			int ch1l;
			int ch2l;

			*((unsigned int *)ch1) = 0;
			*((unsigned int *)ch2) = 0;

			ch1[3-bytes1] = c1;
			ch2[3-bytes1] = c2;

			while (bytes1)
			{
				bytes1--;
				ch1[3 - bytes1] = *str1++;
				ch2[3 - bytes1] = *str2++;

				len--;
			}


			BIN_SEARCH(utf8_tolower_table,0,ARRAY_LEN(utf8_tolower_table),(*((unsigned int *)utf8_tolower_table[m].from) - *((unsigned int *)ch1)),uc1);
			BIN_SEARCH(utf8_tolower_table,0,ARRAY_LEN(utf8_tolower_table),(*((unsigned int *)utf8_tolower_table[m].from) - *((unsigned int *)ch2)),uc2);

			if (uc1) ch1l = *((unsigned int *)uc1->to);
			else ch1l = *((unsigned int *)ch1);

			if (uc2) ch2l = *((unsigned int *)uc2->to);
			else ch2l = *((unsigned int *)ch2);

			if (ch1l != ch2l)
			{
				if (ch1l < ch2l) return -1;
				return 1;
			}
		}
	}
	return 0;
}

/**************************************************************************
 Returns the pointer where the given string starts or NULL
**************************************************************************/
char *utf8stristr(const char *str1, const char *str2)
{
	int str2_len;

	if (!str1 || !str2) return NULL;

	str2_len = strlen(str2);

	while (*str1)
	{
		if (!utf8stricmp_len(str1,str2,str2_len))
			return (char*)str1;
		str1++;
	}
	return NULL;
}


/**************************************************************************
 Converts a single UFT-8 Chracter to a Unicode character very very
 incomplete. Should return NULL if invalid (actualy not implemented)
**************************************************************************/
char *uft8toucs(char *chr, unsigned int *code)
{
	unsigned char c = *chr++;
	unsigned int ucs = 0;
	int i,bytes;
	if (!(c & 0x80))
	{
		*code = c;
		return chr;
	} else
	{
		if (!(c & 0x20))
		{
			bytes = 2;
			ucs = c & 0x1f;
		}
		else if (!(c & 0x10))
		{
			bytes = 3;
			ucs = c & 0xf;
		}
		else if (!(c & 0x08))
		{
			bytes = 4;
			ucs = c & 0x7;
		}
		else if (!(c & 0x04))
		{
			bytes = 5;
			ucs = c & 0x3;
		}
		else /* if (!(c & 0x02)) */
		{
			bytes = 6;
			ucs = c & 0x1;
		}

		for (i=1;i<bytes;i++)
			ucs = (ucs << 6) | ((*chr++)&0x3f);
	}
	*code = ucs;
	return chr;
}

static unsigned char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static short invbase64[128];

static unsigned char ibase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,";
static short iinvbase64[128];

static unsigned char direct[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'(),-./:?";
static unsigned char optional[] = "!\"#$%&*;<=>@[]^_`{|}";
static unsigned char spaces[] = " \011\015\012";		/* space, tab, return, line feed */
static char mustshiftsafe[128];
static char mustshiftopt[128];

static int needtables = 1;

static void tabinit(void)
{
	int i, limit;

	for (i = 0; i < 128; ++i)
	{
		mustshiftopt[i] = mustshiftsafe[i] = 1;
		invbase64[i] = -1;
	}
	limit = strlen(direct);
	for (i = 0; i < limit; ++i)
		mustshiftopt[direct[i]] = mustshiftsafe[direct[i]] = 0;
	limit = strlen(spaces);
	for (i = 0; i < limit; ++i)
		mustshiftopt[spaces[i]] = mustshiftsafe[spaces[i]] = 0;
	limit = strlen(optional);
	for (i = 0; i < limit; ++i)
		mustshiftopt[optional[i]] = 0;
	limit = strlen(base64);
	for (i = 0; i < limit; ++i)
		invbase64[base64[i]] = i;

	/* that's for the modified imap utf7 stuff */
	limit = strlen(ibase64);
	for (i = 0; i < limit; ++i)
		iinvbase64[ibase64[i]] = i;

	needtables = 0;
}

#define DECLARE_BIT_BUFFER register unsigned long BITbuffer = 0, buffertemp = 0; int bufferbits = 0
#define BITS_IN_BUFFER bufferbits
#define WRITE_N_BITS(x, n) ((BITbuffer |= ( ((x) & ~(-1L<<(n))) << (32-(n)-bufferbits) ) ), bufferbits += (n) )
#define READ_N_BITS(n) ((buffertemp = (BITbuffer >> (32-(n)))), (BITbuffer <<= (n)), (bufferbits -= (n)), buffertemp)

/**************************************************************************
 Converts a UTF7 string into UTF8
**************************************************************************/
char *utf7ntoutf8(char *source, int sourcelen)
{
	FILE *fh;
	int base64value=0,base64EOF=0,first=0;
	int shifted = 0;
	char *dest = NULL;
	DECLARE_BIT_BUFFER;

	if (needtables) tabinit();

	if ((fh = tmpfile()))
	{
		int dest_len;

		while (sourcelen)
		{
			unsigned char c = *source++;
			sourcelen--;

			if (shifted)
			{
				if ((base64EOF = (!sourcelen) || (c > 0x7f) || (base64value = invbase64[c]) < 0))
				{
					shifted = 0;
					/* If the character causing us to drop out was SHIFT_IN or
					   SHIFT_OUT, it may be a special escape for SHIFT_IN. The
					   test for SHIFT_IN is not necessary, but allows an alternate
					   form of UTF-7 where SHIFT_IN is escaped by SHIFT_IN. This
					   only works for some values of SHIFT_IN.
					 */
					if (c && sourcelen && (c == '+' || c == '-'))
					{
						/* get another character c */
						unsigned char prevc = c;

						c = *source++;

						/* If no base64 characters were encountered, and the
							 character terminating the shift sequence was
							 SHIFT_OUT, then it's a special escape for SHIFT_IN.
						*/
						if (first && prevc == '-')
						{
							fputc('+',fh);
						}
					}
				} else
				{
					/* Add another 6 bits of base64 to the bit buffer. */
					WRITE_N_BITS(base64value, 6);
					first = 0;
				}
			}

			/* Extract as many full 16 bit characters as possible from the
			   bit buffer.
			 */
			while (BITS_IN_BUFFER >= 16)
			{
				UTF32 src_utf32 = READ_N_BITS(16);
				UTF32 *src_utf32_ptr = &src_utf32;
				UTF8 target_utf8[10];
				UTF8 *target_utf8_ptr = target_utf8;

				ConvertUTF32toUTF8(&src_utf32_ptr,src_utf32_ptr+1,&target_utf8_ptr,target_utf8+10, strictConversion);

				fwrite(target_utf8,1,target_utf8_ptr - target_utf8,fh);
			}

			if (!c) break;

			if (base64EOF) BITS_IN_BUFFER = 0;

			if (!shifted)
			{
				if (c == '+')
				{
					shifted = first = 1;
				} else
				{
					if (c <= 0x7f)
					{
						fputc(c,fh);
					} /* else the source is invalid, so we ignore this */
				}
			}
		}

		if ((dest_len = ftell(fh)))
		{
	    fseek(fh,0,SEEK_SET);
			if ((dest = (char*)malloc(dest_len+1)))
			{
				fread(dest,1,dest_len,fh);
				dest[dest_len]=0;
			}
		}
	}

	return dest;
}

/**************************************************************************
 UTF8 to UTF7 (IMAP modiefied)
**************************************************************************/
char *utf8toiutf7(char *utf8, int sourcelen)
{
	FILE *fh;
	char *dest = NULL;

	if (needtables) tabinit();

	if ((fh = tmpfile()))
	{
		int dest_len;
		int shifted = 0;
		DECLARE_BIT_BUFFER;

		while (1)
		{
			unsigned char c;
			int noshift;

			if (sourcelen)
			{
				c = *utf8;
				noshift = (c >= 0x20 && c <= 0x7e) && (c != '&');
			} else
			{
				c = 0;
				noshift = 1;
			}

			if (shifted)
			{
				while (BITS_IN_BUFFER >= 6)
				{
					unsigned char bits = READ_N_BITS(6);
					fputc(ibase64[bits],fh);
				}

				if (noshift)
				{
					int bits_in_buf = BITS_IN_BUFFER;

					if (bits_in_buf)
					{
						unsigned char bits = READ_N_BITS(bits_in_buf);
						bits <<= 6 - bits_in_buf;
						fputc(ibase64[bits],fh);
					}
					shifted = 0;
					fputc('-',fh);
				}
			}

			if (!c) break;

			if (noshift)
			{
				if (c == '&')
				{
					fputs("&-",fh);
				} else fputc(c,fh);
				utf8++;
				sourcelen--;
			} else
			{
				UTF8 *source = (UTF8*)utf8;
				UTF16 dest = 0;
				UTF16 *dest_ptr = &dest;
				ConversionResult res;

				res = ConvertUTF8toUTF16(&source, source + sourcelen, &dest_ptr, dest_ptr + 1, strictConversion);
				if (res == conversionOK || res == targetExhausted)
				{
					sourcelen -= trailingBytesForUTF8[c] + 1;
					utf8 += trailingBytesForUTF8[c] + 1;

					if (!shifted)
					{
						fputc('&',fh);
						shifted = 1;
					}

					WRITE_N_BITS(dest,16);
				}
			}
		}

		if ((dest_len = ftell(fh)))
		{
	    fseek(fh,0,SEEK_SET);
			if ((dest = (char*)malloc(dest_len+1)))
			{
				fread(dest,1,dest_len,fh);
				dest[dest_len]=0;
			}
		}
	}
	return dest;
}

/**************************************************************************
 Converts a IMAP "UTF7" string into UTF8. (see RFC2060)

 Entw&APw-rfe => EntwÃ¼rfe
**************************************************************************/
char *iutf7ntoutf8(char *source, int sourcelen)
{
	FILE *fh;
	int base64value=0,base64EOF=0,first=0;
	int shifted = 0;
	char *dest = NULL;
	DECLARE_BIT_BUFFER;

	if (needtables) tabinit();

	if ((fh = tmpfile()))
	{
		int dest_len;

		while (sourcelen)
		{
			unsigned char c = *source++;
			sourcelen--;

			if (shifted)
			{
				if ((base64EOF = (!sourcelen) || (c > 0x7f) || (base64value = invbase64[c]) < 0))
				{
					shifted = 0;
					/* If the character causing us to drop out was SHIFT_IN or
					   SHIFT_OUT, it may be a special escape for SHIFT_IN. The
					   test for SHIFT_IN is not necessary, but allows an alternate
					   form of UTF-7 where SHIFT_IN is escaped by SHIFT_IN. This
					   only works for some values of SHIFT_IN.
					 */
					if (c && sourcelen && (c == '-'))
					{
						/* get another character c */
						unsigned char prevc = c;

						c = *source++;

						/* If no base64 characters were encountered, and the
							 character terminating the shift sequence was
							 SHIFT_OUT, then it's a special escape for SHIFT_IN.
						*/
						if (first && prevc == '-')
						{
							fputc('&',fh);
						}
					}
				} else
				{
					/* Add another 6 bits of base64 to the bit buffer. */
					WRITE_N_BITS(base64value, 6);
					first = 0;
				}
			}

			/* Extract as many full 16 bit characters as possible from the
			   bit buffer.
			 */
			while (BITS_IN_BUFFER >= 16)
			{
				UTF32 src_utf32 = READ_N_BITS(16);
				UTF32 *src_utf32_ptr = &src_utf32;
				UTF8 target_utf8[10];
				UTF8 *target_utf8_ptr = target_utf8;

				ConvertUTF32toUTF8(&src_utf32_ptr,src_utf32_ptr+1,&target_utf8_ptr,target_utf8+10, strictConversion);

				fwrite(target_utf8,1,target_utf8_ptr - target_utf8,fh);
			}

			if (!c) break;

			if (base64EOF) BITS_IN_BUFFER = 0;

			if (!shifted)
			{
				if (c == '&')
				{
					shifted = first = 1;
				} else
				{
					if (c <= 0x7f)
					{
						fputc(c,fh);
					} /* else the source is invalid, so we ignore this */
				}
			}
		}

		if ((dest_len = ftell(fh)))
		{
	    fseek(fh,0,SEEK_SET);
			if ((dest = (char*)malloc(dest_len+1)))
			{
				fread(dest,1,dest_len,fh);
				dest[dest_len]=0;
			}
		}
	}

	return dest;
}

/**************************************************************************
 
**************************************************************************/
char *utf8topunycode(const utf8 *source, int sourcelen)
{
	enum punycode_status status;
	const utf8 *sourceend;
	char *punny;
	punycode_uint punny_len;

	punycode_uint *dest, *target;
	punycode_uint dest_len;

	if (!(dest = malloc(sourcelen * sizeof(punycode_uint))))
		return NULL;

	target = dest;
	sourceend = source + sourcelen;

	while (source < sourceend)
	{
		punycode_uint ch = 0;
		unsigned short extraBytesToRead = trailingBytesForUTF8[*source];

		if (source + extraBytesToRead >= sourceend)
		{
			/* source exhausted */
			free(dest);
			return NULL;
		}

		/* Do this check whether lenient or strict */
		if (!isLegalUTF8((UTF8*)source, extraBytesToRead+1))
		{
			free(dest);
			return NULL;
		}

		/*
		 * The cases all fall through.
		 */
		switch (extraBytesToRead) {
			case 3:	ch += *source++; ch <<= 6;
			case 2:	ch += *source++; ch <<= 6;
			case 1:	ch += *source++; ch <<= 6;
			case 0:	ch += *source++;
		}
		ch -= offsetsFromUTF8[extraBytesToRead];

		if (ch <= UNI_MAX_UTF32) {
			*target++ = ch;
		} else if (ch > UNI_MAX_UTF32) {
			*target++ = UNI_REPLACEMENT_CHAR;
		}
	}

	dest_len = target - dest; /* No 0 ending */
	punny_len = dest_len * 2;

	do
	{
		int strored_punny_len = punny_len;

		if (!(punny = malloc(punny_len+5)))
		{
			free(dest);
			return NULL;
		}
		status = punycode_encode(dest_len, dest, NULL /* case flags */, &punny_len, punny);

		if (status == punycode_success)
		{
			free(dest);
			return punny;
		}
		punny_len = strored_punny_len * 2;
	} while (status == punycode_big_output);

	free(punny);
	free(dest);
	return NULL;
}

/**************************************************************************
 Is string ASCII 7 bit only?
**************************************************************************/
int isascii7(const char *str)
{
	char c;
	if (!str) return 1;
	while ((c = *str++))
	{
		if (c & 0x80) return 0;
	}
	return 1;
}

#ifndef SM__MD5_H
#define SM__MD5_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(AMIGA) || defined(_AMIGA) || defined(__AMIGAOS4__) || defined(__MORPHOS__)
 #define HIGHFIRST
#else
 #ifdef WORDS_BIGENDIAN /* by configure */
  #define HIGHFIRST
 #endif
#endif

#ifdef __alpha
typedef unsigned int md5_uint32;
#else
typedef unsigned long md5_uint32;
#endif

struct SM_MD5Context {
	md5_uint32 buf[4];
	md5_uint32 bits[2];
	unsigned char in[64];
};

void MD5Init(struct SM_MD5Context *context);
void MD5Update(struct SM_MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(unsigned char digest[16], struct SM_MD5Context *context);
void MD5Transform(md5_uint32 buf[4], md5_uint32 const in[16]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct SM_MD5Context SM_MD5_CTX;

#endif /* !MD5_H */



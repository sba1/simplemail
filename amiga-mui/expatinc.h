/* This file includes the amiga specific expat files so
   expat using is transparent */

#if defined(__AROS__) || defined(__AMIGAOS4__)
#include <expat.h>
#else
#include <exec/types.h>
#include <expat/expat.h>
#include <proto/expat.h>
#endif


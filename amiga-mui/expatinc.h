/* This file includes the amiga specific expat files so
   expat using is transparent */

#if defined(__AROS__) || defined(__AMIGAOS4__)
#include <expat.h>
#else
#include <exec/types.h>
#include <expat/expat.h>
#include <proto/expat.h>
#ifdef __MORPHOS__
#include <emul/emulregs.h>
#define XML_HANDLER_REF(x) (void *)&x##_trap
extern struct EmulLibEntry xml_start_tag_trap;
extern struct EmulLibEntry xml_end_tag_trap;
extern struct EmulLibEntry xml_char_data_trap;
#endif
#endif


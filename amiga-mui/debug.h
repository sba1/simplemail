#ifndef __DEBUG_H
#define __DEBUG_H

/* Debug Macros */

#include <dos.h>
#include <proto/exec.h>

#ifdef _AROS

#undef DEBUG
#define DEBUG 1
#include <aros/debug.h>

#else /* _AROS */

#define bug kprintf

#ifdef MYDEBUG
void kprintf(char *string, ...);
#define D(x) {kprintf("%s/%ld %ld bytes (%s): ", __FILE__, __LINE__, (ULONG)getreg(REG_A7) - (ULONG)FindTask(NULL)->tc_SPLower/*Upper + 4096*/, FindTask(NULL)->tc_Node.ln_Name);(x);};
#else
#define D(x) ;

#endif /* MYDEBUG */

#endif /*_AROS */

#endif /* __DEBUG_H */

#ifndef SM__COMPILER_H
#define SM__COMPILER_H

/* Some compiler depend stuff */

#if defined(__MAXON__) || defined(__STORM__)
#define ASM
#define SAVEDS
#define FAR
#else
#define FAR __far
#define ASM __asm
#define SAVEDS __saveds
#endif

#endif

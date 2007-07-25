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
** compiler.h
*/

#ifndef SM__COMPILER_H
#define SM__COMPILER_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifdef __AROS__

#define SAVEDS
#define ASM
#define REG(reg,arg) arg

#else /* __AROS__ */

#if (INCLUDE_VERSION < 50) || defined(__MORPHOS__)

#ifdef __SASC

#ifndef ASM
#define ASM __asm
#endif /* ASM */

#ifndef INLINE
#define INLINE __inline
#endif /* INLINE */

#ifndef REG
#define REG(reg,arg) register __##reg arg
#endif /* REG */

#ifndef REGARGS
#define REGARGS __regargs
#endif /* REGARGS */

#ifndef STDARGS
#define STDARGS __stdargs
#endif /* STDARGS */

#ifndef SAVEDS
#define SAVEDS __saveds
#endif /* SAVEDS */

#define __attribute__(dummy)

#endif /* __SASC */

/****************************************************************************/

#ifdef __GNUC__

#ifndef ASM
#define ASM
#endif /* ASM */

#ifndef INLINE
#define INLINE __inline__
#endif /* INLINE */

#ifndef FAR
#define FAR
#endif /* FAR */

#ifdef mc68000

#ifndef REG
#define REG(reg,arg) arg __asm(#reg)
#endif /* REG */

#ifndef REGARGS
#define REGARGS __regargs
#endif /* REGARGS */

#ifndef STDARGS
#define STDARGS __stdargs
#endif /* STDARGS */

#ifndef INTERRUPT
#define INTERRUPT __interrupt
#endif /* INTERRUPT */

#ifndef SAVEDS
#define SAVEDS __saveds
#endif /* SAVEDS */

#else /* PPC */

#ifndef REG
#define REG(reg,arg) arg
#endif /* REG */

#ifndef REGARGS
#define REGARGS
#endif /* REGARGS */

#ifndef STDARGS
#define STDARGS
#endif /* STDARGS */

#ifndef INTERRUPT
#define INTERRUPT
#endif /* INTERRUPT */

#ifndef SAVEDS
#define SAVEDS
#endif /* SAVEDS */

#endif /* neither 68k nor PPC */

#endif /* __GNUC__ */

/****************************************************************************/


#endif

#endif /* __AROS__ */

#endif

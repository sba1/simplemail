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
** arch.h - architecture specific definitions
*/

#ifndef SM__ARCH_H
#define SM__ARCH_H

#ifndef PROTO_EXEC_H
#include <proto/exec.h>
#endif

/* Paths */
#define SM_DIR					"PROGDIR:"
#define SM_CHARSET_DIR	"PROGDIR:Charsets"
#define SM_FOLDER_DIR	"PROGDIR:.folders"
#define SM_CURRENT_DIR	""

/* Operation system */
#ifdef __AMIGAOS4__
#define SM_OPERATIONSYSTEM "AmigaOS4/MUI"
#else
#ifdef __MORPHOS__
#define SM_OPERATIONSYSTEM "MorphOS/MUI"
#else
#define SM_OPERATIONSYSTEM "AmigaOS/MUI"
#endif
#endif

/* Debug - defines ARCH_DEBUG */
#if defined(__GNUC__) && defined(__AMIGAOS4__)
#define ARCH_DEBUG(x) do { (IExec->Forbid)(); (IExec->DebugPrintF)("%s/%ld [%s()] Task \"%s\" => ",__FILE__,__LINE__,__PRETTY_FUNCTION__,(IExec->FindTask)(NULL)->tc_Node.ln_Name); (IExec->DebugPrintF) x; (IExec->Permit)();} while (0)
#undef NDEBUG
#else
void kprintf(char *string, ...);
#define ARCH_DEBUG(x) do { Forbid(); kprintf("%s/%ld Task \"%s\" => ",__FILE__,__LINE__,FindTask(NULL)->tc_Node.ln_Name); kprintf x; Permit();} while (0)
#endif

#endif

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
** muistuff.c
*/

#include <dos.h>

#include <utility/hooks.h>
#include <intuition/classusr.h> /* Object * */
#include <libraries/mui.h>
#include <proto/muimaster.h>

#include "compiler.h"
#include "muistuff.h"

LONG xget(Object * obj, ULONG attribute)
{
  LONG x;
  get(obj, attribute, &x);
  return (x);
}

ULONG DoSuperNew(struct IClass *cl, Object * obj, ULONG tag1,...)
{
  return (DoSuperMethod(cl, obj, OM_NEW, &tag1, NULL));
}

Object *MakeLabel(STRPTR str)
{
  return (MUI_MakeObject(MUIO_Label, str, 0));
}

Object *MakeButton(STRPTR str)
{
  Object *obj = MUI_MakeObject(MUIO_Button, str);
  if (obj)
  {
    set(obj,MUIA_CycleChain, 1);
  }
  return obj;
}

struct Hook hook_standard;

STATIC ASM /*SAVEDS*/ void hook_func_standard(register __a0 struct Hook *h, register __a1 ULONG * funcptr)
{
	void (*func) (ULONG *) = (void (*)(ULONG *)) (*funcptr);
	putreg(REG_A4,(long)h->h_Data);

	if (func)
		func(funcptr + 1);
}

/* Must be called before the hook_standard is used */
void init_hook_standard(void)
{
	hook_standard.h_Entry = (HOOKFUNC)hook_func_standard;
	hook_standard.h_Data = (void*)getreg(REG_A4);
}

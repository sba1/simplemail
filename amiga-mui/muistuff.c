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
** $Id$
*/

#include <dos.h>

#include <utility/hooks.h>
#include <intuition/classusr.h> /* Object * */
#include <libraries/mui.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

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

Object *MakeCheck(STRPTR label, ULONG check)
{
	Object *obj = MUI_MakeObject(MUIO_Checkmark, label);
	if (obj)
	{
		SetAttrs(obj, MUIA_CycleChain, 1, MUIA_Selected, check, TAG_DONE);
	}
	return (obj);
}

Object *MakeCycle(STRPTR label, STRPTR * array)
{
  Object *obj = MUI_MakeObject(MUIO_Cycle, label, array);
  if (obj)
    set(obj, MUIA_CycleChain, 1);
  return (obj);
}

VOID DisposeAllChilds(Object *o)
{
  struct List *child_list = (struct List*)xget(o,MUIA_Group_ChildList);
  Object *cstate = (Object *)child_list->lh_Head;
  Object *child;

  while ((child = (Object*)NextObject(&cstate)))
  {
    DoMethod(o,OM_REMMEMBER,child);
    MUI_DisposeObject(child);
  }
}

struct Hook hook_standard;

STATIC ASM void hook_func_standard(register __a0 struct Hook *h, register __a1 ULONG * funcptr)
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

/* the hook function, it loads the a4 register and call the subentry */
STATIC ASM void myhook_func(register __a0 struct MyHook *h, register __a1 ULONG msg, register __a2 ULONG obj)
{
	__asm int (*func) (register __a0 struct MyHook *, register __a1 ULONG, register __a2) =
		(__asm int (*) (register __a0 struct MyHook *, register __a1 ULONG, register __a2))h->hook.h_SubEntry;

	if (func)
	{
		putreg(REG_A4,(long)h->rega4);
		func(h,msg,obj);
	}
}

/* Initializes a hook */
void init_myhook(struct MyHook *h, unsigned long (*func)(),void *data)
{
	h->hook.h_Entry = (HOOKFUNC)myhook_func;
	h->hook.h_SubEntry = func;
	h->hook.h_Data = data;
	h->rega4 = getreg(REG_A4);
}

/* the hook function, it loads the a4 register and call the subentry */
STATIC ASM void hook_func(register __a0 struct Hook *h, register __a1 ULONG msg, register __a2 ULONG obj)
{
	__asm int (*func) (register __a0 struct Hook *, register __a1 ULONG, register __a2) =
		(__asm int (*) (register __a0 struct Hook *, register __a1 ULONG, register __a2))h->h_SubEntry;

	if (func)
	{
		putreg(REG_A4,(long)h->h_Data);
		func(h,msg,obj);
	}
}

void init_hook(struct Hook *h, unsigned long (*func)())
{
	h->h_Entry = (HOOKFUNC)hook_func;
	h->h_SubEntry = func;
	h->h_Data = (void*)getreg(REG_A4);
}


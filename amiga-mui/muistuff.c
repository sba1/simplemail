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

#include <utility/hooks.h>
#include <intuition/classusr.h> /* Object * */
#include <libraries/mui.h>

#include <clib/alib_protos.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <proto/exec.h>

#include "compiler.h"
#include "muistuff.h"

#include "debug.h"

LONG xget(Object * obj, ULONG attribute)
{
  LONG x;
  get(obj, attribute, &x);
  return (x);
}

#ifdef __AMIGAOS4__
#include <stdarg.h>

APTR VARARGS68K MyNewObject(struct IClass *cl, CONST_STRPTR id, ... )
{
	ULONG *tags;
	Object *o;
	va_list args;

	va_startlinear(args,id);

	tags = va_getlinearva(args,void*);

	o = NewObjectA(cl,id,(struct TagItem*)va_getlinearva(args,ULONG));
	if (o == NULL)
	{
		SM_DEBUGF(5,("FAILED!! cl=0x%lx id=%s\n",cl,id?id:(CONST_STRPTR)""));
	}
	va_end(args);
	return o;
}

ULONG VARARGS68K DoSuperNew(struct IClass *cl, Object * obj, ...)
{
	ULONG rc;
	va_list args;
	va_startlinear(args,obj);

	rc = DoSuperMethod(cl, obj, OM_NEW, va_getlinearva(args,ULONG), NULL);
	va_end(args);
	return rc;
}

void SetRexxVar(void)
{
}

#else

#ifdef __MORPHOS__
APTR MyNewObject(struct IClass *cl, CONST_STRPTR id, ... )
{
	ULONG *tags;
	Object *o;
	va_list args;

	va_start(args,id);

	tags = (ULONG *)args->overflow_arg_area;

	SM_DEBUGF(25,("tag=%p [0]=%p [1]=%p [2]=%p id=%p\n",tags,tags[0],tags[1],tags[2],id));

	o = NewObjectA(cl,id,(struct TagItem *)tags);
	va_end(args);
	return o;
}
#endif /* __MORPHOS__ */

#endif /* __AMIGAOS4__ */

#if defined(__AMIGAOS4__)

struct MUI_CustomClass *CreateMCC(CONST_STRPTR supername, struct MUI_CustomClass *supermcc, int instDataSize, APTR dispatcher)
{
	extern ULONG muiDispatcherEntry(void);
	extern ULONG hookEntry(void);

	struct MUI_CustomClass *cl;

	if (SysBase->lib_Version >= 51 && SysBase->lib_Revision >= 3)
	{
		cl = MUI_CreateCustomClass(NULL,supername,supermcc,instDataSize, dispatcher);
	} else
	{
		if ((cl = MUI_CreateCustomClass(NULL,supername,supermcc,instDataSize, &muiDispatcherEntry)))
		{
			cl->mcc_Class->cl_UserData = dispatcher;
		}
	}
	return cl;
}
#elif defined(__MORPHOS__)
struct MUI_CustomClass *CreateMCC(CONST_STRPTR supername, struct MUI_CustomClass *supermcc, int instDataSize, APTR dispatcher)
{
	extern ULONG muiDispatcherEntry(void);
	extern ULONG hookEntry(void);

	struct MUI_CustomClass *cl;

	if ((cl = MUI_CreateCustomClass(NULL,supername,supermcc,instDataSize, &muiDispatcherEntry)))
	{
		cl->mcc_Class->cl_UserData = dispatcher;
	}
	return cl;
}
#else
struct MUI_CustomClass *CreateMCC(CONST_STRPTR supername, struct MUI_CustomClass *supermcc, int instDataSize, APTR dispatcher)
{
	return MUI_CreateCustomClass(NULL,supername,supermcc,instDataSize, dispatcher);
}

ULONG VARARGS68K DoSuperNew(struct IClass *cl, Object * obj, ...)
{
  return DoSuperMethod(cl, obj, OM_NEW, (((ULONG*)&obj)+1), NULL);
}

APTR VARARGS68K MyNewObject(struct IClass *cl, CONST_STRPTR id, ... )
{
	return NewObjectA(cl,id, (struct TagItem*)((&id)+1));
}

#endif /* __AMIGAOS4__ || __MORPHOS__ */


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

VOID DisposeAllFamilyChilds(Object *o)
{
  struct List *child_list = (struct List*)xget(o,MUIA_Family_List);
  Object *cstate = (Object *)child_list->lh_Head;
  Object *child;

  while ((child = (Object*)NextObject(&cstate)))
  {
    DoMethod(o,OM_REMMEMBER,child);
    MUI_DisposeObject(child);
  }
}

/*
VOID AddButtonToSpeedBar(Object *speedbar, int image_idx, char *text, char *help)
{
	struct MUIS_SpeedBar_Button msb;
	msb.Img = image_idx;
	msb.Text = text;
	msb.Help = help;
	msb.Flags = 0;
	msb.Class = NULL;
	msb.Object = NULL;

	DoMethod(speedbar, MUIM_SpeedBar_AddButton, &msb);
}
*/

#ifdef __AMIGAOS4__
extern void hookEntry(void);
#else
#ifdef __MORPHOS__
#define hookEntry HookEntry
#endif
#endif

struct Hook hook_standard;

STATIC ASM SAVEDS VOID hook_func_standard(REG(a0,struct Hook *h), REG(a2, Object *obj), REG(a1, ULONG * funcptr))
{
	void (*func) (ULONG *) = (void (*)(ULONG *)) (*funcptr);

	if (func)
		func(funcptr + 1);
}

/* Must be called before the hook_standard is used */
void init_hook_standard(void)
{
#if defined(__AMIGAOS4__)
	if (SysBase->lib_Version >= 51 && SysBase->lib_Revision >= 3)
	{
		hook_standard.h_Entry = (HOOKFUNC)hook_func_standard;
	} else
	{
		hook_standard.h_Entry = (HOOKFUNC)hookEntry;
		hook_standard.h_SubEntry = (HOOKFUNC)hook_func_standard;
	}
#elif defined(__MORPHOS__)
	hook_standard.h_Entry = (HOOKFUNC)hookEntry;
	hook_standard.h_SubEntry = (HOOKFUNC)hook_func_standard;
#else
	hook_standard.h_Entry = (HOOKFUNC)hook_func_standard;
#endif
}

#if !defined(__AMIGAOS4__) && !defined(__MORPHOS__)
/* the hook function, it loads the a4 register and call the subentry */
STATIC ASM SAVEDS VOID hook_func(REG(a0,struct Hook *h), REG(a2, ULONG obj), REG(a1, ULONG msg))
{
	ASM VOID (*func) (REG(a0, struct Hook *), REG(a2, ULONG), REG(a1, ULONG)) =
		(ASM  VOID (*) (REG(a0,struct Hook *), REG(a2, ULONG), REG(a1, ULONG)))h->h_SubEntry;

	if (func)
	{
		func(h,obj,msg);
	}
}
#endif

void init_hook(struct Hook *h, unsigned long (*func)(void))
{
#if defined(__AMIGAOS4__)
	if (SysBase->lib_Version >= 51 && SysBase->lib_Revision >= 3)
	{
		h->h_Entry = (HOOKFUNC)func;
	} else
	{
		h->h_Entry = (HOOKFUNC)hookEntry;
		h->h_SubEntry = (HOOKFUNC)func;
	}
#elif defined(__MORPHOS__)
	h->h_Entry = (HOOKFUNC)hookEntry;
	h->h_SubEntry = (HOOKFUNC)func;
#else
	h->h_Entry = (HOOKFUNC)hook_func;
	h->h_SubEntry = func;
#endif
}

void init_hook_with_data(struct Hook *h, unsigned long (*func)(void), void *data)
{
#if defined(__AMIGAOS4__)
	if (SysBase->lib_Version >= 51 && SysBase->lib_Revision >= 3)
	{
		h->h_Entry = (HOOKFUNC)func;
		h->h_Data = data;
	} else
	{
		h->h_Entry = (HOOKFUNC)hookEntry;
		h->h_SubEntry = (HOOKFUNC)func;
		h->h_Data = data;
	}
#elif defined(__MORPHOS__)
	h->h_Entry = (HOOKFUNC)hookEntry;
	h->h_SubEntry = (HOOKFUNC)func;
	h->h_Data = data;
#else
	h->h_Entry = (HOOKFUNC)hook_func;
	h->h_SubEntry = func;
	h->h_Data = data;
#endif
}

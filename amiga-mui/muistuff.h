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

/**
 * @file muistuff.h
 */

#ifndef SM__MUISTUFF_H
#define SM__MUISTUFF_H

#ifndef __AMIGAOS4__
#define VARARGS68K
#endif

#ifndef SM__COMPILER_H
#include "compiler.h"
#endif

#ifndef PROTO_INTUITION_H
#include <proto/intuition.h>
#endif

#ifndef LIBRARIES_MUI_H
#include <libraries/mui.h>
#endif

/* useful MUI supports */

/**
 * Return the value of the given attribute.
 *
 * This assumes that the attribute is actually known to the object.
 *
 * @param obj the object to query
 * @param attribute the attribute to query
 * @return the value of the attribute. If the attribute is not known, this
 *  function returns trash.
 */
IPTR xget(Object * obj, ULONG attribute);
#ifdef __MORPHOS__
/* DoSuperNew is part of libabox.a */
#include <clib/alib_protos.h>
#define MyNewObject NewObject
#elif __AROS__
IPTR DoSuperNew(struct IClass *cl, Object *obj, Tag tag1, ...) __stackparm;
APTR MyNewObject (struct IClass *cl, CONST_STRPTR id, Tag tag1, ...) __stackparm;
#else
ULONG VARARGS68K DoSuperNew(struct IClass *cl, Object * obj, ...);
Object *VARARGS68K MyNewObject(struct IClass *cl, CONST_STRPTR id, ...);
#endif

/**
 * Make a simple label displaying the given str.
 *
 * @param str the string of the label
 * @return the label object
 */
Object *MakeLabel(STRPTR str);

/**
 * Make a simple button object displaying the given str.
 *
 * @param str the string of the button
 * @return the button object
 */
Object *MakeButton(STRPTR str);

/**
 * Make a simple checkbox object.
 *
 * @param label the label of the checkbox, used for keyboard handling
 * @param check the state of the checkbox
 * @return the checkbox object
 */
Object *MakeCheck(STRPTR label, ULONG check);

/**
 * Make a simple cycle object
 *
 * @param label the label of the cycle, used for keyboard handling
 * @param array the array defining the entries to be displayed
 * @return the cycle object
 */
Object *MakeCycle(STRPTR label, STRPTR * array);

/**
 * Dispose all the children associated with the given group object.
 *
 * @param o the group object whose children should be disposed.
 */
VOID DisposeAllChilds(Object *o);

/**
 * Dispose all the children associated with the given family object.
 *
 * @param o the family object whose children should be disposed.
 */
VOID DisposeAllFamilyChilds(Object *o);

/**
 * Add a new button to the given speedbar.
 *
 * @param speedbar the speedbar object to which a new button shall be added.
 * @param image_idx the image index of the new object
 * @param text the text of the new object
 * @param help the help string of the new object
 */
VOID AddButtonToSpeedBar(Object *speedbar, int image_idx, char *text, char *help);

/* Custom class creation helper */
struct MUI_CustomClass *CreateMCC(CONST_STRPTR supername, struct MUI_CustomClass *super_mcc, int instDataSize, ULONG (*dispatcher)(struct IClass *, Object *, Msg));

/* global application object, defined in gui_main.c */
extern Object *App;

/* global hook, use it for easy notifications */
extern struct Hook hook_standard;

/**
 * Initialize the global notify hook.
 *
 * Must be called before the hook_standard is used
 */
void init_hook_standard(void);

/**
 * Prepare the given hook to call the given function.
 *
 * @param h the hook to be initialized
 * @param func the function that shall be associated to the hook
 * @note This function shall be used only, if the data field of the given Hook
 *  is not needed.
 */
void init_hook(struct Hook *h, unsigned long (*func)(void));

/**
 * Prepare the given hook to call the given function.
 *
 * @param h the hook to be initialized
 * @param func the function that shall be associated to the hook
 * @param data defines the data that is accessible via the h_Data field of
 *  the hook when called
 */
void init_hook_with_data(struct Hook *h, unsigned long (*func)(void), void *data);

/* Some macros to be used in custom classes */
#define _between(a,x,b) ((x)>=(a) && (x)<=(b))
#define _isinobject(o,x,y) (_between(_mleft(o),(x),_mright (o)) && _between(_mtop(o) ,(y),_mbottom(o)))
#define _isinwholeobject(o,x,y) (_between(_left(o),(x),_right (o)) && _between(_top(o) ,(y),_bottom(o)))

/* Line Macros */
#define HorizLineObject  RectangleObject,MUIA_VertWeight,0,MUIA_Rectangle_HBar, TRUE,End
#define HorizLineTextObject(text)  RectangleObject,MUIA_VertWeight,0,MUIA_Rectangle_HBar, TRUE,MUIA_Rectangle_BarTitle,text,End

/* start undocumented */
#ifndef MUIM_GoActive
#define MUIM_GoActive                0x8042491a
#endif

#ifndef MUIM_GoInactive
#define MUIM_GoInactive              0x80422c0c
#endif

/* #define MUIC_Dtpic "Dtpic.mui", */
#ifndef MUIA_Dtpic_Name
#define MUIA_Dtpic_Name 0x80423d72
#endif
#define DtpicObject MUI_NewObject("Dtpic.mui"

#ifndef MUIM_CreateDragImage
#define MUIM_CreateDragImage 0x8042EB6F /* Returns MUI_DragImage */
struct  MUIP_CreateDragImage { ULONG MethodID; LONG touchx; LONG touchy; ULONG flags; };
#endif

#ifndef MUIM_DeleteDragImage
struct MUI_DragImage
{
  struct BitMap *bm;
  WORD width;
  WORD height;
  WORD touchx;
  WORD touchy;
  ULONG flags;
};

#define MUIM_DeleteDragImage 0x80423037
struct  MUIP_DeleteDragImage { ULONG MethodID; struct MUI_DragImage *di; };
#endif

#if (MUIMASTER_VMIN < 18)
#ifndef __AROS__ /*allready defined*/
#define MUIM_DoDrag 0x804216bb /* private */ /* V18 */
struct  MUIP_DoDrag { ULONG MethodID; LONG touchx; LONG touchy; ULONG flags; }; /* private */
#endif
#endif

#define MUIV_NListtree_Remove_Flag_NoActive (1<<13) /* internal */

#ifndef MUIA_Application_UsedClasses
#define MUIA_Application_UsedClasses 0x8042e9a7 /* V20 isg STRPTR * */
#endif

/* end undocumented */

#define MUIV_NList_UseImage_All         (-1)

#define MAKECOLOR32(x) (((x)<<24)|((x)<<16)|((x)<<8)|(x))

/*
    AROS definition of BOOPSI_DISPATCHER collides with other systems
    because the opening bracket { is part of the definition.
    Therefore I've defined MY_BOOPSI_DISPATCHER as the platform independent macro.
*/
#ifdef __AROS__
#define MY_BOOPSI_DISPATCHER(rettype,name,cl,obj,msg) rettype name(struct IClass *cl, Object *obj, Msg msg)
#else
#define MY_BOOPSI_DISPATCHER(rettype,name,cl,obj,msg)  ASM SAVEDS rettype name(REG(a0,struct IClass *cl),REG(a2,Object *obj), REG(a1, Msg msg))
#endif

#ifdef __cplusplus
#define set(o, a, v) SetAttrs(o, (a), (v), TAG_DONE)
#define nnset(o, a, v) SetAttrs((o), MUIA_NoNotify, TRUE, (a), (v), TAG_DONE)
static inline void setstring(Object *o, const char *s)
{
	set(o, MUIA_String_Contents, s);
}
template<typename T>
static inline void get(Object *obj, ULONG attr, T *store)
{
	static_assert(sizeof(T) == sizeof(uint32));
	GetAttr(attr, obj, (uint32*)store);
}

static inline void setcheckmark(Object *obj, bool val)
{
	set(obj, MUIA_Selected, val);
}
#endif

#endif /* SM_MUISTUFF_H */


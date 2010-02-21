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
** muistuff.h
*/

#ifndef SM__MUISTUFF_H
#define SM__MUISTUFF_H

#ifndef __AMIGAOS4__
#define VARARGS68K
#endif

/* useful MUI supports */
IPTR xget(Object * obj, ULONG attribute);
#ifdef __MORPHOS__
APTR MyNewObject( struct IClass *cl, CONST_STRPTR id, ...) __attribute__((varargs68k));
#elif __AROS__
IPTR DoSuperNew(struct IClass *cl, Object *obj, Tag tag1, ...) __stackparm;
APTR MyNewObject (struct IClass *cl, CONST_STRPTR id, Tag tag1, ...) __stackparm;
#else
ULONG VARARGS68K DoSuperNew(struct IClass *cl, Object * obj, ...);
APTR VARARGS68K MyNewObject(struct IClass *cl, CONST_STRPTR id, ...);
#endif
Object *MakeLabel(STRPTR str);
Object *MakeButton(STRPTR str);
Object *MakeCheck(STRPTR label, ULONG check);
Object *MakeCycle(STRPTR label, STRPTR * array);
VOID DisposeAllChilds(Object *o);
VOID DisposeAllFamilyChilds(Object *o);
VOID AddButtonToSpeedBar(Object *speedbar, int image_idx, char *text, char *help);

/* Custom class creation helper */
struct MUI_CustomClass *CreateMCC(CONST_STRPTR supername, struct MUI_CustomClass *super_mcc, int instDataSize, APTR dispatcher);

/* global application object, defined in gui_main.c */
extern Object *App;

/* global hook, use it for easy notifications */
extern struct Hook hook_standard;

/* initialized the global notify hook */
void init_hook_standard(void);

/* Use this function if the data field of the hook structure is not needed */
void init_hook(struct Hook *h, unsigned long (*func)(void));
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

#endif /* SM_MUISTUFF_H */


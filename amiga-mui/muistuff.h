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

/* useful MUI supports */
LONG xget(Object * obj, ULONG attribute);
ULONG DoSuperNew(struct IClass *cl, Object * obj, ULONG tag1,...);
Object *MakeLabel(STRPTR str);
Object *MakeButton(STRPTR str);
Object *MakeCheck(STRPTR label, ULONG check);
Object *MakeCycle(STRPTR label, STRPTR * array);
VOID DisposeAllChilds(Object *o);

/* global application object, defined in gui_main.c */
extern Object *App;

/* global hook, use it for easy notifications */
extern struct Hook hook_standard;

/* initialized the global notify hook */
void init_hook_standard(void);

/* An extented hook structure which also stores the a4 register
** Use init_myhook() to initalize it.
*/

struct MyHook
{
	struct Hook hook;
	unsigned long rega4;
};

void init_myhook(struct MyHook *h, unsigned long (*func)(),void *data);

/* Use this function if the data field of the hook structure is not needed */
void init_hook(struct Hook *h, unsigned long (*func)());

/* Line Macros */
#define HorizLineObject  RectangleObject,MUIA_VertWeight,0,MUIA_Rectangle_HBar, TRUE,End
#define HorizLineTextObject(text)  RectangleObject,MUIA_VertWeight,0,MUIA_Rectangle_HBar, TRUE,MUIA_Rectangle_BarTitle,text,End

/* undocumented */
#define MUIM_GoActive                0x8042491a
#define MUIM_GoInactive              0x80422c0c

#define MUIA_Dtpic_Name 0x80423d72
/*#define MUIC_Dtpic "Dtpic.mui",*/
#define DtpicObject MUI_NewObject("Dtpic.mui"

#define MAKECOLOR32(x) (((x)<<24)|((x)<<16)|((x)<<8)|(x))

#endif

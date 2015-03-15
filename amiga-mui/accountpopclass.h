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
** accountpopclass.h
*/

#ifndef SM__ACCOUNTPOPCLASS_H
#define SM__ACCOUNTPOPCLASS_H

#include <exec/types.h>
#include <libraries/mui.h>

/* the objects of the listview */
IMPORT struct MUI_CustomClass *CL_AccountPop;
#define AccountPopObject (Object*)MyNewObject(CL_AccountPop->mcc_Class, NULL

#define MUIA_AccountPop_Account         (TAG_USER | 0x30030001) /* struct account * */
#define MUIA_AccountPop_HasDefaultEntry (TAG_USER | 0x30030002)

#define MUIM_AccountPop_Refresh         (TAG_USER | 0x30030101)
struct  MUIP_AccountPop_Refresh         {ULONG MethodID;};

int create_accountpop_class(void);
void delete_accountpop_class(void);

#endif

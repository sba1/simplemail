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
** filterruleclass.h
*/

#ifndef SM__FILTERRULECLASS_H
#define SM__FILTERRULECLASS_H

IMPORT struct MUI_CustomClass *CL_FilterRule;
#define FilterRuleObject (Object*)MyNewObject(CL_FilterRule->mcc_Class, NULL

#define MUIA_FilterRule_Data (TAG_USER | 0x30120001) /* I.. struct filter_rule * */
#define MUIA_FilterRule_Type (TAG_USER | 0x30120002) /* I.. LONG */

int create_filterrule_class(void);
void delete_filterrule_class(void);

#endif  /* SM__FILTERRULECLASS_H */

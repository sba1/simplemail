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
** signaturecycleclass.h
*/

#ifndef SM__SIGNATURECYCLECLASS_H
#define SM__SIGNATURECYCLECLASS_H

IMPORT struct MUI_CustomClass *CL_SignatureCycle;
#define SignatureCycleObject (Object*)MyNewObject(CL_SignatureCycle->mcc_Class, NULL

/* a default entry will be shown */
#define MUIA_SignatureCycle_HasDefaultEntry (TAG_USER | 0x30130001) /* I.. BOOL */
/* pointer to an SignatureList, defaults to user.config.signature_list */
#define MUIA_SignatureCycle_SignatureList   (TAG_USER | 0x30130002) /* I.. struct list * */

/* special value for setting the Entries */
#define MUIV_SignatureCycle_Default      -10
#define MUIV_SignatureCycle_NoSignature  -11

int create_signaturecycle_class(void);
void delete_signaturecycle_class(void);

#endif

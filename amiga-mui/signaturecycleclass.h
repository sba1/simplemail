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

#include "folder.h"

IMPORT struct MUI_CustomClass *CL_SignatureCycle;
#define SignatureCycleObject (Object*)MyNewObject(CL_SignatureCycle->mcc_Class, NULL

/* a default entry will be shown */
#define MUIA_SignatureCycle_HasDefaultEntry (TAG_USER | 0x30130001) /* I.. BOOL */
/* pointer to an SignatureList, defaults to user.config.signature_list */
#define MUIA_SignatureCycle_SignatureList   (TAG_USER | 0x30130002) /* I.. struct list * */
/* pointer to an SignatureName (UTF8 String)*/
#define MUIA_SignatureCycle_SignatureName   (TAG_USER | 0x30130003) /* ISG char * */
#define MUIA_SignatureCycle_Active          (TAG_USER | 0x30130004) /* ISG LONG */

#define MUIM_SignatureCycle_Refresh         (TAG_USER | 0x30130101)
struct  MUIP_SignatureCycle_Refresh         { ULONG MethodID; struct list *signature_list; };

/* special value for setting the Entries */
#define MUIV_SignatureCycle_Default      FOLDER_SIGNATURE_DEFAULT
#define MUIV_SignatureCycle_NoSignature  FOLDER_SIGNATURE_NO
#define MUIV_SignatureCycle_Active_Next  -1
#define MUIV_SignatureCycle_Active_Prev  -2

int create_signaturecycle_class(void);
void delete_signaturecycle_class(void);

#endif

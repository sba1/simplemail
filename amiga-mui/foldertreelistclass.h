#ifndef SM__FOLDERTREELISTCLASS_H
#define SM__FOLDERTREELISTCLASS_H

IMPORT struct MUI_CustomClass *CL_FolderTreelist;
#define FolderTreelistObject (Object*)NewObject(CL_FolderTreelist->mcc_Class, NULL

#define MUIA_FolderTreelist_MailDrop (TAG_USER+0x31000001) /* (struct folder *) */

int create_foldertreelist_class(void);
void delete_foldertreelist_class(void);


#endif

/*
** transwndclass.h
*/

#ifndef SM__TRANSWNDCLASS_H
#define SM__TRANSWNDCLASS_H

extern struct MUI_CustomClass *CL_transwnd;

#define transwndObject (Object *) NewObject(CL_transwnd->mcc_Class, NULL

int create_transwnd_class(void);
void delete_transwnd_class(void);

#define MUIA_transwnd_Status      (TAG_USER | (116456070 << 16) | 0x0002)
#define MUIA_transwnd_Gauge1_Str  (TAG_USER | (116456070 << 16) | 0x0003)
#define MUIA_transwnd_Gauge1_Max  (TAG_USER | (116456070 << 16) | 0x0004)
#define MUIA_transwnd_Gauge1_Val  (TAG_USER | (116456070 << 16) | 0x0005)
#define MUIA_transwnd_Gauge2_Str  (TAG_USER | (116456070 << 16) | 0x0006)
#define MUIA_transwnd_Gauge2_Max  (TAG_USER | (116456070 << 16) | 0x0007)
#define MUIA_transwnd_Gauge2_Val  (TAG_USER | (116456070 << 16) | 0x0008)
#define MUIA_transwnd_Aborted     (TAG_USER | (116456070 << 16) | 0x0009) /* N */

#endif
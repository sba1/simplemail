/*
** transwndclass.h
*/

#ifndef SM__TRANSWNDCLASS_H
#define SM__TRANSWNDCLASS_H

int create_transwnd_class(void);
void delete_transwnd_class(void);

#define MUIA_transwnd_WinTitle (TAG_USER | (116456070 << 16) | 0x0000)
#define MUIA_transwnd_Status   (TAG_USER | (116456070 << 16) | 0x0001)
#define MUIA_transwnd_Gauge1   (TAG_USER | (116456070 << 16) | 0x0002)
#define MUIA_transwnd_Gauge2   (TAG_USER | (116456070 << 16) | 0x0003)

#endif
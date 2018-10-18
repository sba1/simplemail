#ifndef SM__HOOKENTRY_H
#define SM__HOOKENTRY_H

#ifdef __AMIGAOS4__

extern
#ifdef __cplusplus
"C"
#endif
ULONG hookEntry(void);

extern
#ifdef __cplusplus
"C"
#endif
ULONG muiDispatcherEntry(void);

#else
#if defined(__MORPHOS__) || defined(__AROS__)
#define hookEntry HookEntry
#endif
#endif


#endif

/* NOTE: This class is private!! The interface will change in
 * the future! It's actually not well designed. Only SimpleMail
 * should use it, for now!
 */

#ifndef MUI_SIMPLEHTML_MCC_H
#define MUI_SIMPLEHTML_MCC_H

#ifndef __AMIGSOS4__
extern struct Library *SimpleHTMLBase;
struct MUI_CustomClass *ObtainSimpleHTMLMCC(void);
#ifdef __SASC
#pragma libcall SimpleHTMLBase ObtainSimpleHTMLMCC 1e 00
#endif

#else
#endif

#define SimpleHTMLObject (Object*)(NewObject)(ObtainSimpleHTMLMCC()->mcc_Class, NULL

#define MUIA_SimpleHTML_Buffer					(TAG_USER+0x31200000) /* NS. */
#define MUIA_SimpleHTML_BufferLen			(TAG_USER+0x31200001) /* NS. */
#define MUIA_SimpleHTML_HorizScrollbar	(TAG_USER+0x31200002) /* NS. */
#define MUIA_SimpleHTML_VertScrollbar	(TAG_USER+0x31200003) /* NS. */
#define MUIA_SimpleHTML_Priv1					(TAG_USER+0x31200004)
#define MUIA_SimpleHTML_Priv2					(TAG_USER+0x31200005)


/* The Hook is called with the hook itself in A0,
 * the object in register A2 and a struct MUIP_SimpleHTML_LoadHook * in A1
 * The hook should return 0 if the loading failed (e.g. unsupported transfer protocol)
 * else another value. In this case the data should be in buffer and buffer_len
 */
#define MUIA_SimpleHTML_LoadHook				(TAG_USER+0x31200006)

/* The URI which was clicked */
#define MUIA_SimpleHTML_URIClicked			(TAG_USER+0x31230007) /* ..GN */

#define MUIM_SimpleHTML_AllocateMem		(0x456789)
#define MUIM_SimpleHTML_AppendBuffer    (0x45678a)
#define MUIM_SimpleHTML_FontSubst			(0x45678b)

struct MUIP_SimpleHTML_LoadHook
{
	STRPTR uri; /* the uri which should be loaded */

	APTR buffer; /* this should be modified by the hook */
							 /* it must be allocated with the MUIM_SimpleHTML_AllocateMem function */
							 /* Never access the buffer after the hook finished! */
	ULONG buffer_len; /* this too */
};

struct MUIP_SimpleHTML_AllocateMem
{
	ULONG MethodID;
	ULONG Size;
};

struct MUIP_SimpleHTML_AppendBuffer
{
	ULONG MethodID;
	STRPTR Buffer; /* the buffer which should be appended */
	ULONG BufferLen; /* the length of the buffer */
};

struct MUIP_SimpleHTML_FontSubst
{
	ULONG MethodID;
	STRPTR name;
	LONG size; /* 1 - 7 */
	STRPTR newname; /* new font name (without .font) */
	LONG newsize; /* the new size (in pixels) */
};

#endif

/* NOTE: This class is private!! The interface will change in
 * the future! It's actually not well designed. Only SimpleMail
 * should use it, for now!
 */

#ifndef MUI_SIMPLEHTML_MCC_H
#define MUI_SIMPLEHTML_MCC_H

#ifndef __AMIGAOS4__
extern struct Library *SimpleHTMLBase;
struct MUI_CustomClass *ObtainSimpleHTMLMCC(void);
#ifdef __SASC
#pragma libcall SimpleHTMLBase ObtainSimpleHTMLMCC 1e 00
#else
#if defined(__MORPHOS__) && defined(__GNUC__)
#include <ppcinline/macros.h>
#define ObtainSimpleHTMLMCC() \
	LP0(0x1e, struct MUI_CustomClass *, ObtainSimpleHTMLMCC, \
		, SimpleHTMLBase, 0, 0, 0, 0, 0, 0)
#endif
#endif
#else

#ifndef EXEC_INTERFACES_H
#include <exec/interfaces.h>
#endif

#ifndef EXEC_EMULATION_H
#include <exec/emulation.h>
#endif

struct SimpleHTMLIFace
{
        struct InterfaceData Data;

        ULONG APICALL (*Obtain)(struct SimpleHTMLIFace *Self);
        ULONG APICALL (*Release)(struct SimpleHTMLIFace *Self);
        void APICALL (*Expunge)(struct SimpleHTMLIFace *Self);
        struct Interface * APICALL (*Clone)(struct SimpleHTMLIFace *Self);
        struct MUI_CustomClass * APICALL (*ObtainSimpleHTMLMCC)(struct SimpleHTMLIFace *Self);
};

extern struct SimpleHTMLIFace *ISimpleHTML;
extern struct Library *SimpleHTMLBase;

#define ObtainSimpleHTMLMCC() ({ \
	struct MUI_CustomClass * _ret; \
	if (ISimpleHTML != NULL) { \
		_ret = ISimpleHTML->ObtainSimpleHTMLMCC(); \
	} else { \
		_ret = (struct MUI_CustomClass*)EmulateTags(SimpleHTMLBase, \
			ET_Offset, -30, \
			ET_RegisterA6, SimpleHTMLBase, \
			ET_SaveRegs, TRUE, \
			TAG_DONE); \
	} \
	_ret; \
})

#endif

#define SimpleHTMLObject (Object*)(NewObject)(ObtainSimpleHTMLMCC()->mcc_Class, NULL

#define MUIA_SimpleHTML_Buffer           (TAG_USER+0x31200000) /* IS. */
#define MUIA_SimpleHTML_BufferLen        (TAG_USER+0x31200001) /* IS. */
#define MUIA_SimpleHTML_HorizScrollbar   (TAG_USER+0x31200002) /* IS. */
#define MUIA_SimpleHTML_VertScrollbar    (TAG_USER+0x31200003) /* IS. */
#define MUIA_SimpleHTML_TopHoriz         (TAG_USER+0x31200004) /* ISGN */
#define MUIA_SimpleHTML_TopVert          (TAG_USER+0x31200005) /* ISGN */

/* The Hook is called with the hook itself in A0,
 * the object in register A2 and a struct MUIP_SimpleHTML_LoadHook * in A1
 * The hook should return 0 if the loading failed (e.g. unsupported transfer protocol)
 * else another value. In this case the data should be in buffer and buffer_len
 */
#define MUIA_SimpleHTML_LoadHook         (TAG_USER+0x31200006) /* ISG. */

/* The URI which was clicked */
#define MUIA_SimpleHTML_URIClicked       (TAG_USER+0x31200007) /* ..GN */

#define MUIA_SimpleHTML_VisibleHoriz      (TAG_USER+0x31200008) /* ..GN */
#define MUIA_SimpleHTML_VisibleVert       (TAG_USER+0x31200009) /* ..GN */
#define MUIA_SimpleHTML_TotalHoriz        (TAG_USER+0x3120000a) /* ..GN */
#define MUIA_SimpleHTML_TotalVert         (TAG_USER+0x3120000b) /* ..GN */
#define MUIA_SimpleHTML_ProvideScrollers 	(TAG_USER+0x3120000c) /* I... */

/* The URI on which is the mouse is over */
#define MUIA_SimpleHTML_URIOver						(TAG_USER+0x3120000c) /* ..GN */

/* Methods */
#define MUIM_SimpleHTML_AllocateMem				(0x456789)
#define MUIM_SimpleHTML_AppendBuffer			(0x45678a)
#define MUIM_SimpleHTML_FontSubst					(0x45678b)
#define MUIM_SimpleHTML_Private1					(0x45678c)
#define MUIM_SimpleHTML_GetLinkOnPosition (0x45678d)

#if defined(__GNUC__)
# pragma pack(2)
#endif

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

/* Returns the link (as string) on the given position */
struct MUIP_SimpleHTML_GetLinkOnPosition
{
	ULONG MethodID;
	LONG x,y; /* relative to the window */
};

#if defined(__GNUC__)
# pragma pack()
#endif

#endif


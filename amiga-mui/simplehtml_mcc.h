/* NOTE: This class is private!! The interface will change in
 * the future! It's actually not well designed. Only SimpleMail
 * should use it, for now!
 */

#ifndef MUI_SIMPLEHTML_MCC_H
#define MUI_SIMPLEHTML_MCC_H

extern struct MUI_CustomClass *CL_SimpleHTML;

__regargs int create_simplehtml_class(void);
__regargs void delete_simplehtml_class(void);

#define SimpleHTMLObject (Object*)NewObject(CL_SimpleHTML->mcc_Class, NULL

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

#define MUIM_SimpleHTML_AllocateMem		(0x456789)

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

#endif

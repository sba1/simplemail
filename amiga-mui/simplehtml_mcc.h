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

#endif

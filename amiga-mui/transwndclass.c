/*
** transwndclass.c
*/

#include <string.h>
#include <stdio.h>

#include <libraries/mui.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <libraries/iffparse.h> /* MAKE_ID */

#include "compiler.h"
#include "muistuff.h"

#include "transwndclass.h"

struct transwnd_Data
{
	Object *win;
	Object *gauge1, *gauge2, *status;
};

STATIC ASM SAVEDS ULONG transwnd_New(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	ULONG rc;
	struct transwnd_Data *data;
	
	rc = FALSE;
	
	obj = (Object *) DoSuperMethodA(cl, obj, msg);
	if(obj != NULL)
	{
		data = (struct transwnd_Data *) INST_DATA(cl, obj);
	
		data->win = WindowObject,
			MUIA_Window_ID,	MAKE_ID('T','E','S','T'),
			WindowContents, VGroup,
    	    	Child, data->gauge1 = GaugeObject,
	        		GaugeFrame,
        			MUIA_Gauge_InfoText,		"Mail 0/0",
        			MUIA_Gauge_Horiz,			TRUE,
        		End,
        	
    	    	Child, data->gauge2 = GaugeObject,
	        		GaugeFrame,
        			MUIA_Gauge_InfoText,		"0/0 bytes",
        			MUIA_Gauge_Horiz,			TRUE,
        		End,
        	
    	    	Child, data->status = TextObject,
 	   	    		TextFrame,
   	    	 	End,	
        	End,	
		End;
		
		if(data->win != NULL)
		{
			DoMethod(App, OM_ADDMEMBER, data->win);
			set(data->win, MUIA_Window_Open, TRUE);

			rc = TRUE;
		}
	}
	
	return(rc);
}

STATIC ASM SAVEDS ULONG transwnd_Dispose(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	ULONG rc;
	struct transwnd_Data *data;
	
	rc = FALSE;
	
	obj = (Object *) DoSuperMethodA(cl, obj, msg);
	if(obj != NULL)
	{
		data = (struct transwnd_Data *) INST_DATA(cl, obj);
		set(data->win, MUIA_Window_Open, FALSE);
		
		DoMethod(App, OM_REMMEMBER, data->win);
		MUI_DisposeObject(obj);
		
		rc = TRUE;
	}
	
	return(rc);	
}

STATIC ASM SAVEDS ULONG transwnd_Set(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	struct transwnd_Data *data;
	struct TagItem *tags, *tag;
	
	data = (struct transwnd_Data *) INST_DATA(cl, obj);
		
	for(tags = ((struct opSet *) msg)->ops_AttrList; tag = NextTagItem(&tags);)
	{
		switch (tag->ti_Tag)
		{
			case MUIA_transwnd_Open:
				set(data->win, MUIA_Window_Open, tag->ti_Data);
				MUI_Redraw(obj,MADF_DRAWOBJECT);
				break;
				
			case MUIA_transwnd_WinTitle:	
				set(data->win, MUIA_Window_Title, tag->ti_Data);
				MUI_Redraw(obj,MADF_DRAWOBJECT);
				break;
				
			case MUIA_transwnd_Status:	
				set(data->win, MUIA_Text_Contents, tag->ti_Data);
				MUI_Redraw(obj,MADF_DRAWOBJECT);
				break;
				
			case MUIA_transwnd_Gauge1_Str:	
				set(data->gauge1, MUIA_Gauge_InfoText, tag->ti_Data);
				MUI_Redraw(obj,MADF_DRAWOBJECT);
				break;
				
			case MUIA_transwnd_Gauge1_Max:
				set(data->gauge1, MUIA_Gauge_Max, tag->ti_Data);
				MUI_Redraw(obj,MADF_DRAWOBJECT);
				break;
				
			case MUIA_transwnd_Gauge1_Val:	
				set(data->gauge1, MUIA_Gauge_Current, tag->ti_Data);
				MUI_Redraw(obj,MADF_DRAWOBJECT);
				break;
				
			case MUIA_transwnd_Gauge2_Str:	
				set(data->gauge2, MUIA_Gauge_InfoText, tag->ti_Data);
				MUI_Redraw(obj,MADF_DRAWOBJECT);
				break;
				
			case MUIA_transwnd_Gauge2_Max:
				set(data->gauge2, MUIA_Gauge_Max, tag->ti_Data);
				MUI_Redraw(obj,MADF_DRAWOBJECT);
				break;
				
			case MUIA_transwnd_Gauge2_Val:	
				set(data->gauge2, MUIA_Gauge_Current, tag->ti_Data);
				MUI_Redraw(obj,MADF_DRAWOBJECT);
				break;	
		}
	}
	
	return(DoSuperMethodA(cl, obj, msg));
}

STATIC ASM SAVEDS ULONG transwnd_Get(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	ULONG rc;
	
	rc = FALSE;
	
	obj = (Object *) DoSuperMethodA(cl, obj, msg);
	if(obj != NULL)
	{
		rc = TRUE;
	}
	
	return(rc);
}

STATIC ASM SAVEDS ULONG transwnd_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	switch(msg->MethodID)
	{
		case OM_NEW		: return(transwnd_New		(cl, obj, (APTR) msg));
		case OM_DISPOSE	: return(transwnd_Dispose	(cl, obj, (APTR) msg));
		case OM_SET		: return(transwnd_Set		(cl, obj, (APTR) msg));
	}
	
	return(DoSuperMethodA(cl, obj, msg));
}

struct MUI_CustomClass *CL_transwnd;

int create_transwnd_class(VOID)
{
	int rc;
	
	rc = FALSE;
	
	CL_transwnd = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct transwnd_Data), transwnd_Dispatcher);
	if(CL_transwnd != NULL)
	{
		rc = TRUE;
	}
	
	return(rc);
}

VOID delete_transwnd_class(VOID)
{
	if(CL_transwnd != NULL)
	{
		MUI_DeleteCustomClass(CL_transwnd);
	}	
}
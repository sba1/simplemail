/*
** transwndclass.c
*/

#include <dos.h>
#include <string.h>
#include <stdio.h>

#include <libraries/mui.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <libraries/iffparse.h> /* MAKE_ID */

#include "compiler.h"
#include "muistuff.h"

#include "transwndclass.h"

#define MUIV_transwnd_Aborted 2

struct transwnd_Data
{
	Object *gauge1, *gauge2, *status, *abort;
};

STATIC ULONG transwnd_New(struct IClass *cl, Object *obj, struct opSet *msg)
{
	Object *gauge1,*gauge2,*status,*abort;
	
	obj = (Object *) DoSuperNew(cl, obj,
				WindowContents, VGroup,
					Child, gauge1 = GaugeObject,
	        			GaugeFrame,
		        		MUIA_Gauge_InfoText,		"Mail 0/0",
       			 		MUIA_Gauge_Horiz,			TRUE,
	        		End,
    			    Child, gauge2 = GaugeObject,
	        			GaugeFrame,
			        	MUIA_Gauge_InfoText,		"0/0 bytes",
		        		MUIA_Gauge_Horiz,			TRUE,
	        		End,
    			    Child, status = TextObject,
						TextFrame,
   	    			End,
   	    			Child, abort = MakeButton("_Abort"),
				End,	
				TAG_MORE, msg->ops_AttrList,
				TAG_DONE);

	if (obj != NULL)
	{
		struct transwnd_Data *data = (struct transwnd_Data *) INST_DATA(cl, obj);
		data->gauge1 = gauge1;
		data->gauge2 = gauge2;
		data->status = status;
		data->abort  = abort;
		DoMethod(abort, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_transwnd_Aborted);
	}

	return((ULONG) obj);
}

STATIC VOID transwnd_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	DoSuperMethodA(cl, obj, msg);
}

STATIC ULONG transwnd_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct transwnd_Data *data;
	struct TagItem *tags, *tag;
	
	data = (struct transwnd_Data *) INST_DATA(cl, obj);
		
	for (tags = msg->ops_AttrList; tag = NextTagItem(&tags);)
	{
		switch (tag->ti_Tag)
		{
			case MUIA_transwnd_Status:	
				set(data->status, MUIA_Text_Contents, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge1_Str:	
				set(data->gauge1, MUIA_Gauge_InfoText, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge1_Max:
				set(data->gauge1, MUIA_Gauge_Max, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge1_Val:	
				set(data->gauge1, MUIA_Gauge_Current, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge2_Str:	
				set(data->gauge2, MUIA_Gauge_InfoText, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge2_Max:
				set(data->gauge2, MUIA_Gauge_Max, tag->ti_Data);
				break;
				
			case MUIA_transwnd_Gauge2_Val:	
				set(data->gauge2, MUIA_Gauge_Current, tag->ti_Data);
				break;	
		}
	}
	
	return(DoSuperMethodA(cl, obj, (Msg)msg));
}

STATIC ULONG transwnd_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	ULONG *store = ((struct opGet *)msg)->opg_Storage;
	
	switch (msg->opg_AttrID)
	{
		case MUIA_transwnd_Aborted:
			*store = (ULONG) (DoMethod(App, MUIM_Application_NewInput, 0) == MUIV_transwnd_Aborted);
			return(TRUE);
		break;
	}

	return(DoSuperMethodA(cl, obj, (Msg)msg));
}

STATIC ASM ULONG transwnd_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case OM_NEW: return(transwnd_New		(cl, obj, (struct opSet*) msg));
		case OM_DISPOSE: transwnd_Dispose	(cl, obj,  msg);return 0;
		case OM_SET: return(transwnd_Set		(cl, obj, (struct opSet*) msg));
		case OM_GET: return(transwnd_Get		(cl, obj, (struct opGet*) msg));
	}
	
	return(DoSuperMethodA(cl, obj, msg));
}

struct MUI_CustomClass *CL_transwnd;

int create_transwnd_class(VOID)
{
	int rc;
	
	rc = FALSE;
	
	CL_transwnd = MUI_CreateCustomClass(NULL, MUIC_Window, NULL, sizeof(struct transwnd_Data), transwnd_Dispatcher);
	if(CL_transwnd != NULL)
	{
		CL_transwnd->mcc_Class->cl_UserData = getreg(REG_A4);
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
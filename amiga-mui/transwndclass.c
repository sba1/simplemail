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

#include "compiler.h"
#include "muistuff.h"

#include "transwndclass.h"

STATIC ASM SAVEDS ULONG transwnd_New(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	
}

STATIC ASM SAVEDS ULONG transwnd_Dispose(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	
}

STATIC ASM SAVEDS ULONG transwnd_AskMinMax(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	
}

STATIC ASM SAVEDS ULONG transwnd_Draw(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	
}

STATIC ASM SAVEDS ULONG transwnd_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	switch(msg->MethodID)
	{
		case OM_NEW        : return(transwnd_New      (cl,obj,(APTR)msg));
		case OM_DISPOSE    : return(transwnd_Dispose  (cl,obj,(APTR)msg));
		case MUIM_AskMinMax: return(transwnd_AskMinMax(cl,obj,(APTR)msg));
		case MUIM_Draw     : return(transwnd_Draw     (cl,obj,(APTR)msg));
	}
	
	return(DoSuperMethodA(cl, obj, msg));
}
/***************************************************************************
 SimpleMail - Copyright (C) 2000 Hynek Schlawack and Sebastian Bauer

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
***************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <intuition/intuitionbase.h>
#include <workbench/icon.h>
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <mui/BetterString_mcc.h>
#include <mui/Mailtext_mcc.h>
#include <mui/NListview_mcc.h>
#include <aros/debug.h>
#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/wb.h>
#include <proto/icon.h>

#include "addressbook.h"
#include "configuration.h"
#include "debug.h"
#include "filter.h"
#include "mail.h"
#include "simplemail.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"
#include "text2html.h"
#include "folder.h";
#include "imap.h"
#include "addressbookwnd.h"
#include "amigasupport.h"
#include "compiler.h"
#include "mailinfoclass.h"
#include "muistuff.h"
#include "multistringclass.h"
#include "mailtextview.h"





/**************************************************************************/

struct MessageView_Data
{
	Object *mailinfo;
	Object *display_group;
	Object *mailtext;
	Object *horiz;
	Object *vert;
/*	Object *scroll_button;*/
	Object *mailto_contextmenu;

	int show;

	struct Hook load_hook;

	char *folder_path;
	struct mail_info *ref_mail; /* The mail's reference */

	struct mail_complete *mail; /* The currently displayed mail (instance created within this class) */

	struct FileRequester *file_req; /* Filerequester for saving files */

	int horizbar_visible;

	struct MUI_EventHandlerNode ehnode_mousemove;
};



/******************************************************************
 Show the given mail, which should be a root mail
*******************************************************************/
static void messageview_show_mail(struct MessageView_Data *data)
{
	char *buf;
	char *buf_end;
	struct mail_complete *mail;
	struct codeset *codeset;
	codeset = codesets_find(NULL);
	/* Find out the mail part which should be displayed */
	if (!(mail = mail_find_initial(data->mail)))
		return;


	/* non text types cannot be displayed yet */
	if (mystricmp(mail->content_type,"text"))
		return;

	if (!mail->text) return;
		mail_decode(mail);

	if (mail->decoded_data)
	{
	  
		buf = mail->decoded_data;
		buf_end = buf + mail->decoded_len;
	} else
	{
	   
		buf = mail->text + mail->text_begin;
		buf_end = buf + mail->text_len;
	}
	
	char *tmpbuf;
	tmpbuf = utf8tostrcreate(mail->decoded_data,codeset);
		SetAttrs(data->mailtext,
			 MUIA_Mailtext_Text,tmpbuf
				,
				TAG_DONE);
				
			
}

/******************************************************************
 Argument mail and folder_path can be NULL. In eighter case
 a empty text is displayed.
*******************************************************************/


/******************************************************************
 Cleanups temporary files created for the mail. Returns 1 if can
 continue.
*******************************************************************/
static int messageview_cleanup_temporary_files(struct MessageView_Data *data)
{
  struct ExAllControl *eac;
  BPTR dirlock;
  char filename[100];
  int rc = 1;
  struct mail_complete *mail;

  if (!(mail = data->mail))
    return 1;

  strcpy(filename,"T:");
  mystrlcpy(&filename[2],mail->info->filename,sizeof(filename));
  dirlock = Lock(filename,ACCESS_READ);
  if (!dirlock) return 1;

  if ((eac = (struct ExAllControl *)AllocDosObject(DOS_EXALLCONTROL,NULL)))
    {
      APTR EAData = AllocVec(1024,0);
      if (EAData)
	{
	  int more;
	  BPTR olddir = CurrentDir(dirlock);

	  eac->eac_LastKey = 0;
	  do
	    {
	      struct ExAllData *ead;
	      more = ExAll(dirlock, EAData, 1024, ED_NAME, eac);
	      if ((!more) && (IoErr() != ERROR_NO_MORE_ENTRIES)) break;
	      if (eac->eac_Entries == 0) continue;

	      ead = (struct ExAllData *)EAData;
	            do
		      {
			DeleteFile(ead->ed_Name); /* TODO: Check the result */
			ead = ead->ed_Next;
		      } while (ead);
	    } while (more);
	  CurrentDir(olddir);
	  FreeVec(EAData);
	}
      FreeDosObject(DOS_EXALLCONTROL,eac);
    }
  UnLock(dirlock);
  DeleteFile(filename);
  return rc;
}




static int messageview_setup(struct MessageView_Data *data, struct mail_info *mail, char *folder_path)
{
	int rc = 0;
	BPTR lock;

	/* not specifing a mail is accepted */
	if (!mail || !folder_path)
	{
		char text[256];

		return 1;
	}

	set(App, MUIA_Application_Sleep, TRUE);

	if ((lock = Lock(folder_path,ACCESS_READ))) /* maybe it's better to use an absoulte path here */
	{
		BPTR old_dir = CurrentDir(lock);

		if ((data->mail = mail_complete_create_from_file(mail->filename)))
		{
		  set(data->mailinfo, MUIA_MailInfo_MailInfo, data->mail->info);
			mail_read_contents(NULL,data->mail); /* already cd'ed in */
			messageview_show_mail(data);
			rc = 1;
		}

		CurrentDir(old_dir);
		UnLock(lock);
	}

	set(App, MUIA_Application_Sleep, FALSE);
	return rc;
}

/******************************************************************
 OM_NEW
*******************************************************************/
STATIC ULONG MessageView_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct MessageView_Data *data;
	Object *lv,*mailtext, *horiz, *vert, *mailinfo, *display_group;/*, *scroll_button;*/
	Object *addressbook_menuitem, *writeto_menuitem;
	struct TagItem *oid_tag;

	/* Filter out MUIA_ObjectID tag as this is used for the switch_button */
	if ((oid_tag = FindTagItem(MUIA_ObjectID, msg->ops_AttrList)))
		oid_tag->ti_Tag = TAG_IGNORE;
	if (!(obj=(Object *)DoSuperNew(cl,obj,
		MUIA_Group_Spacing, 0,
		MUIA_Group_Horiz, FALSE,
				       		       Child, mailinfo = MailInfoObject,
					TextFrame,
					MUIA_InnerLeft, 0,
					MUIA_InnerTop, 0,
					MUIA_InnerRight, 0,
					MUIA_InnerBottom, 0,
					oid_tag?MUIA_ObjectID:TAG_IGNORE, oid_tag?oid_tag->ti_Tag:0,
					End,
		Child, display_group = HGroup,
				       	MUIA_Group_Spacing, 0,
			Child, VGroup,
			MUIA_Group_Spacing, 0,
				       
				       Child, lv  = NListviewObject,
				       MUIA_NListview_NList, mailtext = MailtextObject,
				       MUIA_Mailtext_ForbidContextMenu, FALSE,
				       MUIA_Font,                       MUIV_Font_Fixed,
				       MUIA_Frame,                      MUIV_Frame_InputList,
				       MUIA_NList_Input,                FALSE,
				       MUIA_NList_MultiSelect,          FALSE,

				       End,
				       MUIA_CycleChain, TRUE,
				       /* Child,       
				       TextFrame,
					MUIA_InnerLeft, 0,
					MUIA_InnerTop, 0,
					MUIA_InnerRight, 0,
					MUIA_InnerBottom, 0,*/
				       	End,
				       
				       	Child, horiz = ScrollbarObject, MUIA_ShowMe, FALSE, MUIA_Group_Horiz, TRUE, End,
				End,
				       /*Child, vert = ScrollbarObject, End,*/
			
				       End,
				       TAG_MORE,msg->ops_AttrList)))
	{
		return 0;
	}
	data = (struct MessageView_Data*)INST_DATA(cl,obj);
	if (!(data->file_req = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_DoSaveMode, TRUE, TAG_DONE)))
	{
		CoerceMethod(cl,obj,OM_DISPOSE);
		return 0;
	}

	data->mailto_contextmenu = MenustripObject,
		Child, MenuObjectT(_("Address")),
			Child, writeto_menuitem = MenuitemObject, MUIA_Menuitem_Title, _("Write to..."), MUIA_UserData, 1, End,
			Child, addressbook_menuitem = MenuitemObject, MUIA_Menuitem_Title, _("Add to addressbook..."), MUIA_UserData, 2, End,
			End,
		End;
	/*
		DoMethod(mailtext, MUIM_Notify, MUIA_ContextMenuTrigger, (ULONG)writeto_menuitem, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)messageview_writeto, (ULONG)data);
	DoMethod(mailtext, MUIM_Notify, MUIA_ContextMenuTrigger, (ULONG)addressbook_menuitem, (ULONG)App, 4, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)messageview_add, (ULONG)data);
	*/
	data->mailtext = mailtext;
	data->display_group = display_group;
	data->horiz = horiz;
	data->vert = vert;
	data->mailinfo = mailinfo;
/*	data->scroll_button = scroll_button;*/

/*	init_hook_with_data(&data->load_hook, (HOOKFUNC)simplehtml_load_function, data);

	SetAttrs(data->simplehtml,
			MUIA_SimpleHTML_HorizScrollbar, horiz,
			MUIA_SimpleHTML_VertScrollbar, vert,
			MUIA_SimpleHTML_LoadHook, &data->load_hook,
			TAG_DONE);

	DoMethod(data->simplehtml, MUIM_Notify, MUIA_SimpleHTML_URIClicked, MUIV_EveryTime, (ULONG)App, 5, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)messageview_uri_clicked, (ULONG)data, MUIV_TriggerValue);
	DoMethod(data->simplehtml, MUIM_Notify, MUIA_SimpleHTML_URIOver, MUIV_EveryTime, (ULONG)App, 6, MUIM_CallHook, (ULONG)&hook_standard, (ULONG)messageview_uri_over, (ULONG)data, MUIV_TriggerValue, (ULONG)data->simplehtml);
	DoMethod(data->simplehtml, MUIM_Notify, MUIA_SimpleHTML_TotalHoriz, MUIV_EveryTime, (ULONG)App, 4, MUIM_Application_PushMethod, (ULONG)obj, 1, MUIM_MessageView_Changed);
	DoMethod(data->simplehtml, MUIM_Notify, MUIA_SimpleHTML_TotalVert, MUIV_EveryTime, (ULONG)App, 4, MUIM_Application_PushMethod, (ULONG)obj, 1, MUIM_MessageView_Changed);
*/
	return (ULONG)obj;
}

/******************************************************************
 OM_DISPOSE
*******************************************************************/
STATIC ULONG MessageView_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct MessageView_Data *data = (struct MessageView_Data*)INST_DATA(cl,obj);

	messageview_cleanup_temporary_files(data);
	mail_complete_free(data->mail); /* NULL safe */	
	if (data->ref_mail) mail_dereference(data->ref_mail);
	if (data->file_req) MUI_FreeAslRequest(data->file_req);
	if (data->mailto_contextmenu)
	{
		set(obj, MUIA_ContextMenu, NULL);
		MUI_DisposeObject(data->mailto_contextmenu);
	}

	return DoSuperMethodA(cl,obj,msg);
}

/******************************************************************
 OM_SET
*******************************************************************/
STATIC ULONG MessageView_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/******************************************************************
 OM_GET
*******************************************************************/
STATIC ULONG MessageView_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
/*	struct MessageView_Data *data = (struct MessageView_Data*)INST_DATA(cl,obj);*/
  switch (msg->opg_AttrID)
	{
		default:
					return DoSuperMethodA(cl,obj,(Msg)msg);
	}
}

/******************************************************************
 MUIM_Setup
*******************************************************************/
STATIC ULONG MessageView_Show(struct IClass *cl, Object *obj, struct MUIP_Show *msg)
{
	struct MessageView_Data *data = (struct MessageView_Data*)INST_DATA(cl,obj);
	ULONG rc;

	if ((rc = DoSuperMethodA(cl,obj,(Msg)msg)))
		data->show = 1;

	return rc;
}

/******************************************************************
 MUIM_Cleanup
*******************************************************************/
STATIC ULONG MessageView_Hide(struct IClass *cl, Object *obj, struct MUIP_Hide *msg)
{
	struct MessageView_Data *data = (struct MessageView_Data*)INST_DATA(cl,obj);

	data->show = 0;
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

/******************************************************************
 MUIM_MessageView_DisplayMail
*******************************************************************/
STATIC ULONG MessageView_DisplayMail(struct IClass *cl, Object *obj, struct MUIP_MessageView_DisplayMail *msg)
{
	struct MessageView_Data *data = (struct MessageView_Data*)INST_DATA(cl,obj);

	/* cleanup */
	messageview_cleanup_temporary_files(data);
	mail_complete_free(data->mail); /* NULL safe */
	data->mail = NULL;

	/* remove referencs */
	if (data->ref_mail) mail_dereference(data->ref_mail);
	data->ref_mail = NULL;
	free(data->folder_path);

	/* create resources */
	data->folder_path = mystrdup(msg->folder_path);
	if ((data->ref_mail = msg->mail))
		mail_reference(data->ref_mail);

	if (data->show)
	{
		if (!(messageview_setup(data,data->ref_mail,data->folder_path)))
			return 0;
	}

	return 1;
}


/**************************************************************************/

STATIC MY_BOOPSI_DISPATCHER(ULONG, MessageView_Dispatcher, cl, obj, msg)
{
	switch(msg->MethodID)
	{
		case	OM_NEW:				return MessageView_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE:		return MessageView_Dispose(cl,obj,msg);
		case	OM_SET:				return MessageView_Set(cl,obj,(struct opSet*)msg);
		case	OM_GET:				return MessageView_Get(cl,obj,(struct opGet*)msg);
		case	MUIM_Show:			return MessageView_Show(cl,obj,(struct MUIP_Show*)msg);
		case	MUIM_Hide:			return MessageView_Hide(cl,obj,(struct MUIP_Hide*)msg);
		case	MUIM_MessageView_DisplayMail: return MessageView_DisplayMail(cl,obj,(struct MUIP_MessageView_DisplayMail*)msg);
		  /*case	MUIM_MessageView_Changed: return MessageView_Changed(cl,obj,msg);*/
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

/**************************************************************************/

struct MUI_CustomClass *CL_MessageView;

int create_messageview_class(void)
{
	SM_ENTER;
	if ((CL_MessageView = CreateMCC(MUIC_Group,NULL,sizeof(struct MessageView_Data),MessageView_Dispatcher)))
	{
		SM_DEBUGF(15,("Create CL_MessageView: 0x%lx\n",CL_MessageView));
		SM_RETURN(1,"%ld");
	}
	SM_DEBUGF(5,("FAILED! Create CL_MessageView\n"));
	SM_RETURN(0,"%ld");
}

void delete_messageview_class(void)
{
	SM_ENTER;
	if (CL_MessageView)
	{
		if (MUI_DeleteCustomClass(CL_MessageView))
		{
			SM_DEBUGF(15,("Deleted CL_MessageView: 0x%lx\n",CL_MessageView));
			CL_MessageView = NULL;
		} else
		{
			SM_DEBUGF(5,("FAILED! Delete CL_MessageView: 0x%lx\n",CL_MessageView));
		}
	}
	SM_LEAVE;
}

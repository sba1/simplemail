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
#include <libraries/mui.h>
#include <mui/betterstring_mcc.h>

#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "configuration.h"
#include "debug.h"
#include "filter.h"
#include "mail.h"
#include "smintl.h"
#include "support_indep.h"
#include "text2html.h"

#include "compiler.h"
#include "muistuff.h"
#include "multistringclass.h"
#include "messageviewclass.h"
#include "simplehtml_mcc.h"

struct MessageView_Data
{
	Object *simplehtml;
	Object *horiz;
	Object *vert;
	int show;

	struct MyHook load_hook;

	char *folder_path;
	struct mail *ref_mail; /* The mail's reference */

	struct mail *mail; /* The currently displayed mail (instance created within this class) */
};

/******************************************************************
 SimpleHTML Load Hook. Returns 1 if uri can be loaded by the hook
 otherwise 0. -1 means reject this object totaly
*******************************************************************/
STATIC ASM SAVEDS LONG simplehtml_load_function(REG(a0,struct Hook *h), REG(a2, Object *obj), REG(a1,struct MUIP_SimpleHTML_LoadHook *msg))
{
	struct MessageView_Data *data = (struct MessageView_Data*)h->h_Data;
	char *uri = msg->uri;
	struct mail *mail;

	return -1;
#if 0
	if (!(mail = read_get_displayed_mail(data))) return -1;

	if (!mystrnicmp("http://",uri,7) && mail_allowed_to_download(data->mail))
	{
		void *buf;
		int buf_len;
		int rc = 0;

		if (http_download(uri,&buf,&buf_len))
		{
			if ((msg->buffer = (void*)DoMethod(data->html_simplehtml, MUIM_SimpleHTML_AllocateMem, buf_len)))
			{
				msg->buffer_len = buf_len;
				CopyMem(buf,msg->buffer,buf_len);
				rc = 1;
			}

			free(buf);
		}
		return rc;
	}

	if (!(mail = mail_find_compound_object(mail,uri))) return -1;
	mail_decode(mail);
	if (!mail->decoded_data || !mail->decoded_len) return -1;
	msg->buffer = (void*)DoMethod(data->html_simplehtml, MUIM_SimpleHTML_AllocateMem, mail->decoded_len);
	if (!msg->buffer) return 0;
	CopyMem(mail->decoded_data,msg->buffer,mail->decoded_len);
	msg->buffer_len = mail->decoded_len;
	return 1;
#endif
}

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
	struct mail *mail = data->mail;

	if (!data->mail) return 1;
	strcpy(filename,"T:");
	strcat(filename,mail->filename);
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

/******************************************************************
 Show the given mail, which should be a root mail
*******************************************************************/
static void messageview_show_mail(struct MessageView_Data *data)
{
	char *buf;
	char *buf_end;
	struct mail *mail;

	/* Find out the mail part which should be displayed */
	if (!(mail = mail_find_initial(data->mail)))
		return;

	/* non text types cannot be displayed yet */
	if (stricmp(mail->content_type,"text"))
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

	if (!stricmp(mail->content_subtype, "html"))
	{
		SetAttrs(data->simplehtml,
				MUIA_SimpleHTML_Buffer, buf,
				MUIA_SimpleHTML_BufferLen, buf_end - buf,
				TAG_DONE);
	} else
	{
		char *html_mail;
		char *font_buf;

		SetAttrs(data->simplehtml,
				MUIA_SimpleHTML_Buffer,data->mail->html_header,
				MUIA_SimpleHTML_BufferLen,strstr(data->mail->html_header,"</BODY></HTML>") - data->mail->html_header,
				TAG_DONE);

		if ((font_buf = mystrdup(user.config.read_fixedfont)))
		{
			char *end = strchr(font_buf,'/');
			if (end)
			{
				int size = atoi(end+1);
				*end = 0;

				DoMethod(data->simplehtml,MUIM_SimpleHTML_FontSubst,"fixedmail",3,font_buf,size);
			}
			free(font_buf);
		}

		if ((font_buf = mystrdup(user.config.read_propfont)))
		{
			char *end = strchr(font_buf,'/');
			if (end)
			{
				int size = atoi(end+1);
				*end = 0;

				DoMethod(data->simplehtml,MUIM_SimpleHTML_FontSubst,"normal",2,font_buf,size);
				DoMethod(data->simplehtml,MUIM_SimpleHTML_FontSubst,"normal",3,font_buf,size);
				DoMethod(data->simplehtml,MUIM_SimpleHTML_FontSubst,"normal",4,font_buf,size);
			}
			free(font_buf);
		}


		html_mail = text2html(buf, buf_end - buf,
													TEXT2HTML_ENDBODY_TAG|TEXT2HTML_FIXED_FONT|(user.config.read_wordwrap?0:TEXT2HTML_NOWRAP),"<FONT FACE=\"fixedmail\" SIZE=\"+1\">");

		DoMethod(data->simplehtml, MUIM_SimpleHTML_AppendBuffer, html_mail, strlen(html_mail));
	}
}

/******************************************************************
 
*******************************************************************/
static int messageview_setup(struct MessageView_Data *data, struct mail *mail, char *folder_path)
{
	int rc = 0;
	BPTR lock;

	set(App, MUIA_Application_Sleep, TRUE);

	if ((lock = Lock(folder_path,ACCESS_READ))) /* maybe it's better to use an absoulte path here */
	{
		BPTR old_dir = CurrentDir(lock);

		if ((data->mail = mail_create_from_file(mail->filename)))
		{
			mail_read_contents(folder_path,data->mail);
			mail_create_html_header(data->mail,0);
			messageview_show_mail(data);
			rc = 1;
		}
		CurrentDir(old_dir);
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
	struct TagItem *ti;
	Object *simplehtml, *horiz, *vert;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
		MUIA_Group_Spacing, 0,
		MUIA_Group_Horiz, FALSE,
		Child, HGroup,
			MUIA_Group_Spacing, 0,
			Child, simplehtml = SimpleHTMLObject,TextFrame,End,
			Child, vert = ScrollbarObject, End,
			End,
		Child, horiz = ScrollbarObject, MUIA_Group_Horiz, TRUE, End,
		TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MessageView_Data*)INST_DATA(cl,obj);

	data->simplehtml = simplehtml;
	data->horiz = horiz;
	data->vert = vert;

	init_myhook(&data->load_hook, (HOOKFUNC)simplehtml_load_function, data);

	SetAttrs(data->simplehtml,
			MUIA_SimpleHTML_HorizScrollbar, horiz,
			MUIA_SimpleHTML_VertScrollbar, vert,
			MUIA_SimpleHTML_LoadHook, &data->load_hook,
			TAG_DONE);

	return (ULONG)obj;
}

/******************************************************************
 OM_DISPOSE
*******************************************************************/
STATIC ULONG MessageView_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
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
	struct MessageView_Data *data = (struct MessageView_Data*)INST_DATA(cl,obj);
	switch (msg->opg_AttrID)
	{
		default:
					return DoSuperMethodA(cl,obj,(Msg)msg);
	}
}

#undef printf

/******************************************************************
 MUIM_Setup
*******************************************************************/
STATIC ULONG MessageView_Show(struct IClass *cl, Object *obj, struct MUIP_Show *msg)
{
	struct MessageView_Data *data = (struct MessageView_Data*)INST_DATA(cl,obj);
	ULONG rc;

	if ((rc = DoSuperMethodA(cl,obj,(Msg)msg)))
		data->show = 1;

	if (data->ref_mail && data->folder_path)
		messageview_setup(data,data->ref_mail,data->folder_path);

	return rc;
}

/******************************************************************
 MUIM_Cleanup
*******************************************************************/
STATIC ULONG MessageView_Hide(struct IClass *cl, Object *obj, struct MUIP_Hide *msg)
{
	struct MessageView_Data *data = (struct MessageView_Data*)INST_DATA(cl,obj);

	messageview_cleanup_temporary_files(data);
	mail_free(data->mail); /* NULL safe */	
	data->mail = NULL;

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
	mail_free(data->mail); /* NULL safe */
	data->mail = NULL;

	/* remove referencs */
	data->ref_mail = NULL;
	free(data->folder_path);

	/* create resources */
	if (!(data->folder_path = mystrdup(msg->folder_path)))
		return 0;
	data->ref_mail = msg->mail;

	if (data->show)
	{
		if (!(messageview_setup(data,data->ref_mail,data->folder_path)))
			return 0;
	}

	return 1;
}

STATIC BOOPSI_DISPATCHER(ULONG, MessageView_Dispatcher, cl, obj, msg)
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
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

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

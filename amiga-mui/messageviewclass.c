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
#include <mui/betterstring_mcc.h>

#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/wb.h>
#include <proto/icon.h>

#include "amigasupport.h"
#include "configuration.h"
#include "debug.h"
#include "filter.h"
#include "mail.h"
#include "simplemail.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"
#include "text2html.h"

#include "compiler.h"
#include "mailinfoclass.h"
#include "muistuff.h"
#include "multistringclass.h"
#include "messageviewclass.h"
#include "simplehtml_mcc.h"

struct MessageView_Data
{
	Object *mailinfo;
	Object *simplehtml;
	Object *horiz;
	Object *vert;
	int show;

	struct Hook load_hook;

	char *folder_path;
	struct mail_info *ref_mail; /* The mail's reference */

	struct mail_complete *mail; /* The currently displayed mail (instance created within this class) */

	struct FileRequester *file_req; /* Filerequester for saving files */
};

/******************************************************************
 Checks if a given picture describes a picture
*******************************************************************/
static int is_picture(void *data, int len)
{
	char *buffer = (char*)data;

	if (!strncmp(&buffer[6], "JFIF", 4)) return 1;
	if (!strncmp(buffer, "GIF8", 4)) return 1;
	if (!strncmp(&buffer[1], "PNG", 3)) return 1;
	if (!strncmp(buffer, "FORM", 4) && !strncmp(&buffer[8], "ILBM", 4)) return 1;

	return 0;
}

/******************************************************************
 Save the contents of a given mail to a given dest
*******************************************************************/
static void save_contents_to(struct MessageView_Data *data, struct mail_complete *mail, char *drawer, char *file)
{
	BPTR dlock;

	mail_decode(mail);

	if (!file) file = "Unnamed";

	if ((dlock = Lock(drawer,ACCESS_READ)))
	{
		BPTR olock;
		BPTR flock;
		BPTR fh;
		int goon = 1;

		olock = CurrentDir(dlock);

		if ((flock = Lock(file,ACCESS_READ)))
		{
			UnLock(flock);
			goon = sm_request(NULL,_("File %s already exists in %s.\nDo you really want to replace it?"),("_Yes|_No"),file,drawer);
		}

		if (goon)
		{
			if (!mystricmp(mail->content_type,"text") && mystricmp(mail->content_subtype, "html"))
			{
				void *cont; /* mails content */
				int cont_len;

				char *charset;
				char *user_charset;

				if (!(charset = mail->content_charset)) charset = "ISO-8859-1";
				user_charset = user.config.default_codeset?user.config.default_codeset->name:"ISO-8858-1";

				/* Get the contents */
				mail_decoded_data(mail,&cont,&cont_len);
					
				if (mystricmp(user_charset,charset))
				{
					/* The character sets differ so now we create the two strings to see if they differ */
					char *str; /* That's the string encoded in the mails charset */
					char *user_str; /* That's the string encoded in the users charset */

					/* encode now */
					str = utf8tostrcreate((utf8 *)cont, codesets_find(charset));
					user_str = utf8tostrcreate((utf8 *)cont, codesets_find(user_charset));

					if (str && user_str)
					{
						char *towrite;

						/* Let's see if they are different */
						if (strcmp(str,user_str))
						{
							/* Yes, so inform the user */
							char gadgets[200];
							int selection;

							sm_snprintf(gadgets,sizeof(gadgets),_("_Orginal (%s)|_Converted (%s)| _UTF8|_Cancel"),charset,user_charset);
							selection = sm_request(NULL,_("The orginal charset of the attached file differs from yours.\nIn which charset do you want the file being saved?"),gadgets);

							switch (selection)
							{
								case 1: towrite = str; break;
								case 2: towrite = user_str; break;
								case 3: towrite = NULL; goon = 1; break; /* will be writeout as utf8 */
								default: towrite = NULL; goon = 0; break; /* cancel */
							}
						} else towrite = user_str;

						if (towrite)
						{
							/* Now write out the stuff */
							if ((fh = Open(file, MODE_NEWFILE)))
							{
								char *comment = mail_get_from_address(mail_get_root(mail)->info);
								Write(fh,towrite,strlen(towrite));
								Close(fh);

								if (comment)
								{
									SetComment(file,comment);
									free(comment);
								}
								goon = 0;
							}
						}
					}
				}
			}
		}
		
    if (goon)
    {
			if ((fh = Open(file, MODE_NEWFILE)))
			{
				char *comment = mail_get_from_address(mail_get_root(mail)->info);
				void *cont;
				int cont_len;

				mail_decoded_data(mail,&cont,&cont_len);

				Write(fh,cont,cont_len);
				Close(fh);

				if (comment)
				{
					SetComment(file,comment);
					free(comment);
				}
			}
		}

		CurrentDir(olock);
		UnLock(dlock);
	}
}

/******************************************************************
 Save the contents of a given mail
*******************************************************************/
static void save_contents(struct MessageView_Data *data, struct mail_complete *mail)
{
	if (!mail) return;
	if (!mail->num_multiparts)
	{
		if (MUI_AslRequestTags(data->file_req,
					mail->content_name?ASLFR_InitialFile:TAG_IGNORE,mail->content_name,
					TAG_DONE))
		{
			save_contents_to(data,mail,data->file_req->fr_Drawer,data->file_req->fr_File);
		}
	}
}

/******************************************************************
 Open the contents of an icon (requires version 44 of the os)
*******************************************************************/
static void open_contents(struct MessageView_Data *data, struct mail_complete *mail_part)
{
	if (WorkbenchBase->lib_Version >= 44 && IconBase->lib_Version >= 44)
	{
		BPTR fh;
		BPTR newdir;
		BPTR olddir;
		char filename[100];

		/* Write out the file, create an icon, start it via wb.library */
		if (mail_part->content_name)
		{
			strcpy(filename,"T:");
			mystrlcpy(&filename[2],data->mail->info->filename,sizeof(filename));

			if ((newdir = CreateDir(filename)))
				UnLock(newdir);

			if ((newdir = Lock(filename, ACCESS_READ)))
			{
				olddir = CurrentDir(newdir);

				if ((fh = Open(mail_part->content_name,MODE_NEWFILE)))
				{
					struct DiskObject *dobj;
					void *cont;
					int cont_len;

					mail_decode(mail_part);
					mail_decoded_data(mail_part,&cont,&cont_len);

					Write(fh,cont,cont_len);
					Close(fh);

					if ((dobj = GetIconTags(mail_part->content_name,ICONGETA_FailIfUnavailable,FALSE,TAG_DONE)))
					{
						int ok_to_open = 1;
						if (dobj->do_Type == WBTOOL)
						{
							ok_to_open = sm_request(NULL,_("Are you sure that you want to start this executable?"),_("*_Yes|_Cancel"));
						}

						if (ok_to_open) PutIconTagList(mail_part->content_name,dobj,NULL);
						FreeDiskObject(dobj);

						if (ok_to_open)
							OpenWorkbenchObjectA(mail_part->content_name,NULL);
					}
				}

				CurrentDir(olddir);
				UnLock(newdir);
			}
		}
	}
}

/******************************************************************
 SimpleHTML Load Hook. Returns 1 if uri can be loaded by the hook
 otherwise 0. -1 means reject this object totaly
*******************************************************************/
STATIC ASM SAVEDS LONG simplehtml_load_function(REG(a0,struct Hook *h), REG(a2, Object *obj), REG(a1,struct MUIP_SimpleHTML_LoadHook *msg))
{
	struct MessageView_Data *data = (struct MessageView_Data*)h->h_Data;
	char *uri = msg->uri;

	void *decoded_data;
	int decoded_data_len;

/*	struct mail *mail;*/

	if (!mystrnicmp("internalimg:",uri,12))
	{
		void *possible_data = (void*)strtoul(uri+12,NULL,16);

		/* Check if this is really an internal image */
		if (data == possible_data)
		{
			struct mail_complete *m = (struct mail_complete*)strtoul(uri+21,NULL,16);

			mail_decode(m);
			mail_decoded_data(m, &decoded_data, &decoded_data_len);

			if ((msg->buffer = (void*)DoMethod(data->simplehtml, MUIM_SimpleHTML_AllocateMem, decoded_data_len)))
			{
				msg->buffer_len = decoded_data_len;
				CopyMem(decoded_data,msg->buffer,decoded_data_len);
				return 1;
			}
		}
		return -1;
	}

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
 A an uri has been clicked
*******************************************************************/
static void messageview_uri_clicked(void **msg)
{
	struct MessageView_Data *data = (struct MessageView_Data*)msg[0];
	char *uri = (char*)msg[1];

	if (!mystrnicmp(uri,"mailto:",7))
	{
		callback_write_mail_to_str(uri+7,NULL);
	} else
	{
		if (!mystrnicmp(uri,"internalview:",13))
		{
			void *possible_data = (void*)strtoul(uri+13,NULL,16);

			/* Check if this is really an internal anchor */
			if (data == possible_data)
			{
				struct mail_complete *m = (struct mail_complete*)strtoul(uri+22,NULL,16);
				if (m)
					open_contents(data,m);
			}
		} else
		{
			if (!mystrnicmp(uri,"internalsave:",13))
			{
				void *possible_data = (void*)strtoul(uri+13,NULL,16);

				/* Check if this is really an internal anchor */
				if (data == possible_data)
				{
					struct mail_complete *m = (struct mail_complete*)strtoul(uri+22,NULL,16);
					if (m)
						save_contents(data,m);
				}
			} else
			{
				OpenURL(uri);
			}
		}
	}
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

/******************************************************************
 Append and mail
*******************************************************************/
static void messageview_append_as_mail(struct MessageView_Data *data, struct mail_complete *mail, string *str)
{
	int i;

	void *decoded_data;
	int decoded_data_len;

	static char temp_buf[512];

	mail_decoded_data(mail, &decoded_data, &decoded_data_len);

	if (!mail->num_multiparts)
	{
		if (!mystricmp(mail->content_type,"text") && !mystricmp(mail->content_subtype,"plain"))
		{
			char *html_txt = text2html((char*)decoded_data, decoded_data_len, TEXT2HTML_FIXED_FONT|(user.config.read_wordwrap?0:TEXT2HTML_NOWRAP),"<FONT FACE=\"fixedmail\" SIZE=\"+1\">");
			string_append(str,html_txt);
			free(html_txt);		
		} else
		{
			if (is_picture(decoded_data,decoded_data_len))
			{
				sm_snprintf(temp_buf,sizeof(temp_buf),"<table align=\"center\" cellpadding=\"5\"><tr><td><IMG SRC=\"internalimg:%08lx.%08lx /></td></tr></table>",data,mail);
				string_append(str,temp_buf);
			}
		}
	}

	for (i=0;i<mail->num_multiparts;i++)
	{
		messageview_append_as_mail(data,mail->multipart_array[i],str);
	}
}

/******************************************************************
 Append an attachment
*******************************************************************/
static void messageview_append_as_attachment(struct MessageView_Data *data, struct mail_complete *mail, string *str)
{
	int i;

	struct mail_complete *initial_mail;

	static char temp_buf[512]; /* Ok to use a static buffer although this is a recursive call */
	static char content_name[256];

	void *decoded_data;
	int decoded_data_len;

	/* Find out the mail part which is displayed on default */
	if (!(initial_mail = mail_find_initial(data->mail)))
		return;

	for (i=0;i<mail->num_multiparts;i++)
	{
		struct mail_complete *m = mail->multipart_array[i];

		if (initial_mail == m) mystrlcpy(content_name,_("Main Message"),sizeof(content_name));
		else
		{
			if (m->content_name) utf8tostr(m->content_name, content_name, sizeof(content_name), user.config.default_codeset);
			else mystrlcpy(content_name,_("Unnamed"),sizeof(content_name));
		}

		mail_decode(m);
		mail_decoded_data(m, &decoded_data, &decoded_data_len);

		string_append(str,"<tr><td>");
		string_append(str,content_name);
		string_append(str,"</td>");
		
		sm_snprintf(temp_buf,sizeof(temp_buf),"<td><A HREF=\"internalview:%08lx.%08lx\">%s</A></td>",data,m,Q_("?attachment:View"));
		string_append(str,temp_buf);
		sm_snprintf(temp_buf,sizeof(temp_buf),"<td><A HREF=\"internalsave:%08lx.%08lx\">%s</A></td>",data,m,Q_("?attachment:Save"));
		string_append(str,temp_buf);
		sm_snprintf(temp_buf,sizeof(temp_buf),"<td>%s/%s</td>",m->content_type,m->content_subtype);
		string_append(str,temp_buf);
		string_append(str,"<td align=\"right\">");
		sm_snprintf(temp_buf,sizeof(temp_buf),_("%ld bytes"),decoded_data_len);
		string_append(str,temp_buf);
		string_append(str,"</td>");
		string_append(str,"</tr>");
	}
}

/******************************************************************
 Show the given mail, which should be a root mail
*******************************************************************/
static void messageview_show_mail(struct MessageView_Data *data)
{
	char *buf;
	char *buf_end;
	struct mail_complete *mail;

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

	if (!stricmp(mail->content_subtype, "html"))
	{
		SetAttrs(data->simplehtml,
				MUIA_SimpleHTML_Buffer, buf,
				MUIA_SimpleHTML_BufferLen, buf_end - buf,
				TAG_DONE);
	} else
	{
		char *font_buf;
		char buf[300];

		string str;

		if (!string_initialize(&str,2048))
			return;

		/* substituate the fixed font */
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

		/* substituate the proportional font */
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

		sm_snprintf(buf,sizeof(buf),"<HTML><BODY BGCOLOR=\"#%06x\" TEXT=\"#%06x\" LINK=\"#%06x\">",user.config.read_background,user.config.read_text,user.config.read_link);
		string_append(&str,buf);

		messageview_append_as_mail(data,data->mail,&str);

		string_append(&str,"<hr /><table>");
		messageview_append_as_attachment(data,data->mail,&str);
		string_append(&str,"</table>");

		string_append(&str,"</BODY>");

		SetAttrs(data->simplehtml,
				MUIA_SimpleHTML_Buffer, str.str,
				MUIA_SimpleHTML_BufferLen, strlen(str.str),
				TAG_DONE);

		free(str.str);
	}
}

/******************************************************************
 Argument mail and folder_path can be NULL. In eighter case
 a empty text is displayed.
*******************************************************************/
static int messageview_setup(struct MessageView_Data *data, struct mail_info *mail, char *folder_path)
{
	int rc = 0;
	BPTR lock;

	/* not specifing a mail is accepted */
	if (!mail || !folder_path)
	{
		char text[256];

		set(data->mailinfo, MUIA_MailInfo_MailInfo, NULL);

		sm_snprintf(text,sizeof(text),"<HTML><BODY BGCOLOR=\"#%06x\" TEXT=\"#%06x\" LINK=\"#%06x\"></BODY></HTML>",user.config.read_background,user.config.read_text,user.config.read_link);

		SetAttrs(data->simplehtml,
			MUIA_SimpleHTML_Buffer, text,
			MUIA_SimpleHTML_BufferLen, strlen(text),
			TAG_DONE);
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
			mail_create_html_header(data->mail,0);
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
	Object *simplehtml, *horiz, *vert, *mailinfo;

	if (!(obj=(Object *)DoSuperNew(cl,obj,
		MUIA_Group_Spacing, 0,
		MUIA_Group_Horiz, FALSE,
		Child, mailinfo = MailInfoObject, End,
		Child, HGroup,
			MUIA_Group_Spacing, 0,
			Child, simplehtml = SimpleHTMLObject,TextFrame,End,
			Child, vert = ScrollbarObject, End,
			End,
		Child, horiz = ScrollbarObject, MUIA_Group_Horiz, TRUE, End,
		TAG_MORE,msg->ops_AttrList)))
		return 0;

	data = (struct MessageView_Data*)INST_DATA(cl,obj);

	if (!(data->file_req = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_DoSaveMode, TRUE, TAG_DONE)))
	{
		CoerceMethod(cl,obj,OM_DISPOSE);
		return 0;
	}


	data->simplehtml = simplehtml;
	data->horiz = horiz;
	data->vert = vert;
	data->mailinfo = mailinfo;

	init_hook_with_data(&data->load_hook, (HOOKFUNC)simplehtml_load_function, data);

	SetAttrs(data->simplehtml,
			MUIA_SimpleHTML_HorizScrollbar, horiz,
			MUIA_SimpleHTML_VertScrollbar, vert,
			MUIA_SimpleHTML_LoadHook, &data->load_hook,
			TAG_DONE);

	DoMethod(data->simplehtml, MUIM_Notify, MUIA_SimpleHTML_URIClicked, MUIV_EveryTime, App, 5, MUIM_CallHook, &hook_standard, messageview_uri_clicked, data, MUIV_TriggerValue);

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

	if (data->folder_path && !data->mail)
		messageview_setup(data,data->ref_mail,data->folder_path);

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

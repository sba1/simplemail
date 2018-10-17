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

/**
 * @file
 */

#include "request.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/iffparse.h> /* MAKE_ID */
#include <mui/BetterString_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/NListtree_mcc.h>

#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>

#include "pgp.h"
#include "smintl.h"
#include "support_indep.h"

#include "foldertreelistclass.h"
#include "gui_main_amiga.h"
#include "muistuff.h"
#include "pgplistclass.h"
#include "support.h"

/*****************************************************************************/

char *sm_request_file(char *title, char *path, int save, char *extension)
{
	char *rc = NULL;
	struct FileRequester *fr;

	char *aosext;

	if (extension)
	{
		if (!(aosext = (char *)malloc(strlen(extension)+10)))
			return NULL;
		strcpy(aosext,"#?");
		strcat(aosext,extension);
	} else aosext = NULL;

	if ((fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
				TAG_DONE)))
	{
		if (AslRequestTags(fr,
			ASLFR_InitialDrawer, path,
			aosext?ASLFR_InitialPattern:TAG_IGNORE, aosext,
			aosext?ASLFR_DoPatterns:TAG_IGNORE, TRUE,
			ASLFR_DoSaveMode, save,
			TAG_DONE))
		{
			int len = strlen(fr->fr_File) + strlen(fr->fr_Drawer) + 5;

			if ((rc = (char *)malloc(len)))
			{
				strcpy(rc, fr->fr_Drawer);
				if(!AddPart(rc, fr->fr_File, len) || !strlen(fr->fr_File))
				{
					free(rc);
					rc = NULL;
				}
			}
		}

		FreeAslRequest(fr);
	}

	return rc;
}

/*****************************************************************************/

int sm_request(char *title, char *text, char *gadgets, ...)
{
	int rc;
	char *text_buf;
	va_list ap;

  extern int vsnprintf(char *buffer, size_t buffersize, const char *fmt0, va_list ap);

	if (!(text_buf = (char *)malloc(2048)))
		return 0;

  va_start(ap, gadgets);
  vsnprintf(text_buf, 2048, text, ap);
  va_end(ap);

	if (MUIMasterBase)
	{
		rc = MUI_RequestA(App, NULL, 0, title, gadgets, text_buf, NULL);
	} else
	{
		struct EasyStruct es;
		memset(&es,0,sizeof(es));
		es.es_StructSize = sizeof(es);
		es.es_Title = title?title:"SimpleMail";
		es.es_TextFormat = text_buf;
		es.es_GadgetFormat = gadgets;

		rc = EasyRequestArgs(NULL,&es,NULL,NULL);
	}

	free(text_buf);
	return rc;
}

/*****************************************************************************/

char *sm_request_string(char *title, char *text, char *contents, int secret)
{
	char *ret = NULL;
	Object *string;
	Object *wnd;

	wnd = WindowObject,
		MUIA_Window_Title, title?title:"SimpleMail",
		MUIA_Window_ID, MAKE_ID('S','R','E','Q'),
		WindowContents, VGroup,
			Child, TextObject, MUIA_Text_PreParse, "\033c", MUIA_Text_Contents, text, End,
			Child, string = BetterStringObject,
				StringFrame,
				MUIA_String_Secret, secret,
				MUIA_String_Contents, contents,
				End,
			End,
		End;

	if (wnd)
	{
		ULONG cancel=0;
		DoMethod(App, OM_ADDMEMBER, (ULONG)wnd);
		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);
		DoMethod(string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

		set(wnd,MUIA_Window_Open,TRUE);
		set(wnd,MUIA_Window_ActiveObject,string);
		loop();

		if (!cancel)
		{
			ret = mystrdup((char*)xget(string,MUIA_String_Contents));
		}
		DoMethod(App, OM_REMMEMBER, (ULONG)wnd);
		MUI_DisposeObject(wnd);
	}
	return ret;
}

/*****************************************************************************/

int sm_request_login(char *text, char *login, char *password, int len)
{
	int ret = 0;
	Object *login_string;
	Object *pass_string;
	Object *wnd;
	Object *ok_button, *cancel_button;

	char buf[256];
	sm_snprintf(buf,sizeof(buf),_("Please enter the login for %s"),text);

	wnd = WindowObject,
		MUIA_Window_Title, "SimpleMail - Login",
		MUIA_Window_ID, MAKE_ID('S','L','O','G'),
		WindowContents, VGroup,
			Child, TextObject, MUIA_Text_PreParse, "\033c", MUIA_Text_Contents, buf, End,
			Child, HGroup,
				Child, MakeLabel(_("Login/UserID")),
				Child, login_string = BetterStringObject,
					StringFrame,
					MUIA_String_Contents, login,
					MUIA_String_AdvanceOnCR, TRUE,
					MUIA_CycleChain, 1,
					End,
				End,
			Child, HGroup,
				Child, MakeLabel(_("Password")),
				Child, pass_string = BetterStringObject,
					StringFrame,
					MUIA_String_Secret, 1,
					MUIA_CycleChain, 1,
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton(_("_Ok")),
				Child, cancel_button = MakeButton(_("_Cancel")),
				End,
			End,
		End;

	if (wnd)
	{
		ULONG cancel=0;
		DoMethod(App, OM_ADDMEMBER, (ULONG)wnd);
		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);
		DoMethod(pass_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

		set(wnd,MUIA_Window_Open,TRUE);
		set(wnd,MUIA_Window_ActiveObject,(*login)?pass_string:login_string);
		loop();

		if (!cancel)
		{
			mystrlcpy(login, (char*)xget(login_string,MUIA_String_Contents), len);
			mystrlcpy(password, (char*)xget(pass_string,MUIA_String_Contents), len);
			ret = 1;
		}
		DoMethod(App, OM_REMMEMBER, (ULONG)wnd);
		MUI_DisposeObject(wnd);
	}
	return ret;
}

/*****************************************************************************/

char *sm_request_pgp_id(char *text)
{
	char *ret = NULL;
	Object *pgp_list;
	Object *wnd;
	Object *ok_button, *cancel_button;

	wnd = WindowObject,
		MUIA_Window_Title, _("SimpleMail - Select PGP Key"),
		MUIA_Window_ID, MAKE_ID('S','R','P','G'),
		WindowContents, VGroup,
			Child, TextObject, MUIA_Text_PreParse, "\033c", MUIA_Text_Contents, text, End,
			Child, NListviewObject,
				MUIA_NListview_NList, pgp_list = PGPListObject,
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton(_("_Ok")),
				Child, HVSpace,
				Child, cancel_button = MakeButton(_("_Cancel")),
				End,
			End,
		End;

	if (wnd)
	{
		ULONG cancel=0;
		DoMethod(App, OM_ADDMEMBER, (ULONG)wnd);
		DoMethod(pgp_list, MUIM_PGPList_Refresh);

		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);
		DoMethod(pgp_list, MUIM_Notify, MUIA_NList_DoubleClick, TRUE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);

		set(wnd,MUIA_Window_Open,TRUE);
		loop();

		if (!cancel)
		{
			struct pgp_key *key;
			DoMethod(pgp_list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, (ULONG)&key);
			if (key)
			{
				ret = (char *)malloc(16);
				sprintf(ret,"0x%08X",key->keyid);
			}
		}
		DoMethod(App, OM_REMMEMBER, (ULONG)wnd);
		MUI_DisposeObject(wnd);
	}
	return ret;
}

/*****************************************************************************/

struct folder *sm_request_folder(char *text, struct folder *exclude)
{
	struct folder *selected_folder = NULL;
	Object *folder_tree;
	Object *wnd;
	Object *ok_button, *cancel_button;

	wnd = WindowObject,
		MUIA_Window_Title, _("SimpleMail - Select a Folder"),
		MUIA_Window_ID, MAKE_ID('S','R','F','O'),
		WindowContents, VGroup,
			Child, TextObject, MUIA_Text_PreParse, "\033c", MUIA_Text_Contents, text, End,
			Child, NListviewObject,
				MUIA_NListview_NList, folder_tree = FolderTreelistObject,
					MUIA_CycleChain, 1,
					End,
				End,
			Child, HorizLineObject,
			Child, HGroup,
				Child, ok_button = MakeButton(_("_Ok")),
				Child, HVSpace,
				Child, cancel_button = MakeButton(_("_Cancel")),
				End,
			End,
		End;

	if (wnd)
	{
		ULONG cancel=0;
		DoMethod(App, OM_ADDMEMBER, (ULONG)wnd);
		DoMethod(folder_tree, MUIM_FolderTreelist_Refresh, (ULONG)exclude);

		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(ok_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)App, 3, MUIM_WriteLong, 1, (ULONG)&cancel);

		set(wnd,MUIA_Window_Open,TRUE);
		set(wnd,MUIA_Window_ActiveObject,folder_tree);
		loop();

		if (!cancel)
		{
			struct MUI_NListtree_TreeNode *tree_node;
			tree_node = (struct MUI_NListtree_TreeNode *)xget(folder_tree,MUIA_NListtree_Active);

			if (tree_node)
			{
				if (tree_node->tn_User)
				{
					selected_folder = (struct folder*)tree_node->tn_User;
				}
			}
		}
		DoMethod(App, OM_REMMEMBER, (ULONG)wnd);
		MUI_DisposeObject(wnd);
	}
	return selected_folder;
}

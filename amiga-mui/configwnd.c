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

/*
** $Id$
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <mui/betterstring_mcc.h>
#include <mui/nlistview_mcc.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "configuration.h"
#include "lists.h"

#include "muistuff.h"
#include "support.h"

static Object *config_wnd;
static Object *user_realname_string;
static Object *user_email_string;
static Object *pop3_login_string;
static Object *pop3_server_string;
static Object *pop3_password_string;
static Object *pop3_delete_check;
static Object *smtp_domain_string;
static Object *smtp_server_string;

static void close_config(void)
{
	set(config_wnd, MUIA_Window_Open, FALSE);
	DoMethod(App, OM_REMMEMBER, config_wnd);
	MUI_DisposeObject(config_wnd);
	config_wnd = NULL;
}

static void config_use(void)
{
	if (user.config.realname) free(user.config.realname);
	if (user.config.email) free(user.config.email);
	if (user.config.smtp_server) free(user.config.smtp_server);
	if (user.config.smtp_domain) free(user.config.smtp_domain);
	if (user.config.pop_server) free(user.config.pop_server);
	if (user.config.pop_login) free(user.config.pop_login);
	if (user.config.pop_password) free(user.config.pop_password);

	user.config.realname = mystrdup((char*)xget(user_realname_string, MUIA_String_Contents));
	user.config.email = mystrdup((char*)xget(user_email_string, MUIA_String_Contents));
	user.config.smtp_domain = mystrdup((char*)xget(smtp_domain_string, MUIA_String_Contents));
	user.config.smtp_server = mystrdup((char*)xget(smtp_server_string, MUIA_String_Contents));
	user.config.pop_server = mystrdup((char*)xget(pop3_server_string, MUIA_String_Contents));
	user.config.pop_login = mystrdup((char*)xget(pop3_login_string, MUIA_String_Contents));
	user.config.pop_password = mystrdup((char*)xget(pop3_password_string, MUIA_String_Contents));
	user.config.pop_delete = xget(pop3_delete_check,MUIA_Selected);

	close_config();
}

static void config_save(void)
{
	config_use();
	save_config();
}

static void init_config(void)
{
	Object *save_button, *use_button, *cancel_button;

	config_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('C','O','N','F'),
    MUIA_Window_Title, "SimpleMail - Configure",
    WindowContents, VGroup,
    	Child, HorizLineTextObject("User"),
			Child, HGroup,
				Child, MakeLabel("Real Name"),
				Child, user_realname_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_String_Contents, user.config.realname,
					End,
				Child, MakeLabel("E-Mail Address"),
				Child, user_email_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_String_Contents, user.config.email,
					End,
				End,

    	Child, ColGroup(2),
    		Child, HorizLineTextObject("POP3 Server"),
    		Child, HorizLineTextObject("SMTP Server"),

				Child, ColGroup(2),
					Child, MakeLabel("Login/User ID"),
					Child, pop3_login_string = BetterStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						MUIA_String_Contents, user.config.pop_login,
						End,
					Child, MakeLabel("Server"),
					Child, pop3_server_string = BetterStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						MUIA_String_Contents, user.config.pop_server,
						End,
					Child, MakeLabel("Password"),
					Child, pop3_password_string = BetterStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						MUIA_String_Contents, user.config.pop_password,
						End,
					Child, MakeLabel("_Delete mails"),
					Child, HGroup, Child, pop3_delete_check = MakeCheck("_Delete mails", user.config.pop_delete), Child, HSpace(0), End,
					End,

				Child, ColGroup(2),
					Child, MakeLabel("Domain"),
					Child, smtp_domain_string = BetterStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						MUIA_String_Contents, user.config.smtp_domain,
						End,
					Child, MakeLabel("Server"),
					Child, smtp_server_string = BetterStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						MUIA_String_Contents, user.config.smtp_server,
						End,
					End,
    		End,
    	Child, HorizLineObject,
    	Child, HGroup,
    		Child, save_button = MakeButton("_Save"),
    		Child, use_button = MakeButton("_Use"),
    		Child, cancel_button = MakeButton("_Cancel"),
    		End,
			End,
		End;

	if (config_wnd)
	{
		DoMethod(App, OM_ADDMEMBER, config_wnd);
		DoMethod(config_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, close_config);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, close_config);
		DoMethod(use_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, config_use);
		DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, config_save);
	}
}

void open_config(void)
{
	if (!config_wnd) init_config();
	if (config_wnd)
	{
		set(config_wnd, MUIA_Window_Open, TRUE);
	}
}


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
** configwnd.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <mui/betterstring_mcc.h>
#include <mui/nlistview_mcc.h>
#include <mui/nlisttree_mcc.h>
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
static Object *pop3_port_string;
static Object *pop3_password_string;
static Object *pop3_delete_check;
static Object *smtp_domain_string;
static Object *smtp_server_string;
static Object *smtp_port_string;
static Object *smtp_auth_check;
static Object *smtp_login_string;
static Object *smtp_password_string;

static Object *config_group;
static Object *config_tree;

static Object *user_group;
static Object *tcpip_send_group;
static Object *tcpip_receive_group;

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
	if (user.config.smtp_login) free(user.config.smtp_login);
	if (user.config.smtp_password) free(user.config.smtp_password);
	if (user.config.pop_server) free(user.config.pop_server);
	if (user.config.pop_login) free(user.config.pop_login);
	if (user.config.pop_password) free(user.config.pop_password);

	user.config.realname = mystrdup((char*)xget(user_realname_string, MUIA_String_Contents));
	user.config.email = mystrdup((char*)xget(user_email_string, MUIA_String_Contents));
	user.config.smtp_domain = mystrdup((char*)xget(smtp_domain_string, MUIA_String_Contents));
	user.config.smtp_server = mystrdup((char*)xget(smtp_server_string, MUIA_String_Contents));
	user.config.smtp_login = mystrdup((char*)xget(smtp_login_string, MUIA_String_Contents));
	user.config.smtp_password = mystrdup((char*)xget(smtp_password_string, MUIA_String_Contents));
	user.config.smtp_auth = xget(smtp_auth_check, MUIA_Selected);
	user.config.pop_server = mystrdup((char*)xget(pop3_server_string, MUIA_String_Contents));
	user.config.pop_login = mystrdup((char*)xget(pop3_login_string, MUIA_String_Contents));
	user.config.pop_password = mystrdup((char*)xget(pop3_password_string, MUIA_String_Contents));
	user.config.pop_delete = xget(pop3_delete_check, MUIA_Selected);

	close_config();
}

static void config_save(void)
{
	config_use();
	save_config();
}

static void config_tree_active(void)
{
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)xget(config_tree, MUIA_NListtree_Active);
	if (treenode)
	{
		Object *group = (Object*)treenode->tn_User;
		if (group)
		{
			static Object *last_group;
			DoMethod(config_group,MUIM_Group_InitChange);
			if (last_group) set(last_group,MUIA_ShowMe,FALSE);
			last_group = group;
			set(group,MUIA_ShowMe,TRUE);
			DoMethod(config_group,MUIM_Group_ExitChange);
		}
	}
}

static int init_user_group(void)
{
	user_group = ColGroup(2),
		MUIA_ShowMe, FALSE,
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
		End;
	if (!user_group) return 0;
	return 1;
}

static int init_tcpip_receive_group(void)
{
	tcpip_receive_group = ColGroup(2),
		MUIA_ShowMe, FALSE,
		Child, MakeLabel("POP3 Server"),
		Child, HGroup,
			Child, pop3_server_string = BetterStringObject,
				StringFrame,
				MUIA_CycleChain, 1,
				MUIA_String_Contents, user.config.pop_server,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
			Child, MakeLabel("Port"),
			Child, pop3_port_string = BetterStringObject,
				StringFrame,
				MUIA_Weight, 33,
				MUIA_CycleChain,1,
				MUIA_String_AdvanceOnCR, TRUE,
				MUIA_String_Integer, user.config.pop_port,
				MUIA_String_Accept, "0123456789",
				End,
			End,
		Child, MakeLabel("Login/User ID"),
		Child, pop3_login_string = BetterStringObject,
			StringFrame,
			MUIA_CycleChain, 1,
			MUIA_String_Contents, user.config.pop_login,
			MUIA_String_AdvanceOnCR, TRUE,
			End,
		Child, MakeLabel("Password"),
		Child, pop3_password_string = BetterStringObject,
			StringFrame,
			MUIA_CycleChain, 1,
			MUIA_String_Contents, user.config.pop_password,
			MUIA_String_Secret, TRUE,
			MUIA_String_AdvanceOnCR, TRUE,
			End,
		Child, MakeLabel("_Delete mails"),
		Child, HGroup, Child, pop3_delete_check = MakeCheck("_Delete mails", user.config.pop_delete), Child, HSpace(0), End,
		End;

	if (!tcpip_receive_group) return 0;
	return 1;
}

static int init_tcpip_send_group(void)
{
	tcpip_send_group =  ColGroup(2),
		MUIA_ShowMe, FALSE,
		Child, MakeLabel("Domain"),
		Child, smtp_domain_string = BetterStringObject,
			StringFrame,
			MUIA_CycleChain, 1,
			MUIA_String_Contents, user.config.smtp_domain,
			MUIA_String_AdvanceOnCR, TRUE,
			End,
		Child, MakeLabel("SMTP Server"),
		Child, HGroup,
			Child, smtp_server_string = BetterStringObject,
				StringFrame,
				MUIA_CycleChain, 1,
				MUIA_String_Contents, user.config.smtp_server,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
			Child, MakeLabel("Port"),
			Child, smtp_port_string = BetterStringObject,
				StringFrame,
				MUIA_Weight, 33,
				MUIA_CycleChain,1,
				MUIA_String_Integer, user.config.smtp_port,
				MUIA_String_AdvanceOnCR,TRUE,
				MUIA_String_Accept,"0123456789",
				End,
			End,
		Child, MakeLabel("Use SMTP AUTH"),
		Child, HGroup,
			Child, smtp_auth_check = MakeCheck("Use SMTP Auth", user.config.smtp_auth),
			Child, HSpace(0),
			End,
		Child, MakeLabel("Login/User ID"),
		Child, smtp_login_string = BetterStringObject,
			StringFrame,
			MUIA_Disabled, TRUE,
			MUIA_CycleChain, 1,
			MUIA_String_Contents, user.config.smtp_login,
			MUIA_String_AdvanceOnCR, TRUE,
			End,
		Child, MakeLabel("Password"),
		Child, smtp_password_string = BetterStringObject,
			StringFrame,
			MUIA_CycleChain, 1,
			MUIA_Disabled, TRUE,
			MUIA_String_Contents, user.config.smtp_password,
			MUIA_String_AdvanceOnCR, TRUE,
			MUIA_String_Secret, TRUE,
			End,
		End;

	if (!tcpip_send_group) return 0;

	DoMethod(smtp_auth_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, smtp_login_string, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
	DoMethod(smtp_auth_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, smtp_password_string, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
	DoMethod(smtp_auth_check, MUIM_Notify, MUIA_Selected, TRUE, MUIV_Notify_Window, 3, MUIM_Set, MUIA_Window_ActiveObject, smtp_login_string);

	return 1;
}

static void init_config(void)
{
	Object *save_button, *use_button, *cancel_button;

	init_user_group();
	init_tcpip_send_group();
	init_tcpip_receive_group();

	config_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('C','O','N','F'),
    MUIA_Window_Title, "SimpleMail - Configure",
    WindowContents, VGroup,
    	Child, HGroup,
    		Child, NListviewObject,
    			MUIA_HorizWeight, 33,
    			MUIA_NListview_NList, config_tree = NListtreeObject,
    				End,
    			End,
    		Child, BalanceObject, End,
    		Child, VGroup,
    			Child, HVSpace,
	    		Child, config_group = VGroup,
  	  			Child, user_group,
    				Child, tcpip_send_group,
    				Child, tcpip_receive_group,
    				Child, VSpace(0),
    				End,
    			Child, HVSpace,
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
		APTR treenode;

		DoMethod(App, OM_ADDMEMBER, config_wnd);
		DoMethod(config_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, close_config);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, close_config);
		DoMethod(use_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, config_use);
		DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, config_save);
		DoMethod(config_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard,config_tree_active);

		DoMethod(config_tree, MUIM_NListtree_Insert, "User", user_group, MUIV_NListtree_Insert_ListNode_Root, MUIV_NListtree_Insert_PrevNode_Tail, MUIV_NListtree_Insert_Flag_Active);

		if ((treenode = (APTR)DoMethod(config_tree, MUIM_NListtree_Insert, "TCP/IP", NULL, MUIV_NListtree_Insert_ListNode_Root, MUIV_NListtree_Insert_PrevNode_Tail, TNF_LIST|TNF_OPEN)))
		{
			DoMethod(config_tree, MUIM_NListtree_Insert, "Send mail", tcpip_send_group, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			DoMethod(config_tree, MUIM_NListtree_Insert, "Receive mail", tcpip_receive_group, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
		}

		if ((treenode = (APTR)DoMethod(config_tree, MUIM_NListtree_Insert, "Mails", NULL, MUIV_NListtree_Insert_ListNode_Root, MUIV_NListtree_Insert_PrevNode_Tail, TNF_LIST|TNF_OPEN)))
		{
			DoMethod(config_tree, MUIM_NListtree_Insert, "Read", NULL, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			DoMethod(config_tree, MUIM_NListtree_Insert, "Write", NULL, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			DoMethod(config_tree, MUIM_NListtree_Insert, "Reply", NULL, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			DoMethod(config_tree, MUIM_NListtree_Insert, "Forward", NULL, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			DoMethod(config_tree, MUIM_NListtree_Insert, "Signature", NULL, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
		}
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


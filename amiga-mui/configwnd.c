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

#include <dos.h>
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
#include "pop3.h"

#include "compiler.h"
#include "configwnd.h"
#include "muistuff.h"
#include "support.h"

static struct MUI_CustomClass *CL_Sizes;
static int create_sizes_class(void);
static void delete_sizes_class(void);
static int value2size(int val);
static int size2value(int val);
#define SizesObject (Object*)NewObject(CL_Sizes->mcc_Class, NULL

static Object *config_wnd;
static Object *user_realname_string;
static Object *user_email_string;
static Object *receive_preselection_radio;
static Object *receive_sizes_sizes;
static Object *pop3_login_string;
static Object *pop3_server_string;
static Object *pop3_port_string;
static Object *pop3_password_string;
static Object *pop3_delete_check;
static Object *smtp_domain_string;
static Object *smtp_ip_check;
static Object *smtp_server_string;
static Object *smtp_port_string;
static Object *smtp_auth_check;
static Object *smtp_login_string;
static Object *smtp_password_string;
static Object *read_wrap_checkbox;

static Object *config_group;
static Object *config_tree;
static struct Hook config_construct_hook, config_destruct_hook, config_display_hook;
static APTR receive_treenode;
static struct list receive_list; /* nodes are struct pop3_server * */
static struct pop3_server *receive_last_selected; /* last selected pop3_server */

static Object *user_group;
static Object *tcpip_send_group;
static Object *tcpip_receive_group;
static Object *tcpip_pop3_group;
static Object *mails_read_group;

static Object *config_last_visisble_group;

/******************************************************************
 Gets the current settings of the POP3 server
*******************************************************************/
static void get_pop3_server(void)
{
	if (receive_last_selected)
	{
		/* Save the pop3 server if a server was selected */
		if (receive_last_selected->name) free(receive_last_selected->name);
		if (receive_last_selected->login) free(receive_last_selected->login);
		if (receive_last_selected->passwd) free(receive_last_selected->passwd);
		receive_last_selected->name = mystrdup((char*)xget(pop3_server_string, MUIA_String_Contents));
		receive_last_selected->login = mystrdup((char*)xget(pop3_login_string, MUIA_String_Contents));
		receive_last_selected->passwd = mystrdup((char*)xget(pop3_password_string, MUIA_String_Contents));
		receive_last_selected->del = xget(pop3_delete_check, MUIA_Selected);
		receive_last_selected->port = xget(pop3_port_string, MUIA_String_Integer);
		receive_last_selected = NULL;
	}
}

/******************************************************************
 Use the config
*******************************************************************/
static void config_use(void)
{
	struct pop3_server *pop;

	if (user.config.realname) free(user.config.realname);
	if (user.config.email) free(user.config.email);
	if (user.config.smtp_server) free(user.config.smtp_server);
	if (user.config.smtp_domain) free(user.config.smtp_domain);
	if (user.config.smtp_login) free(user.config.smtp_login);
	if (user.config.smtp_password) free(user.config.smtp_password);

	user.config.realname = mystrdup((char*)xget(user_realname_string, MUIA_String_Contents));
	user.config.email = mystrdup((char*)xget(user_email_string, MUIA_String_Contents));
	user.config.smtp_domain = mystrdup((char*)xget(smtp_domain_string, MUIA_String_Contents));
	user.config.smtp_ip_as_domain = xget(smtp_ip_check, MUIA_Selected);
	user.config.smtp_server = mystrdup((char*)xget(smtp_server_string, MUIA_String_Contents));
	user.config.smtp_login = mystrdup((char*)xget(smtp_login_string, MUIA_String_Contents));
	user.config.smtp_password = mystrdup((char*)xget(smtp_password_string, MUIA_String_Contents));
	user.config.smtp_auth = xget(smtp_auth_check, MUIA_Selected);
	user.config.read_wordwrap = xget(read_wrap_checkbox, MUIA_Selected);

	user.config.receive_preselection = xget(receive_preselection_radio,MUIA_Radio_Active);
	user.config.receive_size = value2size(xget(receive_sizes_sizes, MUIA_Numeric_Value));

  get_pop3_server();

	/* Copy the pop3 servers */
	free_config_pop();
	pop = (struct pop3_server *)list_first(&receive_list);
	while (pop)
	{
		insert_config_pop(pop);
		pop = (struct pop3_server *)node_next(&pop->node);
	}

	close_config();
}

/******************************************************************
 Save the configuration
*******************************************************************/
static void config_save(void)
{
	config_use();
	save_config();
}

/******************************************************************
 A new entry in the config listtree has been selected
*******************************************************************/
static void config_tree_active(void)
{
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)xget(config_tree, MUIA_NListtree_Active);
	struct MUI_NListtree_TreeNode *list_treenode = (struct MUI_NListtree_TreeNode *)xget(config_tree, MUIA_NListtree_ActiveList);
	if (treenode)
	{
		Object *group = (Object*)treenode->tn_User;
		if (group)
		{
			int init_change;

			if (group != config_last_visisble_group)
			{
				DoMethod(config_group,MUIM_Group_InitChange);
				if (config_last_visisble_group) set(config_last_visisble_group,MUIA_ShowMe,FALSE);
				config_last_visisble_group = group;
				set(group,MUIA_ShowMe,TRUE);
				init_change = 1;
			} else init_change = 0;

			get_pop3_server();

			if (list_treenode == receive_treenode)
			{
				/* A POP3 Server has been selected */
				int pop3_num = 0;
				struct pop3_server *server;
				APTR tn = treenode;

				/* Find out the position of the new selected server in the list */
				while ((tn = (APTR) DoMethod(config_tree, MUIM_NListtree_GetEntry,tn,MUIV_NListtree_GetEntry_Position_Previous,0)))
					pop3_num++;

				if ((server = (struct pop3_server*)list_find(&receive_list,pop3_num)))
				{
					setstring(pop3_server_string,server->name);
					setstring(pop3_login_string,server->login);
					setstring(pop3_password_string,server->passwd);
					set(pop3_port_string, MUIA_String_Integer,server->port);
					setcheckmark(pop3_delete_check, server->del);
				}
				receive_last_selected = server;
			}

			if (init_change) DoMethod(config_group,MUIM_Group_ExitChange);
		}
	}
}

/******************************************************************
 Init the user group
*******************************************************************/
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

/******************************************************************
 Add a new pop3 server
*******************************************************************/
static void tcpip_receive_add_pop3(void)
{
	struct pop3_server *pop3 = pop_malloc();
	if (pop3)
	{
		list_insert_tail(&receive_list, &pop3->node);
		DoMethod(config_tree, MUIM_NListtree_Insert, "POP3 Server", tcpip_pop3_group, receive_treenode, MUIV_NListtree_Insert_PrevNode_Tail, MUIV_NListtree_Insert_Flag_Active);
	}
}

/******************************************************************
 Remove the current selected pop3 server
*******************************************************************/
static void tcpip_receive_rem_pop3(void)
{
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)xget(config_tree, MUIA_NListtree_Active);
	if (treenode && !(treenode->tn_Flags & TNF_LIST))
	{
		struct pop3_server *pop3;
		struct MUI_NListtree_TreeNode *list_treenode = (struct MUI_NListtree_TreeNode *)xget(config_tree, MUIA_NListtree_ActiveList);
		APTR tn = treenode;
		int pop3_num = 0;

		/* Find out the position of the new selected server in the list */
		while ((tn = (APTR) DoMethod(config_tree, MUIM_NListtree_GetEntry,tn,MUIV_NListtree_GetEntry_Position_Previous,0)))
			pop3_num++;

		if ((pop3 = (struct pop3_server*)list_find(&receive_list,pop3_num)))
		{
			node_remove(&pop3->node);
			pop_free(pop3);

			DoMethod(config_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Active, MUIV_NListtree_Remove_TreeNode_Active,0);
		}
	}
}

/******************************************************************
 Initialize the receive group
*******************************************************************/
static int init_tcpip_receive_group(void)
{
	Object *add;
	static const char *preselection[] = {
		"Disabled","Only Sizes", "Enabled", NULL
	};

	tcpip_receive_group = VGroup,
		MUIA_ShowMe, FALSE,
		Child, HorizLineTextObject("Preselection"),
		Child, HGroup,
			Child, receive_preselection_radio = RadioObject,
				MUIA_Radio_Entries, preselection,
				MUIA_Radio_Active, user.config.receive_preselection,
				End,
			Child, RectangleObject, MUIA_Weight, 33, End,
			Child, VGroup,
				Child, HVSpace,
				Child, receive_sizes_sizes = SizesObject,
					MUIA_CycleChain, 1,
					MUIA_Numeric_Min,0,
					MUIA_Numeric_Max,50,
					MUIA_Numeric_Value, size2value(user.config.receive_size),
					End,
				Child, HVSpace,
				End,
			Child, RectangleObject, MUIA_Weight, 33, End,
			End,
		Child, HorizLineObject,
		Child, add = MakeButton("Add new POP3 Server"),
		End;

	if (!tcpip_receive_group) return 0;

	DoMethod(add, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, tcpip_receive_add_pop3);
	return 1;
}

/******************************************************************
 Initialize the receive group
*******************************************************************/
static int init_tcpip_pop3_group(void)
{
	Object *add, *rem;

	tcpip_pop3_group = VGroup,
		MUIA_ShowMe, FALSE,
		Child, ColGroup(2),
			Child, MakeLabel("POP3 Server"),
			Child, HGroup,
				Child, pop3_server_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_String_AdvanceOnCR, TRUE,
					End,
				Child, MakeLabel("Port"),
				Child, pop3_port_string = BetterStringObject,
					StringFrame,
					MUIA_Weight, 33,
					MUIA_CycleChain,1,
					MUIA_String_AdvanceOnCR, TRUE,
					MUIA_String_Accept, "0123456789",
					End,
				End,
			Child, MakeLabel("Login/User ID"),
			Child, pop3_login_string = BetterStringObject,
				StringFrame,
				MUIA_CycleChain, 1,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
			Child, MakeLabel("Password"),
			Child, pop3_password_string = BetterStringObject,
				StringFrame,
				MUIA_CycleChain, 1,
				MUIA_String_Secret, TRUE,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
			Child, MakeLabel("_Delete mails"),
			Child, HGroup, Child, pop3_delete_check = MakeCheck("_Delete mails", FALSE/*user.config.pop_delete*/), Child, HSpace(0), End,
			End,
		Child, HGroup,
			Child, add = MakeButton("Add new POP3 Server"),
			Child, rem = MakeButton("Remove POP3 Server"),
			End,
		End;

	if (!tcpip_receive_group) return 0;

	DoMethod(add, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, tcpip_receive_add_pop3);
	DoMethod(rem, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, tcpip_receive_rem_pop3);
	return 1;
}

/******************************************************************
 Initalize the send group
*******************************************************************/
static int init_tcpip_send_group(void)
{
	tcpip_send_group =  ColGroup(2),
		MUIA_ShowMe, FALSE,
		Child, MakeLabel("Domain"),
		Child, HGroup,
			Child, smtp_domain_string = BetterStringObject,
				StringFrame,
				MUIA_Disabled, user.config.smtp_ip_as_domain,
				MUIA_CycleChain, 1,
				MUIA_String_Contents, user.config.smtp_domain,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
			Child, MakeLabel("Use IP as domain"),
			Child, smtp_ip_check = MakeCheck("Use IP as domain",user.config.smtp_ip_as_domain),
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
			MUIA_Disabled, !user.config.smtp_auth,
			MUIA_CycleChain, 1,
			MUIA_String_Contents, user.config.smtp_login,
			MUIA_String_AdvanceOnCR, TRUE,
			End,
		Child, MakeLabel("Password"),
		Child, smtp_password_string = BetterStringObject,
			StringFrame,
			MUIA_CycleChain, 1,
			MUIA_Disabled, !user.config.smtp_auth,
			MUIA_String_Contents, user.config.smtp_password,
			MUIA_String_AdvanceOnCR, TRUE,
			MUIA_String_Secret, TRUE,
			End,
		End;

	if (!tcpip_send_group) return 0;

	DoMethod(smtp_auth_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, smtp_login_string, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
	DoMethod(smtp_auth_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, smtp_password_string, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
	DoMethod(smtp_auth_check, MUIM_Notify, MUIA_Selected, TRUE, MUIV_Notify_Window, 3, MUIM_Set, MUIA_Window_ActiveObject, smtp_login_string);
	DoMethod(smtp_ip_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, smtp_domain_string, 3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);
	DoMethod(smtp_ip_check, MUIM_Notify, MUIA_Selected, FALSE, MUIV_Notify_Window, 3, MUIM_Set, MUIA_Window_ActiveObject, smtp_domain_string);

	return 1;
}

/******************************************************************
 Init the read group
*******************************************************************/
static int init_mails_read_group(void)
{
	mails_read_group =  ColGroup(2),
		MUIA_ShowMe, FALSE,
		Child, MakeLabel("Wordwrap plain text"),
		Child, HGroup,
			Child, read_wrap_checkbox = MakeCheck("Wordwrap plain text",user.config.read_wordwrap),
			Child, HVSpace,
			End,
		End;

	if (!mails_read_group) return 0;
	return 1;
}

/******************************************************************
 Init the config window
*******************************************************************/
static void init_config(void)
{
	Object *save_button, *use_button, *cancel_button;

	if (!create_sizes_class()) return;

	init_user_group();
	init_tcpip_send_group();
	init_tcpip_receive_group();
	init_tcpip_pop3_group();
	init_mails_read_group();

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
    				Child, tcpip_pop3_group,
    				Child, mails_read_group,
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

		list_init(&receive_list);
		receive_last_selected = NULL;

		DoMethod(App, OM_ADDMEMBER, config_wnd);
		DoMethod(config_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, close_config);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, close_config);
		DoMethod(use_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, config_use);
		DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, config_save);
		DoMethod(config_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard,config_tree_active);

		DoMethod(config_tree, MUIM_NListtree_Insert, "User", user_group, MUIV_NListtree_Insert_ListNode_Root, MUIV_NListtree_Insert_PrevNode_Tail, MUIV_NListtree_Insert_Flag_Active);

		if ((treenode = (APTR)DoMethod(config_tree, MUIM_NListtree_Insert, "TCP/IP", NULL, MUIV_NListtree_Insert_ListNode_Root, MUIV_NListtree_Insert_PrevNode_Tail, TNF_LIST|TNF_OPEN)))
		{
			struct pop3_server *pop;
			DoMethod(config_tree, MUIM_NListtree_Insert, "Send mail", tcpip_send_group, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			receive_treenode = (APTR)DoMethod(config_tree, MUIM_NListtree_Insert, "Receive mail", tcpip_receive_group, treenode, MUIV_NListtree_Insert_PrevNode_Tail, TNF_LIST|TNF_OPEN);

			/* Insert the pop3 servers */
			pop = (struct pop3_server*)list_first(&user.config.receive_list);
			while (pop)
			{
				struct pop3_server *new_pop = pop_duplicate(pop);
				if (new_pop)
				{
					list_insert_tail(&receive_list,&new_pop->node);
					DoMethod(config_tree, MUIM_NListtree_Insert, "POP3 Server", tcpip_pop3_group, receive_treenode, MUIV_NListtree_Insert_PrevNode_Tail,0);
				}
				pop = (struct pop3_server*)node_next(&pop->node);
			}
		}

		if ((treenode = (APTR)DoMethod(config_tree, MUIM_NListtree_Insert, "Mails", NULL, MUIV_NListtree_Insert_ListNode_Root, MUIV_NListtree_Insert_PrevNode_Tail, TNF_LIST|TNF_OPEN)))
		{
			DoMethod(config_tree, MUIM_NListtree_Insert, "Read", mails_read_group, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			DoMethod(config_tree, MUIM_NListtree_Insert, "Write", NULL, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			DoMethod(config_tree, MUIM_NListtree_Insert, "Reply", NULL, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			DoMethod(config_tree, MUIM_NListtree_Insert, "Forward", NULL, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			DoMethod(config_tree, MUIM_NListtree_Insert, "Signature", NULL, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
		}
	}
}

/******************************************************************
 Open the config window
*******************************************************************/
void open_config(void)
{
	if (!config_wnd) init_config();
	if (config_wnd)
	{
		set(config_wnd, MUIA_Window_Open, TRUE);
	}
}

/******************************************************************
 Close and dispose the config window
*******************************************************************/
void close_config(void)
{
	struct pop3_server *pop;

	if (config_wnd)
	{
		set(config_wnd, MUIA_Window_Open, FALSE);
		DoMethod(App, OM_REMMEMBER, config_wnd);
		MUI_DisposeObject(config_wnd);
		config_wnd = NULL;
		config_last_visisble_group = NULL;

		while ((pop = (struct pop3_server*)list_remove_tail(&receive_list)))
			pop_free(pop);
	}
	delete_sizes_class();
}

/******************************************************************
 The size custom class. Only used in this file.
*******************************************************************/
STATIC ASM ULONG Sizes_Dispatcher(register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg)
{
	putreg(REG_A4,cl->cl_UserData);
	switch(msg->MethodID)
	{
		case	MUIM_Numeric_Stringify:
					{
						static char buf[64];
						LONG val = ((struct MUIP_Numeric_Stringify*)msg)->value;
						if (!val) return (ULONG)"All messages";
						val = value2size(val);
						sprintf(buf, "> %ld KB",val);
						return (ULONG)buf;
					}
					break;
		default: return DoSuperMethodA(cl,obj,msg);
	}
}

static int create_sizes_class(void)
{
	if ((CL_Sizes = MUI_CreateCustomClass(NULL,MUIC_Slider,NULL,4,Sizes_Dispatcher)))
	{
		CL_Sizes->mcc_Class->cl_UserData = getreg(REG_A4);
		return 1;
	}
	return 0;
}

static void delete_sizes_class(void)
{
	if (CL_Sizes)
	{
		MUI_DeleteCustomClass(CL_Sizes);
		CL_Sizes = NULL;
	}
}

static int value2size(int val)
{
	if (val > 35) val = (val - 33)*100;
	else if (val > 16) val = (val - 15)*10;
	return val;
}

static int size2value(int val)
{
	if (val >= 300) return (val/100)+33;
	if (val >= 20) return (val/10)+15;
	if (val >= 16) return 16;
	return val;
}

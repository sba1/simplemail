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

#include <libraries/asl.h>
#include <libraries/iffparse.h> /* MAKE_ID */
#include <libraries/mui.h>
#include <mui/texteditor_mcc.h>
#include <mui/betterstring_mcc.h>
#include <mui/nlistview_mcc.h>
#include <mui/nlisttree_mcc.h>
#include <mui/popplaceholder_mcc.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "account.h"
#include "configuration.h"
#include "lists.h"
#include "pop3.h"
#include "signature.h"
#include "simplemail.h"

#include "compiler.h"
#include "configtreelistclass.h"
#include "configwnd.h"
#include "muistuff.h"
#include "support_indep.h"

static struct MUI_CustomClass *CL_Sizes;
static int create_sizes_class(void);
static void delete_sizes_class(void);
static int value2size(int val);
static int size2value(int val);
#define SizesObject (Object*)NewObject(CL_Sizes->mcc_Class, NULL

static Object *config_wnd;
static Object *user_dst_check;
static Object *receive_preselection_radio;
static Object *receive_sizes_sizes;
static Object *read_fixedfont_string;
static Object *read_propfont_string;
static Object *read_wrap_checkbox;
static Object *read_linkunderlined_checkbox;
static Object *read_smilies_checkbox;
static Object *read_palette;
static struct MUI_Palette_Entry read_palette_entries[7];

static Object *write_welcome_popph;
static Object *write_close_string;

static Object *account_name_string;
static Object *account_email_string;
static Object *account_reply_string;
static Object *account_recv_server_string;
static Object *account_recv_port_string;
static Object *account_recv_login_string;
static Object *account_recv_password_string;
static Object *account_recv_active_check;
static Object *account_recv_delete_check;
static Object *account_recv_ssl_check;
static Object *account_send_server_string;
static Object *account_send_port_string;
static Object *account_send_login_string;
static Object *account_send_password_string;
static Object *account_send_auth_check;
static Object *account_send_pop3_check;
static Object *account_send_ip_check;
static Object *account_add_button;
static Object *account_remove_button;

static Object *signatures_use_checkbox;

static Object *signature_texteditor;
static Object *signature_name_string;

static Object *config_group;
static Object *config_tree;
static struct Hook config_construct_hook, config_destruct_hook, config_display_hook;

static APTR accounts_treenode;
static struct list account_list; /* nodes are struct account * */
static struct account *account_last_selected;

static APTR signatures_treenode;
static struct list signature_list;
static struct signature *signature_last_selected;

static Object *user_group;
static Object *tcpip_receive_group;
static Object *write_group;
static Object *accounts_group;
static Object *account_group;
static Object *mails_read_group;
static Object *signatures_group;
static Object *signature_group;

static Object *config_last_visisble_group;

/******************************************************************
 Gets the Account which was last selected
*******************************************************************/
static void get_account(void)
{
	if (account_last_selected)
	{
		/* Save the account if a server was selected */
		if (account_last_selected->name) free(account_last_selected->name);
		if (account_last_selected->email) free(account_last_selected->email);
		if (account_last_selected->reply) free(account_last_selected->reply);
		if (account_last_selected->pop->name) free(account_last_selected->pop->name);
		if (account_last_selected->pop->login) free(account_last_selected->pop->login);
		if (account_last_selected->pop->passwd) free(account_last_selected->pop->passwd);
		if (account_last_selected->smtp->name) free(account_last_selected->pop->name);
		if (account_last_selected->smtp->auth_login) free(account_last_selected->smtp->auth_login);
		if (account_last_selected->smtp->auth_password) free(account_last_selected->smtp->auth_password);

		account_last_selected->name = mystrdup((char*)xget(account_name_string, MUIA_String_Contents));
		account_last_selected->email = mystrdup((char*)xget(account_email_string, MUIA_String_Contents));
		account_last_selected->reply = mystrdup((char*)xget(account_reply_string, MUIA_String_Contents));
		account_last_selected->pop->name = mystrdup((char*)xget(account_recv_server_string, MUIA_String_Contents));
		account_last_selected->pop->login = mystrdup((char*)xget(account_recv_login_string, MUIA_String_Contents));
		account_last_selected->pop->passwd = mystrdup((char*)xget(account_recv_password_string, MUIA_String_Contents));
		account_last_selected->pop->active = xget(account_recv_active_check, MUIA_Selected);
		account_last_selected->pop->del = xget(account_recv_delete_check, MUIA_Selected);
		account_last_selected->pop->ssl = xget(account_recv_ssl_check, MUIA_Selected);
		account_last_selected->pop->port = xget(account_recv_port_string, MUIA_String_Integer);
		account_last_selected->smtp->port = xget(account_send_port_string, MUIA_String_Integer);
		account_last_selected->smtp->ip_as_domain = xget(account_send_ip_check, MUIA_Selected);
		account_last_selected->smtp->pop3_first = xget(account_send_pop3_check, MUIA_Selected);
		account_last_selected->smtp->name = mystrdup((char*)xget(account_send_server_string, MUIA_String_Contents));
		account_last_selected->smtp->auth = xget(account_send_auth_check, MUIA_Selected);
		account_last_selected->smtp->auth_login = mystrdup((char*)xget(account_send_login_string, MUIA_String_Contents));
		account_last_selected->smtp->auth_password = mystrdup((char*)xget(account_send_password_string, MUIA_String_Contents));
		account_last_selected = NULL;
	}
}

/******************************************************************
 Gets the Signature which was last selected
*******************************************************************/
static void get_signature(void)
{
	if (signature_last_selected)
	{
		char *text_buf;
		if (signature_last_selected->name) free(signature_last_selected->name);
		if (signature_last_selected->signature) free(signature_last_selected->signature);

		text_buf = (char*)DoMethod(signature_texteditor, MUIM_TextEditor_ExportText);

		signature_last_selected->name = mystrdup((char*)xget(signature_name_string,MUIA_String_Contents));
		signature_last_selected->signature = mystrdup(text_buf);
		if (text_buf) FreeVec(text_buf);
		signature_last_selected = NULL;
	}
}


/******************************************************************
 Use the config
*******************************************************************/
static void config_use(void)
{
	struct account *account;
	struct signature *signature;

	if (user.config.read_propfont) free(user.config.read_propfont);
	if (user.config.read_fixedfont) free(user.config.read_fixedfont);

	user.config.dst = xget(user_dst_check,MUIA_Selected);
	user.config.receive_preselection = xget(receive_preselection_radio,MUIA_Radio_Active);
	user.config.receive_size = value2size(xget(receive_sizes_sizes, MUIA_Numeric_Value));
	user.config.signatures_use = xget(signatures_use_checkbox, MUIA_Selected);
	user.config.read_propfont = mystrdup((char*)xget(read_propfont_string,MUIA_String_Contents));
	user.config.read_fixedfont = mystrdup((char*)xget(read_fixedfont_string,MUIA_String_Contents));
	user.config.read_background = ((read_palette_entries[0].mpe_Red >> 24)<<16) | ((read_palette_entries[0].mpe_Green>>24)<<8) | (read_palette_entries[0].mpe_Blue>>24);
	user.config.read_text = ((read_palette_entries[1].mpe_Red >> 24)<<16)       | ((read_palette_entries[1].mpe_Green>>24)<<8) | (read_palette_entries[1].mpe_Blue>>24);
	user.config.read_quoted = ((read_palette_entries[2].mpe_Red >> 24)<<16)     | ((read_palette_entries[2].mpe_Green>>24)<<8) | (read_palette_entries[2].mpe_Blue>>24);
	user.config.read_old_quoted = ((read_palette_entries[3].mpe_Red >> 24)<<16) | ((read_palette_entries[3].mpe_Green>>24)<<8) | (read_palette_entries[3].mpe_Blue>>24);
	user.config.read_link = ((read_palette_entries[4].mpe_Red >> 24)<<16)       | ((read_palette_entries[4].mpe_Green>>24)<<8) | (read_palette_entries[4].mpe_Blue>>24);
	user.config.read_wordwrap = xget(read_wrap_checkbox, MUIA_Selected);
	user.config.read_link_underlined = xget(read_linkunderlined_checkbox,MUIA_Selected);
	user.config.read_smilies = xget(read_smilies_checkbox, MUIA_Selected);
	user.config.write_welcome = mystrdup((char*)xget(write_welcome_popph,MUIA_Popph_Contents));
	user.config.write_close = mystrdup((char*)xget(write_close_string,MUIA_String_Contents));

  get_account();
  get_signature();

	/* Copy the accounts */
	clear_config_accounts();
	account = (struct account *)list_first(&account_list);
	while (account)
	{
		insert_config_account(account);
		account = (struct account *)node_next(&account->node);
	}

	/* Copy the signature */
	clear_config_signatures();
	signature = (struct signature*)list_first(&signature_list);
	while (signature)
	{
		insert_config_signature(signature);
		signature = (struct signature*)node_next(&signature->node);
	}

	close_config();
	callback_config_changed();
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

			get_account();
			get_signature();

			if (list_treenode == accounts_treenode)
			{
				int account_num = 0;
				struct account *account;
				APTR tn = treenode;

				/* Find out the position of the new selected account in the list */
				while ((tn = (APTR) DoMethod(config_tree, MUIM_NListtree_GetEntry, tn, MUIV_NListtree_GetEntry_Position_Previous,0)))
					account_num++;

				if ((account = (struct account*)list_find(&account_list,account_num)))
				{
					setstring(account_name_string,account->name);
					setstring(account_email_string,account->email);
					setstring(account_reply_string,account->reply);
					setstring(account_recv_server_string,account->pop->name);
					set(account_recv_port_string,MUIA_String_Integer,account->pop->port);
					setstring(account_recv_login_string,account->pop->login);
					setstring(account_recv_password_string,account->pop->passwd);
					setcheckmark(account_recv_active_check,account->pop->active);
					setcheckmark(account_recv_delete_check,account->pop->del);
					nnset(account_recv_ssl_check,MUIA_Selected,account->pop->ssl);
					setstring(account_send_server_string,account->smtp->name);
					set(account_send_port_string,MUIA_String_Integer,account->smtp->port);
					setstring(account_send_login_string,account->smtp->auth_login);
					setstring(account_send_password_string,account->smtp->auth_password);
					setcheckmark(account_send_auth_check,account->smtp->auth);
					setcheckmark(account_send_pop3_check,account->smtp->pop3_first);
					setcheckmark(account_send_ip_check,account->smtp->ip_as_domain);
				}
				account_last_selected = account;
			}

			if (list_treenode == signatures_treenode)
			{
				int signature_num = 0;
				struct signature *signature;
				APTR tn = treenode;

				/* Find out the position of the new selected account in the list */
				while ((tn = (APTR) DoMethod(config_tree, MUIM_NListtree_GetEntry, tn, MUIV_NListtree_GetEntry_Position_Previous,0)))
					signature_num++;

				if ((signature = (struct signature*)list_find(&signature_list,signature_num)))
				{
					setstring(signature_name_string,signature->name);
					set(signature_texteditor,MUIA_TextEditor_Contents, signature->signature?signature->signature:"");
				}
				signature_last_selected = signature;
				
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
	user_group = VGroup,
		MUIA_ShowMe, FALSE,
		Child, HGroup,
			Child, MakeLabel("Add adjustment for daylight saving time"),
			Child, user_dst_check = MakeCheck("Add adjustment for daylight saving time",user.config.dst),
			Child, HSpace(0),
			End,
		End;
	if (!user_group) return 0;
	return 1;
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
		End;

	if (!tcpip_receive_group) return 0;

	return 1;
}

/******************************************************************
 Initialize the write group
*******************************************************************/
static int init_write_group(void)
{
	static const char *write_popph_array[] =
	{
		"\\n|Line break",
		"%r|Recipient: Name",
		"%f|Recipient: First Name",
		"%a|Recipient: Address",
		NULL
	};
	write_group = ColGroup(2),
		MUIA_ShowMe, FALSE,
		Child, MakeLabel("Welcome phrase"),
		Child, write_welcome_popph = PopphObject,
			MUIA_Popph_Contents, user.config.write_welcome,
			MUIA_Popph_Array, write_popph_array,
			End,

/*		Child, MakeLabel("Welcome phrase with address"),
		Child, BetterStringObject,StringFrame,End,
*/
		Child, MakeLabel("Closing phrase"),
		Child, write_close_string = BetterStringObject,
			StringFrame,
			MUIA_String_Contents, user.config.write_close,
			End,
		End;
	if (!write_group) return 0;
	return 1;
}

/******************************************************************
 Add a new account
*******************************************************************/
static void account_add(void)
{
	struct account *account = account_malloc();
	if (account)
	{
		list_insert_tail(&account_list, &account->node);
		DoMethod(config_tree, MUIM_NListtree_Insert, "Account", account_group, accounts_treenode, MUIV_NListtree_Insert_PrevNode_Tail, MUIV_NListtree_Insert_Flag_Active);
	}
}

/******************************************************************
 Remove the current account
*******************************************************************/
static void account_remove(void)
{
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)xget(config_tree, MUIA_NListtree_Active);
	if (treenode && !(treenode->tn_Flags & TNF_LIST))
	{
		struct account *account;
		APTR tn = treenode;
		int account_num = 0;

		/* Find out the position of the new selected server in the list */
		while ((tn = (APTR) DoMethod(config_tree, MUIM_NListtree_GetEntry,tn,MUIV_NListtree_GetEntry_Position_Previous,0)))
			account_num++;

		if ((account = (struct account*)list_find(&account_list,account_num)))
		{
			node_remove(&account->node);
			account_free(account);

			DoMethod(config_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Active, MUIV_NListtree_Remove_TreeNode_Active,0);
		}
	}
}


/******************************************************************
 
*******************************************************************/
static int init_accounts_group(void)
{
	Object *add;
	accounts_group = HGroup,
		MUIA_ShowMe, FALSE,
		Child, add = MakeButton("Add new account"),
		End;

	if (!accounts_group) return 0;

	DoMethod(add, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, account_add);

	return 1;
}

/******************************************************************
 Initalize the account group
*******************************************************************/
static int init_account_group(void)
{
	account_group = VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_Weight, 10000,

		Child, VGroupV,
		MUIA_Weight, 10000,

		VirtualFrame,
		Child, HorizLineTextObject("User"),
		Child, ColGroup(2),
			Child, MakeLabel("Name"),
			Child, account_name_string = BetterStringObject,
				StringFrame,
				MUIA_CycleChain, 1,
				MUIA_String_Contents, NULL,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
			Child, MakeLabel("E-Mail Address"),
			Child, account_email_string = BetterStringObject,
				StringFrame,
				MUIA_CycleChain, 1,
				MUIA_String_Contents, NULL,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
			Child, MakeLabel("Reply Address"),
			Child, account_reply_string = BetterStringObject,
				StringFrame,
				MUIA_CycleChain, 1,
				MUIA_String_Contents, NULL,
				MUIA_String_AdvanceOnCR, TRUE,
				End,
			End,
		Child, HorizLineTextObject("Receive"),
		Child, ColGroup(2),
			Child, MakeLabel("POP3 Server"),
			Child, HGroup,
				Child, account_recv_server_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_String_AdvanceOnCR, TRUE,
				End,
				Child, MakeLabel("Port"),
				Child, account_recv_port_string = BetterStringObject,
					StringFrame,
					MUIA_Weight, 33,
					MUIA_CycleChain,1,
					MUIA_String_AdvanceOnCR, TRUE,
					MUIA_String_Accept, "0123456789",
					End,
				End,
			Child, MakeLabel("Login"),
			Child,  HGroup,
				Child, account_recv_login_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_String_AdvanceOnCR, TRUE,
					End,
				Child, MakeLabel("Password"),
				Child, account_recv_password_string = BetterStringObject,
					StringFrame,
					MUIA_CycleChain, 1,
					MUIA_String_Secret, TRUE,
					MUIA_String_AdvanceOnCR, TRUE,
					End,
				End,
			End,
		Child, HGroup,
			Child, MakeLabel("Active"),
			Child, HGroup, Child, account_recv_active_check = MakeCheck("Active", FALSE), Child, HSpace(0), End,
			Child, MakeLabel("_Delete mails"),
			Child, HGroup, Child, account_recv_delete_check = MakeCheck("_Delete mails", FALSE), Child, HSpace(0), End,
			Child, MakeLabel("Use SSL"),
			Child, HGroup, Child, account_recv_ssl_check = MakeCheck("Use SSL", FALSE), Child, HSpace(0), End,
			Child, HVSpace,
			End,

		Child, HorizLineTextObject("Send"),
		Child, VGroup,
			Child, ColGroup(2),

/*
				Child, MakeLabel("Domain"),
				Child, HGroup,
					Child, BetterStringObject,
						StringFrame,
						MUIA_Disabled, user.config.smtp_ip_as_domain,
						MUIA_CycleChain, 1,
						MUIA_String_Contents, user.config.smtp_domain,
						MUIA_String_AdvanceOnCR, TRUE,
						End,
					Child, MakeLabel("Use IP as domain"),
					Child, MakeCheck("Use IP as domain",user.config.smtp_ip_as_domain),
					End,
*/
				Child, MakeLabel("SMTP Server"),
				Child, HGroup,
					Child, account_send_server_string = BetterStringObject,
						StringFrame,
						MUIA_CycleChain, 1,
						MUIA_String_AdvanceOnCR, TRUE,
						End,
					Child, MakeLabel("Port"),
					Child, account_send_port_string = BetterStringObject,
						StringFrame,
						MUIA_Weight, 33,
						MUIA_CycleChain,1,
						MUIA_String_AdvanceOnCR,TRUE,
						MUIA_String_Accept,"0123456789",
						End,
					End,
				Child, MakeLabel("Login"),
				Child, HGroup,
					Child, account_send_login_string = BetterStringObject,
						StringFrame,
						MUIA_Disabled, TRUE,
						MUIA_CycleChain, 1,
						MUIA_String_AdvanceOnCR, TRUE,
						End,
					Child, MakeLabel("Password"),
					Child, account_send_password_string = BetterStringObject,
						StringFrame,
						MUIA_Disabled, TRUE,
						MUIA_CycleChain, 1,
						MUIA_String_AdvanceOnCR, TRUE,
						MUIA_String_Secret, TRUE,
						End,
					End,
				End,
			Child, HGroup,
				Child, MakeLabel("Use SMTP AUTH"),
				Child, account_send_auth_check = MakeCheck("Use SMTP AUTH", FALSE),
				Child, HVSpace,
				Child, MakeLabel("Log into pop3 server first"),
				Child, account_send_pop3_check = MakeCheck("Log into pop3 server first",FALSE),
				Child, HVSpace,
				Child, MakeLabel("Use IP as domain"),
				Child, account_send_ip_check = MakeCheck("Use IP as domain", FALSE),
				Child, HVSpace,

				End,
			End,
		End,

		Child, HGroup,
			Child, account_add_button = MakeButton("Add new account"),
			Child, account_remove_button = MakeButton("Remove account"),
			End,
		End;

	if (!account_group) return 0;

	DoMethod(account_send_auth_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, account_send_login_string, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
	DoMethod(account_send_auth_check, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, account_send_password_string, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
	DoMethod(account_send_auth_check, MUIM_Notify, MUIA_Selected, TRUE, MUIV_Notify_Window, 3, MUIM_Set, MUIA_Window_ActiveObject, account_send_login_string);

	DoMethod(account_recv_ssl_check, MUIM_Notify, MUIA_Selected, TRUE, account_recv_port_string, 3, MUIM_Set, MUIA_String_Integer, 995);
	DoMethod(account_recv_ssl_check, MUIM_Notify, MUIA_Selected, FALSE, account_recv_port_string, 3, MUIM_Set, MUIA_String_Integer, 110);

	DoMethod(account_add_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, account_add);
	DoMethod(account_remove_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, account_remove);

	return 1;
}

/******************************************************************
 Init the read group
*******************************************************************/
static int init_mails_read_group(void)
{
	static const char *read_palette_names[] =
	{
		"Background",
		"Text",
		"Quoted Text",
		"Old Quoted Text",
		"Link Text",
		NULL,
	};

	read_palette_entries[0].mpe_ID = 0;
	read_palette_entries[0].mpe_Red = MAKECOLOR32((user.config.read_background&0x00ff0000)>>16);
	read_palette_entries[0].mpe_Green = MAKECOLOR32((user.config.read_background&0x0000ff00)>>8);
	read_palette_entries[0].mpe_Blue = MAKECOLOR32((user.config.read_background&0x000000ff));
	read_palette_entries[0].mpe_Group = 0;

	read_palette_entries[1].mpe_ID = 1;
	read_palette_entries[1].mpe_Red = MAKECOLOR32((user.config.read_text&0x00ff0000)>>16);
	read_palette_entries[1].mpe_Green = MAKECOLOR32((user.config.read_text&0x0000ff00)>>8);
	read_palette_entries[1].mpe_Blue = MAKECOLOR32((user.config.read_text&0x00ff));
	read_palette_entries[1].mpe_Group = 1;

	read_palette_entries[2].mpe_ID = 2;
	read_palette_entries[2].mpe_Red = MAKECOLOR32((user.config.read_quoted&0x00ff0000)>>16);
	read_palette_entries[2].mpe_Green = MAKECOLOR32((user.config.read_quoted&0x0000ff00)>>8);
	read_palette_entries[2].mpe_Blue = MAKECOLOR32((user.config.read_quoted&0x000000ff));
	read_palette_entries[2].mpe_Group = 2;

	read_palette_entries[3].mpe_ID = 3;
	read_palette_entries[3].mpe_Red = MAKECOLOR32((user.config.read_old_quoted&0x00ff0000)>>16);
	read_palette_entries[3].mpe_Green = MAKECOLOR32((user.config.read_old_quoted&0x0000ff00)>>8);
	read_palette_entries[3].mpe_Blue = MAKECOLOR32((user.config.read_old_quoted&0x0000ff));
	read_palette_entries[3].mpe_Group = 3;

	read_palette_entries[4].mpe_ID = 4;
	read_palette_entries[4].mpe_Red = MAKECOLOR32((user.config.read_link&0x00ff0000)>>16);
	read_palette_entries[4].mpe_Green = MAKECOLOR32((user.config.read_link&0x0000ff00)>>8);
	read_palette_entries[4].mpe_Blue = MAKECOLOR32((user.config.read_link&0x0000ff));
	read_palette_entries[4].mpe_Group = 4;

	read_palette_entries[5].mpe_ID = MUIV_Palette_Entry_End;
	read_palette_entries[5].mpe_Red = 0;
	read_palette_entries[5].mpe_Green = 0;
	read_palette_entries[5].mpe_Blue = 0;
	read_palette_entries[5].mpe_Group = 0;

	mails_read_group =  VGroup,
		MUIA_ShowMe, FALSE,

		Child, HorizLineTextObject("Fonts"),
		Child, ColGroup(2),
			Child, MakeLabel("Proportional Font"),
			Child, PopaslObject,
				MUIA_Popasl_Type, ASL_FontRequest,
				MUIA_Popstring_String, read_propfont_string = BetterStringObject, StringFrame, MUIA_String_Contents, user.config.read_propfont,End,
				MUIA_Popstring_Button, PopButton(MUII_PopUp),
				End,

			Child, MakeLabel("Fixed Font"),
			Child, PopaslObject,
				MUIA_Popasl_Type, ASL_FontRequest,
				MUIA_Popstring_String, read_fixedfont_string = BetterStringObject, StringFrame, MUIA_String_Contents, user.config.read_fixedfont, End,
				MUIA_Popstring_Button, PopButton(MUII_PopUp),
				ASLFO_FixedWidthOnly, TRUE,
				End,
			End,

		Child, HorizLineTextObject("Colors"),
		Child, read_palette = PaletteObject,
			MUIA_Palette_Entries, read_palette_entries,
			MUIA_Palette_Names  , read_palette_names,
			MUIA_Palette_Groupable, FALSE,
			End,

		Child, HorizLineObject,

		Child, HGroup,
			Child, MakeLabel("Wordwrap plain text"),
			Child, read_wrap_checkbox = MakeCheck("Wordwrap plain text",user.config.read_wordwrap),
			Child, MakeLabel("Underline links"),
			Child, read_linkunderlined_checkbox = MakeCheck("Underline links",user.config.read_link_underlined),
			Child, MakeLabel("Use graphical smilies"),
			Child, read_smilies_checkbox = MakeCheck("Use graphical smilies",user.config.read_smilies),
			Child, HVSpace,
			End,
		End;

	if (!mails_read_group) return 0;
	return 1;
}

/******************************************************************
 Add a new signature
*******************************************************************/
static void signature_add(void)
{
	struct signature *s = signature_malloc();
	if (s)
	{
		list_insert_tail(&signature_list, &s->node);
		DoMethod(config_tree, MUIM_NListtree_Insert, "Signature", signature_group, signatures_treenode, MUIV_NListtree_Insert_PrevNode_Tail, MUIV_NListtree_Insert_Flag_Active);
	}
}

/******************************************************************
 Remove a signature
*******************************************************************/
static void signature_remove(void)
{
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)xget(config_tree, MUIA_NListtree_Active);
	if (treenode && !(treenode->tn_Flags & TNF_LIST))
	{
		struct signature *signature;
		APTR tn = treenode;
		int signature_num = 0;

		/* Find out the position of the new selected server in the list */
		while ((tn = (APTR) DoMethod(config_tree, MUIM_NListtree_GetEntry,tn,MUIV_NListtree_GetEntry_Position_Previous,0)))
			signature_num++;

		if ((signature = (struct signature*)list_find(&signature_list,signature_num)))
		{
			node_remove(&signature->node);
			signature_free(signature);

			DoMethod(config_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Active, MUIV_NListtree_Remove_TreeNode_Active,0);
		}
	}

}

/******************************************************************
 Init the signatures group
*******************************************************************/
static int init_signatures_group(void)
{
	Object *add_button;
	signatures_group =  VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_Weight,300,
  	Child, VGroup,
  		Child, HGroup,
  			Child, MakeLabel("Us_e signatures"),
  			Child, signatures_use_checkbox = MakeCheck("_Use signatures",user.config.signatures_use),
  			Child, HSpace(0),
	  		End,
	  	Child, HorizLineObject,
			Child, add_button = MakeButton("_Add new signature"),
			End,
		End;

	if (!signatures_group) return 0;

	DoMethod(add_button,MUIM_Notify, MUIA_Pressed,FALSE,App,6,MUIM_Application_PushMethod,App,3,MUIM_CallHook,&hook_standard, signature_add);

	return 1;
}

/******************************************************************
 Init the signature group
*******************************************************************/
static int init_signature_group(void)
{
/*	Object *edit_button;*/
	Object *slider = ScrollbarObject, End;
/*
	Object *tagline_button;
	Object *env_button;
*/
	Object *add_button, *rem_button;

	signature_group =  VGroup,
		MUIA_ShowMe, FALSE,
		MUIA_Weight,300,
		Child, HGroup,
			Child, MakeLabel("Name"),
			Child, signature_name_string = BetterStringObject, StringFrame, End,
/*			Child, edit_button = MakeButton("Edit in external editor"),*/
			End,
		Child, HGroup,
			MUIA_Group_Spacing, 0,
			Child, signature_texteditor = TextEditorObject,
				InputListFrame,
				MUIA_CycleChain, 1,
				MUIA_TextEditor_Slider, slider,
				MUIA_TextEditor_FixedFont, TRUE,
				End,
			Child, slider,
			End,
/*		Child, HGroup,
  		Child, tagline_button = MakeButton("Insert random tagline"),
  		Child, env_button = MakeButton("Insert ENV:Signature"),
  		End,*/
  	Child, HorizLineObject,
  	Child, HGroup,
			Child, add_button = MakeButton("Add new signature"),
			Child, rem_button = MakeButton("Remove signature"),
			End,
		End;

	if (!signature_group) return 0;
/*	set(edit_button, MUIA_Weight,0);*/

/*	DoMethod(tagline_button,MUIM_Notify,MUIA_Pressed,FALSE,signature_texteditor,3,MUIM_TextEditor_InsertText,"%t",MUIV_TextEditor_InsertText_Cursor);
	DoMethod(env_button,MUIM_Notify,MUIA_Pressed,FALSE,signature_texteditor,3,MUIM_TextEditor_InsertText,"%e",MUIV_TextEditor_InsertText_Cursor);
*/
	DoMethod(add_button,MUIM_Notify, MUIA_Pressed,FALSE,App,6,MUIM_Application_PushMethod,App,3,MUIM_CallHook,&hook_standard, signature_add);
	DoMethod(rem_button,MUIM_Notify, MUIA_Pressed,FALSE,App,6,MUIM_Application_PushMethod,App,3,MUIM_CallHook,&hook_standard, signature_remove);
	return 1;
}

/******************************************************************
 Init the config window
*******************************************************************/
static void init_config(void)
{
	Object *save_button, *use_button, *cancel_button;

	if (!create_sizes_class()) return;

	init_accounts_group();
	init_account_group();
	init_user_group();
	init_tcpip_receive_group();
	init_mails_read_group();
	init_signatures_group();
	init_signature_group();
	init_write_group();

	config_wnd = WindowObject,
		MUIA_Window_ID, MAKE_ID('C','O','N','F'),
    MUIA_Window_Title, "SimpleMail - Configure",
    WindowContents, VGroup,
    	Child, HGroup,
    		Child, NListviewObject,
    			MUIA_HorizWeight, 33,
    			MUIA_NListview_NList, config_tree = ConfigTreelistObject,
    				End,
    			End,
    		Child, BalanceObject, End,
    		Child, VGroup,
	    		Child, config_group = VGroup,
  	  			Child, user_group,
  	  			Child, accounts_group,
  	  			Child, account_group,
    				Child, tcpip_receive_group,
    				Child, mails_read_group,
    				Child, signatures_group,
    				Child, signature_group,
    				Child, write_group,
    				Child, RectangleObject,
	   				MUIA_Weight, 1,
  						End,
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
		APTR treenode;

		list_init(&account_list);
		list_init(&signature_list);
		account_last_selected = NULL;
		signature_last_selected = NULL;

		DoMethod(App, OM_ADDMEMBER, config_wnd);
		DoMethod(config_wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, close_config);
		DoMethod(cancel_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, close_config);
		DoMethod(use_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, config_use);
		DoMethod(save_button, MUIM_Notify, MUIA_Pressed, FALSE, App, 6, MUIM_Application_PushMethod, App, 3, MUIM_CallHook, &hook_standard, config_save);
		DoMethod(config_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, App, 3, MUIM_CallHook, &hook_standard,config_tree_active);

		DoMethod(config_tree, MUIM_NListtree_Insert, "User", user_group, MUIV_NListtree_Insert_ListNode_Root, MUIV_NListtree_Insert_PrevNode_Tail, MUIV_NListtree_Insert_Flag_Active);

		if ((treenode = accounts_treenode = (APTR)DoMethod(config_tree, MUIM_NListtree_Insert, "Accounts", accounts_group, MUIV_NListtree_Insert_ListNode_Root, MUIV_NListtree_Insert_PrevNode_Tail, TNF_LIST|TNF_OPEN)))
		{
			struct account *account;
			account = (struct account*)list_first(&user.config.account_list);

			while (account)
			{
				struct account *new_account = account_duplicate(account);
				if (new_account)
				{
					list_insert_tail(&account_list,&new_account->node);
					DoMethod(config_tree, MUIM_NListtree_Insert, "Account", account_group, treenode, MUIV_NListtree_Insert_PrevNode_Tail,0);
				}
				account = (struct account*)node_next(&account->node);
			}
		}

		DoMethod(config_tree, MUIM_NListtree_Insert, "Receive mail", tcpip_receive_group, NULL, MUIV_NListtree_Insert_PrevNode_Tail, 0);

		if ((treenode = (APTR)DoMethod(config_tree, MUIM_NListtree_Insert, "Mails", NULL, MUIV_NListtree_Insert_ListNode_Root, MUIV_NListtree_Insert_PrevNode_Tail, TNF_LIST|TNF_OPEN)))
		{
			DoMethod(config_tree, MUIM_NListtree_Insert, "Read", mails_read_group, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			DoMethod(config_tree, MUIM_NListtree_Insert, "Write", write_group, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			DoMethod(config_tree, MUIM_NListtree_Insert, "Reply", NULL, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
			DoMethod(config_tree, MUIM_NListtree_Insert, "Forward", NULL, treenode, MUIV_NListtree_Insert_PrevNode_Tail, 0);
		}

		if ((treenode = signatures_treenode = (APTR)DoMethod(config_tree, MUIM_NListtree_Insert, "Signatures", signatures_group, NULL, MUIV_NListtree_Insert_PrevNode_Tail, TNF_LIST|TNF_OPEN)))
		{
			struct signature *signature;
			signature = (struct signature*)list_first(&user.config.signature_list);

			while (signature)
			{
				struct signature *new_signature = signature_duplicate(signature);
				if (new_signature)
				{
					list_insert_tail(&signature_list,&new_signature->node);
					DoMethod(config_tree, MUIM_NListtree_Insert, "Signature", signature_group, treenode, MUIV_NListtree_Insert_PrevNode_Tail,0);
				}
				signature = (struct signature*)node_next(&signature->node);
			}
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
	struct account *account;

	if (config_wnd)
	{
		set(config_wnd, MUIA_Window_Open, FALSE);
		DoMethod(App, OM_REMMEMBER, config_wnd);
		MUI_DisposeObject(config_wnd);
		config_wnd = NULL;
		config_last_visisble_group = NULL;

		while ((account = (struct account*)list_remove_tail(&account_list)))
			account_free(account);
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

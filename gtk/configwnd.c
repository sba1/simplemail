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

#include <gtk/gtk.h>

#include "account.h"
#include "configuration.h"
#include "lists.h"
#include "pop3.h"
#include "smintl.h"
#include "support_indep.h"

#define SAFESTR(x) ((x)?(x):"")

#define ACCOUNT_POINTER 0
#define ACCOUNT_EMAIL   1
#define ACCOUNT_POP3    2
#define ACCOUNT_SMTP    3

static GtkWidget *config_wnd;

static GtkWidget *config_account_box;
static GtkWidget *config_account_tree;
static GtkWidget *config_account_liststore;
static GtkWidget *config_account_add_button;
static GtkWidget *config_account_remove_button;
static GtkWidget *config_account_user_name_text;
static GtkWidget *config_account_user_email_text;
static GtkWidget *config_account_user_reply_text;
static GtkWidget *config_account_receive_server_text;
static GtkWidget *config_account_receive_login_text;
static GtkWidget *config_account_receive_password_text;
static GtkWidget *config_account_receive_ask_checkbox;
static GtkObject *config_account_receive_port_spin_adj;
static GtkWidget *config_account_receive_port_spin;
static GtkWidget *config_account_receive_active_checkbox;
static GtkWidget *config_account_receive_remove_checkbox;
static GtkWidget *config_account_receive_avoid_checkbox;
static GtkWidget *config_account_receive_secure_checkbox;
static GtkWidget *config_account_receive_stls_checkbox;
static GtkObject *config_account_send_port_spin_adj;
static GtkWidget *config_account_send_port_spin;
static GtkWidget *config_account_send_server_text;
static GtkWidget *config_account_send_login_text;
static GtkWidget *config_account_send_password_text;
static GtkWidget *config_account_send_auth_checkbox;
static GtkWidget *config_account_send_ip_checkbox;
static GtkWidget *config_account_send_secure_checkbox;
static GtkWidget *config_account_send_login_checkbox;

/* The currently selected account, maybe NULL */
static struct account *account_selected;


#if 0

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
#endif

static void account_cell_data_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	struct account *ac;
	gtk_tree_model_get(model,iter,ACCOUNT_POINTER,&ac,-1);
	char *text;

	switch ((int)data)
	{
		case	ACCOUNT_EMAIL:
			text = ac->email;
			break;
		case	ACCOUNT_POP3:
			text = ac->pop->name;
			break;
		case	ACCOUNT_SMTP:
			text = ac->smtp->name;
			break;

	}
	if (!text) text = "-";
	g_object_set(cell,"text",text, NULL);
}

static void add_account(struct account *account, int make_active)
{
	GtkTreeIter iter;
	GtkTreePath *path;

	gtk_list_store_append(config_account_liststore,&iter);
	gtk_list_store_set(config_account_liststore,&iter, ACCOUNT_POINTER, account, -1);
	
	if (make_active)
	{
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(config_account_liststore),&iter);
		gtk_tree_view_set_cursor(config_account_tree,path,NULL,0);
	}
}

static void store_account(struct account *account)
{
	if (!account) return;

	account->name = mystrdup(gtk_entry_get_text(GTK_ENTRY(config_account_user_name_text)));
	account->email = mystrdup(gtk_entry_get_text(GTK_ENTRY(config_account_user_email_text)));
	account->reply = mystrdup(gtk_entry_get_text(GTK_ENTRY(config_account_user_reply_text)));
	account->pop->name = mystrdup(gtk_entry_get_text(GTK_ENTRY(config_account_receive_server_text)));
	account->pop->login = mystrdup(gtk_entry_get_text(GTK_ENTRY(config_account_receive_login_text)));
	account->pop->ask = gtk_toggle_button_get_active(GTK_WIDGET(config_account_receive_ask_checkbox));
	account->pop->passwd = account->pop->ask?NULL:mystrdup(gtk_entry_get_text(GTK_WIDGET(config_account_receive_password_text)));
	account->pop->active = gtk_toggle_button_get_active(GTK_WIDGET(config_account_receive_active_checkbox));
	account->pop->del = gtk_toggle_button_get_active(GTK_WIDGET(config_account_receive_remove_checkbox));
	account->pop->ssl = gtk_toggle_button_get_active(GTK_WIDGET(config_account_receive_secure_checkbox));
	account->pop->stls = gtk_toggle_button_get_active(GTK_WIDGET(config_account_receive_stls_checkbox));
	account->pop->nodupl = gtk_toggle_button_get_active(GTK_WIDGET(config_account_receive_avoid_checkbox));
	account->pop->port = gtk_spin_button_get_value_as_int(GTK_WIDGET(config_account_receive_port_spin));
	account->smtp->port = gtk_spin_button_get_value_as_int(GTK_WIDGET(config_account_send_port_spin));
	account->smtp->ip_as_domain = gtk_toggle_button_get_active(GTK_WIDGET(config_account_send_ip_checkbox));
	account->smtp->pop3_first = gtk_toggle_button_get_active(GTK_WIDGET(config_account_send_login_checkbox));
	account->smtp->secure = gtk_toggle_button_get_active(GTK_WIDGET(config_account_send_secure_checkbox));
	account->smtp->name = mystrdup(gtk_entry_get_text(GTK_WIDGET(config_account_send_server_text)));
	account->smtp->auth = gtk_toggle_button_get_active(GTK_WIDGET(config_account_send_auth_checkbox));
	account->smtp->auth_login = mystrdup(gtk_entry_get_text(GTK_WIDGET(config_account_send_login_text)));
	account->smtp->auth_password = mystrdup(gtk_entry_get_text(GTK_WIDGET(config_account_send_password_text)));
}

static void get_account(struct account *account)
{
	if (!account) return;

	gtk_entry_set_text(GTK_ENTRY(config_account_user_name_text),SAFESTR(account->name));
	gtk_entry_set_text(GTK_ENTRY(config_account_user_email_text),SAFESTR(account->email));
	gtk_entry_set_text(GTK_ENTRY(config_account_user_reply_text),SAFESTR(account->reply));
	gtk_entry_set_text(GTK_ENTRY(config_account_receive_server_text),SAFESTR(account->pop->name));
	gtk_entry_set_text(GTK_ENTRY(config_account_receive_login_text),SAFESTR(account->pop->login));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_receive_ask_checkbox),account->pop->ask);
	gtk_entry_set_text(GTK_ENTRY(config_account_receive_password_text),SAFESTR(account->pop->passwd));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_receive_remove_checkbox),account->pop->del);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_receive_secure_checkbox),account->pop->ssl);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_receive_stls_checkbox),account->pop->stls);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_receive_avoid_checkbox),account->pop->nodupl);
	gtk_spin_button_set_value(GTK_WIDGET(config_account_receive_port_spin),account->pop->port);
	gtk_entry_set_text(GTK_ENTRY(config_account_send_server_text),SAFESTR(account->smtp->name));
	gtk_spin_button_set_value(GTK_WIDGET(config_account_send_port_spin),account->smtp->port);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_send_ip_checkbox),account->smtp->ip_as_domain);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_send_login_checkbox),account->smtp->pop3_first);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_send_secure_checkbox),account->smtp->secure);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_send_auth_checkbox),account->smtp->auth);
	gtk_entry_set_text(GTK_ENTRY(config_account_send_login_text),SAFESTR(account->smtp->auth_login));
	gtk_entry_set_text(GTK_ENTRY(config_account_send_password_text),SAFESTR(account->smtp->auth_password));
}

static void on_config_account_tree_cursor_changed(void)
{
	GtkTreeSelection *sel;

	store_account(account_selected);

	sel = gtk_tree_view_get_selection(config_account_tree);
	if (sel)
	{
		GtkTreeModel *model;
		GtkTreeIter iter;

		if (gtk_tree_selection_get_selected(sel, &model, &iter))
		{
			struct account *ac;
			gtk_tree_model_get(model,&iter,ACCOUNT_POINTER,&ac,-1);
			account_selected = ac;
			get_account(ac);
		}
	}

}

static void on_config_account_add_button_clicked(GtkButton *button, gpointer user_data)
{
	struct account *ac = account_malloc();
	if (account_selected)
	{
		store_account(account_selected);
		ac->name = mystrdup(account_selected->name);
	}
	add_account(ac,1);
}

static void on_config_account_remove_button_clicked(GtkButton *button, gpointer user_data)
{
	printf("remove\n");
}

static void init_account_group(void)
{
  GtkWidget *hbox4;
  GtkWidget *scrolledwindow2;
  GtkWidget *vbuttonbox1;
  GtkWidget *alignment1;
  GtkWidget *hbox5;
  GtkWidget *image1;
  GtkWidget *label1;
  GtkWidget *alignment2;
  GtkWidget *hbox6;
  GtkWidget *image2;
  GtkWidget *label2;
  GtkWidget *frame5;
  GtkWidget *table1;
  GtkWidget *label8;
  GtkWidget *label9;
  GtkWidget *label7;
  GtkWidget *label6;
  GtkWidget *frame3;
  GtkWidget *table2;
  GtkWidget *label10;
  GtkWidget *label11;
  GtkWidget *hbox7;
  GtkWidget *label13;
  GtkWidget *hbox8;
  GtkWidget *label12;
  GtkWidget *hbox9;
  GtkWidget *label4;
  GtkWidget *frame4;
  GtkWidget *table3;
  GtkWidget *label14;
  GtkWidget *label15;
  GtkWidget *hbox13;
  GtkWidget *label17;
  GtkWidget *hbox10;
  GtkWidget *hbox11;
  GtkWidget *label16;
  GtkWidget *label5;

  config_account_box = gtk_vbox_new (FALSE, 0);

  hbox4 = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (config_account_box), hbox4, TRUE, TRUE, 0);

  scrolledwindow2 = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox4), scrolledwindow2, TRUE, TRUE, 0);

  config_account_tree = gtk_tree_view_new ();
  gtk_container_add (GTK_CONTAINER (scrolledwindow2), config_account_tree);

  /* Create account tree stuff */
	{
		gint col_offset;
		GtkTreeViewColumn *column;
		GtkCellRenderer *renderer;

		config_account_liststore = gtk_list_store_new(1, G_TYPE_POINTER);
		gtk_tree_view_set_model(GTK_TREE_VIEW(config_account_tree),GTK_TREE_MODEL(config_account_liststore));

		renderer = gtk_cell_renderer_text_new();
		col_offset = gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(config_account_tree),-1,"EMail",renderer,account_cell_data_func,ACCOUNT_EMAIL,NULL);
		col_offset = gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(config_account_tree),-1,"POP3",renderer,account_cell_data_func,ACCOUNT_POP3,NULL);
		col_offset = gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(config_account_tree),-1,"SMTP",renderer,account_cell_data_func,ACCOUNT_SMTP,NULL);
	}

  vbuttonbox1 = gtk_vbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (hbox4), vbuttonbox1, FALSE, TRUE, 4);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox1), GTK_BUTTONBOX_SPREAD);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (vbuttonbox1), 0);

  config_account_add_button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), config_account_add_button);
  GTK_WIDGET_SET_FLAGS (config_account_add_button, GTK_CAN_DEFAULT);

  alignment1 = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_container_add (GTK_CONTAINER (config_account_add_button), alignment1);

  hbox5 = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (alignment1), hbox5);

  image1 = gtk_image_new_from_stock ("gtk-add", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (hbox5), image1, FALSE, FALSE, 0);

  label1 = gtk_label_new_with_mnemonic (_("Add Account"));
  gtk_box_pack_start (GTK_BOX (hbox5), label1, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label1), GTK_JUSTIFY_LEFT);

  config_account_remove_button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (vbuttonbox1), config_account_remove_button);
  GTK_WIDGET_SET_FLAGS (config_account_remove_button, GTK_CAN_DEFAULT);

  alignment2 = gtk_alignment_new (0.5, 0.5, 0, 0);
  gtk_container_add (GTK_CONTAINER (config_account_remove_button), alignment2);

  hbox6 = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (alignment2), hbox6);

  image2 = gtk_image_new_from_stock ("gtk-remove", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (hbox6), image2, FALSE, FALSE, 0);

  label2 = gtk_label_new_with_mnemonic (_("Remove Account"));
  gtk_box_pack_start (GTK_BOX (hbox6), label2, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_LEFT);

  frame5 = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (config_account_box), frame5, FALSE, TRUE, 0);

  table1 = gtk_table_new (3, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (frame5), table1);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table1), 2);

  config_account_user_name_text = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table1), config_account_user_name_text, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  config_account_user_email_text = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table1), config_account_user_email_text, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  config_account_user_reply_text = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table1), config_account_user_reply_text, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  label8 = gtk_label_new (_("EMail Address"));
  gtk_table_attach (GTK_TABLE (table1), label8, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label8), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label8), 0, 0.5);

  label9 = gtk_label_new (_("Replyaddress"));
  gtk_table_attach (GTK_TABLE (table1), label9, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label9), GTK_JUSTIFY_RIGHT);
  gtk_misc_set_alignment (GTK_MISC (label9), 0, 0.5);

  label7 = gtk_label_new (_("Name"));
  gtk_table_attach (GTK_TABLE (table1), label7, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label7), GTK_JUSTIFY_RIGHT);
  gtk_misc_set_alignment (GTK_MISC (label7), 0, 0.5);

  label6 = gtk_label_new (_("User"));
  gtk_frame_set_label_widget (GTK_FRAME (frame5), label6);
  gtk_label_set_justify (GTK_LABEL (label6), GTK_JUSTIFY_LEFT);

  frame3 = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (config_account_box), frame3, FALSE, TRUE, 0);

  table2 = gtk_table_new (3, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (frame3), table2);
  gtk_table_set_row_spacings (GTK_TABLE (table2), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table2), 2);

  label10 = gtk_label_new (_("POP3 Server"));
  gtk_table_attach (GTK_TABLE (table2), label10, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label10), GTK_JUSTIFY_RIGHT);
  gtk_misc_set_alignment (GTK_MISC (label10), 0, 0.5);

  label11 = gtk_label_new (_("Login"));
  gtk_widget_show (label11);
  gtk_table_attach (GTK_TABLE (table2), label11, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label11), GTK_JUSTIFY_RIGHT);
  gtk_misc_set_alignment (GTK_MISC (label11), 0, 0.5);

  hbox7 = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox7);
  gtk_table_attach (GTK_TABLE (table2), hbox7, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  config_account_receive_server_text = gtk_entry_new ();
  gtk_widget_show (config_account_receive_server_text);
  gtk_box_pack_start (GTK_BOX (hbox7), config_account_receive_server_text, TRUE, TRUE, 0);

  label13 = gtk_label_new (_("Port"));
  gtk_widget_show (label13);
  gtk_box_pack_start (GTK_BOX (hbox7), label13, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label13), GTK_JUSTIFY_LEFT);

  config_account_receive_port_spin_adj = gtk_adjustment_new (110, 1, 65536, 1, 10, 10);
  config_account_receive_port_spin = gtk_spin_button_new (GTK_ADJUSTMENT (config_account_receive_port_spin_adj), 1, 0);
  gtk_widget_show (config_account_receive_port_spin);
  gtk_box_pack_start (GTK_BOX (hbox7), config_account_receive_port_spin, FALSE, TRUE, 0);

  config_account_receive_active_checkbox = gtk_check_button_new_with_mnemonic (_("Active"));
  gtk_widget_show (config_account_receive_active_checkbox);
  gtk_box_pack_start (GTK_BOX (hbox7), config_account_receive_active_checkbox, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (config_account_receive_active_checkbox), TRUE);

  hbox8 = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox8);
  gtk_table_attach (GTK_TABLE (table2), hbox8, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  config_account_receive_login_text = gtk_entry_new ();
  gtk_widget_show (config_account_receive_login_text);
  gtk_box_pack_start (GTK_BOX (hbox8), config_account_receive_login_text, TRUE, TRUE, 0);
  gtk_entry_set_width_chars (GTK_ENTRY (config_account_receive_login_text), 10);

  label12 = gtk_label_new (_("Password"));
  gtk_widget_show (label12);
  gtk_box_pack_start (GTK_BOX (hbox8), label12, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label12), GTK_JUSTIFY_LEFT);

  config_account_receive_password_text = gtk_entry_new ();
  gtk_widget_show (config_account_receive_password_text);
  gtk_box_pack_start (GTK_BOX (hbox8), config_account_receive_password_text, TRUE, TRUE, 0);
  gtk_entry_set_visibility (GTK_ENTRY (config_account_receive_password_text), FALSE);
  gtk_entry_set_width_chars (GTK_ENTRY (config_account_receive_password_text), 10);

  config_account_receive_ask_checkbox = gtk_check_button_new_with_mnemonic (_("Ask"));
  gtk_widget_show (config_account_receive_ask_checkbox);
  gtk_box_pack_start (GTK_BOX (hbox8), config_account_receive_ask_checkbox, FALSE, FALSE, 0);

  hbox9 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox9);
  gtk_table_attach (GTK_TABLE (table2), hbox9, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  config_account_receive_remove_checkbox = gtk_check_button_new_with_mnemonic (_("Remove mails"));
  gtk_widget_show (config_account_receive_remove_checkbox);
  gtk_box_pack_start (GTK_BOX (hbox9), config_account_receive_remove_checkbox, FALSE, FALSE, 0);

  config_account_receive_avoid_checkbox = gtk_check_button_new_with_mnemonic (_("Avoid duplicates"));
  gtk_widget_show (config_account_receive_avoid_checkbox);
  gtk_box_pack_start (GTK_BOX (hbox9), config_account_receive_avoid_checkbox, FALSE, FALSE, 0);

  config_account_receive_secure_checkbox = gtk_check_button_new_with_mnemonic (_("Secure"));
  gtk_widget_show (config_account_receive_secure_checkbox);
  gtk_box_pack_start (GTK_BOX (hbox9), config_account_receive_secure_checkbox, FALSE, FALSE, 0);

  config_account_receive_stls_checkbox = gtk_check_button_new_with_mnemonic (_("with STLS"));
  gtk_widget_show (config_account_receive_stls_checkbox);
  gtk_box_pack_start (GTK_BOX (hbox9), config_account_receive_stls_checkbox, FALSE, FALSE, 0);

  label4 = gtk_label_new (_("Receive"));
  gtk_widget_show (label4);
  gtk_frame_set_label_widget (GTK_FRAME (frame3), label4);
  gtk_label_set_justify (GTK_LABEL (label4), GTK_JUSTIFY_LEFT);

  frame4 = gtk_frame_new (NULL);
  gtk_widget_show (frame4);
  gtk_box_pack_start (GTK_BOX (config_account_box), frame4, FALSE, TRUE, 0);

  table3 = gtk_table_new (3, 2, FALSE);
  gtk_widget_show (table3);
  gtk_container_add (GTK_CONTAINER (frame4), table3);
  gtk_table_set_row_spacings (GTK_TABLE (table3), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table3), 2);

  label14 = gtk_label_new (_("SMTP Server"));
  gtk_widget_show (label14);
  gtk_table_attach (GTK_TABLE (table3), label14, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label14), GTK_JUSTIFY_RIGHT);
  gtk_misc_set_alignment (GTK_MISC (label14), 0, 0.5);

  label15 = gtk_label_new (_("Login"));
  gtk_widget_show (label15);
  gtk_table_attach (GTK_TABLE (table3), label15, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label15), GTK_JUSTIFY_RIGHT);
  gtk_misc_set_alignment (GTK_MISC (label15), 0, 0.5);

  hbox13 = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox13);
  gtk_table_attach (GTK_TABLE (table3), hbox13, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  config_account_send_login_text = gtk_entry_new ();
  gtk_widget_show (config_account_send_login_text);
  gtk_box_pack_start (GTK_BOX (hbox13), config_account_send_login_text, TRUE, TRUE, 0);
  gtk_entry_set_width_chars (GTK_ENTRY (config_account_send_login_text), 10);

  label17 = gtk_label_new (_("Password"));
  gtk_widget_show (label17);
  gtk_box_pack_start (GTK_BOX (hbox13), label17, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label17), GTK_JUSTIFY_LEFT);

  config_account_send_password_text = gtk_entry_new ();
  gtk_widget_show (config_account_send_password_text);
  gtk_box_pack_start (GTK_BOX (hbox13), config_account_send_password_text, TRUE, TRUE, 0);
  gtk_entry_set_visibility (GTK_ENTRY (config_account_send_password_text), FALSE);
  gtk_entry_set_width_chars (GTK_ENTRY (config_account_send_password_text), 10);

  config_account_send_auth_checkbox = gtk_check_button_new_with_mnemonic (_("Needs authentification"));
  gtk_widget_show (config_account_send_auth_checkbox);
  gtk_box_pack_start (GTK_BOX (hbox13), config_account_send_auth_checkbox, FALSE, FALSE, 0);

  hbox10 = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox10);
  gtk_table_attach (GTK_TABLE (table3), hbox10, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  config_account_send_ip_checkbox = gtk_check_button_new_with_mnemonic (_("IP as domain"));
  gtk_widget_show (config_account_send_ip_checkbox);
  gtk_box_pack_start (GTK_BOX (hbox10), config_account_send_ip_checkbox, FALSE, FALSE, 0);

  config_account_send_secure_checkbox = gtk_check_button_new_with_mnemonic (_("Secure"));
  gtk_widget_show (config_account_send_secure_checkbox);
  gtk_box_pack_start (GTK_BOX (hbox10), config_account_send_secure_checkbox, FALSE, FALSE, 0);

  config_account_send_login_checkbox = gtk_check_button_new_with_mnemonic (_("Login into POP3 server first"));
  gtk_widget_show (config_account_send_login_checkbox);
  gtk_box_pack_start (GTK_BOX (hbox10), config_account_send_login_checkbox, FALSE, FALSE, 0);

  hbox11 = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox11);
  gtk_table_attach (GTK_TABLE (table3), hbox11, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  config_account_send_server_text = gtk_entry_new ();
  gtk_widget_show (config_account_send_server_text);
  gtk_box_pack_start (GTK_BOX (hbox11), config_account_send_server_text, TRUE, TRUE, 0);

  label16 = gtk_label_new (_("Port"));
  gtk_widget_show (label16);
  gtk_box_pack_start (GTK_BOX (hbox11), label16, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label16), GTK_JUSTIFY_LEFT);

  config_account_send_port_spin_adj = gtk_adjustment_new (25, 0, 65536, 1, 10, 10);
  config_account_send_port_spin = gtk_spin_button_new (GTK_ADJUSTMENT (config_account_send_port_spin_adj), 1, 0);
  gtk_widget_show (config_account_send_port_spin);
  gtk_box_pack_start (GTK_BOX (hbox11), config_account_send_port_spin, FALSE, TRUE, 0);

  label5 = gtk_label_new (_("Send"));
  gtk_widget_show (label5);
  gtk_frame_set_label_widget (GTK_FRAME (frame4), label5);
  gtk_label_set_justify (GTK_LABEL (label5), GTK_JUSTIFY_LEFT);

	/* Callbacks */
	g_signal_connect(config_account_tree, "cursor_changed", G_CALLBACK(on_config_account_tree_cursor_changed), NULL);

	gtk_signal_connect(GTK_OBJECT(config_account_add_button), "clicked",
                      GTK_SIGNAL_FUNC (on_config_account_add_button_clicked),
                      NULL);

	gtk_signal_connect(GTK_OBJECT(config_account_remove_button), "clicked",
                      GTK_SIGNAL_FUNC (on_config_account_remove_button_clicked),
                      NULL);
}

static void on_config_cancel_button_clicked(GtkButton *button, gpointer user_data)
{
	gtk_widget_destroy(config_wnd);
	config_wnd = NULL;
}

static void on_config_apply_button_clicked(GtkButton *button, gpointer user_data)
{
	struct account *account;
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* Copy the accounts */
	clear_config_accounts();
	if (account_selected)
		store_account(account_selected);

	model = gtk_tree_view_get_model(config_account_tree);
	if (gtk_tree_model_get_iter_first(model,&iter))
	{
		do
		{
			struct account *ac;
			gtk_tree_model_get(model,&iter,ACCOUNT_POINTER,&ac,-1);
			insert_config_account(ac);
		}	while ((gtk_tree_model_iter_next(model,&iter)));
	}

	on_config_cancel_button_clicked(NULL,user_data);
}


static void on_config_save_button_clicked(GtkButton *button, gpointer user_data)
{
	on_config_apply_button_clicked(NULL,user_data);
	save_config();
}

static void set_account_group(struct account *account)
{
	gtk_entry_set_text(GTK_ENTRY(config_account_user_name_text),SAFESTR(account->name));
	gtk_entry_set_text(GTK_ENTRY(config_account_user_email_text),SAFESTR(account->email));
	gtk_entry_set_text(GTK_ENTRY(config_account_user_reply_text),SAFESTR(account->reply));
	gtk_entry_set_text(GTK_ENTRY(config_account_receive_server_text),SAFESTR(account->pop->name));
	gtk_entry_set_text(GTK_ENTRY(config_account_receive_login_text),SAFESTR(account->pop->login));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_receive_ask_checkbox),account->pop->ask);
	gtk_entry_set_text(GTK_ENTRY(config_account_receive_password_text),SAFESTR(account->pop->passwd));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_receive_remove_checkbox),account->pop->del);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_receive_secure_checkbox),account->pop->ssl);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_receive_stls_checkbox),account->pop->stls);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_receive_avoid_checkbox),account->pop->nodupl);
	gtk_spin_button_set_value(GTK_WIDGET(config_account_receive_port_spin),account->pop->port);

	gtk_entry_set_text(GTK_ENTRY(config_account_send_server_text),SAFESTR(account->smtp->name));
	gtk_spin_button_set_value(GTK_WIDGET(config_account_send_port_spin),account->smtp->port);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_send_ip_checkbox),account->smtp->ip_as_domain);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_send_login_checkbox),account->smtp->pop3_first);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_send_secure_checkbox),account->smtp->secure);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(config_account_send_auth_checkbox),account->smtp->auth);
	gtk_entry_set_text(GTK_ENTRY(config_account_send_login_text),SAFESTR(account->smtp->auth_login));
	gtk_entry_set_text(GTK_ENTRY(config_account_send_password_text),SAFESTR(account->smtp->auth_password));
}

/******************************************************************
 Init the config window
*******************************************************************/
static void init_config(void)
{
	GtkWidget *vbox1;
	GtkWidget *hbox2;
	GtkWidget *hpaned1;
	GtkWidget *scrolledwindow1;
	GtkWidget *treeview1;
	GtkWidget *frame1;
	GtkWidget *config_scrolledwindow;
	GtkWidget *config_label;
	GtkWidget *hbuttonbox3;
	GtkWidget *config_save_button;
	GtkWidget *config_apply_button;
	GtkWidget *config_cancel_button;

	config_wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(config_wnd), "SimpleMail - Configuration");
	gtk_window_set_default_size(GTK_WINDOW(config_wnd),640,400);
	gtk_window_set_position(GTK_WINDOW(config_wnd),GTK_WIN_POS_CENTER);

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (config_wnd), vbox1);

	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox2, TRUE, TRUE, 0);

	hpaned1 = gtk_hpaned_new ();
	gtk_widget_show (hpaned1);
	gtk_box_pack_start (GTK_BOX (hbox2), hpaned1, TRUE, TRUE, 0);

	/* The config pages */
	scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_paned_pack1 (GTK_PANED (hpaned1), scrolledwindow1, FALSE, TRUE);
	gtk_widget_set_usize (scrolledwindow1, 120, -2);

	treeview1 = gtk_tree_view_new ();
	gtk_container_add (GTK_CONTAINER (scrolledwindow1), treeview1);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview1), FALSE);

	frame1 = gtk_frame_new (NULL);
	gtk_paned_pack2 (GTK_PANED (hpaned1), frame1, TRUE, TRUE);

	config_scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(config_scrolledwindow),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (frame1), config_scrolledwindow);

	config_label = gtk_label_new (_("label5"));
	gtk_frame_set_label_widget (GTK_FRAME (frame1), config_label);
	gtk_label_set_justify (GTK_LABEL (config_label), GTK_JUSTIFY_LEFT);

	hbuttonbox3 = gtk_hbutton_box_new ();
	gtk_box_pack_start (GTK_BOX (vbox1), hbuttonbox3, FALSE, TRUE, 5);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox3), GTK_BUTTONBOX_SPREAD);
	gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox3), 0);

	config_save_button = gtk_button_new_from_stock ("gtk-save");
	gtk_container_add (GTK_CONTAINER (hbuttonbox3), config_save_button);
	GTK_WIDGET_SET_FLAGS (config_save_button, GTK_CAN_DEFAULT);

	config_apply_button = gtk_button_new_from_stock ("gtk-apply");
	gtk_container_add (GTK_CONTAINER (hbuttonbox3), config_apply_button);
	GTK_WIDGET_SET_FLAGS (config_apply_button, GTK_CAN_DEFAULT);

	config_cancel_button = gtk_button_new_from_stock ("gtk-cancel");
	gtk_container_add (GTK_CONTAINER (hbuttonbox3), config_cancel_button);
	GTK_WIDGET_SET_FLAGS (config_cancel_button, GTK_CAN_DEFAULT);

	/* Callbacks */
	gtk_signal_connect (GTK_OBJECT (config_save_button), "clicked",
                      GTK_SIGNAL_FUNC (on_config_save_button_clicked),
                      NULL);
	gtk_signal_connect (GTK_OBJECT (config_apply_button), "clicked",
                      GTK_SIGNAL_FUNC (on_config_apply_button_clicked),
                      NULL);
	gtk_signal_connect (GTK_OBJECT (config_cancel_button), "clicked",
                      GTK_SIGNAL_FUNC (on_config_cancel_button_clicked),
                      NULL);


	//gtk_signal_connect(GTK_OBJECT(config_wnd), "destroy",GTK_SIGNAL_FUNC (config_quit), NULL);

	init_account_group();
	{
		struct account *account;
		int first = 1;

		account = (struct account*)list_first(&user.config.account_list);

		while (account)
		{
			struct account *new_account = account_duplicate(account);
			if (new_account)
			{
				add_account(new_account,first);
				first = 0;
			}
//			set_account_group(account);
			account = (struct account*)node_next(&account->node);
		}
	}

	gtk_scrolled_window_add_with_viewport(GTK_CONTAINER (config_scrolledwindow), config_account_box);

#if 0
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
#endif
}

/******************************************************************
 Open the config window
*******************************************************************/
void open_config(void)
{
	account_selected = NULL;

	if (!config_wnd) init_config();
	if (config_wnd)
	{
		gtk_widget_show_all(config_wnd);
	}
}

/******************************************************************
 Close and dispose the config window
*******************************************************************/
void close_config(void)
{
#if 0
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
#endif
}

#if 0

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

#endif



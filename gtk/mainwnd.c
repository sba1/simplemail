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
** mainwnd.c
*/

#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>

#include "SimpleMail_rev.h"

#include "folder.h"
#include "lists.h"
#include "mail.h"
#include "simplemail.h"
#include "support.h"

#include "gtksupport.h"

static GtkWidget *main_wnd;
static GtkWidget *toolbar;

static GtkWidget *mail_ctree;
static struct list mail_ctree_string_list;

static GtkWidget *folder_ctree; /* the folders */
static GtkCTreeNode *folder_ctree_mailbox_node; /* the mailbox node */
static struct list folder_ctree_string_list; /* the list of all strings the folder list contains */

/******************************************************************
 Returns all selected entries
*******************************************************************/
GList *gtk_ctree_find_all_selected(GtkCTree *ctree, GtkCTreeNode *node)
{
	GList *list = NULL;

	g_return_val_if_fail (ctree != NULL, NULL);
	g_return_val_if_fail (GTK_IS_CTREE (ctree), NULL);

	/* if node == NULL then look in the whole tree */
	if (!node)
		node = GTK_CTREE_NODE (GTK_CLIST (ctree)->row_list);

	while (node)
	{
		if (GTK_CTREE_ROW(node)->row.state == GTK_STATE_SELECTED)
		    list = g_list_append (list, node);

		if (GTK_CTREE_ROW (node)->children)
		{
			GList *sub_list;

			sub_list = gtk_ctree_find_all_selected (ctree, GTK_CTREE_ROW(node)->children);
			list = g_list_concat (list, sub_list);
		}
		node = GTK_CTREE_ROW (node)->sibling;
	}
	return list;
}

/******************************************************************
 Clears the whole mail list
*******************************************************************/
static void mail_ctree_clear(void)
{
	gtk_clist_clear(GTK_CLIST(mail_ctree));
	string_list_clear(&mail_ctree_string_list);
}

/******************************************************************
 Adds a new entry to the mail list
*******************************************************************/
static void mail_ctree_insert(struct mail *m)
{
	gchar *text[4];
	text[0] = "";
	text[1] = string_list_insert_tail(&folder_ctree_string_list, m->from)->string;
	text[2] = string_list_insert_tail(&folder_ctree_string_list, m->subject)->string;
	text[3] = "";

	gtk_ctree_insert_node(GTK_CTREE(mail_ctree), NULL, NULL, text, 0, NULL, NULL, NULL, NULL, TRUE, FALSE);
	gtk_clist_set_column_width(GTK_CLIST(mail_ctree),0,gtk_clist_optimal_column_width(GTK_CLIST(mail_ctree),0));
	gtk_clist_set_column_width(GTK_CLIST(mail_ctree),1,gtk_clist_optimal_column_width(GTK_CLIST(mail_ctree),1));
	gtk_clist_set_column_width(GTK_CLIST(mail_ctree),2,gtk_clist_optimal_column_width(GTK_CLIST(mail_ctree),2));
}


/******************************************************************
 Clears the whole folder list
*******************************************************************/
static void folder_ctree_clear(void)
{
	gtk_clist_clear(GTK_CLIST(folder_ctree));
        folder_ctree_mailbox_node = NULL;
	string_list_clear(&folder_ctree_string_list);
}

/******************************************************************
 Adds a new entry to the folder list
*******************************************************************/
static void folder_ctree_insert(struct folder *f)
{
	gchar *text[2];
	char buf[256];
	char *name = f->name;
	int mails = f->num_mails;

	if (!folder_ctree_mailbox_node)
        {
		text[0] = string_list_insert_tail(&folder_ctree_string_list, "Mailboxes")->string;
		text[1] = "";

		folder_ctree_mailbox_node = gtk_ctree_insert_node(GTK_CTREE(folder_ctree), NULL, NULL, text, 0, NULL, NULL, NULL, NULL, FALSE, TRUE);
		gtk_ctree_node_set_row_data(GTK_CTREE(folder_ctree),folder_ctree_mailbox_node,NULL);
	}

	sprintf(buf,"%d",mails);
	text[0] = string_list_insert_tail(&folder_ctree_string_list, name)->string;
	text[1] = string_list_insert_tail(&folder_ctree_string_list, buf)->string;

	gtk_ctree_node_set_row_data(GTK_CTREE(folder_ctree),gtk_ctree_insert_node(GTK_CTREE(folder_ctree), folder_ctree_mailbox_node, NULL, text, 0, NULL, NULL, NULL, NULL, TRUE, FALSE),f);
	gtk_clist_set_column_width(GTK_CLIST(folder_ctree),0,gtk_clist_optimal_column_width(GTK_CLIST(folder_ctree),0));
}

static void main_refresh_folders_text(void);

/******************************************************************
 Callback to quit SimpleMail
*******************************************************************/
static void main_quit(void)
{
        gtk_exit(0);
}

/******************************************************************
 Callback when a new folder gets selected
*******************************************************************/
static void folder_select_row_callback(GtkCTree *ctree, GList *node, gint column, gpointer user_data)
{
	callback_folder_active();
}

/******************************************************************
 Initialize the main window
*******************************************************************/
int main_window_init(void)
{
	GtkWidget *vbox, *hbox, *hpaned;
	static const gchar *mail_names[] = {
		"Status",
                "From",
                "Subject",
                "Date",
                NULL
	};
        static const gchar *folder_names[] = {
		"Name",
		"Mails",
		NULL
	};

	/* Create the window */
	main_wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(main_wnd), "SimpleMail");
	gtk_signal_connect(GTK_OBJECT(main_wnd), "destroy",GTK_SIGNAL_FUNC (main_quit), NULL);

        /* Create the vertical box */
        vbox = gtk_vbox_new(0,4);
        gtk_container_add(GTK_CONTAINER(main_wnd), vbox);

        /* Create the toolbar */
        toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL,GTK_TOOLBAR_BOTH);
        gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0 /* Padding */); /* only use minimal height */

        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Read", "Reads the selected mail", NULL /* private TT */, create_pixmap(main_wnd,"MailRead.xpm"), NULL /* CALLBACK */, NULL /* UDATA */);
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Edit", "Edit the selected mail", NULL /* private TT */, create_pixmap(main_wnd,"MailModify.xpm"), NULL /* CALLBACK */, NULL /* UDATA */);
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Delete", "Delete the selected mail", NULL /* private TT */, create_pixmap(main_wnd,"MailDelete.xpm"), NULL /* CALLBACK */, NULL /* UDATA */);
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Get Addr", "Insert the address into the addressbook", NULL /* private TT */, create_pixmap(main_wnd,"MailGetAddress.xpm"), NULL /* CALLBACK */, NULL /* UDATA */);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "New", "Create a new mail", NULL /* private TT */, create_pixmap(main_wnd,"MailNew.xpm"), callback_new_mail /* CALLBACK */, NULL /* UDATA */);
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Reply", "Reply the mail", NULL /* private TT */, create_pixmap(main_wnd,"MailReply.xpm"), NULL /* CALLBACK */, NULL /* UDATA */);
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Forward", "Forward the mail", NULL /* private TT */, create_pixmap(main_wnd,"MailForward.xpm"), NULL /* CALLBACK */, NULL /* UDATA */);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Fetch", "Fetch the mails from the POP3 Servers", NULL /* private TT */, create_pixmap(main_wnd,"MailsFetch.xpm"), callback_fetch_mails /* CALLBACK */, NULL /* UDATA */);
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Send", "Send the mails out", NULL /* private TT */, create_pixmap(main_wnd,"MailsSend.xpm"), NULL /* CALLBACK */, NULL /* UDATA */);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "ABook", "Open the addressbook", NULL /* private TT */, create_pixmap(main_wnd,"Addressbook.xpm"), NULL /* CALLBACK */, NULL /* UDATA */);
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Config", "Configure SimpleMail", NULL /* private TT */, create_pixmap(main_wnd,"Config.xpm"), NULL /* CALLBACK */, NULL /* UDATA */);

        /* Create the horzontal box */
        hbox = gtk_hbox_new(0,4);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0 /* Padding */);

	/* Create the horizobtal paned between folders and mails */
	hpaned = gtk_hpaned_new();
	gtk_container_add(GTK_CONTAINER(hbox), hpaned);

        /* Create the folder tree */
        folder_ctree = gtk_ctree_new_with_titles(2,0,folder_names);
	gtk_signal_connect(GTK_OBJECT(folder_ctree), "tree-select-row",GTK_SIGNAL_FUNC(folder_select_row_callback), NULL);

        /* Create the mail tree */
	mail_ctree = gtk_ctree_new_with_titles(4,0,mail_names);

	/* Add them to the paned */
	gtk_paned_add1(GTK_PANED(hpaned),folder_ctree);
	gtk_paned_add2(GTK_PANED(hpaned),mail_ctree);

	/* Show the window */
	gtk_widget_show(folder_ctree);
	gtk_widget_show(mail_ctree);
	gtk_widget_show(hpaned);
	gtk_widget_show(hbox);
        gtk_widget_show(toolbar);
	gtk_widget_show(vbox);
	gtk_widget_show(main_wnd);
#if 0
	int rc;
	rc = FALSE;

	win_main = WindowObject,
		MUIA_Window_ID, MAKE_ID('M','A','I','N'),
    MUIA_Window_Title, VERS,

		WindowContents, main_group = VGroup,
			Child, buttons_group = HGroupV,
				Child, HGroup,
					MUIA_Group_Spacing, 0,
					MUIA_Weight, 200,
					Child, button_read = MakePictureButton("R_ead","PROGDIR:Images/MailRead"),
					Child, button_change = MakePictureButton("_Modify","PROGDIR:Images/MailModify"),
					Child, button_delete = MakePictureButton("_Delete","PROGDIR:Images/MailDelete"),
					Child, button_getadd = MakePictureButton("_GetAdd","PROGDIR:Images/MailGetAddress"),
					End,
				Child, HGroup,
					MUIA_Group_Spacing, 0,
					MUIA_Weight, 150,
					Child, button_new = MakePictureButton("_New","PROGDIR:Images/MailNew"),
					Child, button_reply = MakePictureButton("_Reply","PROGDIR:Images/MailReply"),
					Child, button_forward = MakePictureButton("F_orward","PROGDIR:Images/MailForward"),
					End,
				Child, HGroup,
					MUIA_Group_Spacing, 0,
					Child, button_fetch = MakePictureButton("_Fetch","PROGDIR:Images/MailsFetch"),
					Child, button_send = MakePictureButton("_Send","PROGDIR:Images/MailsSend"),
					End,
				Child, HGroup,
					MUIA_Group_Spacing, 0,
					Child, button_abook = MakePictureButton("_Abook","PROGDIR:Images/Addressbook"),
					Child, button_config = MakePictureButton("_Config","PROGDIR:Images/Config"),
					End,
				End,

			Child, folder_group = HGroup,
				MUIA_Group_Spacing,0,
				Child, folder_text = TextObject, TextFrame, MUIA_Text_PreParse, MUIX_C, End,
				Child, folder_popupmenu = PopupmenuObject,
					ImageButtonFrame,
					MUIA_CycleChain,1,
					MUIA_Image_Spec, MUII_PopUp,
					MUIA_Image_FreeVert, TRUE,
					End,
				Child, switch1_button = PopButton(MUII_ArrowLeft),
				End,

			Child, mail_tree_group = HGroup,
				Child, folder_listview_group = VGroup,
					MUIA_HorizWeight, 33,
					Child, folder_listview = NListviewObject,
						MUIA_CycleChain, 1,
						MUIA_NListview_NList, folder_tree = FolderTreelistObject,
							MUIA_NList_Exports, MUIV_NList_Exports_ColWidth|MUIV_NList_Exports_ColOrder,
							MUIA_NList_Imports, MUIV_NList_Imports_ColWidth|MUIV_NList_Imports_ColOrder,
							MUIA_ObjectID, MAKE_ID('M','W','F','T'),
							End,
						End,
					Child, HGroup,
						MUIA_Group_Spacing, 0,
						Child, folder2_text = TextObject, TextFrame,MUIA_Text_SetMin,FALSE,End,
						Child, switch2_button = PopButton(MUII_ArrowRight),
						End,
					End,
				Child, folder_balance = BalanceObject, End,
				Child, mail_listview = NListviewObject,
					MUIA_CycleChain,1,
					MUIA_NListview_NList, mail_tree = MailTreelistObject,
						MUIA_NList_TitleMark, MUIV_NList_TitleMark_Down | 4,
						MUIA_NList_Exports, MUIV_NList_Exports_ColWidth|MUIV_NList_Exports_ColOrder,
						MUIA_NList_Imports, MUIV_NList_Imports_ColWidth|MUIV_NList_Imports_ColOrder,
						MUIA_ObjectID, MAKE_ID('M','W','M','T'),
						End,
					End,
				End,
			End,
		End;
	
	if(win_main != NULL)
	{
		if (!init_folder_placement()) return 0;
		if (xget(folder_tree, MUIA_Version) < 1 || (xget(folder_tree, MUIA_Version) >= 1 && xget(folder_tree, MUIA_Revision)<7))
    {
    	printf("SimpleMail needs at least version 1.7 of the NListtree mui subclass!\nIt's available at http://www.aphaso.de\n");
    	return 0;
    }

		DoMethod(App, OM_ADDMEMBER, win_main);
		DoMethod(win_main, MUIM_Notify, MUIA_Window_CloseRequest, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
		DoMethod(button_read, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_read_mail);
		DoMethod(button_getadd, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_get_address);
		DoMethod(button_delete, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_delete_mails);
		DoMethod(button_fetch, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_fetch_mails);
		DoMethod(button_send, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_send_mails);
		DoMethod(button_new, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_new_mail);
		DoMethod(button_reply, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_reply_mail);
		DoMethod(button_forward, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_forward_mail);
		DoMethod(button_change, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_change_mail);
		DoMethod(button_abook, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_addressbook);
		DoMethod(button_config, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_config);
		DoMethod(switch1_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, switch_folder_view);
		DoMethod(switch2_button, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, switch_folder_view);
		DoMethod(mail_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3,  MUIM_CallHook, &hook_standard, callback_read_mail);
		DoMethod(mail_tree, MUIM_Notify, MUIA_NList_TitleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, mailtreelist_title_click);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_folder_active);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_Active, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, main_refresh_folders_text);
		DoMethod(folder_tree, MUIM_Notify, MUIA_FolderTreelist_MailDrop, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, foldertreelist_maildrop);
		DoMethod(folder_tree, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, callback_edit_folder);
		DoMethod(folder_popupmenu, MUIM_Notify, MUIA_Popupmenu_Selected, MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook, &hook_standard, popup_selected);
		set(folder_tree,MUIA_UserData,mail_tree); /* for the drag'n'drop support */
		rc = TRUE;
	} else
	{
		tell("Can\'t create window!");
	}
	
	return(rc);
#endif
	return 1;
}

/******************************************************************
 Open the main window
*******************************************************************/
int main_window_open(void)
{
	if (!main_wnd) main_window_init();
	if (main_wnd)
	{
	}
	return 1;
#if 0
	int rc;
	rc = FALSE;
	
	if(win_main != NULL)
	{
		set(win_main, MUIA_Window_Open, TRUE);
		rc = TRUE;
	}

	return(rc);
#endif
}

/******************************************************************
 Refreshs the folder text line
*******************************************************************/
static void main_refresh_folders_text(void)
{
#if 0
	char buf[256];

	if (folder_text)
	{
		struct folder *f = main_get_folder();
		if (f)
		{
			sprintf(buf, MUIX_B "Folder:"  MUIX_N "%s " MUIX_B "Messages:"  MUIX_N "%ld " MUIX_B "New:"  MUIX_N "%ld: " MUIX_B "Unread:"  MUIX_N "%ld",f->name,f->num_mails,f->new_mails,f->unread_mails);
			set(folder_text, MUIA_Text_Contents,buf);
		}
	}

	if (folder2_text)
	{
		struct folder *f = folder_first();
		int total_msg = 0;
		int total_unread = 0;
		int total_new = 0;
		while (f)
		{
			total_msg += f->num_mails;
			total_unread += f->unread_mails;
			total_new += f->new_mails;
			f = folder_next(f);
		}
		sprintf(buf, "Total:%ld New:%ld Unread:%ld",total_msg,total_new,total_unread);
		set(folder2_text,MUIA_Text_Contents,buf);
	}
#endif
}

/******************************************************************
 Refreshs the folder list
*******************************************************************/
void main_refresh_folders(void)
{
	struct folder *f = folder_first();

	while (f)
        {
		folder_ctree_insert(f);
		f = folder_next(f);
        }
#if 0
	struct folder *f = folder_first();
	char buf[256];

	int act = xget(folder_tree, MUIA_NList_Active);

	if (folder_popupmenu) DoMethod(folder_popupmenu,MUIM_Popupmenu_Clear);

	set(folder_tree,MUIA_NListtree_Quiet,TRUE);

	DoMethod(folder_tree, MUIM_NListtree_Clear, NULL, 0);
/*	DoMethod(folder_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root, MUIV_NListtree_Remove_TreeNode_All, MUIV_NListtree_Remove_Flag_NoActive);*/

	while (f)
	{
		DoMethod(folder_tree,MUIM_NListtree_Insert,"" /*name*/, f, /*udata */
						 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);

		if (folder_popupmenu)
		{
			sprintf(buf,"%s (T:%ld N:%ld U:%ld)",f->name,f->num_mails,f->new_mails,f->unread_mails);
			DoMethod(folder_popupmenu,MUIM_Popupmenu_AddEntry, buf);
		}
		f = folder_next(f);
	}
	nnset(folder_tree,MUIA_NList_Active,act);
	set(folder_tree,MUIA_NListtree_Quiet,FALSE);
	main_refresh_folders_text();
#endif
}

/******************************************************************
 Refreshs a single folder entry
*******************************************************************/
void main_refresh_folder(struct folder *folder)
{
#if 0
	struct MUI_NListtree_TreeNode *tree_node = FindListtreeUserData(folder_tree, folder);
	if (tree_node)
	{
		DoMethod(folder_tree,MUIM_NListtree_Redraw,tree_node,0);
	}
	main_refresh_folders_text();
#endif
}

/******************************************************************
 Inserts a new mail into the listview
*******************************************************************/
void main_insert_mail(struct mail *mail)
{
	mail_ctree_insert(mail);
}

/******************************************************************
 Remove a given mail from the listview
*******************************************************************/
void main_remove_mail(struct mail *mail)
{
#if 0
	struct MUI_NListtree_TreeNode *treenode = FindListtreeUserData(mail_tree, mail);
	if (treenode)
	{
		DoMethod(mail_tree, MUIM_NListtree_Remove, NULL, treenode,0);
	}
#endif
}

/******************************************************************
 Replaces a mail with a new mail
*******************************************************************/
void main_replace_mail(struct mail *oldmail, struct mail *newmail)
{
#if 0
	struct MUI_NListtree_TreeNode *treenode = FindListtreeUserData(mail_tree, oldmail);
	if (treenode)
	{
/*		DoMethod(mail_tree, MUIM_NListtree_Rename, treenode, newmail, MUIV_NListtree_Rename_Flag_User);*/
		set(mail_tree, MUIA_NListtree_Quiet, TRUE);
		DoMethod(mail_tree, MUIM_NListtree_Remove, NULL, treenode,0);
		main_insert_mail(newmail);
		set(mail_tree, MUIA_NListtree_Quiet, FALSE);
	}
#endif
}

/******************************************************************
 Refresh a mail (if it status has changed fx)
*******************************************************************/
void main_refresh_mail(struct mail *m)
{
#if 0
	struct MUI_NListtree_TreeNode *treenode = FindListtreeUserData(mail_tree, m);
	if (treenode)
	{
		DoMethod(mail_tree, MUIM_NListtree_Redraw, treenode, 0);
		main_refresh_folders_text();
	}
#endif
}

/******************************************************************
 Updates the mail trees with the mails in the given folder
*******************************************************************/
static void main_insert_mail_threaded(struct folder *folder, struct mail *mail, void *parentnode)
{
#if 0
	int mail_flags = 0;
	struct mail *submail;
	APTR newnode;

	if ((submail = mail->sub_thread_mail))
	{
		mail_flags = TNF_LIST|TNF_OPEN;
	}

	newnode = (APTR)DoMethod(mail_tree,MUIM_NListtree_Insert,"" /*name*/, mail, /*udata */
				 parentnode,MUIV_NListtree_Insert_PrevNode_Tail,mail_flags);

	while (submail)
	{
		main_insert_mail_threaded(folder,submail,newnode);
		submail = submail->next_thread_mail;
	}
#endif
}

/******************************************************************
 Clears the folder list
*******************************************************************/
void main_clear_folder_mails(void)
{
#if 0
	DoMethod(mail_tree, MUIM_NListtree_Clear, NULL, 0);
#endif
}

/******************************************************************
 Updates the mail trees with the mails in the given folder
*******************************************************************/
void main_set_folder_mails(struct folder *folder)
{ 
	void *handle = NULL;
	struct mail *m;
	int primary_sort = folder_get_primary_sort(folder)&FOLDER_SORT_MODEMASK;
	int threaded = folder->type == FOLDER_TYPE_MAILINGLIST;

	mail_ctree_clear();

	while ((m = folder_next_mail(folder,&handle)))
	{
		mail_ctree_insert(m);
	}


#if 0
	set(mail_tree, MUIA_NListtree_Quiet, TRUE);

	main_clear_folder_mails();
/*	DoMethod(mail_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root, MUIV_NListtree_Remove_TreeNode_All, MUIV_NListtree_Remove_Flag_NoActive);*/

	set(mail_tree, MUIA_MailTreelist_FolderType, folder_get_type(folder));

	if ((primary_sort == FOLDER_SORT_FROMTO || primary_sort == FOLDER_SORT_SUBJECT) && !threaded)
	{
		struct mail *lm = NULL; /* last mail */
		APTR treenode = NULL;

		SetAttrs(mail_tree,
				MUIA_NListtree_TreeColumn, (primary_sort==2)?3:(folder_get_type(folder)==FOLDER_TYPE_SEND?2:1),
				MUIA_NListtree_ShowTree, TRUE,
				TAG_DONE);

		while ((m = folder_next_mail(folder,&handle)))
		{
			if (primary_sort == FOLDER_SORT_FROMTO)
			{
				int res;

				if (folder_get_type(folder) == FOLDER_TYPE_SEND) res = mystricmp(m->to, lm->to);
				else res = mystricmp(m->from, lm->from);

				if (!lm || res)
				{
					treenode = (APTR)DoMethod(mail_tree, MUIM_NListtree_Insert, (folder_get_type(folder) == FOLDER_TYPE_SEND)?m->to:m->from, MUIV_MailTreelist_UserData_Name, /* special hint */
							 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,TNF_LIST|TNF_OPEN);
				}
			} else
			{
				if (!lm || mystricmp(m->subject,lm->subject))
				{
					treenode = (APTR)DoMethod(mail_tree, MUIM_NListtree_Insert, m->subject, MUIV_MailTreelist_UserData_Name, /* special hint */
							 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,TNF_LIST|TNF_OPEN);
				}
			}

			if (!treenode) break;

			DoMethod(mail_tree, MUIM_NListtree_Insert,"" /*name*/, m, /*udata */
						 treenode,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);

			lm = m;
		}
	} else
	{
		if (!threaded)
		{
			SetAttrs(mail_tree,
					MUIA_NListtree_TreeColumn, 0,
					MUIA_NListtree_ShowTree, FALSE,
					TAG_DONE);

			while ((m = folder_next_mail(folder,&handle)))
			{
				DoMethod(mail_tree,MUIM_NListtree_Insert,"" /*name*/, m, /*udata */
							 MUIV_NListtree_Insert_ListNode_Root,MUIV_NListtree_Insert_PrevNode_Tail,0/*flags*/);
			}
		} else
		{
			SetAttrs(mail_tree,
					MUIA_NListtree_TreeColumn, 0,
					MUIA_NListtree_ShowTree, TRUE,
					TAG_DONE);

			while ((m = folder_next_mail(folder,&handle)))
			{
				if (!m->child_mail)
					main_insert_mail_threaded(folder,m,(void*)MUIV_NListtree_Insert_ListNode_Root);
			}
		}
	}

	SetAttrs(mail_tree,
				MUIA_NListtree_Quiet, FALSE,
				TAG_DONE);

	mailtreelist_update_title_markers();
#endif
}

/******************************************************************
 Returns the current selected folder, NULL if no real folder
 has been selected. It returns the true folder like it is
 in the folder list
*******************************************************************/
struct folder *main_get_folder(void)
{
	GList *sel_list = gtk_ctree_find_all_selected(GTK_CTREE(folder_ctree),NULL);
	if (sel_list)
	{
		GtkCTreeNode *node = GTK_CTREE_NODE(sel_list->data);
		if (node) return (struct folder*)gtk_ctree_node_get_row_data(GTK_CTREE(folder_ctree),node);
	}

#if 0
	struct MUI_NListtree_TreeNode *tree_node;
	tree_node = (struct MUI_NListtree_TreeNode *)xget(folder_tree,MUIA_NListtree_Active);

	if (tree_node)
	{
		if (tree_node->tn_User)
		{
			return (struct folder*)tree_node->tn_User;
		}
	}
	return NULL;
#endif	
	return NULL;
}

/******************************************************************
 Returns the current selected folder drawer, NULL if no folder
 has been selected
*******************************************************************/
char *main_get_folder_drawer(void)
{
#if 0
	struct folder *folder = main_get_folder();
	if (folder) return folder->path;
#endif
	return NULL;
}

/******************************************************************
 Returns the active mail. NULL if no one is active
*******************************************************************/
struct mail *main_get_active_mail(void)
{
#if 0
	struct MUI_NListtree_TreeNode *tree_node;
	tree_node = (struct MUI_NListtree_TreeNode *)xget(mail_tree,MUIA_NListtree_Active);

	if (tree_node)
	{
		if (tree_node->tn_User && tree_node->tn_User != (void*)MUIV_MailTreelist_UserData_Name)
		{
			return (struct mail*)tree_node->tn_User;
		}
	}
#endif
	return NULL;
}

/******************************************************************
 Returns the filename of the active mail, NULL if no thing is
 selected
*******************************************************************/
char *main_get_mail_filename(void)
{
#if 0
	struct mail *m = main_get_active_mail();
	if (m) return m->filename;
#endif
	return NULL;
}

/******************************************************************
 Returns the first selected mail. NULL if no mail is selected
*******************************************************************/
struct mail *main_get_mail_first_selected(void *handle)
{
#if 0
	struct MUI_NListtree_TreeNode *treenode = (struct MUI_NListtree_TreeNode *)MUIV_NListtree_NextSelected_Start;
	DoMethod(mail_tree, MUIM_NListtree_NextSelected, &treenode);
	if (treenode == (struct MUI_NListtree_TreeNode *)MUIV_NListtree_NextSelected_End) return NULL;
	*((struct MUI_NListtree_TreeNode **)handle) = treenode;
	return (struct mail*)treenode->tn_User;
#endif
	return NULL;
}

/******************************************************************
 Returns the next selected mail. NULL if no more mails are
 selected
*******************************************************************/
struct mail *main_get_mail_next_selected(void *handle)
{
#if 0
	struct MUI_NListtree_TreeNode *treenode = *((struct MUI_NListtree_TreeNode **)handle);
	DoMethod(mail_tree, MUIM_NListtree_NextSelected, &treenode);
	if (treenode == (struct MUI_NListtree_TreeNode *)MUIV_NListtree_NextSelected_End) return NULL;
	*((struct MUI_NListtree_TreeNode **)handle) = treenode;
	return (struct mail*)treenode->tn_User;
#endif
	return NULL;
}

/******************************************************************
 Remove all selected mails,
*******************************************************************/
void main_remove_mails_selected(void)
{
#if 0
/*
	DoMethod(mail_tree, MUIM_NListtree_Remove,
			MUIV_NListtree_Remove_ListNode_Root, MUIV_NListtree_Remove_TreeNode_Selected, 0);
*/

	struct MUI_NListtree_TreeNode *treenode;
	int j = 0, i = 0;
	struct MUI_NListtree_TreeNode **array;

	set(mail_tree, MUIA_NListtree_Quiet, TRUE);

	treenode = (struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_Start;

	for (;;)
	{
		DoMethod(mail_tree, MUIM_NListtree_PrevSelected, &treenode);
		if (treenode==(struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_End)
			break;
		i++;
	}

	if ((array = (struct MUI_NListtree_TreeNode **)AllocVec(sizeof(struct MUI_NListtree_TreeNode *)*i,0)))
	{
		treenode = (struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_Start;

		for (;;)
		{
			DoMethod(mail_tree, MUIM_NListtree_PrevSelected, &treenode);
			if (treenode==(struct MUI_NListtree_TreeNode *)MUIV_NListtree_PrevSelected_End)
				break;
			array[j++] = treenode;
		}

		for (i=0;i<j;i++)
		{
			if (array[i]->tn_Flags & TNF_LIST)
			{
				struct MUI_NListtree_TreeNode *node = (struct MUI_NListtree_TreeNode *)
					DoMethod(mail_tree, MUIM_NListtree_GetEntry, array[i], MUIV_NListtree_GetEntry_Position_Head, 0);

				while (node)
				{
					struct MUI_NListtree_TreeNode *nextnode = (struct MUI_NListtree_TreeNode *)
						DoMethod(mail_tree, MUIM_NListtree_GetEntry, node, MUIV_NListtree_GetEntry_Position_Next, 0);

					DoMethod(mail_tree, MUIM_NListtree_Move, array[i], node, MUIV_NListtree_Move_NewListNode_Root, MUIV_NListtree_Move_NewTreeNode_Tail);
					node = nextnode;
				}
			}

			DoMethod(mail_tree, MUIM_NListtree_Remove, MUIV_NListtree_Remove_ListNode_Root,array[i],0);
		}		

		FreeVec(array);
	}

	set(mail_tree, MUIA_NListtree_Quiet, FALSE);
#endif
}

















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

#define FOLDER_NAME_COLUMN  0
#define FOLDER_MAILS_COLUMN 1

#define MAIL_STATUS_COLUMN  0
#define MAIL_FROM_COLUMN 1
#define MAIL_SUBJECT_COLUMN 2
#define MAIL_DATE_COLUMN 3
#define MAIL_FILENAME_COLUMN 4
#define MAIL_PTR_COLUMN 5

static GtkWidget *main_wnd;
static GtkWidget *toolbar;

static GtkTreeStore *folder_treestore;
static GtkWidget *folder_treeview;
static GtkWidget *folder_scrolled_window;

static GtkTreeStore *mail_treestore;
static GtkWidget *mail_treeview;
static GtkWidget *mail_scrolled_window;

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
static void folder_cursor_changed(GtkTreeView *treeview, gpointer user_data)
{
	callback_folder_active();
}

/******************************************************************
 Callback when a mail has been activated
*******************************************************************/
static void mail_row_activated(void)//GtkTreeView *treeview, GtkTypeTreePath *arg1, GtkTreeViewColumn *arg2, gpointer user_data))
{
	callback_read_mail();
}

/******************************************************************
 Initialize the main window
*******************************************************************/
int main_window_init(void)
{
	GtkWidget *vbox, *hbox, *hpaned;
	GtkWidget *read_button, *edit_button;

	/* Create the window */
	main_wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(main_wnd), "SimpleMail");
	gtk_window_set_default_size(GTK_WINDOW(main_wnd),640,400);
	gtk_window_set_position(GTK_WINDOW(main_wnd),GTK_WIN_POS_CENTER);
	gtk_signal_connect(GTK_OBJECT(main_wnd), "destroy",GTK_SIGNAL_FUNC (main_quit), NULL);

        /* Create the vertical box */
        vbox = gtk_vbox_new(0,4);
        gtk_container_add(GTK_CONTAINER(main_wnd), vbox);

        /* Create the toolbar */
        toolbar = gtk_toolbar_new();//GTK_ORIENTATION_HORIZONTAL,GTK_TOOLBAR_BOTH);
        gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0 /* Padding */); /* only use minimal height */

	gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Read", "Reads the selected mail", NULL /* private TT */, create_pixmap(main_wnd,"MailRead.xpm"), callback_read_mail /* CALLBACK */, NULL /* UDATA */);
	gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Edit", "Edit the selected mail", NULL /* private TT */, create_pixmap(main_wnd,"MailModify.xpm"), callback_change_mail /* CALLBACK */, NULL /* UDATA */);
	gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Delete", "Delete the selected mail", NULL /* private TT */, create_pixmap(main_wnd,"MailDelete.xpm"), NULL /* CALLBACK */, NULL /* UDATA */);
	gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Get Addr", "Insert the address into the addressbook", NULL /* private TT */, create_pixmap(main_wnd,"MailGetAddress.xpm"), NULL /* CALLBACK */, NULL /* UDATA */);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "New", "Create a new mail", NULL /* private TT */, create_pixmap(main_wnd,"MailNew.xpm"), callback_new_mail /* CALLBACK */, NULL /* UDATA */);
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Reply", "Reply the mail", NULL /* private TT */, create_pixmap(main_wnd,"MailReply.xpm"), callback_reply_mail /* CALLBACK */, NULL /* UDATA */);
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Forward", "Forward the mail", NULL /* private TT */, create_pixmap(main_wnd,"MailForward.xpm"), callback_forward_mail /* CALLBACK */, NULL /* UDATA */);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Fetch", "Fetch the mails from the POP3 Servers", NULL /* private TT */, create_pixmap(main_wnd,"MailsFetch.xpm"), callback_fetch_mails /* CALLBACK */, NULL /* UDATA */);
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Send", "Send the mails out", NULL /* private TT */, create_pixmap(main_wnd,"MailsSend.xpm"), callback_send_mails /* CALLBACK */, NULL /* UDATA */);
	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "ABook", "Open the addressbook", NULL /* private TT */, create_pixmap(main_wnd,"Addressbook.xpm"), NULL /* CALLBACK */, NULL /* UDATA */);
        gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), "Config", "Configure SimpleMail", NULL /* private TT */, create_pixmap(main_wnd,"Config.xpm"), callback_config /* CALLBACK */, NULL /* UDATA */);

        /* Create the horzontal box */
        hbox = gtk_hbox_new(0,4);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0 /* Padding */);

	/* Create the horizobtal paned between folders and mails */
	hpaned = gtk_hpaned_new();
	gtk_paned_set_position(GTK_PANED(hpaned),150);
	gtk_container_add(GTK_CONTAINER(hbox), hpaned);

        /* Create the folder tree */
	{
		gint col_offset;
		GtkTreeViewColumn *column;
		GtkCellRenderer *renderer;

		folder_scrolled_window = gtk_scrolled_window_new(NULL,NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(folder_scrolled_window),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
		folder_treestore = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_INT);

		folder_treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(folder_treestore));

		renderer = gtk_cell_renderer_text_new();
		col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (folder_treeview), -1, "Name", renderer,
						"text", FOLDER_NAME_COLUMN, NULL);

		renderer = gtk_cell_renderer_text_new();
		col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (folder_treeview), -1, "Mails", renderer,
						"text", FOLDER_MAILS_COLUMN, NULL);

		gtk_container_add(GTK_CONTAINER(folder_scrolled_window),folder_treeview);

		g_signal_connect(G_OBJECT(folder_treeview), "cursor-changed", G_CALLBACK(folder_cursor_changed), NULL);
	}

        /* Create the mail tree */
	{
		gint col_offset;
		GtkTreeViewColumn *column;
		GtkCellRenderer *renderer;

		mail_scrolled_window = gtk_scrolled_window_new(NULL,NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(mail_scrolled_window),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
		mail_treestore = gtk_tree_store_new(6, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

		mail_treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(mail_treestore));

		renderer = gtk_cell_renderer_text_new();
		col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(mail_treeview), -1, "Status", renderer,
						"text", MAIL_STATUS_COLUMN, NULL);
		column = gtk_tree_view_get_column(GTK_TREE_VIEW(mail_treeview), col_offset - 1);
		gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);

		renderer = gtk_cell_renderer_text_new();
		col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(mail_treeview), -1, "From", renderer,
						"text", MAIL_FROM_COLUMN, NULL);
		column = gtk_tree_view_get_column(GTK_TREE_VIEW(mail_treeview), col_offset - 1);
		gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);

		renderer = gtk_cell_renderer_text_new();
		col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(mail_treeview), -1, "Subject", renderer,
						"text", MAIL_SUBJECT_COLUMN, NULL);
		column = gtk_tree_view_get_column(GTK_TREE_VIEW(mail_treeview), col_offset - 1);
		gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);

		renderer = gtk_cell_renderer_text_new();
		col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(mail_treeview), -1, "Date", renderer,
						"text", MAIL_DATE_COLUMN, NULL);
		column = gtk_tree_view_get_column(GTK_TREE_VIEW(mail_treeview), col_offset - 1);
		gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);

		renderer = gtk_cell_renderer_text_new();
		col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(mail_treeview), -1, "Filename", renderer,
						"text", MAIL_FILENAME_COLUMN, NULL);
		column = gtk_tree_view_get_column(GTK_TREE_VIEW(mail_treeview), col_offset - 1);
		gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);

		renderer = gtk_cell_renderer_text_new();
		col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(mail_treeview), -1, "Ptr", renderer,
//						"visible",FALSE,
						"text", MAIL_PTR_COLUMN,
						NULL);

		gtk_container_add(GTK_CONTAINER(mail_scrolled_window),mail_treeview);

		g_signal_connect(mail_treeview, "row_activated", G_CALLBACK(mail_row_activated), NULL);
	}

	/* Add them to the paned */
	gtk_paned_add1(GTK_PANED(hpaned),folder_scrolled_window);
	gtk_paned_add2(GTK_PANED(hpaned),mail_scrolled_window);

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
		gtk_widget_show_all(main_wnd);
	}
	return 1;
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
		GtkTreeIter iter;

		gtk_tree_store_append(folder_treestore, &iter, NULL);
		gtk_tree_store_set(folder_treestore, &iter,
			FOLDER_NAME_COLUMN, f->name,
			FOLDER_MAILS_COLUMN, folder_number_of_mails(f),
			-1);

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
	void *handle = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model;

	/* load the folder */
	folder_next_mail(folder,&handle);

	/* This should be moved in a extra helper function */
	model = gtk_tree_view_get_model(folder_treeview);
	if (gtk_tree_model_get_iter_first(model,&iter))
	{
		do
		{
			gchar *name;
			gtk_tree_model_get(model,&iter,FOLDER_NAME_COLUMN,&name,-1);
			if (!strcmp(name,folder->name))
			{
				gtk_tree_store_set(folder_treestore, &iter,
					FOLDER_MAILS_COLUMN, folder_number_of_mails(folder),
					-1);
				break;
			}
		}	while ((gtk_tree_model_iter_next(model,&iter)));
	}
}

/******************************************************************
 Inserts a new mail into the listview
*******************************************************************/
void main_insert_mail(struct mail *mail)
{
	GtkTreeIter iter;
	char buf[60];

	/* This is ultra primitiv, we store the mail pointer as "text" in a separate column
	 * but currently I really have no better idea
	 */

	sprintf(buf,"%x",mail);
	gtk_tree_store_append(mail_treestore,&iter,NULL);
	gtk_tree_store_set(mail_treestore, &iter,
			MAIL_SUBJECT_COLUMN, mail->subject,
			MAIL_FROM_COLUMN, mail->from_phrase?mail->from_phrase:mail->from_addr,
			MAIL_FILENAME_COLUMN, mail->filename,
			MAIL_PTR_COLUMN, buf,
			-1);
}

/******************************************************************
 Inserts a new mail into the listview after a given position
*******************************************************************/
void main_insert_mail_pos(struct mail *mail, int after)
{
	printf("main_insert_mail_pos() not implemented\n");
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
	gtk_tree_store_clear(mail_treestore);
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

	gtk_tree_store_clear(mail_treestore);

	main_refresh_folder(folder);

	while ((m = folder_next_mail(folder,&handle)))
	{
		main_insert_mail(m);
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
	GtkTreeSelection *sel;

	sel = gtk_tree_view_get_selection(folder_treeview);
	if (sel)
	{
		GtkTreeModel *model;
		GtkTreeIter iter;

		if (gtk_tree_selection_get_selected(sel, &model, &iter))
		{
			gchar *name;
			gtk_tree_model_get(model,&iter,FOLDER_NAME_COLUMN,&name,-1);
			return folder_find_by_name(name);
		}
	}
	return NULL;
}

/******************************************************************
 Returns the current selected folder drawer, NULL if no folder
 has been selected. TODO: This is gui indep
*******************************************************************/
char *main_get_folder_drawer(void)
{
	struct folder *folder = main_get_folder();
	if (folder) return folder->path;
}

/******************************************************************
 Returns the active mail. NULL if no one is active
*******************************************************************/
struct mail *main_get_active_mail(void)
{
	GtkTreeSelection *sel;

	sel = gtk_tree_view_get_selection(mail_treeview);
	if (sel)
	{
		GtkTreeModel *model;
		GtkTreeIter iter;

		if (gtk_tree_selection_get_selected(sel, &model, &iter))
		{
			gchar *buf;
			struct mail *m;

			gtk_tree_model_get(model,&iter,MAIL_PTR_COLUMN,&buf,-1);
			m = (struct mail*)strtoul(buf,NULL,16);

			return m;
		}
	}

	return NULL;
}

/******************************************************************
 Returns the filename of the active mail, NULL if no thing is
 selected. TODO: This is gui indep
*******************************************************************/
char *main_get_mail_filename(void)
{
	struct mail *m = main_get_active_mail();
	if (m) return m->filename;
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

/******************************************************************
 Build the check singe account menu
*******************************************************************/
void main_build_accounts(void)
{
}


/******************************************************************
 Build the addressbook
*******************************************************************/
void main_build_addressbook(void)
{
}

/******************************************************************
 Freeze the mail list
*******************************************************************/
void main_freeze_mail_list(void)
{
//	set(mail_tree,MUIA_NListtree_Quiet,TRUE);
}

/******************************************************************
 Thaws the mail list
*******************************************************************/
void main_thaw_mail_list(void)
{
//	set(mail_tree,MUIA_NListtree_Quiet,FALSE);
}

/******************************************************************
 Select a mail, specified by number
*******************************************************************/
void main_select_mail(int mail)
{
//	set(mail_tree,MUIA_NList_Active, mail);
}

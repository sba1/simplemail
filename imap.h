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
** imap.h
*/

#ifndef SM__IMAP_H
#define SM__IMAP_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

struct folder;
struct mail_info;

struct imap_server
{
	struct node node;

	char *name;
	unsigned int port;
	char *login;
	char *passwd;

	int active;
	int ssl;

	char *title; /* normaly NULL, will hold a copy of account->account_name while fetching mails */
};

void imap_synchronize_really(struct list *imap_list, int called_by_auto);

int imap_get_folder_list(struct imap_server *server, void (*callback)(struct imap_server *, struct list *, struct list *));
int imap_submit_folder_list(struct imap_server *server, struct list *list);

struct imap_server *imap_malloc(void);
struct imap_server *imap_duplicate(struct imap_server *imap);
void imap_free(struct imap_server *imap);

void imap_thread_connect(struct folder *folder);
int imap_download_mail(struct folder *f, struct mail_info *m);
int imap_move_mail(struct mail_info *mail, struct folder *src_folder, struct folder *dest_folder);
int imap_delete_mail_by_filename(char *filename, struct folder *folder);
int imap_append_mail(struct mail_info *mail, char *source_dir, struct folder *dest_folder);


#endif

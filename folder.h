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
** folder.h
*/

#ifndef SM__FOLDER_H
#define SM__FOLDER_H

#ifndef SM__MAIL_H
#include "mail.h"
#endif

#ifndef SM__SUBTHREADS_H
#include "subthreads.h"
#endif

struct folder
{
	char *name; /* the name like it is displayed */
	char *path; /* the path of the folder */

	/* the following data is private only */

	/* the array of mails, no list used here because this would slowdown maybe the processing later */
	struct mail **mail_array; /* but don't access this here, use folder_next_mail() */
	int mail_array_allocated; /* how many entries could be in the array */
	int num_mails; /* number of mails in the mail_array. Might be 0 if index is not loaded! */

	int primary_sort;
	int secondary_sort;
	
	char *def_to; /* default to - useful for mailinglists */

	struct mail **sorted_mail_array; /* the sorted mail array, NULL if not sorted, 
																			the size of this array is always big as mail_array */

	int index_uptodate; /* 1 if the indexfile is uptodate */
	int mail_infos_loaded; /* 1 if the mailinfos has loaded */

	int type; /* see below */
	int special; /* see below */
	int closed; /* node is closed */

	int new_mails; /* number of new mails */
	int unread_mails; /* number of unread mails */
	int num_index_mails; /* number of mails, might be -1 for being unknown  */

	struct folder *parent_folder; /* pointer to the parent folder */

  semaphore_t sem; /* use folder_lock()/folder_unlock() */

	int is_imap; /* true if folder is imap folder */
	char *imap_server;
	char *imap_user;
	char *imap_path; /* the imap path on the server */
	int imap_popup_edit; /* state variable */
	struct list imap_all_folder_list; /* string_node * */
	struct list imap_sub_folder_list; /* string_node * */

	/* more will follow */
};

/* the sort modes of the folder */
#define FOLDER_SORT_STATUS		0
#define FOLDER_SORT_FROMTO		1
#define FOLDER_SORT_SUBJECT	2
#define FOLDER_SORT_REPLY		3
#define FOLDER_SORT_DATE			4
#define FOLDER_SORT_SIZE			5
#define FOLDER_SORT_FILENAME	6
#define FOLDER_SORT_POP3			7
#define FOLDER_SORT_RECV			8
#define FOLDER_SORT_THREAD		15  /* sort by thread, secondary is ignored */
#define FOLDER_SORT_MODEMASK	(0xf)
#define FOLDER_SORT_REVERSE	(0x80) /* reverse sort mode */

#define FOLDER_TYPE_RECV			0 /* received emails */
#define FOLDER_TYPE_SEND			1 /* send emails */
#define FOLDER_TYPE_SENDRECV	2 /* both */
#define FOLDER_TYPE_MAILINGLIST 3 /* it's a mailing list */

#define FOLDER_SPECIAL_NO 0
#define FOLDER_SPECIAL_INCOMING 1
#define FOLDER_SPECIAL_OUTGOING 2
#define FOLDER_SPECIAL_SENT 3
#define FOLDER_SPECIAL_DELETED 4
#define FOLDER_SPECIAL_GROUP 5

char *default_folder_path(void);
char *new_folder_path(void);

void folder_create_imap(void);

int init_folders(void);
void del_folders(void);

int folder_add_mail(struct folder *folder, struct mail *mail, int sort);
int folder_add_mail_incoming(struct mail *mail);
void folder_mark_deleted(struct folder *folder, struct mail *mail);
void folder_mark_undeleted(struct folder *folder, struct mail *mail);
void folder_replace_mail(struct folder *folder, struct mail *toreplace, struct mail *newmail);
int folder_number_of_mails(struct folder *folder);
int folder_number_of_unread_mails(struct folder *folder);
int folder_number_of_new_mails(struct folder *folder);
void folder_set_mail_status(struct folder *folder, struct mail *mail, int status_new);
struct mail *folder_find_mail_by_filename(struct folder *folder, char *filename);
struct mail *folder_imap_find_mail_by_uid(struct folder *folder, unsigned int uid);
void folder_imap_set_folders(struct folder *folder, struct list *all_folders_list, struct list *sub_folders_list);

int folder_set(struct folder *f, char *newname, char *newpath, int newtype, char *newdefto, int prim_sort, int second_sort);
int folder_set_would_need_reload(struct folder *f, char *newname, char *newpath, int newtype, char *newdefto);

struct folder *folder_first(void);
void folder_get_stats(int *total_msg_ptr, int *total_unread_ptr, int *total_new_ptr);

/* Like a linear list */
struct folder *folder_next(struct folder *f);
struct folder *folder_find(int pos);
int folder_position(struct folder *f);
struct folder *folder_find_by_name(char *name);
struct folder *folder_find_by_path(char *name);
struct folder *folder_find_by_mail(struct mail *mail);
struct folder *folder_find_by_imap(char *server, char *path);
struct mail *folder_find_mail(struct folder *f,int position);
struct mail *folder_find_next_mail_by_filename(char *folder_path, char *mail_filename);
struct mail *folder_find_prev_mail_by_filename(char *folder_path, char *mail_filename);
struct mail *folder_find_best_mail_to_select(struct folder *folder);
int folder_get_index_of_mail(struct folder *f, struct mail *mail);
int folder_size_of_mails(struct folder *f);
struct folder *folder_incoming(void);
struct folder *folder_outgoing(void);
struct folder *folder_sent(void);
struct folder *folder_deleted(void);
int folder_move_mail(struct folder *from_folder, struct folder *dest_folder, struct mail *mail);
int folder_delete_mail(struct folder *from_folder, struct mail *mail);
void folder_delete_deleted(void);
int folder_save_index(struct folder *f);
void folder_delete_all_indexfiles(void);
struct folder *folder_add_with_name(char *path, char *name);
struct folder *folder_add_group(char *name);
struct folder *folder_add_imap(struct folder *parent, char *imap_path);
int folder_remove(struct folder *f);

void folder_unlink_all(void);
void folder_add_to_tree(struct folder *fold,struct folder *parent);

/* This was a macro, but now is a function. Handle must point to NULL to get the first mail */
struct mail *folder_next_mail(struct folder *folder, void **handle);
/* Use this rarly */
struct mail **folder_get_mail_array(struct folder *folder);

int folder_get_primary_sort(struct folder *folder);
void folder_set_primary_sort(struct folder *folder, int sort_mode);
int folder_get_secondary_sort(struct folder *folder);
void folder_set_secondary_sort(struct folder *folder, int sort_mode);

#define folder_get_type(f) ((f)->type)

struct filter *folder_mail_can_be_filtered(struct folder *folder, struct mail *m, int action);
int folder_filter(struct folder *fold);
int folder_apply_filter(struct folder *folder, struct filter *filter);
void folder_start_search(struct search_options *sopt);

void folder_lock(struct folder *f);
int folder_attempt_lock(struct folder *f);
void folder_unlock(struct folder *f);
void folders_lock(void);
void folders_unlock(void);

void folder_load_order(void);
void folder_save_order(void);

void folder_config_save(struct folder *f);

#endif

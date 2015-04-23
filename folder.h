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

struct search_options;

struct folder
{
	char *name; /* the name like it is displayed */
	char *path; /* the path of the folder */

	/* the following data is private only */

	/* the array of mails, no list used here because this would slowdown maybe the processing later */
	struct mail_info **mail_info_array; /* but don't access this here, use folder_next_mail() */
	int mail_info_array_allocated; /* how many entries could be in the array */
	int num_mails; /* number of mails in the mail_array. Might be 0 if index is not loaded! */

	/* the array of pending added mails. This is used if you add mails but mail infos has been not read */
	struct mail_info **pending_mail_info_array;
	int pending_mail_info_array_allocated;
	int num_pending_mails;

	int primary_sort;
	int secondary_sort;
	
	char *def_to; /* default to - useful for mailinglists */
	char *def_from; /* default from address - also useful for mls */
	char *def_replyto; /* default replyto - " */
	char *def_signature; /* default Signature */ 

	struct mail_info **sorted_mail_info_array; /* the sorted mail array, NULL if not sorted, 
																			the size of this array is always big as mail_array */

	int index_uptodate; /* 1 if the indexfile is uptodate */
	int mail_infos_loaded; /* 1 if the mailinfos has loaded */
	int to_be_rescanned; /* 1 if the folder shall be rescanned */
	int rescanning; /* 1, if the folder is currently being rescanned */


	int type; /* see below */
	int special; /* see below */
	int closed; /* node is closed */

	int new_mails; /* number of new mails */
	int unread_mails; /* number of unread mails */
	int partial_mails; /* number of partial mails */
	int num_index_mails; /* real number of mails, might be -1 for being unknown  */

	struct folder *parent_folder; /* pointer to the parent folder */

	semaphore_t sem; /* use folder_lock()/folder_unlock() */

	int is_imap; /* true if folder is imap folder */
	char *imap_server;
	char *imap_user;
	char *imap_path; /* the imap path on the server */
	char *imap_hierarchy_delimiter; /* the delimiter for separating folders */

	int imap_download; /* 1, if complete emails shall be downloaded */

	/**
	 * Set to 1, if IMAP UIDs should not be used.
	 */
	int imap_dont_use_uids;

	/**
	 * The UIDVALIDITY field as determined during the most recent
	 * access to this folder on its imap server.
	 */
	unsigned int imap_uid_validity;

	/**
	 * The UIDNEXT field as determined during the most recent
	 * access to this folder on its imap serve
	 */
	unsigned int imap_uid_next;

	struct string_list imap_all_folder_list;
	struct string_list imap_sub_folder_list;

	/* Live filter support */
	utf8 *filter;
	struct folder *ref_folder;

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
#define FOLDER_SPECIAL_SPAM 6

#define FOLDER_SIGNATURE_DEFAULT NULL           /* the default signature for init */
#define FOLDER_SIGNATURE_NO      "NoSignature"  /* the no signature */

char *default_folder_path(void);
char *new_folder_path(void);

void folder_create_imap(void);

int init_folders(void);
void del_folders(void);

int folder_add_mail(struct folder *folder, struct mail_info *mail, int sort);
int folder_add_mail_incoming(struct mail_info *mail);
void folder_mark_mail_as_deleted(struct folder *folder, struct mail_info *mail);
void folder_mark_mail_as_undeleted(struct folder *folder, struct mail_info *mail);
void folder_replace_mail(struct folder *folder, struct mail_info *toreplace, struct mail_info *newmail);
int folder_number_of_mails(struct folder *folder);
int folder_number_of_unread_mails(struct folder *folder);
int folder_number_of_new_mails(struct folder *folder);
void folder_set_mail_status(struct folder *folder, struct mail_info *mail, int status_new);
void folder_set_mail_flags(struct folder *folder, struct mail_info *mail, int flags_new);
int folder_count_signatures(char *def_signature);
struct mail_info *folder_find_mail_by_filename(struct folder *folder, char *filename);
struct mail_info *folder_imap_find_mail_by_uid(struct folder *folder, unsigned int uid);
void folder_imap_set_folders(struct folder *folder, struct string_list *all_folders_list, struct string_list *sub_folders_list);

int folder_set(struct folder *f, char *newname, char *newpath, int newtype, char *newdefto, char *newdeffrom, char *newdefreplyto, char *newdefsignature, int prim_sort, int second_sort);
int folder_set_would_need_reload(struct folder *f, char *newname, char *newpath, int newtype, char *newdefto);

struct folder *folder_first(void);
void folder_get_stats(int *total_msg_ptr, int *total_unread_ptr, int *total_new_ptr);

/* Like a linear list */
struct folder *folder_prev(struct folder *f);
struct folder *folder_next(struct folder *f);
struct folder *folder_find(int pos);
struct folder *folder_find_special(int sp);
int folder_position(struct folder *f);
struct folder *folder_find_by_name(char *name);
struct folder *folder_find_by_path(char *name);
struct folder *folder_find_by_file(char *filename);
struct folder *folder_find_by_mail(struct mail_info *mail);
struct folder *folder_find_by_imap(char *user, char *server, char *path);
struct mail_info *folder_find_mail_by_position(struct folder *f,int position);
struct mail_info *folder_find_next_mail_info_by_filename(char *folder_path, char *mail_filename);
struct mail_info *folder_find_prev_mail_info_by_filename(char *folder_path, char *mail_filename);
struct mail_info *folder_find_best_mail_info_to_select(struct folder *folder);
int folder_get_index_of_mail(struct folder *f, struct mail_info *mail);
int folder_size_of_mails(struct folder *f);
struct folder *folder_incoming(void);
struct folder *folder_outgoing(void);
struct folder *folder_sent(void);
struct folder *folder_deleted(void);
struct folder *folder_spam(void);
int folder_move_mail(struct folder *from_folder, struct folder *dest_folder, struct mail_info *mail);
int folder_move_mail_array(struct folder *from_folder, struct folder *dest_folder, struct mail_info **mail_array, int num_mails);
int folder_delete_mail(struct folder *from_folder, struct mail_info *mail);
void folder_delete_deleted(void);
int folder_save_index(struct folder *f);
void folder_save_all_indexfiles(void);
void folder_delete_all_indexfiles(void);
int folder_rescan(struct folder *folder);
struct folder *folder_add_with_name(char *path, char *name);
struct folder *folder_add_group(char *name);
struct folder *folder_add_imap(struct folder *parent, char *imap_path);
int folder_remove(struct folder *f);
struct folder *folder_create_live_filter(struct folder *folder, utf8 *filter);
void folder_delete_live_folder(struct folder *live_folder);

void folder_unlink_all(void);
void folder_add_to_tree(struct folder *fold,struct folder *parent);

/* This was a macro, but now is a function. Handle must point to NULL to get the first mail */
struct mail_info *folder_next_mail(struct folder *folder, void **handle);
#define folder_next_mail_info(f,handle) folder_next_mail(f,handle)
/* Use this rarly */
struct mail_info **folder_get_mail_info_array(struct folder *folder);
/* query function, free the result with free() */
#define FOLDER_QUERY_MAILS_PROP_SPAM 1
struct mail_info **folder_query_mails(struct folder *folder, int properties);

int folder_get_primary_sort(struct folder *folder);
void folder_set_primary_sort(struct folder *folder, int sort_mode);
int folder_get_secondary_sort(struct folder *folder);
void folder_set_secondary_sort(struct folder *folder, int sort_mode);

#define folder_get_type(f) ((f)->type)

struct filter *folder_mail_can_be_filtered(struct folder *folder, struct mail_info *m, int action);
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

/* imap related */
int folder_on_same_imap_server(struct folder *f1, struct folder *f2);

/* misplaced and needs a rework */
int mail_matches_filter(struct folder *folder, struct mail_info *m, struct filter *filter);

/* needed for the subject truncation in the new mailtreelistclass */
char *mail_get_compare_subject(char *subj);

#endif

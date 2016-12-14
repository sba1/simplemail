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

/**
 * @file folder.h
 */

#ifndef SM__FOLDER_H
#define SM__FOLDER_H

#ifndef SM__STRING_LISTS_H
#include "string_lists.h"
#endif

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
	int to_be_saved; /* 1, if the index file should be saved but it couldn't be done */


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


/**
 * @brief Returns a possible path for new folders.
 *
 * @note a static buffer is used which means that you must not free the result.
 *  Additionally, this function is not thread-safe.
 * @return
 */
char *folder_get_possible_path(void);

/**
 * @brief Create necessary IMAP folders according to the current account configuration.
 *
 * Folders that exist are not created again.
 */
void folder_create_imap(void);

/**
 * @brief Initializes the folder subsystem.
 *
 * @return 0 on failure, otherwise 1.
 */
int init_folders(void);

/**
 * Cleanups the folder.
 */
void del_folders(void);

/**
 * Adds a new mail into the given folder. The mail is added at the
 * end and the sorted order is destroyed if sort = 0. Else the mail is
 * correctly sorted in and the sorted array is not destroyed.
 * The position of the mail is returned if the array was sorted or
 * the number of mails if not else (-1) for an error.
 *
 * @param folder the folder that should get the given mail added.
 * @param mail the mail to be added.
 * @param sort weather the mail should be correctly enqueued according to the
 *  current sorting order or not.
 * @return 1 if the call was successful, otherwise 0.
 */
int folder_add_mail(struct folder *folder, struct mail_info *mail, int sort);

/**
 * Mark the mail as deleted.
 *
 * @param folder the folder where the mail is located (must be an IMAP folder).
 * @param mail the mail that should be marked as deleted.
 */
void folder_mark_mail_as_deleted(struct folder *folder, struct mail_info *mail);

/**
 * Mark the mail as undeleted (only imap folders)
 *
 * @param folder the folder where the mail is located (must be an IMAP folder)
 * @param mail the mail that should be marked as undeleted.
 */
void folder_mark_mail_as_undeleted(struct folder *folder, struct mail_info *mail);

/**
 * Replaces a mail with a new one (the replaced mail isn't freed)
 * in the given folder
 *
 * @param folder the folder in which the replacement shall take place.
 * @param toreplace the mail to be replaced that is in the given folder.
 * @param newmail the mail that is going to replace the toreplace mail.
 */
void folder_replace_mail(struct folder *folder, struct mail_info *toreplace, struct mail_info *newmail);

/**
 * Returns the number of mails. For groups it counts the whole
 * number of mails.
 *
 * @param folder the folder of which the number of mails shall be determined.
 * @return number of mails or -1 for an error.
 */
int folder_number_of_mails(struct folder *folder);

/**
 * Returns the number of unread mails. For groups it counts the whole
 * number of unread mails. -1 for an error
 *
 * @param folder
 * @return
 */
int folder_number_of_unread_mails(struct folder *folder);

/**
 * Returns the number of new mails. For groups it counts the whole
 * number of unread mails. -1 for an error
 *
 * @param folder
 * @return
 */
int folder_number_of_new_mails(struct folder *folder);

/**
 * Sets a new status of a mail which is inside the given folder.
 * It also renames the file, to match the
 * status. (on the Amiga this will be done by setting a new comment later)
 * Also note that this function makes no check if the status change makes
 * sense.
 *
 * @param folder
 * @param mail
 * @param status_new
 */
void folder_set_mail_status(struct folder *folder, struct mail_info *mail, int status_new);

/**
 * Set the flags of a mail.
 *
 * @param folder
 * @param mail
 * @param flags_new
 */
void folder_set_mail_flags(struct folder *folder, struct mail_info *mail, int flags_new);

/**
 * Count how many folders use the given signature.
 *
 * @param def_signature
 * @return the number of signatures.
 */
int folder_count_signatures(char *def_signature);

/**
 * Finds a mail with a given filename in the given folder
 *
 * @param folder
 * @param filename
 * @return
 */
struct mail_info *folder_find_mail_by_filename(struct folder *folder, char *filename);

/**
 * Find a mail with a given uid (which maps to a filename) in the
 * given imap folder
 *
 * @param folder
 * @param uid
 * @return
 */
struct mail_info *folder_imap_find_mail_by_uid(struct folder *folder, unsigned int uid);

/**
 * Sets the imap folder lists of a given folders. The list elements
 * are copied.
 *
 * @param folder
 * @param all_folders_list
 * @param sub_folders_list
 */
void folder_imap_set_folders(struct folder *folder, struct string_list *all_folders_list, struct string_list *sub_folders_list);

/**
 * Set some folder attributes. Returns 1 if the folder must be
 * refreshed in the gui.
 *
 * @param f
 * @param newname
 * @param newpath
 * @param newtype
 * @param newdefto
 * @param newdeffrom
 * @param newdefreplyto
 * @param newdefsignature
 * @param prim_sort
 * @param second_sort
 * @return
 */
int folder_set(struct folder *f, char *newname, char *newpath, int newtype, char *newdefto, char *newdeffrom, char *newdefreplyto, char *newdefsignature, int prim_sort, int second_sort);

/**
 * Test if the setting the folder setting would require a reload.
 * The mails whould get disposed and reloaded in this case.
 *
 * @param f
 * @param newname
 * @param newpath
 * @param newtype
 * @param newdefto
 * @return
 */
int folder_set_would_need_reload(struct folder *f, char *newname, char *newpath, int newtype, char *newdefto);

/**
 * Returns the first folder.
 *
 * @return the first folder.
 */
struct folder *folder_first(void);

/**
 * Get status information for all folders.
 *
 * @param total_msg_ptr
 * @param total_unread_ptr
 * @param total_new_ptr
 */
void folder_get_stats(int *total_msg_ptr, int *total_unread_ptr, int *total_new_ptr);

/**
 * Return the given folder's predecessor.
 *
 * @param f the folder whose predecessor shall be returned
 * @return the predecessor of f or NULL if f is the first folder
 */
struct folder *folder_prev(struct folder *f);

/**
 * Returns the given folder's successor.
 *
 * @param f the folder whose successor shall be returned
 * @return the successor of f or NULL if f is the last folder.
 */
struct folder *folder_next(struct folder *f);

/**
 * Finds a folder at the given position.
 * @param pos
 * @return
 */
struct folder *folder_find(int pos);

/**
 * Finds a folder with the given special attribute.
 *
 * @param sp the sought special attribute
 * @return the folder that has the given special attribute.
 */
struct folder *folder_find_special(int sp);

/**
 * Returns the position of the folder. -1 if not in the list.
 *
 * @param f
 * @return
 */
int folder_position(struct folder *f);

/**
 * Finds a folder by name. Returns NULL if folder hasn't found
 *
 * @param name
 * @return
 */
struct folder *folder_find_by_name(char *name);

/**
 * Finds a folder by path. Returns NULL if folder hasn't found.
 *
 * @param name
 * @return
 */
struct folder *folder_find_by_path(char *name);

/**
 * Returns the folder that contains a file with the given name.
 *
 * @param filename the name may be absolute or relative (in this case,
 *        the folder may be not the only one)
 * @return
 */
struct folder *folder_find_by_file(char *filename);

/**
 * Finds the folder of a mail.
 *
 * @param mail the mail whose folder should be returned.
 * @return the mail that is to the
 */
struct folder *folder_find_by_mail(struct mail_info *mail);

/**
 * Returns the folder represting the given imap server data.
 *
 * @param user defines the user/login of the account
 * @param server defines the server name of the account
 * @param path defines the path of the folder.
 * @return
 */
struct folder *folder_find_by_imap(char *user, char *server, char *path);

/**
 * Returns the mail at the given position respecting the sorted order.
 *
 * @param f the folder
 * @param position the position of the mail to return.
 * @return the mail info or NULL.
 */
struct mail_info *folder_find_mail_by_position(struct folder *f,int position);

/**
 * Finds from a folder (which is given by its path) the mail's (which is given
 * by the filename) successor.
 *
 * @param folder_path the folder that contains the mail.
 * @param mail_filename the name of the mail whose successor should be
 *  determined.
 * @return
 */
struct mail_info *folder_find_next_mail_info_by_filename(char *folder_path, char *mail_filename);

/**
 * Finds from a folder (which is given by its path) the mail's (which is given
 * by the filename) predecessor.
 *
 * @param folder_path the folder that contains the mail.
 * @param mail_filename the name of the mail whose predecessor should be
 *  determined.
 * @return the predecessor or NULL of no such mail exists.
 */
struct mail_info *folder_find_prev_mail_info_by_filename(char *folder_path, char *mail_filename);

/**
 * Finds a mail that is the best candidate to be selected. Depending on the
 * folder type this could be a unread or a held mail.
 *
 * @param folder the folder
 * @return the best mail info that could be selected.
 */
struct mail_info *folder_find_best_mail_info_to_select(struct folder *folder);

/**
 * Returns the index (starting with 0) of the mail in this folder.
 * -1 if mail is not found.
 *
 * @param f the folder that contains the mail.
 * @param mail the mail whose index shall be determined.
 * @return the index or -1 if the mail is not contained.
 */
int folder_get_index_of_mail(struct folder *f, struct mail_info *mail);

/**
 * Returns the size of the mails in this folder
 *
 * @param f the folder from which to determine the size of the mails.
 * @return the size of all mails in f.
 */
int folder_size_of_mails(struct folder *f);
struct folder *folder_incoming(void);
struct folder *folder_outgoing(void);
struct folder *folder_sent(void);
struct folder *folder_deleted(void);

/**
 * Returns the local spam folder.
 *
 * @return the local spam folder.
 */
struct folder *folder_spam(void);

/**
 * Move a mail from source folder to a destination folder.  If mail has sent
 * status and moved to a outgoing drawer it gets the wait-send status.
 *
 * @param from_folder the source folder
 * @param dest_folder the dest folder
 * @param mail the mail
 * @return 0 if the moving has failed.
 */
int folder_move_mail(struct folder *from_folder, struct folder *dest_folder, struct mail_info *mail);


/**
 * Move num_mail mails from source folder to a destination folder.
 *
 * If mail has sent status and moved to a outgoing drawer it get's
 * the waitsend status.
 *
 * @param from_folder the source folder
 * @param dest_folder the dest folder
 * @param mail_array the array with mails to be moved
 * @param num_mails the number of mails within mail_array
 * @return the number of successfully moved mails.
 */
int folder_move_mail_array(struct folder *from_folder, struct folder *dest_folder, struct mail_info **mail_array, int num_mails);

/**
 * Deletes the given mail permanently
 *
 * @param from_folder
 * @param mail
 * @return
 */
int folder_delete_mail(struct folder *from_folder, struct mail_info *mail);

/**
 * Really delete all mails in the delete folder.
 */
void folder_delete_deleted(void);

/**
 * Save the index of an folder. If no mail infos are loaded or
 * the index is uptodate nothing happens.
 */
int folder_save_index(struct folder *f);

/**
 * Save the index files of all folders.
 */
void folder_save_all_indexfiles(void);

/**
 * @brief delete all the index files.
 */
void folder_delete_all_indexfiles(void);

/**
 * Rescan the given folder, i.e., index all mails in the folder.
 *
 * @param folder the folder to be rescanned.
 * @param status_callback defines the function that is called for staus updates.
 * @return 0 on failure, everything else on success.
 */
int folder_rescan(struct folder *folder, void (*status_callback)(const char *txt));

/**
 * Rescan the given folder in an asychronous manner, i.e., index all mails in the folder.
 *
 * @param folder the folder to be rescanned.
 * @param status_callback defines the function that is called for staus updates.
 * @return 0 on failure, everything else on success.
 */
int folder_rescan_async(struct folder *folder, void (*status_callback)(const char *txt), void (*completed)(char *folder_path, void *udata), void *udata);

/**
 * Adds a new folder that stores messages in the given path
 * and has the given name.
 *
 * @param path
 * @param name
 * @return
 */
struct folder *folder_add_with_name(char *path, char *name);

/**
 * Adds a folder group to the internal folder list with a given name.
 *
 * @param name
 * @return
 */
struct folder *folder_add_group(char *name);

/**
 * Adds an folder specified by the given path to an imap folder.
 *
 * @param parent specifies the parent imap folder.
 * @param imap_path the imap path of the new folder.
 * @return
 */
struct folder *folder_add_imap(struct folder *parent, char *imap_path);

/**
 * Removes the given folder from the folder list, if possible.
 *
 * @param f folder to be removed.
 * @return 0 on failure, otherwise the call has been succeeded.
 */
int folder_remove(struct folder *f);

/**
 * Create a live filter folder using the given filter string.
 *
 * @todo Add support for imap. Add better ref support. Add real live
 * support.
 *
 * @param folder the folder for which the live folder shall be created.
 * @param filter the string that defines the mails that should be present in
 *  the live folder.
 * @return the folder or NULL in case of an error.
 */
struct folder *folder_create_live_filter(struct folder *folder, utf8 *filter);

/**
 * Free all memory associated with a live folder
 *
 * @param live_folder the folder that shall be deleted.
 */
void folder_delete_live_folder(struct folder *live_folder);

/**
 * Unlink all folders from the list
 *
 */
void folder_unlink_all(void);

/**
 * Adds a folder to parent folder.
 *
 * @param f the folder that is added to the parent.
 * @param parent the parent
 */
void folder_add_to_tree(struct folder *f, struct folder *parent);


/**
 * The mail iterating function. To get the first mail let handle
 * point to NULL. If needed this function sorts the mails according
 * to the sort mode. Handle will be updated every time.
 * While iterating through the mails you aren't allowed to (re)move a
 * mail within the folder.
 *
 * @param folder
 * @param handle
 * @return
 */
struct mail_info *folder_next_mail(struct folder *folder, void **handle);

#define folder_next_mail_info(f,handle) folder_next_mail(f,handle)

/**
 * This function is the same as above but it returns the complete
 * array or NULL for an error. Check folder.num_mails for size of
 * the array. NOTE: function can disappear in a future, so use
 * it rarely. It's better to use folder_next_mail() instead
 *
 * @param folder the folder whose mail infos shall be retrieved.
 * @return the array of mail_info elements.
 */
struct mail_info **folder_get_mail_info_array(struct folder *folder);


/* query function, free the result with free() */
#define FOLDER_QUERY_MAILS_PROP_SPAM 1

/**
 * Returns a NULL terminated array of mail with the given properties
 * or NULL. No properties means all mails. The array must be freed
 * with free()
 *
 * @param folder
 * @param properties defines the properties, e.g., FOLDER_QUERY_MAILS_PROP_SPAM
 *  or 0 for no property.
 * @return
 */
struct mail_info **folder_query_mails(struct folder *folder, int properties);

/**
 * Returns the primary sort mode.
 *
 * @param folder the folder whose primary sort mode shall be get.
 * @return the primary sort mode
 */
int folder_get_primary_sort(struct folder *folder);

/**
 * Sets the primary sort mode of the given folder.
 *
 * @param folder the folder whose primary sort mode shall be set.
 * @param sort_mode the sort mode.
 */
void folder_set_primary_sort(struct folder *folder, int sort_mode);

/**
 * Returns the secondary sort mode.
 *
 * @param folder the folder whose secondary sort mode shall be get.
 * @return the secondary sort mode of the given folder.
 */
int folder_get_secondary_sort(struct folder *folder);

/**
 * Sets the secondary sort mode of the given folder.
 *
 * @param folder the folder whose secondary sort mode shall be set.
 * @param sort_mode the sort mode.
 */
void folder_set_secondary_sort(struct folder *folder, int sort_mode);

#define folder_get_type(f) ((f)->type)

/**
 * Checks if a mail should be filtered.
 *
 * @param folder the folder in which the mail to be checked resides.
 * @param m the mail that should be checked.
 * @param action the type of filter that should be considered. 0 on request,
 *  1 for new mails, 2 for sent mails.
 * @return the first filter that matches the mail.
 */
struct filter *folder_mail_can_be_filtered(struct folder *folder, struct mail_info *m, int action);

/**
 * Filter all mails in the given folder.
 *
 * @param folder the folder on which all active filters are to be applied.
 * @return 1 on success, 0 otherwise.
 */
int folder_filter(struct folder *folder);

/**
 * Filter all mails in the given folder using a single filter.
 *
 * @param folder the folder on which the filter is to be applied.
 * @param filter the filter that is applied.
 * @return 1 on success or 0 failure.
 */
int folder_apply_filter(struct folder *folder, struct filter *filter);

/**
 * Lock the folder, to prevent any change to it.
 *
 * @param f
 */
void folder_lock(struct folder *f);

/**
 * Tries to lock the folder. Returns FALSE if folder is used.
 *
 * @param f
 * @return
 */
int folder_attempt_lock(struct folder *f);

/**
 * Unlock the folder.
 *
 * @param f
 */
void folder_unlock(struct folder *f);

/**
 * Lock the global folder semaphore
 */
void folders_lock(void);

/**
 * Unlock the global folder semaphore.
 */
void folders_unlock(void);

/**
 * @brief Loads the order of the folders
 */
void folder_load_order(void);

/**
 * @brief Saves the order of the folders.
 */
void folder_save_order(void);

/**
 * Save the current configuration for the folder
 *
 * @param f the folder whose configuration shall be saved.
 */
void folder_config_save(struct folder *f);

/* imap related */

/**
 * @brief Checks whether two folders are on the same imap server.
 * Returns 1 if this is the case else 0.
 *
 * @param f1
 * @param f2
 * @return
 */
int folder_on_same_imap_server(struct folder *f1, struct folder *f2);

/**
 * Checks if the given filter matches the mail.
 *
 * @param folder where the mail is located. Can be NULL.
 * @param m the mail that should be checked against the given filter.
 * @param filter the filter that should be checked.
 * @return checks if the given matches the filter.
 * @todo misplaced in this module. Needs a rework
 */
int mail_matches_filter(struct folder *folder, struct mail_info *m, struct filter *filter);

/**
 * Given a subject line, return a subject line that can be used for comparing.
 *
 * @param subj the string that should be transformed
 * @return the transformed string
 */
char *mail_get_compare_subject(char *subj);

#endif

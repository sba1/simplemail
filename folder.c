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
** folder.c
*/

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h> /* toupper() */
#include <dirent.h> /* unix dir stuff */
#include <sys/stat.h> /* state() */
#include <unistd.h>

#include "codesets.h"
#include "configuration.h"
#include "lists.h"
#include "filter.h"
#include "folder.h"
#include "parse.h"
#include "simplemail.h"
#include "smintl.h"
#include "support.h"
#include "support_indep.h"

#include "gui_main.h" /* gui_execute_arexx() */
#include "searchwnd.h"

#define FOLDER_INDEX_VERSION 6

static void folder_remove_mail(struct folder *folder, struct mail *mail);

/* folder sort stuff
   to control the compare functions */
static int compare_primary_reverse;
static int (*compare_primary)(const struct mail *arg1, const struct mail *arg2, int reverse);
static int compare_secondary_reverse;
static int (*compare_secondary)(const struct mail *arg1, const struct mail *arg2, int reverse);

/* the global folder lock semaphore */
static semaphore_t folders_semaphore;

/******************************************************************
 The general sorting function
*******************************************************************/
static int mail_compare(const void *arg1, const void *arg2)
{
	int ret = 0;

	if (compare_primary) ret = compare_primary(*(const struct mail**)arg1,*(const struct mail**)arg2,compare_primary_reverse);
	if (ret == 0 && compare_secondary) ret = compare_secondary(*(const struct mail**)arg1,*(const struct mail**)arg2,compare_secondary_reverse);
	return ret;
}

/******************************************************************
 The special sorting functions
*******************************************************************/
static int mail_compare_status(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	int rc;

	if (arg1->flags & MAIL_FLAGS_NEW && !(arg2->flags & MAIL_FLAGS_NEW)) rc = -1;
	else if (arg2->flags & MAIL_FLAGS_NEW && !(arg1->flags & MAIL_FLAGS_NEW)) rc = 1;
	else rc = (arg1->status & MAIL_STATUS_MASK) - (arg2->status & MAIL_STATUS_MASK);
	if (reverse) rc *= -1;
	return rc;
}

static int mail_compare_from(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	int rc = utf8stricmp(mail_get_from(arg1),mail_get_from(arg2));
	if (reverse) rc *= -1;
	return rc;
}

static int mail_compare_to(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	int rc = utf8stricmp(mail_get_to(arg1),mail_get_to(arg2));
	if (reverse) rc *= -1;
	return rc;
}

static int mail_compare_subject(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	int rc = utf8stricmp(arg1->subject,arg2->subject);
	if (reverse) rc *= -1;
	return rc;
}

static int mail_compare_reply(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	int rc = utf8stricmp(arg1->reply_addr,arg2->reply_addr);
	if (reverse) rc *= -1;
	return rc;
}

static int mail_compare_date(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	if (arg1->seconds > arg2->seconds) return reverse?(-1):1;
	else if (arg1->seconds == arg2->seconds) return 0;
	return reverse?1:(-1);
}

static int mail_compare_size(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	if (arg1->size > arg2->size) return reverse?(-1):1;
	else if (arg1->size == arg2->size) return 0;
	return reverse?1:(-1);
}

static int mail_compare_filename(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	return mystricmp(arg1->filename, arg2->filename);
}

static int mail_compare_pop3(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	return mystricmp(arg1->pop3_server, arg2->pop3_server);
}

static int mail_compare_recv(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	if (arg1->received > arg2->received) return reverse?(-1):1;
	else if (arg1->received == arg2->received) return 0;
	return reverse?1:(-1);
}

/******************************************************************
 Returns the correct sorting function and fills the reverse pointer
*******************************************************************/
static void *get_compare_function(int sort_mode, int *reverse, int folder_type)
{
	if (sort_mode & FOLDER_SORT_REVERSE) *reverse = 1;
	else *reverse = 0;

	switch (sort_mode & FOLDER_SORT_MODEMASK)
	{
		case	FOLDER_SORT_STATUS: return mail_compare_status;
		case	FOLDER_SORT_FROMTO: return folder_type?mail_compare_to:mail_compare_from;
		case	FOLDER_SORT_SUBJECT: return mail_compare_subject;
		case	FOLDER_SORT_REPLY: return mail_compare_reply;
		case	FOLDER_SORT_DATE: return mail_compare_date;
		case	FOLDER_SORT_SIZE: return mail_compare_size;
		case	FOLDER_SORT_FILENAME: return mail_compare_filename;
		case	FOLDER_SORT_POP3: return mail_compare_pop3;
		case	FOLDER_SORT_RECV: return mail_compare_recv;
	}
	return NULL; /* thread */
}

/******************************************************************
 Set the sort functions for mail_compare
*******************************************************************/
static void mail_compare_set_sort_mode(struct folder *folder)
{
	compare_primary = (int (*)(const struct mail *, const struct mail *, int))get_compare_function(folder->primary_sort, &compare_primary_reverse, folder->type);
	compare_secondary = (int (*)(const struct mail *, const struct mail *, int))get_compare_function(folder->secondary_sort, &compare_secondary_reverse, folder->type);
}



static void folder_delete_mails(struct folder *folder);
static int folder_read_mail_infos(struct folder *folder, int only_num_mails);

static struct list folder_list;
static struct folder *folder_root;

struct folder_node
{
	struct node node;
	struct folder folder; /* this must follow! */
};

/******************************************************************
 Returns the node of the folder, for list handling
*******************************************************************/
static struct folder_node *find_folder_node_by_folder(struct folder *f)
{
	struct folder_node *node = (struct folder_node*)list_first(&folder_list);

	while (node)
	{
		if (&node->folder == f)
		{
			return node;
		}
		node = (struct folder_node*)node_next(&node->node);
	}
	return NULL;
}


static char *fread_str(FILE *fh);
static char *fread_str_no_null(FILE *fh);
static void folder_config_save(struct folder *f);
static int folder_config_load(struct folder *f);

/******************************************************************
 Opens the indexfile of the given folder
*******************************************************************/
static FILE *folder_open_indexfile(struct folder *f, char *mode)
{
	FILE *fh;
	char *path;
	char *index_name;
	char *filename_ptr;

	char cpath[256];

	if (!f || !f->path) return 0;
	if (f->special == FOLDER_SPECIAL_GROUP) return 0;
	if (!(path = mystrdup(f->path))) return 0;

	*sm_path_part(path) = 0;
	filename_ptr = sm_file_part(f->path);
	index_name = (char*)malloc(strlen(filename_ptr)+16);
	if (!index_name)
	{
		free(path);
		return 0;
	}

	sprintf(index_name,"%s.index",filename_ptr);

	getcwd(cpath, sizeof(cpath));
	if(chdir(path) == -1)
	{
		free(index_name);
		free(path);
		return 0;
	}

	fh = fopen(index_name,mode);

	chdir(cpath);
	free(index_name);
	free(path);
	return fh;
}

/******************************************************************
 Delete the indexfile of the given folder
*******************************************************************/
static void folder_delete_indexfile(struct folder *f)
{
	char *path;
	char *index_name;
	char *filename_ptr;

	char cpath[256];

	if (!f || !f->path) return;
	if (!(path = mystrdup(f->path))) return;

	*sm_path_part(path) = 0;
	filename_ptr = sm_file_part(f->path);
	index_name = (char*)malloc(strlen(filename_ptr)+16);
	if (!index_name)
	{
		free(path);
		return;
	}

	sprintf(index_name,"%s.index",filename_ptr);

	getcwd(cpath, sizeof(cpath));
	if(chdir(path) == -1)
	{
		free(index_name);
		free(path);
		return;
	}

	remove(index_name);

	chdir(cpath);
	free(index_name);
	free(path);
}

/******************************************************************
 Delete all the index files
*******************************************************************/
void folder_delete_all_indexfiles(void)
{
	struct folder *f = folder_first();
	while (f)
	{
		folder_delete_indexfile(f);
		f = folder_next(f);
	}
}

/******************************************************************
 Prepare the folder for a given ammount of mails
*******************************************************************/
static int folder_prepare_for_additional_mails(struct folder *folder, int num_mails)
{
	folder->mail_array_allocated += num_mails + 5;
	folder->mail_array = realloc(folder->mail_array,folder->mail_array_allocated*sizeof(struct mail*));
	return folder->mail_array?1:0;
}

/******************************************************************
 Adds a new mail into the given folder. The mail is added at the
 end and the sort is destroyed if sort = 0. Else the mail is
 correctly sorted in and the sorted array is not destroyed.
 The position of the mail is returned if the array was sorted or
 the number of mails if not else (-1) for an error.
*******************************************************************/
int folder_add_mail(struct folder *folder, struct mail *mail, int sort)
{
	int i,pos;

	/* If mails info is not read_yet, read it now */
	if (!folder->mail_infos_loaded)
	{
		folder_read_mail_infos(folder,0);
		
		/* check if the mail is already inside, this happens if you want to add a new mail and the mail infos
		   must have been loaded */
		for (i=0;i<folder->num_mails;i++)
		{
			struct mail *checkm = folder->mail_array[i];
			if (!mystricmp(mail->filename,checkm->filename))
			{
				/* we found the mail, now the easiest way is to remove it, but's not the fastest */
				folder_remove_mail(folder,checkm);
				mail_free(checkm);
				break;
			}
		}
	}

	/* free the sorted mail array */
	if (folder->sorted_mail_array && !sort)
	{
		free(folder->sorted_mail_array);
		folder->sorted_mail_array = NULL;
	} else if (!folder->sorted_mail_array && sort)
	{
		void *handle = NULL;
		/* this should sort the folder */
		folder_next_mail(folder, &handle);
	}

	/* delete the indexfile if not already done */
	if (folder->index_uptodate)
	{
		folder_delete_indexfile(folder);
		folder->index_uptodate = 0;
	}

	if (folder->mail_array_allocated == folder->num_mails)
	{
		folder->mail_array_allocated += 50;
		folder->mail_array = realloc(folder->mail_array,folder->mail_array_allocated*sizeof(struct mail*));

		if (folder->sorted_mail_array)
			folder->sorted_mail_array = realloc(folder->sorted_mail_array,folder->mail_array_allocated*sizeof(struct mail*));
	}

	if (!folder->mail_array)
	{
		folder->mail_array_allocated = 0;
		return -1;
	}

	if (mail->message_id)
	{
		/* check if there is already an mail with the same message id, this would cause
  	   problems */
		for (i=0;i<folder->num_mails;i++)
		{
			struct mail *fm = folder->mail_array[i];
			if (!(mystricmp(mail->message_id,fm->message_id)))
			{
				if (mail->message_id)
				{
					free(mail->message_id);
					mail->message_id = NULL;
				}
			}
		}
	}

	if (folder->sorted_mail_array)
	{
		mail_compare_set_sort_mode(folder);

		/* this search routine has O(n) but should be improved to O(log n) with binary serach */
		for (pos=0;pos<folder->num_mails;pos++)
		{
			if (mail_compare(&folder->sorted_mail_array[pos],&mail) > 0) break;
		}

		memmove(&folder->sorted_mail_array[pos+1],&folder->sorted_mail_array[pos],(folder->num_mails - pos)*sizeof(struct mail*));
		folder->sorted_mail_array[pos] = mail;
	} else pos = folder->num_mails;

	folder->mail_array[folder->num_mails++] = mail;
	if (folder->num_mails > folder->num_index_mails) folder->num_index_mails = folder->num_mails;
	if (mail_get_status_type(mail) == MAIL_STATUS_UNREAD) folder->unread_mails++;
	if (mail->flags & MAIL_FLAGS_NEW) folder->new_mails++;

	/* sort the mails for threads */
	if (mail->message_id)
	{
		for (i=0;i<folder->num_mails-1;i++) /* ignore the last mail since it is this mail */
		{
			struct mail *fm = folder->mail_array[i];
			if (!(mystricmp(mail->message_id,fm->message_reply_id)))
			{
				if (!mail->sub_thread_mail)
				{
					mail->sub_thread_mail = fm;
				} else
				{
					struct mail *m = mail->sub_thread_mail;
					struct mail *next = m->next_thread_mail;
					while (next)
					{
						m = next;
						next = next->next_thread_mail;
					}
					m->next_thread_mail = fm;
				}
				fm->child_mail = 1;
			}
		}
	}
	if (mail->message_reply_id)
	{
		for (i=0;i<folder->num_mails-1;i++) /* ignore the last mail since it is this mail */
		{
			struct mail *fm = folder->mail_array[i];
			if (!(mystricmp(mail->message_reply_id,fm->message_id)))
			{
				if (!fm->sub_thread_mail)
				{
					fm->sub_thread_mail = mail;
				} else
				{
					struct mail *m = fm->sub_thread_mail;
					struct mail *next = m->next_thread_mail;
					while (next)
					{
						m = next;
						next = next->next_thread_mail;
					}
					m->next_thread_mail = mail;
				}
				mail->child_mail = 1;
				break;
			}
		}
	}

	return pos;
}

/******************************************************************
 Removes a mail from the given folder.
 (does not free it)
*******************************************************************/
static void folder_remove_mail(struct folder *folder, struct mail *mail)
{
	int i;
	struct mail *submail;

	/* lock the folder, because we are going to remove something */
	folder_lock(folder);

	/* If mails info is not read_yet, read it now */
	if (!folder->mail_infos_loaded)
		folder_read_mail_infos(folder,0);

	/* free the sorted mail array */
	if (folder->sorted_mail_array)
	{
		free(folder->sorted_mail_array);
		folder->sorted_mail_array = NULL;
	}

	/* delete the indexfile if not already done */
	if (folder->index_uptodate)
	{
		folder_delete_indexfile(folder);
		folder->index_uptodate = 0;
	}

	for (i=0; i < folder->num_mails; i++)
	{
		if (folder->mail_array[i]->sub_thread_mail == mail)
		{
			folder->mail_array[i]->sub_thread_mail = mail->next_thread_mail;
/*		struct mail *nm = mail->next_thread_mail;
			folder->mail_array[i]->sub_thread_mail = NULL;
			while (nm)
			{
				struct mail *save = nm;
				nm = mail->next_thread_mail;
				save->child_mail = 0;
				save->next_thread_mail = NULL;
			}*/
		}

		if (folder->mail_array[i]->next_thread_mail == mail)
		{
			folder->mail_array[i]->next_thread_mail = mail->next_thread_mail;
		}
	}

	if ((submail = mail->sub_thread_mail))
	{
		while (submail)
		{
			struct mail *next = submail->next_thread_mail;
			submail->next_thread_mail = NULL;
			submail->child_mail = 0;
			submail = next;
		}
	}
	mail->sub_thread_mail = NULL;
	mail->next_thread_mail = NULL;
	mail->child_mail = 0;

	for (i=0; i < folder->num_mails; i++)
	{
		if (folder->mail_array[i] == mail)
		{
			folder->num_mails--;

			for (; i < folder->num_mails; i++)
			{
				folder->mail_array[i] = folder->mail_array[i+1];
			}
		}
	}

	folder->num_index_mails--;
	if ((mail_get_status_type(mail) == MAIL_STATUS_UNREAD) && folder->unread_mails) folder->unread_mails--;
	if ((mail->flags & MAIL_FLAGS_NEW) && folder->new_mails) folder->new_mails--;

	folder_unlock(folder);
}

/******************************************************************
 Replaces a mail with a new one (the replaced mail isn't freed)
 in the given folder
*******************************************************************/
void folder_replace_mail(struct folder *folder, struct mail *toreplace, struct mail *newmail)
{
	int i;

  folder_lock(folder);

	/* If mails info is not read_yet, read it now */
	if (!folder->mail_infos_loaded)
		folder_read_mail_infos(folder,0);

	/* free the sorted mail array */
	if (folder->sorted_mail_array)
	{
		free(folder->sorted_mail_array);
		folder->sorted_mail_array = NULL;
	}

	/* Delete the indexfile if not already done */
	if (folder->index_uptodate)
	{
		folder_delete_indexfile(folder);
		folder->index_uptodate = 0;
	}

	for (i=0; i < folder->num_mails; i++)
	{
		if (folder->mail_array[i] == toreplace)
		{
			folder->mail_array[i] = newmail;
			break;
		}
	}

	/* update the mail statistics */
	if ((mail_get_status_type(toreplace) == MAIL_STATUS_UNREAD) && folder->unread_mails) folder->unread_mails--;
	if ((toreplace->flags & MAIL_FLAGS_NEW) && folder->new_mails) folder->new_mails--;

	if (mail_get_status_type(newmail) == MAIL_STATUS_UNREAD) folder->unread_mails++;
	if (newmail->flags & MAIL_FLAGS_NEW) folder->new_mails++;

  folder_unlock(folder);
}

/******************************************************************
 Returns the number of mails. For groups it counts the whole
 number of mails. -1 for an error
*******************************************************************/
int folder_number_of_mails(struct folder *folder)
{
	if (folder->special == FOLDER_SPECIAL_GROUP)
	{
		int num_mails = 0;
		struct folder *iter = folder_next(folder);
		struct folder *parent = folder->parent_folder;
		
		while (iter)
		{
			if (iter->parent_folder == parent) break;
			if (iter->special != FOLDER_SPECIAL_GROUP)
			{
				if (iter->num_index_mails == -1) return -1;
				num_mails += iter->num_index_mails;
			}
			iter = folder_next(iter);
		}

		return num_mails;
	} else return folder->num_index_mails;
}

/******************************************************************
 Returns the number of unread mails. For groups it counts the whole
 number of unread mails. -1 for an error
*******************************************************************/
int folder_number_of_unread_mails(struct folder *folder)
{
	if (folder->special == FOLDER_SPECIAL_GROUP)
	{
		int num_mails = 0;
		struct folder *iter = folder_next(folder);
		struct folder *parent = folder->parent_folder;
		
		while (iter)
		{
			if (iter->parent_folder == parent) break;
			if (iter->special != FOLDER_SPECIAL_GROUP)
			{
				if (iter->unread_mails == -1) return -1;
				num_mails += iter->unread_mails;
			}
			iter = folder_next(iter);
		}

		return num_mails;
	} else return folder->unread_mails;
}

/******************************************************************
 Returns the number of new mails. For groups it counts the whole
 number of unread mails. -1 for an error
*******************************************************************/
int folder_number_of_new_mails(struct folder *folder)
{
	if (folder->special == FOLDER_SPECIAL_GROUP)
	{
		int num_mails = 0;
		struct folder *iter = folder_next(folder);
		struct folder *parent = folder->parent_folder;
		
		while (iter)
		{
			if (iter->parent_folder == parent) break;
			if (iter->special != FOLDER_SPECIAL_GROUP)
			{
				if (iter->unread_mails == -1) return -1;
				num_mails += iter->new_mails;
			}
			iter = folder_next(iter);
		}

		return num_mails;
	} else return folder->new_mails;
}

/******************************************************************
 Sets a new status of a mail which is inside the given folder.
 It also renames the file, to match the
 status. (on the Amiga this will be done by setting a new comment later)
 Also note that this function makes no check if the status change makes
 sense.
*******************************************************************/
void folder_set_mail_status(struct folder *folder, struct mail *mail, int status_new)
{
	int i;
	for (i=0;i<folder->num_mails;i++)
	{
		if (folder->mail_array[i]==mail)
		{
			char *filename;

			if (status_new == mail->status)
				return;

			/* update the mail statistics */
			if (mail_get_status_type(mail) == MAIL_STATUS_UNREAD && folder->unread_mails) folder->unread_mails--;
			if ((status_new & MAIL_STATUS_MASK) == MAIL_STATUS_UNREAD) folder->unread_mails++;

			mail->status = status_new;
			if (!mail->filename) return;

			filename = mail_get_status_filename(mail->filename, status_new);

			if (strcmp(mail->filename,filename))
			{
				char buf[256];
				int renamed = 0;

				getcwd(buf, sizeof(buf));
				chdir(folder->path);

				if (rename(mail->filename,filename))
				{
					/* renaming went wrong, try a different name */
					free(filename);
					filename = mail_get_new_name(status_new);
					if (!rename(mail->filename,filename)) renamed = 1;
				} else renamed = 1;

				if (renamed)
				{
					free(mail->filename);
					mail->filename = filename;
				}

				chdir(buf);

				/* Delete the indexfile if not already done */
				if (folder->index_uptodate)
				{
					folder_delete_indexfile(folder);
					folder->index_uptodate = 0;
				}
			}
		}
	}
}

/******************************************************************
 Finds a mail with a given filename in the given folder
*******************************************************************/
struct mail *folder_find_mail_by_filename(struct folder *folder, char *filename)
{
	int i;

	for (i=0; i < folder->num_mails; i++)
	{
		if (!mystricmp(folder->mail_array[i]->filename,filename))
		{
			return folder->mail_array[i];
		}
	}
	return NULL;
}

/******************************************************************
 Reads the all mail infos in the given folder.
 TODO: Speed up this by using indexfiles (also get rid of readdir
 ans friends)
 returns 0 if an error has happended otherwise 0
*******************************************************************/
static int folder_read_mail_infos(struct folder *folder, int only_num_mails)
{
	FILE *fh;
  int mail_infos_read = 0;

	if (folder->special == FOLDER_SPECIAL_GROUP) return 0;

	if ((fh = folder_open_indexfile(folder,"rb")))
	{
		char buf[4];
		fread(buf,1,4,fh);
		if (!strncmp("SMFI",buf,4))
		{
			int ver;
			fread(&ver,1,4,fh);
			if (ver == FOLDER_INDEX_VERSION)
			{
				int num_mails;
				int unread_mails=0;
				
				fread(&num_mails,1,4,fh);
				fread(&unread_mails,1,4,fh);

				if (!only_num_mails)
				{
					folder->mail_infos_loaded = 1; /* must happen before folder_add_mail() */
					mail_infos_read = 1;
					folder->unread_mails=0;
					folder_prepare_for_additional_mails(folder, num_mails);
					while (num_mails--)
					{
						struct mail *m = mail_create();
						if (m)
						{
							int first = 1;
							int num_to = 0;
							int num_cc = 0;

							fread(&num_to,1,4,fh);
							fread(&num_cc,1,4,fh);

							m->subject = fread_str(fh);
							m->filename = fread_str(fh);
							m->from_phrase = fread_str_no_null(fh);
							m->from_addr = fread_str_no_null(fh);

							/* Read the to list */
							if ((m->to_list = malloc(sizeof(struct list))))
								list_init(m->to_list);

							while (num_to--)
							{
								char *realname = fread_str_no_null(fh);
								char *email = fread_str_no_null(fh);
								struct address *addr;

								if (first)
								{
									m->to_phrase = mystrdup(realname);
									m->to_addr = mystrdup(email);
									first = 0;
								}

								if (m->to_list)
								{
									if ((addr = malloc(sizeof(struct address))))
									{
										addr->realname = realname;
										addr->email = email;
										list_insert_tail(m->to_list, &addr->node);
									} /* should free realname and email on failure */
								}
							}

							/* Read the cc list */
							if ((m->cc_list = malloc(sizeof(struct list))))
								list_init(m->cc_list);

							while (num_cc--)
							{
								char *realname = fread_str_no_null(fh);
								char *email = fread_str_no_null(fh);
								struct address *addr;

								if (m->cc_list)
								{
									if ((addr = malloc(sizeof(struct address))))
									{
										addr->realname = realname;
										addr->email = email;
										list_insert_tail(m->cc_list, &addr->node);
									} /* should free realname and email on failure */
								}
							}

							m->pop3_server = fread_str_no_null(fh);
							m->message_id = fread_str_no_null(fh);
							m->message_reply_id = fread_str_no_null(fh);
							m->reply_addr = fread_str_no_null(fh);

							fseek(fh,ftell(fh)%2,SEEK_CUR);
							fread(&m->size,1,sizeof(m->size),fh);
							fread(&m->seconds,1,sizeof(m->seconds),fh);
							fread(&m->received,1,sizeof(m->received),fh);
							fread(&m->flags,1,sizeof(m->flags),fh);
							mail_identify_status(m);
							mail_process_headers(m);
							m->flags &= ~MAIL_FLAGS_NEW;
							folder_add_mail(folder,m,0);
						}
					}
				} else
				{
					folder->num_index_mails = num_mails;
					folder->unread_mails = unread_mails;
				}
			}
		}
		fclose(fh);
	}

	if (only_num_mails)
		return 1;

	folder->index_uptodate = mail_infos_read;

	if (!mail_infos_read)
	{
		DIR *dfd; /* directory descriptor */
		struct dirent *dptr; /* dir entry */

		char path[256];

		getcwd(path, sizeof(path));
		if(chdir(folder->path) == -1) return 0;

#ifdef _AMIGA
		if ((dfd = opendir("")))
#else
		if ((dfd = opendir("./")))
#endif
		{
			folder->mail_infos_loaded = 1; /* must happen before folder_add_mail() */
			folder->num_index_mails = 0;

			while ((dptr = readdir(dfd)) != NULL)
			{
				struct mail *m;

				if (!strcmp(".",dptr->d_name) || !strcmp("..",dptr->d_name)) continue;

				if ((m = mail_create_from_file(dptr->d_name)))
				{
					folder_add_mail(folder,m,0);
				}
			}
			closedir(dfd);
		}// else printf("%s\n",strerror(errno));

		chdir(path);
	}
	return 1;
}

/******************************************************************
 Adds a mail to the incoming folder (the mail not actually not
 copied). The mail will get a New Flag. The mail is correctly
 sorted in and the position is returned (-1 for an error)
*******************************************************************/
int folder_add_mail_incoming(struct mail *mail)
{
	struct folder *folder = folder_incoming(); /* currently this is the incoming folder */
	if (folder)
	{
		mail->flags |= MAIL_FLAGS_NEW;
		return folder_add_mail(folder,mail,1);
	}
	return -1;
}

/******************************************************************
 Adds a folder to the internal folder list
*******************************************************************/
static struct folder *folder_add(char *path)
{
	struct folder_node *node;

	if (folder_find_by_path(path)) return NULL;

	if ((node = (struct folder_node*)malloc(sizeof(struct folder_node))))
	{
		/* Initialize everything with 0 */
		memset(node,0,sizeof(struct folder_node));
		node->folder.num_index_mails = -1;

		/* create the directory if it doesn't exists */
		if (sm_makedir(path))
		{
			if ((node->folder.sem = thread_create_semaphore()))
			{
				if ((node->folder.path = mystrdup(path)))
				{
					char buf[256];

					node->folder.primary_sort = FOLDER_SORT_DATE;
					node->folder.secondary_sort = FOLDER_SORT_FROMTO;

					sprintf(buf,"%s.config",path);

					if (!folder_config_load(&node->folder))
					{
						char *folder_name = sm_file_part(path);
						if ((node->folder.name = mystrdup(folder_name)))
						{
							node->folder.name[0] = toupper((unsigned char)(node->folder.name[0]));
						}
						if (!mystricmp(folder_name,"Income") || !mystricmp(folder_name,"Incoming")) node->folder.special = FOLDER_SPECIAL_INCOMING;
						if (!mystricmp(folder_name,"Outgoing")) node->folder.special = FOLDER_SPECIAL_OUTGOING;
						if (!mystricmp(folder_name,"Sent")) node->folder.special = FOLDER_SPECIAL_SENT;
						if (!mystricmp(folder_name,"Deleted")) node->folder.special = FOLDER_SPECIAL_DELETED;
						folder_config_save(&node->folder);
					}
					folder_read_mail_infos(&node->folder,1);
					list_insert_tail(&folder_list,&node->node);
					return &node->folder;
				}
				thread_dispose_semaphore(node->folder.sem);
			}
		}
		free(node);
	}
	return NULL;
}

/******************************************************************
 Adds a folder to the internal folder list with a given name
*******************************************************************/
struct folder *folder_add_with_name(char *path, char *name)
{
	struct folder *f = folder_add(path);
	if (f)
	{
		if (f->name) free(f->name);
		f->name = mystrdup(name);

		folder_config_save(f);
	}
	return f;
}

/******************************************************************
 Creates a new group but does not add it
*******************************************************************/
static struct folder_node *folder_create_group(char *name)
{
	struct folder_node *node;
	if ((node = (struct folder_node*)malloc(sizeof(struct folder_node))))
	{
		/* Initialize everything with 0 */
		memset(node,0,sizeof(struct folder_node));
		node->folder.name = mystrdup(name);
		node->folder.special = FOLDER_SPECIAL_GROUP;

		if ((node->folder.sem = thread_create_semaphore()))
		{
			return node;
		}
		/* leaks */
	}
	return NULL;
}

/******************************************************************
 Adds a folder to the internal folder list with a given name
*******************************************************************/
struct folder *folder_add_group(char *name)
{
	struct folder_node *node = folder_create_group(name);
	if (node)
	{
		list_insert_tail(&folder_list,&node->node);
		return &node->folder;
	}
	return NULL;
}

/******************************************************************
 Remove given folder from the folder list, if possible
*******************************************************************/
int folder_remove(struct folder *f)
{
  if (!folder_attempt_lock(f))
  {
  	sm_request(NULL,_("Can't delete folder because it is actually in use."),_("_Ok"));
  	return 0;
  }

	if (f->special == FOLDER_SPECIAL_NO)
	{
		struct folder_node *node = (struct folder_node*)list_first(&folder_list);
		while (node)
		{
			if (&node->folder == f)
			{
				if (sm_request(NULL,
					_("Do you really want to delete the folder\nand all its mails?"),
					_("_Yes|_No")))
				{
					char buf[512];

					node_remove(&node->node);
					folder_delete_mails(f);
					sprintf(buf,"%s.index",f->path);
					remove(buf);
					sprintf(buf,"%s.config",f->path);
					remove(buf);
					remove(f->path);
					folder_unlock(f);
					thread_dispose_semaphore(node->folder.sem);
					free(node);
					return 1;
				}
			}
			node = (struct folder_node*)node_next(&node->node);
		}
	} else if (f->special == FOLDER_SPECIAL_GROUP)
	{
		struct folder_node *node = (struct folder_node*)list_first(&folder_list);
		struct folder *parent = f->parent_folder;
		int rc = 0;

		while (node)
		{
			if (&node->folder == f)
			{
				if (sm_request(NULL,
					_("Do you really want to delete this group?\nOnly the group entry is deleted,\nnot the folders inside the group"),_("_Yes|_No")))
				{
					node_remove(&node->node);
					folder_unlock(f);
					free(node);
					rc = 1;
				}
			}
			node = (struct folder_node*)node_next(&node->node);
		}

		if (rc)
		{
			/* Set a new parent to all the folders */
			node = (struct folder_node*)list_first(&folder_list);
			while (node)
			{
				if (node->folder.parent_folder == f)
				{
					node->folder.parent_folder = parent;
				}
				node = (struct folder_node*)node_next(&node->node);
			}
		} else
		{
			folder_unlock(f);
		}
		return rc;
	}
	folder_unlock(f);
	return 0;
}

/******************************************************************
 Unlink all folders from the list
*******************************************************************/
void folder_unlink_all(void)
{
	struct folder_node *node;
	while ((node = (struct folder_node*)list_remove_tail(&folder_list)));
}

/******************************************************************
 Adds a folder to the tree
*******************************************************************/
void folder_add_to_tree(struct folder *f,struct folder *parent)
{
	struct folder_node *fnode = (struct folder_node*)(((char*)f)-sizeof(struct node));

	f->parent_folder = parent;
	list_insert_tail(&folder_list,&fnode->node);
}


int read_line(FILE *fh, char *buf);

/******************************************************************
 Load the configuration for the folder
*******************************************************************/
static int folder_config_load(struct folder *f)
{
	char buf[256];
	int rc = 0;
	FILE *fh;

	sprintf(buf,"%s.config",f->path);

	if ((fh = fopen(buf,"r")))
	{
		read_line(fh,buf);

		if (!mystrnicmp("FICO",buf,4))
		{
			rc = 1;
			while (read_line(fh,buf))
			{
				if (!mystrnicmp("Name=",buf,5))
				{
					if (f->name) free(f->name);
					f->name = mystrdup(&buf[5]);
				}
				else if (!mystrnicmp("Type=",buf,5)) f->type = atoi(&buf[5]);
				else if (!mystrnicmp("Special=",buf,8)) f->special = atoi(&buf[8]);
				else if (!mystrnicmp("PrimarySort=",buf,12)) f->primary_sort = atoi(&buf[12]);
				else if (!mystrnicmp("DefaultTo=",buf,10))
				{
					free(f->def_to);
					f->def_to = mystrdup(&buf[10]);
				}
			}
		}
		fclose(fh);
	}
	return rc;
}

/******************************************************************
 Save the current configuration for the folder
*******************************************************************/
static void folder_config_save(struct folder *f)
{
	char buf[256];
	FILE *fh;

	if (!f->path) return;
	if (f->special == FOLDER_SPECIAL_GROUP) return;

	sprintf(buf,"%s.config",f->path);

	if ((fh = fopen(buf,"w")))
	{
		fputs("FICO\n",fh);
		fprintf(fh,"Name=%s\n",f->name);
		fprintf(fh,"Type=%d\n",f->type);
		fprintf(fh,"Special=%d\n",f->special);
		fprintf(fh,"PrimarySort=%d\n",f->primary_sort);
		fprintf(fh,"DefaultTo=%s\n", f->def_to?f->def_to:"");
		fclose(fh);
	}
}

/******************************************************************
 Test if the setting the foldersetting would require a reload
 (the mails would get disposed and reloaded)
*******************************************************************/
int folder_set_would_need_reload(struct folder *f, char *newname, char *newpath, int newtype, char *newdefto)
{
	/* Currentry we need never to reload the mails because the message id and
     in reply to field are always read and hold in the index file */
	return 0;
#if 0
	int rescan = 0;

	/* Check if the path name has changed */
	if (newpath && mystricmp(f->path,newpath)) rescan = 1;

	/* Check if the type change require a reload */
	if (newtype != f->type)
	{
		if (newtype == FOLDER_TYPE_MAILINGLIST || f->type == FOLDER_TYPE_MAILINGLIST)
			rescan = 1;
	}	

	return rescan;
#endif
}

/******************************************************************
 Set some folder attributes. Returns 1 if the folder must be
 refreshed in the gui.
*******************************************************************/
int folder_set(struct folder *f, char *newname, char *newpath, int newtype, char *newdefto)
{
	int refresh = 0;
	int rescan = 0;
	int changed = 0;

	if (newname && mystrcmp(f->name,newname))
	{
		if ((newname = mystrdup(newname)))
		{
			if (f->name) free(f->name);
			f->name = newname;
		}
		changed = 1;
	}

	if (newpath && f->special != FOLDER_SPECIAL_GROUP &&
		  mystrcmp(f->path,newpath))
	{
		if ((newpath = mystrdup(newpath)))
		{
			refresh = !!mystricmp(newpath,f->path);

			if (f->index_uptodate && refresh)
			{
				folder_delete_indexfile(f);
				f->index_uptodate = 0;
			}

			if (f->path) free(f->path);
			f->path = newpath;
			rescan = refresh;
		}
		changed = 1;
	}

	if (newtype != f->type)
	{
		refresh = 1;
		if (newtype == FOLDER_TYPE_MAILINGLIST || f->type == FOLDER_TYPE_MAILINGLIST)
		{
			if (f->index_uptodate)
			{
				folder_delete_indexfile(f);
				f->index_uptodate = 0;
			}
			rescan = 1;
		}
		f->type = newtype;
		changed = 1;
	}
	
	if (mystrcmp(newdefto,f->def_to) != 0)
	{
		free(f->def_to);
		f->def_to = mystrdup(newdefto);
		changed = 1;
	}

	/* Save the settings if the folder settings has been changed */
	if (changed) folder_config_save(f);

	if (rescan)
	{
		int i;

		/* free all kind of data */

		/* free the sorted mail array */
		if (f->sorted_mail_array)
		{
			free(f->sorted_mail_array);
			f->sorted_mail_array = NULL;
		}

		for (i=0;i < f->num_mails; i++)
			mail_free(f->mail_array[i]);

		if (f->mail_array)
		{
			free(f->mail_array);
			f->mail_array = NULL;
		}
		f->num_mails = 0;
		f->mail_array_allocated = 0;
		f->new_mails = 0;
		f->unread_mails = 0;
		f->num_index_mails = -1;

		folder_read_mail_infos(f,0);
	}

	return refresh;
}

/******************************************************************
 Gets the first folder
*******************************************************************/
struct folder *folder_first(void)
{
	struct folder_node *node = (struct folder_node*)list_first(&folder_list);
	if (node) return &node->folder;
	return NULL;
}

/******************************************************************
 Returns the next folder
*******************************************************************/
struct folder *folder_next(struct folder *f)
{
	if (f)
	{
		struct folder_node *node = (struct folder_node*)(((char*)f)-sizeof(struct node));
		if ((node = (struct folder_node*)node_next(&node->node)))
		{
			return &node->folder;
		}
	}
	return NULL;
}

/******************************************************************
 Finds a special folder
*******************************************************************/
struct folder *folder_find_special(int sp)
{
	struct folder *f = folder_first();
	while (f)
	{
		if (f->special == sp) return f;
		f = folder_next(f);
	}
	return NULL;
}

/******************************************************************
 Returns the incoming folder
*******************************************************************/
struct folder *folder_incoming(void)
{
	return folder_find_special(FOLDER_SPECIAL_INCOMING);
}

/******************************************************************
 Returns the outgoing folder
*******************************************************************/
struct folder *folder_outgoing(void)
{
	return folder_find_special(FOLDER_SPECIAL_OUTGOING);
}

/******************************************************************
 Returns the sent folder
*******************************************************************/
struct folder *folder_sent(void)
{
	return folder_find_special(FOLDER_SPECIAL_SENT);
}

/******************************************************************
 Returns the deleleted folder
*******************************************************************/
struct folder *folder_deleted(void)
{
	return folder_find_special(FOLDER_SPECIAL_DELETED);
}

/******************************************************************
 Finds a folder
*******************************************************************/
struct folder *folder_find(int pos)
{
	struct folder_node *node = (struct folder_node*)list_find(&folder_list,pos);
	if (!node) return NULL;
	return &node->folder;
}

/******************************************************************
 Returns the position of the folder. -1 if not in the list.
*******************************************************************/
int folder_position(struct folder *f)
{
	struct folder *lf = folder_first();
	int pos = 0;
	while (lf)
	{
		if (lf == f) return pos;
		lf = folder_next(lf);
		pos++;
	}
	return -1;
}

/******************************************************************
 Finds a folder by name. Returns NULL if folder hasn't found
*******************************************************************/
struct folder *folder_find_by_name(char *name)
{
	struct folder_node *node = (struct folder_node*)list_first(&folder_list);
	while (node)
	{
		if (!mystricmp(name, node->folder.name)) return &node->folder;
		node = (struct folder_node *)node_next(&node->node);
	}
	return NULL;
}

/******************************************************************
 Finds a group folder by name. Returns NULL if folder hasn't found
*******************************************************************/
static struct folder *folder_find_group_by_name(char *name)
{
	struct folder_node *node = (struct folder_node*)list_first(&folder_list);
	while (node)
	{
		if (!mystricmp(name, node->folder.name) && node->folder.special == FOLDER_SPECIAL_GROUP) return &node->folder;
		node = (struct folder_node *)node_next(&node->node);
	}
	return NULL;
}

/******************************************************************
 Finds a folder by path. Returns NULL if folder hasn't found
*******************************************************************/
struct folder *folder_find_by_path(char *name)
{
	struct folder_node *node = (struct folder_node*)list_first(&folder_list);
	while (node)
	{
		if (!mystricmp(name, node->folder.path)) return &node->folder;
		node = (struct folder_node *)node_next(&node->node);
	}
	return NULL;
}

/******************************************************************
 Finds the folder of a mail.
*******************************************************************/
struct folder *folder_find_by_mail(struct mail *mail)
{
	struct folder_node *node = (struct folder_node*)list_first(&folder_list);
	while (node)
	{
		struct folder *folder = &node->folder;
		int i;
		for (i=0; i < folder->num_mails; i++)
		{
			if (folder->mail_array[i] == mail)
				return folder;
		}
		node = (struct folder_node *)node_next(&node->node);
	}
	return NULL;
}

/******************************************************************
 Returns the mail at the given position
*******************************************************************/
struct mail *folder_find_mail(struct folder *f, int position)
{
	void *handle = NULL;
	if (!f->sorted_mail_array) return NULL;
	if (position >= f->num_mails) return NULL;
	folder_next_mail(f,&handle); /* sort the stuff */
	if (f->sorted_mail_array) return f->sorted_mail_array[position];
	return f->mail_array[position];
}

/******************************************************************
 Finds from a folder (which is given by its path) the mail's
 successor (which is given by the filename)
*******************************************************************/
struct mail *folder_find_next_mail_by_filename(char *folder_path, char *mail_filename)
{
	void *handle = NULL;
	struct folder *f = folder_find_by_path(folder_path);
	struct mail *m;

	if (!f) return NULL;

	while ((m = folder_next_mail(f, &handle)))
	{
		if (!mystricmp(m->filename,mail_filename))
		{
			return folder_next_mail(f,&handle);
		}
	}
	return NULL;
}

/******************************************************************
 Finds from a folder (which is given by its path) the mail's
 pred (which is given by the filename)
*******************************************************************/
struct mail *folder_find_prev_mail_by_filename(char *folder_path, char *mail_filename)
{
	void *handle = NULL;
	struct folder *f = folder_find_by_path(folder_path);
	struct mail *lm = NULL;
	struct mail *m;

	if (!f) return NULL;

	while ((m = folder_next_mail(f, &handle)))
	{
		if (!mystricmp(m->filename,mail_filename))
		{
			return lm;
		}
		lm = m;
	}

	return NULL;
}

/******************************************************************
 Finds a mail which should be selected (depending on the folder
 type this could be a unread or a held mail)
*******************************************************************/
struct mail *folder_find_best_mail_to_select(struct folder *folder)
{
	void *handle = NULL;
	struct mail *m;

	int relevant_types;

	if (folder->special == FOLDER_SPECIAL_OUTGOING) relevant_types = MAIL_STATUS_HOLD;
	else relevant_types = MAIL_STATUS_UNREAD;

	while ((m = folder_next_mail(folder, &handle)))
	{
		if (mail_get_status_type(m) == relevant_types) return m;
	}

	/* take the first mail */
	handle = NULL;
	m = folder_next_mail(folder, &handle);
	return m;
}

/******************************************************************
 Returns the index (starting with 0) of the mail in this folder.
 -1 if mail is not found.
*******************************************************************/
int folder_get_index_of_mail(struct folder *f, struct mail *mail)
{
	int index = 0;
	void *handle = 0;
	struct mail *m;

	while ((m = folder_next_mail(f, &handle)))
	{
		if (m == mail) return index;
		index ++;
	}
	return -1;
}

/******************************************************************
 Returns the size of the mails in this folder
*******************************************************************/
int folder_size_of_mails(struct folder *f)
{
	int size = 0;
	void *handle = NULL;
	struct mail *m;

	if (!f) return NULL;

	while ((m = folder_next_mail(f, &handle)))
		size += m->size;
	return size;
}

/******************************************************************
 Move a mail from source folder to a destination folder. 0 if the
 moving has failed.
 If mail has sent status and moved to a outgoing drawer it get's
 the waitsend status.
*******************************************************************/
int folder_move_mail(struct folder *from_folder, struct folder *dest_folder, struct mail *mail)
{
	char *buf;
	int rc = 0;

	if (!from_folder || !dest_folder) return 0;
	if (from_folder == dest_folder) return 1;
	if (from_folder->special == FOLDER_SPECIAL_GROUP || 
			dest_folder->special == FOLDER_SPECIAL_GROUP) return 0;

	if ((buf = (char*)malloc(1024)))
	{
		char *src_buf = buf;
		char *dest_buf = buf + 512;

		strcpy(src_buf,from_folder->path);
		strcpy(dest_buf,dest_folder->path);
		sm_add_part(src_buf,mail->filename,512);

		if (dest_folder->special == FOLDER_SPECIAL_OUTGOING && mail_get_status_type(mail) == MAIL_STATUS_SENT)
		{
			char *newfilename;

			mail->status = MAIL_STATUS_WAITSEND | (mail->status & MAIL_STATUS_FLAG_MARKED);

			if ((newfilename = mail_get_status_filename(mail->filename, mail->status)))
			{
				if (mail->filename) free(mail->filename);
				mail->filename = newfilename;
			}
		}
		sm_add_part(dest_buf,mail->filename,512);

		if (!rename(src_buf,dest_buf)) rc = 1;
		else
		{
			/* Renaming failed so we need a new name */
			char path[512];
			getcwd(path,sizeof(path));
			if (chdir(dest_folder->path) != -1)
			{
				char *new_name = mail_get_new_name(mail->status);
				if (new_name)
				{
					if (!rename(src_buf,new_name))
					{
						free(mail->filename);
						mail->filename = new_name;
						rc = 1;
					} else free(new_name);
				}
				chdir(path);
			}
		}
		free(buf);

		if (rc)
		{
			folder_remove_mail(from_folder,mail);
			folder_add_mail(dest_folder,mail,1);
		}
	}
	return rc;
}

/******************************************************************
 Deletes a mail permanently
*******************************************************************/
int folder_delete_mail(struct folder *from_folder, struct mail *mail)
{
	char path[512];
	if (!from_folder) return 0;
	folder_remove_mail(from_folder,mail);
	strcpy(path,from_folder->path);
	sm_add_part(path,mail->filename,512);
	remove(path);
	mail_free(mail);
	return 1;
}

/******************************************************************
 Really delete all mails in the given folder
*******************************************************************/
static void folder_delete_mails(struct folder *folder)
{
	int i;
	char path[256];

	if (!folder) return;

	getcwd(path, sizeof(path));
	if(chdir(folder->path) == -1) return;

	/* free the sorted mail array */
	if (folder->sorted_mail_array)
	{
		free(folder->sorted_mail_array);
		folder->sorted_mail_array = NULL;
	}

	for (i=0;i<folder->num_mails;i++)
	{
		remove(folder->mail_array[i]->filename);
		mail_free(folder->mail_array[i]);
	}

	folder->num_mails = 0;
	folder->unread_mails = 0;
	folder->new_mails = 0;
	folder->mail_array_allocated = 0;

	if (folder->mail_array)
	{
		free(folder->mail_array);
		folder->mail_array = NULL;
	}

	chdir(path);
}

/******************************************************************
 Really delete all mails in the delete folder
*******************************************************************/
void folder_delete_deleted(void)
{
	struct folder *f = folder_deleted();
	if (!f) return;
	if (!folder_attempt_lock(f)) return;
	folder_delete_mails(f);
	folder_unlock(f);
}

/******************************************************************
 Writes a string into a filehandle. Returns 0 for an error else
 the number of bytes which has been written (at least two).
*******************************************************************/
static int fwrite_str(FILE *fh, char *str)
{
	if (str)
	{
		int len;
		int strl = strlen(str);

		if (fputc((strl/256)%256,fh)==EOF) return 0;
		if (fputc(strl%256,fh)==EOF) return 0;

		len = fwrite(str,1,strl,fh);
		if (len == strl) return len + 2;
	}
	fputc(0,fh);
	fputc(0,fh);
	return 2;
}

/******************************************************************
 Reads a string from a filehandle. It is allocated with malloc()
*******************************************************************/
static char *fread_str(FILE *fh)
{
	unsigned char a;
	char *txt;
	int len;

	a = fgetc(fh);
	len = a << 8;
	a = fgetc(fh);
	len += a;

	if ((txt = malloc(len+1)))
	{
		fread(txt,1,len,fh);
		txt[len]=0;
	}
	return txt;
}

/******************************************************************
 Reads a string from a filehandle. It is allocated with malloc().
 Returns NULL if the string has an length of 0.
*******************************************************************/
static char *fread_str_no_null(FILE *fh)
{
	unsigned char a;
	char *txt;
	int len;

	a = fgetc(fh);
	len = a << 8;
	a = fgetc(fh);
	len += a;

	if (!len) return NULL;

	if ((txt = malloc(len+1)))
	{
		fread(txt,1,len,fh);
		txt[len]=0;
	}
	return txt;
}

/******************************************************************
 Saved the index of an folder
*******************************************************************/
int folder_save_index(struct folder *f)
{
	FILE *fh;

	if (!f->mail_infos_loaded) return 0;

	if ((fh = folder_open_indexfile(f,"wb")))
	{
		int i;
		int ver = FOLDER_INDEX_VERSION;

		fwrite("SMFI",1,4,fh);
		fwrite(&ver,1,4,fh);
		fwrite(&f->num_mails,1,4,fh);
		fwrite(&f->unread_mails,1,4,fh);

		for (i=0; i < f->num_mails; i++)
		{
			struct mail *m = f->mail_array[i];
			int len = 0;
			int len_add;
			int num_to = 0;
			int num_cc = 0;
			struct address *to_addr = NULL;
			struct address *cc_addr = NULL;

			if (m->to_list)
			{
				num_to = list_length(m->to_list);
				to_addr = (struct address*)list_first(m->to_list);
			}

			if (m->cc_list)
			{
				num_cc = list_length(m->cc_list);
				cc_addr = (struct address*)list_first(m->cc_list);
			}

			fwrite(&num_to,1,4,fh);
			fwrite(&num_cc,1,4,fh);

			if (!(len_add = fwrite_str(fh, f->mail_array[i]->subject))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, f->mail_array[i]->filename))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, f->mail_array[i]->from_phrase))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, f->mail_array[i]->from_addr))) break;
			len += len_add;
			
			while (to_addr)
			{
				if (!(len_add = fwrite_str(fh, to_addr->realname))) break;
				len += len_add;
				if (!(len_add = fwrite_str(fh, to_addr->email))) break;
				len += len_add;
				to_addr = (struct address*)node_next(&to_addr->node);
			}

			while (cc_addr)
			{
				if (!(len_add = fwrite_str(fh, cc_addr->realname))) break;
				len += len_add;
				if (!(len_add = fwrite_str(fh, cc_addr->email))) break;
				len += len_add;
				cc_addr = (struct address*)node_next(&cc_addr->node);
			}

			if (!(len_add = fwrite_str(fh, f->mail_array[i]->pop3_server))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, f->mail_array[i]->message_id))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, f->mail_array[i]->message_reply_id))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, f->mail_array[i]->reply_addr))) break;
			len += len_add;

			/* so that integervars are aligned */
			if (len % 2) fputc(0,fh);

			fwrite(&f->mail_array[i]->size,1,sizeof(f->mail_array[i]->size),fh);
			fwrite(&f->mail_array[i]->seconds,1,sizeof(f->mail_array[i]->seconds),fh);
			fwrite(&f->mail_array[i]->received,1,sizeof(f->mail_array[i]->received),fh);
			fwrite(&f->mail_array[i]->flags,1,sizeof(f->mail_array[i]->flags),fh);
		}
		fclose(fh);
	}

	return 1;
}

/******************************************************************
 Get informations about the folder stats
*******************************************************************/
void folder_get_stats(int *total_msg_ptr, int *total_unread_ptr, int *total_new_ptr)
{
	struct folder *f = folder_first();
	int total_msg = 0;
	int total_unread = 0;
	int total_new = 0;
	while (f)
	{
		if (f->num_index_mails != -1) total_msg += f->num_index_mails;
		total_unread += f->unread_mails;
		total_new += f->new_mails;
		f = folder_next(f);
	}

	*total_msg_ptr = total_msg;
	*total_unread_ptr = total_unread;
	*total_new_ptr = total_new;
}


/******************************************************************
 The mail iterating function. To get the first mail let handle
 point to NULL. If needed this function sorts the mails according
 to the sort mode. Handle will be updated every time.
 While iterating through the mails you aren't allowed to (re)move a
 mail withing the folder,
*******************************************************************/
struct mail *folder_next_mail(struct folder *folder, void **handle)
{
	struct mail **mail_array;

	/* If mails info is not read_yet, read it now */
	if (!folder->mail_infos_loaded)
		folder_read_mail_infos(folder,0);

	mail_array = folder->mail_array;
	if (!folder->sorted_mail_array && /* *((int**)handle) == 0 && */ folder->num_mails && folder->mail_array_allocated)
	{
		/* the array is not sorted, so sort it now */
		folder->sorted_mail_array = (struct mail**)malloc(sizeof(struct mail*)*folder->mail_array_allocated);
		if (folder->sorted_mail_array)
		{
			/* copy the mail pointers into the buffer */
			memcpy(folder->sorted_mail_array, folder->mail_array, sizeof(struct mail*)*folder->num_mails);

			/* set the correct search function */
			mail_compare_set_sort_mode(folder);

			if (compare_primary) qsort(folder->sorted_mail_array, folder->num_mails, sizeof(struct mail*),mail_compare);
			mail_array = folder->sorted_mail_array;
		}
	}

	if (folder->sorted_mail_array) mail_array = folder->sorted_mail_array;
	return (struct mail*)(((*((int*)handle))<folder->num_mails)?(mail_array[(*((int*)handle))++]):NULL);
}
/* we define a macro for mail iterating, handle must be initialzed to NULL at start */
/*#define folder_next_mail(folder,handle) ( ((*((int*)handle))<folder->num_mails)?(folder->mail_array[(*((int*)handle))++]):NULL)*/

/******************************************************************
 Returns the primary sort mode
*******************************************************************/
int folder_get_primary_sort(struct folder *folder)
{
	return folder->primary_sort;
}

/******************************************************************
 Sets the primary sort mode
*******************************************************************/
void folder_set_primary_sort(struct folder *folder, int sort_mode)
{
	if (folder->primary_sort != sort_mode)
	{
		folder->primary_sort = sort_mode;

		/* free the sorted mail array */
		if (folder->sorted_mail_array)
		{
			free(folder->sorted_mail_array);
			folder->sorted_mail_array = NULL;
		}
	}
}

/******************************************************************
 Returns the secondary sort mode
*******************************************************************/
int folder_get_secondary_sort(struct folder *folder)
{
	return folder->secondary_sort;
}

/******************************************************************
 Sets the secondary sort mode
*******************************************************************/
void folder_set_secondary_sort(struct folder *folder, int sort_mode)
{
	if (folder->secondary_sort != sort_mode)
	{
		folder->secondary_sort = sort_mode;

		/* free the sorted mail array */
		if (folder->sorted_mail_array)
		{
			free(folder->sorted_mail_array);
			folder->sorted_mail_array = NULL;
		}
	}
}


/******************************************************************
 Checks if the given filter matches the mail.
 folder is the folder where the mail is located
*******************************************************************/
int mail_matches_filter(struct folder *folder, struct mail *m,
											  struct filter *filter)
{
	struct filter_rule *rule = (struct filter_rule*)list_first(&filter->rules_list);

	while (rule)
	{
		int take = 0;

		switch (rule->type)
		{
			case	RULE_FROM_MATCH:
						if (rule->u.from.from)
						{
							int i;

							i = 0;
							while (!take && rule->u.from.from[i])
								take = !!utf8stristr(m->from_addr,rule->u.from.from[i++]);

							if (!take)
							{
								i = 0;
								while (!take && rule->u.from.from[i])
									take = !!utf8stristr(m->from_phrase,rule->u.from.from[i++]);
							}
						}
						break;

			case	RULE_RCPT_MATCH:
						if (rule->u.rcpt.rcpt)
						{
							int i;

							i = 0;
							while (!take && rule->u.rcpt.rcpt[i])
							{
								struct address *addr;
								addr = (struct address*)list_first(m->to_list);
								while (!take && addr)
								{
									take = !!utf8stristr(addr->realname,rule->u.rcpt.rcpt[i]);
									if (!take) take = !!utf8stristr(addr->email,rule->u.rcpt.rcpt[i]);
									addr = (struct address*)node_next(&addr->node);
								}

								if (!take)
								{
									addr = (struct address*)list_first(m->cc_list);
									while (!take && addr)
									{
										take = !!utf8stristr(addr->realname,rule->u.rcpt.rcpt[i]);
										if (!take) take = !!utf8stristr(addr->email,rule->u.rcpt.rcpt[i]);
										addr = (struct address*)node_next(&addr->node);
									}
								}
								i++;
							}
						}
						break;

			case	RULE_SUBJECT_MATCH:
						if (rule->u.subject.subject)
						{
							int i = 0;
							while (!take && rule->u.subject.subject[i])
								take = !!utf8stristr(m->subject,rule->u.subject.subject[i++]);
						}
						break;

			case	RULE_HEADER_MATCH:
						/* NOTE: This doesn't work in subthreads but is actually not used anywhy */
						{
							struct header *header;

							mail_read_header_list_if_empty(m);

							if ((header = mail_find_header(m,rule->u.header.name)))
							{
								char *cont = NULL;
								parse_text_string(header->contents, &cont);

								if (cont)
								{
									int i = 0;
									while (!take && rule->u.header.contents[i])
										take = !!mystristr(cont,rule->u.header.contents[i++]);
									free(cont);
								}
							}
						}
						break;

			case	RULE_ATTACHMENT_MATCH:
						take = !!(m->flags & MAIL_FLAGS_ATTACH);
						break;

			case	RULE_STATUS_MATCH:
						if (rule->u.status.status == RULE_STATUS_NEW && (m->flags & MAIL_FLAGS_NEW)) take = 1;
						else if (rule->u.status.status == RULE_STATUS_READ && mail_get_status_type(m)==MAIL_STATUS_READ) take = 1;
						else if (rule->u.status.status == RULE_STATUS_UNREAD && mail_get_status_type(m)==MAIL_STATUS_UNREAD) take = 1;
						else if (rule->u.status.status == RULE_STATUS_REPLIED && (mail_get_status_type(m)==MAIL_STATUS_REPLIED || mail_get_status_type(m)==MAIL_STATUS_REPLFORW)) take = 1;
						else if (rule->u.status.status == RULE_STATUS_FORWARDED && (mail_get_status_type(m)==MAIL_STATUS_FORWARD || mail_get_status_type(m)==MAIL_STATUS_REPLFORW)) take = 1;
						else if (rule->u.status.status == RULE_STATUS_PENDING && mail_get_status_type(m)==MAIL_STATUS_WAITSEND) take = 1;
						else if (rule->u.status.status == RULE_STATUS_SENT && mail_get_status_type(m)==MAIL_STATUS_SENT) take = 1;
						break;

			default:
						break;
		}

		if (!take && !filter->mode) return 0;
		if (take && filter->mode) return 1;

		rule = (struct filter_rule*)node_next(&rule->node);
	}

	if (!filter->mode) return 1;
	return 0;
}

/******************************************************************
 Checks if a mail should be filtered (returns the filter which
 actions should performed or NULL)
*******************************************************************/
struct filter *folder_mail_can_be_filtered(struct folder *folder, struct mail *m, int action)
{
	struct filter *filter = filter_list_first();

	while (filter)
	{
		if ((action == 0 && (filter->flags & FILTER_FLAG_REQUEST)) ||
				(action == 1 && (filter->flags & FILTER_FLAG_NEW)) ||
				(action == 2 && (filter->flags & FILTER_FLAG_SENT)))
		{
			if (mail_matches_filter(folder,m,filter))
				return filter;
		}

		filter = filter_list_next(filter);
	}

	return 0;
}

/******************************************************************
 Filter all mails in the given folder using a single filter
*******************************************************************/
int folder_apply_filter(struct folder *folder, struct filter *filter)
{
	void *handle = NULL;
	struct mail *m;
	char path[512];

	getcwd(path, sizeof(path));
	if(chdir(folder->path) == -1) return 0;

	for (;;)
	{
		void *old_handle = handle;

		if (!(m = folder_next_mail(folder,&handle))) break;
		
		if (mail_matches_filter(folder,m,filter))
		{
			if (filter->use_dest_folder && filter->dest_folder)
			{
				struct folder *dest_folder = folder_find_by_name(filter->dest_folder);
				if (dest_folder && dest_folder != folder)
				{
					/* very slow, because the sorted array is rebuilded in the both folders! */
					callback_move_mail(m, folder, dest_folder);

					handle = old_handle;
				}
			}

			if (filter->sound_file && filter->use_sound_file)
			{
				chdir(path);
				sm_play_sound(filter->sound_file);
				if(chdir(folder->path) == -1) return 0;
			}

			if (filter->arexx_file && filter->use_arexx_file)
			{
				chdir(path);
				gui_execute_arexx(filter->arexx_file);
				if(chdir(folder->path) == -1) return 0;
			}

			if (filter->search_filter)
			{
				/* It's a search filter so inform simplemail about a new found mail */
			}
		}
	}
	chdir(path);
	return 1;
}

/******************************************************************
 Filter all mails in the given folder
*******************************************************************/
int folder_filter(struct folder *folder)
{
	void *handle = NULL;
	struct mail *m;
	char path[512];

	getcwd(path, sizeof(path));
	if(chdir(folder->path) == -1) return 0;

	for (;;)
	{
		void *old_handle = handle;
		struct filter *f;

		if (!(m = folder_next_mail(folder,&handle))) break;
		
		if ((f = folder_mail_can_be_filtered(folder,m,0)))
		{
			if (f->use_dest_folder && f->dest_folder)
			{
				struct folder *dest_folder = folder_find_by_name(f->dest_folder);
				if (dest_folder && dest_folder != folder)
				{
					/* very slow, because the sorted array is rebuilded in the both folders! */
					callback_move_mail(m, folder, dest_folder);
					handle = old_handle;
				}
			}

			if (f->sound_file && f->use_sound_file)
			{
				chdir(path);
				sm_play_sound(f->sound_file);
				if(chdir(folder->path) == -1) return 0;
			}

			if (f->arexx_file && f->use_arexx_file)
			{
				chdir(path);
				gui_execute_arexx(f->arexx_file);
				if(chdir(folder->path) == -1) return 0;
			}
		}
	}
	chdir(path);
	return 1;
}

struct search_msg
{
	int f_array_len;
	struct folder **f_array;
	struct search_options *sopt;
};

/**************************************************************************
 The thread entry for the search
**************************************************************************/
static void folder_start_search_entry(struct search_msg *msg)
{
	struct folder **f_array = NULL;
	int f_array_len;
	struct filter *filter = NULL;
	struct filter_rule *rule;
	struct search_options *sopt;
#define NUM_FOUND 100
	struct mail *found_array[NUM_FOUND];
	unsigned int secs = sm_get_current_seconds(); /* used to reduce the amount of notifing the parent task */

	sopt = search_options_duplicate(msg->sopt);
	f_array_len = msg->f_array_len;
	if ((f_array = malloc((f_array_len+1)*sizeof(struct folder*))))
	{
		int i;
		for (i=0;i<f_array_len;i++)
		{
			f_array[i] = msg->f_array[i];
			/* Prevent folder changes */
			folder_lock(f_array[i]);
		}
		f_array[i] = NULL;
	}

  if (thread_parent_task_can_contiue())
	{
		if (!sopt || !f_array) goto fail;

		filter = filter_create();
		if (!filter) goto fail;
	
		if (sopt->from)
		{
			if ((rule = filter_create_and_add_rule(filter,RULE_FROM_MATCH)))
				rule->u.from.from = array_add_string(NULL,sopt->from);
		}
	
		if (sopt->subject)
		{
			if ((rule = filter_create_and_add_rule(filter,RULE_SUBJECT_MATCH)))
				rule->u.subject.subject = array_add_string(NULL,sopt->subject);
		}
	
		if (sopt->body)
		{
			if ((rule = filter_create_and_add_rule(filter,RULE_BODY_MATCH)))
				rule->u.body.body = mystrdup(sopt->body);
		}
	
		if (sopt->to)
		{
			if ((rule = filter_create_and_add_rule(filter,RULE_RCPT_MATCH)))
				rule->u.rcpt.rcpt = array_add_string(NULL,sopt->to);
		}
	
		filter->search_filter = 1;
	
		/* folder_apply_filter(f,filter); */ /* Not safe currently to be called from subthreads */
		{
			int folder_num = 0;
			int found_num = 0;
			struct mail *m;
			struct folder *f;
			char path[512];

			getcwd(path, sizeof(path));

			thread_call_parent_function_sync(search_enable_search, 0);
	
			while ((f = f_array[folder_num++]))
			{
				int i;

				if (chdir(f->path) == -1) break;

				for (i=0;i<f->num_mails;i++)
				{
					if (!(m = f->mail_array[i]))
						break;

					/* mail_matches_filter() is thread safe as long as no header search is used, like here */
					if (mail_matches_filter(f,m,filter))
					{
						unsigned int new_secs = sm_get_current_seconds();

						found_array[found_num++] = m;

						if (found_num == NUM_FOUND || new_secs != secs)
						{
							thread_call_parent_function_sync(search_add_result, 2, found_array, found_num);
							found_num = 0;
							secs = new_secs;
						}
					}

					if (thread_aborted()) goto cancel;
				}
			}
cancel:
			chdir(path);

			if (found_num)
				thread_call_parent_function_sync(search_add_result, 2, found_array, found_num);

			thread_call_parent_function_sync(search_disable_search, 0);
		}
	}

fail:
	if (filter) filter_dispose(filter);
	if (f_array)
	{
		int i;
		for (i=0;i<f_array_len;i++)
			folder_unlock(f_array[i]);
		free(f_array);
	}
	if (sopt) search_options_free(sopt);
}

/**************************************************************************
 Start the search with the given options. It starts a separate thread for
 doing so.
**************************************************************************/
void folder_start_search(struct search_options *sopt)
{
	struct search_msg msg;
	struct folder *f;
	struct folder *start;
	struct folder *end;
	struct folder **array;
	int num;

	if (sopt->folder)
	{
		start = folder_find_by_name(sopt->folder);
		if (start->special != FOLDER_SPECIAL_GROUP) end = folder_next(start);
		else
		{
			struct folder *parent;
			parent = start->parent_folder;
			f = folder_next(start);
			while (f && f->parent_folder != parent)
				f = folder_next(f);
			end = f;
		}
	} else
	{
		start = folder_first();
		end = NULL;
	}

	num = 0;
	f = start;
	while (f && f != end)
	{
		if (f->special != FOLDER_SPECIAL_GROUP) num++;
		f = folder_next(f);
	}

	array = malloc((num+1)*sizeof(struct folder*));
	if (array)
	{
		int i = 0;

		f = start;

		while (f && f != end)
		{
			if (f->special != FOLDER_SPECIAL_GROUP)
			{
				void *handle = NULL;
			  folder_next_mail(f,&handle);
				array[i++]  = f;
			}
			f = folder_next(f);
		}
		array[i] = NULL;

		msg.sopt = sopt;
		msg.f_array = array;
		msg.f_array_len = num;

		thread_start(THREAD_FUNCTION(&folder_start_search_entry),&msg);
		free(array);
	}
}

/******************************************************************
 Opens the order file and returns the FILE *
*******************************************************************/
static FILE *folder_open_order_file(char *mode)
{
	FILE *fh;
	char *order_path;

	order_path = mycombinepath(user.folder_directory,".order");
	if (!order_path) return NULL;

	fh = fopen(order_path,mode);
	free(order_path);
	return fh;
}

/******************************************************************
 Loades the order of the folders
*******************************************************************/
void folder_load_order(void)
{
	FILE *fh;

	if ((fh = folder_open_order_file("r")))
	{
		struct list new_order_list;
		char *buf = (char*)malloc(1024);
		if (buf)
		{
			list_init(&new_order_list);

			/* Move all nodes to the new order list in the right order */
			while ((fgets(buf,1024,fh)))
			{
				char *path;
				char *temp_buf;
				int special, parent;

				if ((path = strchr(buf,'\t')))
				{
					*path++ = 0;

					if ((temp_buf = strchr(path,'\t')))
					{
						*temp_buf++ = 0;
						special = atoi(temp_buf);
						if ((temp_buf = strchr(temp_buf,'\t')))
						{
							struct folder *new_folder;
							struct folder_node *new_folder_node;
							int closed = 0;
							temp_buf++;
							parent = atoi(temp_buf);

							temp_buf = strchr(temp_buf,'\t');
							if (temp_buf) closed = atoi(temp_buf+1);

							if (special == FOLDER_SPECIAL_GROUP) new_folder = folder_find_group_by_name(buf);
							else new_folder = folder_find_by_path(path);

							if (new_folder)
							{
								if (parent != -1)
									new_folder->parent_folder = &((struct folder_node*)list_find(&new_order_list,parent))->folder;
								new_folder_node = find_folder_node_by_folder(new_folder);
								node_remove(&new_folder_node->node);
								list_insert_tail(&new_order_list,&new_folder_node->node);
								new_folder->closed = closed;
							}
						}
					}
				}
			}
			free(buf);

			/* Move the nodes into the main folder list again */
			{
				struct folder_node *folder_node;
				while ((folder_node = (struct folder_node*)list_remove_tail(&new_order_list)))
					list_insert(&folder_list, &folder_node->node, NULL);
			}
		}
		fclose(fh);
	}
}

/******************************************************************
 Saves the order of the folders
*******************************************************************/
void folder_save_order(void)
{
	struct folder *f = folder_first();
	FILE *fh;

	if (!(fh = folder_open_order_file("w")))
		return;

	while (f)
	{
		struct folder_node *fnode;

		if (f->parent_folder) fnode = (struct folder_node*)(((char*)f->parent_folder)-sizeof(struct node));
		else fnode = NULL;

		fprintf(fh,"%s\t%s\t%d\t%d\t%d\n",f->name?f->name:"",f->path?f->path:"",f->special,node_index(&fnode->node),f->closed);
		f = folder_next(f);
	}
	fclose(fh);
}

/******************************************************************
 Returns a possible path for new folders (in a static buffer)
*******************************************************************/
char *new_folder_path(void)
{
	static char buf[512];
	char *buf2;
	int i=0;

	strcpy(buf,user.folder_directory);
	sm_add_part(buf,"folder",256);
	buf2 = buf + strlen(buf);

	while (1)
	{
		DIR *dfd;

		sprintf(buf2,"%d",i);

		if (!(dfd = opendir(buf)))
			break;

		closedir(dfd);
		
		i++;
	}

	return buf;
}

/******************************************************************
 Initializes the default folders
*******************************************************************/
int init_folders(void)
{
	DIR *dfd;
	FILE *fh;
	struct dirent *dptr; /* dir entry */
	struct stat *st;

	if (!(folders_semaphore = thread_create_semaphore()))
		return 0;

	list_init(&folder_list);

	/* Read in the .orders file at first */
	if ((fh = folder_open_order_file("r")))
	{
		char *buf = (char*)malloc(1024);
		if (buf)
		{
			while ((fgets(buf,1024,fh)))
			{
				char *path;
				char *temp_buf;
				int special, parent;

				if ((path = strchr(buf,'\t')))
				{
					*path++ = 0;

					if ((temp_buf = strchr(path,'\t')))
					{
						*temp_buf++ = 0;
						special = atoi(temp_buf);
						if ((temp_buf = strchr(temp_buf,'\t')))
						{
							struct folder *new_folder;
							int closed = 0;
							temp_buf++;
							parent = atoi(temp_buf);

							temp_buf = strchr(temp_buf,'\t');
							if (temp_buf) closed = atoi(temp_buf+1);


							if (special == FOLDER_SPECIAL_GROUP)
							{
								new_folder = folder_add_group(buf);
								new_folder->closed = closed;
							} else
							{
								new_folder = folder_add(path);
							}
							if (new_folder && parent != -1)
							{
								new_folder->parent_folder = folder_find(parent);
							}
						}
					}
				}
				
			}
			free(buf);
		}
		fclose(fh);
	}

	if (!(st = malloc(sizeof(struct stat))))
		return 0;

	if ((dfd = opendir(user.folder_directory)))
	{
		while ((dptr = readdir(dfd)) != NULL)
		{
			char buf[256];
			if (!strcmp(dptr->d_name,".") || !strcmp(dptr->d_name,"..")) continue;
			strcpy(buf,user.folder_directory);
			sm_add_part(buf,dptr->d_name,sizeof(buf));
			if (!stat(buf,st))
			{
				if (st->st_mode & S_IFDIR)
				{
					folder_add(buf);
				}
			}
		}

		closedir(dfd);
	}

	if (!folder_incoming())
	{
		char *new_folder = mycombinepath(user.folder_directory,"incoming");
		if (new_folder)
		{
			folder_add(new_folder);
			free(new_folder);
		}
	}

	if (!folder_outgoing())
	{
		char *new_folder = mycombinepath(user.folder_directory,"outgoing");
		if (new_folder)
		{
			folder_add(new_folder);
			free(new_folder);
		}
	}

	if (!folder_sent())
	{
		char *new_folder = mycombinepath(user.folder_directory,"sent");
		if (new_folder)
		{
			folder_add(new_folder);
			free(new_folder);
		}
	}

	if (!folder_deleted())
	{
		char *new_folder = mycombinepath(user.folder_directory,"deleted");
		if (new_folder)
		{
			folder_add(new_folder);
			free(new_folder);
		}
	}

	if (!folder_incoming() || !folder_outgoing() || !folder_deleted() || !folder_sent())
		return 0;

	folder_outgoing()->type = FOLDER_TYPE_SEND;
	folder_sent()->type = FOLDER_TYPE_SEND;

	return 1;
}

/******************************************************************
 Removes all folders (before quit).
*******************************************************************/
void del_folders(void)
{
	struct folder *f = folder_first();
	while (f)
	{
		if (!f->index_uptodate) folder_save_index(f);
		f = folder_next(f);
	}

	thread_dispose_semaphore(folders_semaphore);
}

/******************************************************************
 Lock the folder, to prevent any change to it
*******************************************************************/
void folder_lock(struct folder *f)
{
	if (!f) return;
	thread_lock_semaphore(f->sem);
}

/******************************************************************
 Tries to lock the folder. Returns FALSE if folder is used.
*******************************************************************/
int folder_attempt_lock(struct folder *f)
{
	if (!f) return 0;
	return thread_attempt_lock_semaphore(f->sem);
}

/******************************************************************
 Unlock the folder
*******************************************************************/
void folder_unlock(struct folder *f)
{
	if (!f) return;
	thread_unlock_semaphore(f->sem);
}

/******************************************************************
 Lock the global folder semaphore
*******************************************************************/
void folders_lock(void)
{
	thread_lock_semaphore(folders_semaphore);
}

/******************************************************************
 Unlock the global folder semaphore
*******************************************************************/
void folders_unlock(void)
{
	thread_unlock_semaphore(folders_semaphore);
}

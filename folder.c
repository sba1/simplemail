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
 * @file folder.c
 */

#include "folder.h"

#include <ctype.h> /* toupper() */
#include <dirent.h> /* unix dir stuff */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> /* state() */
#include <unistd.h>

#include "account.h"
#include "addresslist.h"
#include "atcleanup.h"
#include "codesets.h"
#include "configuration.h"
#include "debug.h"
#include "filter.h"
#include "imap.h"
#include "lists.h"
#include "mail_context.h"
#include "mail_support.h"
#include "parse.h"
#include "progmon.h"
#include "qsort.h"
#include "simplemail.h"
#include "smintl.h"
#include "status.h"
#include "string_pools.h"
#include "subthreads.h"
#include "support_indep.h"

#include "gui_main.h" /* gui_execute_arexx() */
#include "request.h"
#include "searchwnd.h"
#include "support.h"
#include "timesupport.h"

#define FOLDER_INDEX_VERSION 8

static void folder_remove_mail_info(struct folder *folder, struct mail_info *mail);

/* folder sort stuff to control the compare functions */
static int compare_primary_reverse;
static int (*compare_primary)(const struct mail_info *arg1, const struct mail_info *arg2, int reverse);
static int compare_secondary_reverse;
static int (*compare_secondary)(const struct mail_info *arg1, const struct mail_info *arg2, int reverse);

/* the global folder lock semaphore */
static semaphore_t folders_semaphore;

/* the global mail context for all mails associated to folders */
static mail_context *folder_mail_context;

/**
 * Compare two mails with respect to their status.
 *
 * @param arg1 first mail to be compared
 * @param arg2 second mail to be compared
 * @param reverse negates the result
 * @return > 0 if arg1 is larger than arg2
 */
static int mail_compare_status(const struct mail_info *arg1, const struct mail_info *arg2, int reverse)
{
	int rc;

	if ((arg1->flags & MAIL_FLAGS_NEW) && !(arg2->flags & MAIL_FLAGS_NEW)) rc = -1;
	else if ((arg2->flags & MAIL_FLAGS_NEW) && !(arg1->flags & MAIL_FLAGS_NEW)) rc = 1;
	else rc = (arg1->status & MAIL_STATUS_MASK) - (arg2->status & MAIL_STATUS_MASK);
	if (reverse) rc *= -1;
	return rc;
}

/**
 * Compares two mails with respect to the from field.
 *
 * @param arg1 first mail to be compared
 * @param arg2 second mail to be compared
 * @param reverse negates the result
 * @return > 0 if arg1 is larger than arg2
 */
static int mail_compare_from(const struct mail_info *arg1, const struct mail_info *arg2, int reverse)
{
	int rc = utf8stricmp(mail_info_get_from(arg1),mail_info_get_from(arg2));
	if (reverse) rc *= -1;
	return rc;
}

/**
 * Compares two mails with respect to the to field.
 *
 * @param arg1 first mail to be compared
 * @param arg2 second mail to be compared
 * @param reverse negates the result
 * @return > 0 if arg1 is larger than arg2
 */
static int mail_compare_to(const struct mail_info *arg1, const struct mail_info *arg2, int reverse)
{
	int rc = utf8stricmp(mail_info_get_to(arg1),mail_info_get_to(arg2));
	if (reverse) rc *= -1;
	return rc;
}

/**
 * Compares two mails with respect to the subject field.
 *
 * @param arg1 first mail to be compared
 * @param arg2 second mail to be compared
 * @param reverse negates the result
 * @return > 0 if arg1 is larger than arg2
 */
static int mail_compare_subject(const struct mail_info *arg1, const struct mail_info *arg2, int reverse)
{
	int rc = utf8stricmp(mail_get_compare_subject(arg1->subject),mail_get_compare_subject(arg2->subject));
	if (reverse) rc *= -1;
	return rc;
}

/**
 * Compares two mails with respect to the reply field.
 *
 * @param arg1 first mail to be compared
 * @param arg2 second mail to be compared
 * @param reverse negates the result
 * @return > 0 if arg1 is larger than arg2
 */
static int mail_compare_reply(const struct mail_info *arg1, const struct mail_info *arg2, int reverse)
{
	int rc = utf8stricmp(arg1->reply_addr, arg2->reply_addr);
	if (reverse) rc *= -1;
	return rc;
}

/**
 * Compares two mails with respect to the date field.
 *
 * @param arg1 first mail to be compared
 * @param arg2 second mail to be compared
 * @param reverse negates the result
 * @return > 0 if arg1 is larger than arg2
 */
static int mail_compare_date(const struct mail_info *arg1, const struct mail_info *arg2, int reverse)
{
	if (arg1->seconds > arg2->seconds) return reverse?(-1):1;
	else if (arg1->seconds == arg2->seconds) return 0;
	return reverse?1:(-1);
}

/**
 * Compares two mails with respect to the size field.
 *
 * @param arg1 first mail to be compared
 * @param arg2 second mail to be compared
 * @param reverse negates the result
 * @return > 0 if arg1 is larger than arg2
 */

static int mail_compare_size(const struct mail_info *arg1, const struct mail_info *arg2, int reverse)
{
	if (arg1->size > arg2->size) return reverse?(-1):1;
	else if (arg1->size == arg2->size) return 0;
	return reverse?1:(-1);
}

/**
 * Compares two mails with respect to the filename field.
 *
 * @param arg1 first mail to be compared
 * @param arg2 second mail to be compared
 * @param reverse negates the result
 * @return > 0 if arg1 is larger than arg2
 */
static int mail_compare_filename(const struct mail_info *arg1, const struct mail_info *arg2, int reverse)
{
	int rc = mystricmp(arg1->filename, arg2->filename);
	if (reverse) rc *= -1;
	return rc;
}

/**
 * Compares two mails with respect to the pop3 field.
 *
 * @param arg1 first mail to be compared
 * @param arg2 second mail to be compared
 * @param reverse negates the result
 * @return > 0 if arg1 is larger than arg2
 */
static int mail_compare_pop3(const struct mail_info *arg1, const struct mail_info *arg2, int reverse)
{
	int rc = mystricmp(mail_get_pop3_server(arg1), mail_get_pop3_server(arg2));
	if (reverse) rc *= -1;
	return rc;
}

/**
 * Compares two mails with respect to the received (time) field.
 *
 * @param arg1 first mail to be compared
 * @param arg2 second mail to be compared
 * @param reverse negates the result
 * @return > 0 if arg1 is larger than arg2
 */
static int mail_compare_recv(const struct mail_info *arg1, const struct mail_info *arg2, int reverse)
{
	if (arg1->received > arg2->received) return reverse?(-1):1;
	else if (arg1->received == arg2->received) return 0;
	return reverse?1:(-1);
}


/**
 * The general sorting function that is usable for qsort().
 * It invokes compare_primary() and compare_secondary(). If
 * both return 0, mails are sorted according to the date.
 *
 * @param arg1
 * @param arg2
 * @return
 */
static int mail_compare(const void *arg1, const void *arg2)
{
	int ret = 0;
	const struct mail_info *m1, *m2;

	m1 = *(const struct mail_info**)arg1;
	m2 = *(const struct mail_info**)arg2;

	if (compare_primary) ret = compare_primary(m1,m2,compare_primary_reverse);
	if (ret == 0 && compare_secondary) ret = compare_secondary(m1,m2,compare_secondary_reverse);
	if (ret == 0) ret = mail_compare_date(m1,m2,0);
	return ret;
}

/**
 * Returns a pointer to a function that correspond to the given sort mode.
 *
 * @param sort_mode
 * @param reverse
 * @param folder_type
 * @return
 */
static int (*get_compare_function(int sort_mode, int *reverse, int folder_type))(const struct mail_info *, const struct mail_info *, int)
{
	if (sort_mode & FOLDER_SORT_REVERSE) *reverse = 1;
	else *reverse = 0;

	switch (sort_mode & FOLDER_SORT_MODEMASK)
	{
		case	FOLDER_SORT_STATUS: return mail_compare_status;
		case	FOLDER_SORT_FROMTO: return (folder_type == FOLDER_TYPE_SEND)?mail_compare_to:mail_compare_from;
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

/**
 * Set the sort functions for mail_compare in accordance of the current
 * settings.
 *
 * @param folder
 */
static void mail_compare_set_sort_mode(struct folder *folder)
{
	compare_primary = get_compare_function(folder->primary_sort, &compare_primary_reverse, folder->type);
	compare_secondary = get_compare_function(folder->secondary_sort, &compare_secondary_reverse, folder->type);
	if (compare_primary == compare_secondary) compare_secondary = NULL;
}

/**
 * Sort the given mail array by date and the secondary sorting_mode currently set.
 *
 * @param mails
 * @param num_mails
 */
static void folder_sort_mails_by_date(struct mail_info **mails, int num_mails)
{
#define mail_date_lt(a,b) (((*a)->seconds < (*b)->seconds)?1:(((*a)->seconds > (*b)->seconds)?0:(compare_secondary?compare_secondary(*a,*b,compare_secondary_reverse)<0:0)))
	QSORT(struct mail_info *, mails, num_mails, mail_date_lt);
}

/**
 * Sort the given mail array by reverse date and the secondary sorting_mode currently set.
 *
 * @param mails
 * @param num_mails
 */
static void folder_sort_mails_by_date_rev(struct mail_info **mails, int num_mails)
{
#define mail_date_lt_rev(a,b) (((*a)->seconds > (*b)->seconds)?1:(((*a)->seconds < (*b)->seconds)?0:(compare_secondary?compare_secondary(*a,*b,compare_secondary_reverse)<0:0)))
	QSORT(struct mail_info *, mails, num_mails, mail_date_lt_rev);
}


static void folder_delete_mails(struct folder *folder);
static int folder_read_mail_infos(struct folder *folder, int only_num_mails);

static struct list folder_list; /**< The global folder list */

struct folder_node
{
	struct node node; /**< Embedded node structure */
	struct folder folder; /**< The actual folder, most follow */
};

/**
 * Returns the folder node of a given plain folder.
 *
 * @param f the folder data
 * @return the entire folder_node
 */
static struct folder_node *find_folder_node_by_folder(struct folder *f)
{
	/* TODO: This could be done by pointer arithmetic much faster */
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


/**
 * Writes a string into a filehandle. Returns 0 for an error else
 * the number of bytes which has been written (at least two).
 *
 * @param fh
 * @param str
 * @param sp the optional string pool that can be used to get the string id.
 * @return
 */
static int fwrite_str(FILE *fh, char *str, struct string_pool *sp)
{
	if (str)
	{
		int sp_id;
		int len;
		unsigned char upper, lower;
		int strl = strlen(str);

		/* We only support up to 32767 characters */
		if (strl > (1<<15))
		{
			return 0;
		}

		sp_id = sp?string_pool_get_id(sp, str):-1;
		if (sp_id != -1)
		{
			upper = (sp_id >> 24) & 0x7f;
			upper |= 1<<7; /* string pool ref mark */
			lower = sp_id & 0xff;

			if (fputc(upper,fh)==EOF) return 0;
			if (fputc((sp_id >> 16) & 0xff, fh)==EOF) return 0;
			if (fputc((sp_id >> 8) & 0xff, fh)==EOF) return 0;
			if (fputc(lower,fh)==EOF) return 0;
			return 4;
		} else
		{
			upper = (strl/256)%256;
			lower = strl%256;
			if (fputc(upper,fh)==EOF) return 0;
			if (fputc(lower,fh)==EOF) return 0;

			len = fwrite(str,1,strl,fh);
			if (len == strl) return len + 2;
		}
	}
	fputc(0,fh);
	fputc(0,fh);
	return 2;
}

/**
 * Reads a string from a filehandle. It is allocated with malloc(), except
 * if sp_id is given and it filled with a meaningful value.
 *
 * @param fh
 * @param sp
 * @param zero_is_null if set to 1, NULL is returned for 0 length strings.
 *  Otherwise, an empty string is allocated if it is necessary.
 * @param sp_id_ptr pointer to an integer variable, in which the id of the string
 *  is stored. If the pointer is non-NULL, no string will be allocated and returned
 *  if the string was read as an id.
 * @return the string or NULL (which may or not may be due to an error)
 */
static char *fread_str(FILE *fh, struct string_pool *sp, int zero_is_null, int *sp_id_ptr)
{
	unsigned char upper;
	char *txt;

	txt = NULL;

	if (sp_id_ptr)
	{
		*sp_id_ptr = -1;
	}

	upper = fgetc(fh);
	if (upper & (1<<7))
	{
		/* It's a string pool ref */
		int sp_id;
		unsigned char m2, m1, lower;
		char *src_txt;

		m2 = fgetc(fh);
		m1 = fgetc(fh);
		lower = fgetc(fh);

		sp_id = ((upper & 0x7f) << 24) | (m2 << 16) | (m1 << 8) | lower;
		src_txt = string_pool_get(sp, sp_id);
		if (src_txt && sp_id_ptr)
		{
			*sp_id_ptr = sp_id;
			return NULL;
		}
		if (src_txt && (txt = malloc(strlen(src_txt) + 1)))
		{
			strcpy(txt, src_txt);
		}
	} else
	{
		int len;

		len = upper << 8;
		upper = fgetc(fh);
		len += upper;

		if (zero_is_null && !len)
		{
			return NULL;
		}

		if ((txt = (char*)malloc(len+1)))
		{
			fread(txt,1,len,fh);
			txt[len]=0;
		} else
		{
			fseek(fh, len, SEEK_CUR);
		}
	}
	return txt;
}

/**
 * Reads a string from a file handle. It is allocated with malloc().
 * Returns NULL if the string has an length of 0.
 *
 * @param fh
 * @param sp_id_ptr pointer to an integer variable, in which the id of the string
 *  is stored. If the pointer is non-NULL, no string will be allocated and returned
 *  if the string was read as an id.
 * @return the string or NULL (which may or not may be due to an error)
 */
static char *fread_str_no_null(FILE *fh, struct string_pool *sp, int *sp_id_ptr)
{
	return fread_str(fh, sp, 1, sp_id_ptr);
}

static int folder_config_load(struct folder *f);

/**
 * Returns the filename of the string pool that belongs to the index.
 *
 * @param f
 * @return the name of the folder that needs to be freed via free().
 */
static char *folder_get_string_pool_name(struct folder *f)
{
	char *sp_name;

	if ((sp_name = malloc(strlen(f->path) + 12)))
	{
		strcpy(sp_name, f->path);
		strcat(sp_name, ".index.sp");
	}
	return sp_name;
}

/**
 * Opens the indexfile of the given folder and return the filehandle.
 *
 * @param f the folder for which the index file should be opened.
 * @param mode the open mode, like fopen().
 * @return the filehandle or NULL on error.
 */
static FILE *folder_indexfile_open(struct folder *f, const char *mode)
{
	FILE *fh;
	char *path;
	char *index_name;
	char *filename_ptr;

	char cpath[256];

	if (!f || !f->path) return 0;
	if (f->special == FOLDER_SPECIAL_GROUP)
	{
		SM_DEBUGF(5, ("Folder \"%s\" is a group. No index file support for now.\n"));
		return 0;
	}
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

/**
 * Delete the indexfile of the given folder.
 *
 * @param f the folder for which the index file should be deleted.
 */
static void folder_indexfile_delete(struct folder *f)
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

/**
 * @brief invalidates (i.e., deletes) the indexfile of the given folder.
 *
 * @param folder specifies the folder of which the index should
 * be made invalid.
 */
static void folder_indexfile_invalidate(struct folder *folder)
{
	if (folder->index_uptodate)
	{
		folder_indexfile_delete(folder);
		folder->index_uptodate = 0;
	}
}

/*****************************************************************************/

void folder_delete_all_indexfiles(void)
{
	struct folder *f = folder_first();
	while (f)
	{
		folder_indexfile_delete(f);
		f->index_uptodate = 0;
		f = folder_next(f);
	}
}

/**
 * Prepare the folder to contain the given amount of mails.
 *
 *
 * @param folder the folder to be prepared
 * @param num_mails number of mails that the folder shall contain at least
 * @return 0 on failure, else 1
 */
static int folder_prepare_for_additional_mails(struct folder *folder, int num_mails)
{
	struct mail_info **new_mail_array;
	int new_mail_array_allocated = folder->mail_info_array_allocated + num_mails + 5;

	new_mail_array = (struct mail_info**)realloc(folder->mail_info_array, new_mail_array_allocated*sizeof(struct mail_info*));
	if (!new_mail_array) return 0;

	folder->mail_info_array_allocated = new_mail_array_allocated;
	folder->mail_info_array = new_mail_array;

	return 1;
}

struct folder_index_magic
{
	char magic[4];
	int ver;
};

/**
 * Resets the indexuptodate field within the folders indexfile.
 *
 * This flag is an indication that the indexfile is not uptodate
 * (does not reflect the contents of the folder)
 *
 * The use of this flags is, if if this flag is set and there are
 * no pending mails the indexfile mustn't be read instead the whole
 * directory must be rescanned. This should only happen if SimpleMail
 * has not been shut down properly.
 *
 * @param folder the folder whose pending flag should be set.
 * @return 0 on failure, else 1.
 */
static int folder_set_pending_flag_in_indexfile(struct folder *folder)
{
	int rc = 0;
	FILE *fh;

	SM_ENTER;

	if (folder->special == FOLDER_SPECIAL_GROUP) SM_RETURN(0,"%ld");

	if ((fh = folder_indexfile_open(folder,"rb+")))
	{
		/* Move at the position of the field */

		if (fseek(fh,sizeof(struct folder_index_magic),SEEK_SET) == 0)
		{
			int pending = 1;
			if (fwrite(&pending,1,4,fh) == 4)
				rc = 1;
		}
		fclose(fh);
	}

	if (rc)
	{
		SM_DEBUGF(10,("Set pending flag in index file of folder %s\n",folder->name));
	}

	SM_RETURN(rc,"%ld");
}

/*****************************************************************************/

int folder_add_mail(struct folder *folder, struct mail_info *mail, int sort)
{
	int i,pos;

	if (!folder->mail_infos_loaded)
	{
		/* No mail infos has been read. We could load them now but
		   instead we add the new mail to the pending mail array. This
		   makes this operation a lot of faster and the overall operation
		   of SimpleMail as well if this folder is not viewed (which is
		   often the case for the spam folder) */

		/* Add the mail to the pending array, this is not necessary if the
		   there is no usable indexfile for this folder (num_index_mails == -1
		   in such case) */
		if (folder->num_index_mails == -1)
		{
			SM_DEBUGF(10,("Mail added not to pending mails because number of mails is not known (indexfile not available)\n"));
			return 1;
		}

		if (folder->num_pending_mails == folder->pending_mail_info_array_allocated)
		{
			struct mail_info **array = (struct mail_info**)realloc(folder->pending_mail_info_array,(folder->pending_mail_info_array_allocated + 16)*sizeof(struct mail_info*));
			if (!array) return 0;

			folder->pending_mail_info_array_allocated += 16;
			folder->pending_mail_info_array = array;
		}

		/* set the pending flag within the indexfile if there were no pending mails before */
		if (folder->num_pending_mails == 0)
		{
			if (!folder_set_pending_flag_in_indexfile(folder)) return 0;
		}

		folder->pending_mail_info_array[folder->num_pending_mails++] = mail;
		folder->num_index_mails++;
		if (mail_info_get_status_type(mail) == MAIL_STATUS_UNREAD) folder->unread_mails++;
		if (mail->flags & MAIL_FLAGS_NEW) folder->new_mails++;
		SM_DEBUGF(10,("Mail has been successfully added as a pending mail\n"));
		return 1;
	}

	/* free the sorted mail array */
	if (folder->sorted_mail_info_array && !sort)
	{
		free(folder->sorted_mail_info_array);
		folder->sorted_mail_info_array = NULL;
	} else if (!folder->sorted_mail_info_array && sort)
	{
		void *handle = NULL;
		/* this should sort the folder */
		folder_next_mail_info(folder, &handle);
	}

	/* delete the indexfile if not already done */
	folder_indexfile_invalidate(folder);

	if (folder->mail_info_array_allocated == folder->num_mails)
	{
		folder->mail_info_array_allocated += 50;
		folder->mail_info_array = (struct mail_info **)realloc(folder->mail_info_array,folder->mail_info_array_allocated*sizeof(struct mail_info *));

		if (folder->sorted_mail_info_array)
			folder->sorted_mail_info_array = (struct mail_info **)realloc(folder->sorted_mail_info_array,folder->mail_info_array_allocated*sizeof(struct mail_info *));
	}

	if (!folder->mail_info_array)
	{
		folder->mail_info_array_allocated = 0;
		return -1;
	}

	if (mail->message_id)
	{
		/* check if there is already an mail with the same message id, this would cause problems */
		for (i=0;i<folder->num_mails;i++)
		{
			struct mail_info *fm = folder->mail_info_array[i];
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

	if (folder->sorted_mail_info_array)
	{
		mail_compare_set_sort_mode(folder);

#if 0
		/* this search routine has O(n) but should be improved to O(log n) with binary serach */
		for (pos=0;pos<folder->num_mails;pos++)
		{
			if (mail_compare(&folder->sorted_mail_info_array[pos],&mail) > 0) break;
		}
#else
		/* here comes the binary search. because this is my (= bgol) first try in such an
		   algorythmus I left the old code in. */
		{
			int low=0, high=folder->num_mails+1;

			/* For the beginning, low must be (start-1) and high must be (end+1).
			   As we are in C the array goes from start=0 to end=(n-1) but this code
			   doesn't work with low < 0. So, I'm calculating the pos counter one to
			   high and use for the mail_compare() [pos-1]. This has the advantage
			   that the pos counter is allready correct after the calculation.
			   The search also doesn't stop at a match (mail_compare==0) because the
			   mail must be placed at the end of the list of same mails. */

			pos = (low + high) / 2;
			while (pos > low)
			{
				if (mail_compare(&folder->sorted_mail_info_array[pos-1],&mail) <= 0)
					low = pos;
				else
					high = pos;
				pos = (low + high) / 2;
			}
		}
#endif

		memmove(&folder->sorted_mail_info_array[pos+1],&folder->sorted_mail_info_array[pos],(folder->num_mails - pos)*sizeof(struct mail*));
		folder->sorted_mail_info_array[pos] = mail;
	} else pos = folder->num_mails;

	folder->mail_info_array[folder->num_mails++] = mail;
	if (folder->num_mails > folder->num_index_mails) folder->num_index_mails = folder->num_mails;
	if (mail_info_get_status_type(mail) == MAIL_STATUS_UNREAD) folder->unread_mails++;
	if (mail->flags & MAIL_FLAGS_NEW) folder->new_mails++;
	if (mail->flags & MAIL_FLAGS_PARTIAL) folder->partial_mails++;

	/* Disabled because slow, buggy, and not really used */
#if 0
	/* sort the mails for threads */
	if (mail->message_id)
	{
		for (i=0;i<folder->num_mails-1;i++) /* ignore the last mail since it is this mail */
		{
			struct mail_info *fm = folder->mail_info_array[i];
			if (!(mystricmp(mail->message_id,fm->message_reply_id)))
			{
				if (!mail->sub_thread_mail)
				{
					mail->sub_thread_mail = fm;
				} else
				{
					struct mail_info *m = mail->sub_thread_mail;
					struct mail_info *next = m->next_thread_mail;
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
			struct mail_info *fm = folder->mail_info_array[i];
			if (!(mystricmp(mail->message_reply_id,fm->message_id)))
			{
				if (!fm->sub_thread_mail)
				{
					fm->sub_thread_mail = mail;
				} else
				{
					struct mail_info *m = fm->sub_thread_mail;
					struct mail_info *next = m->next_thread_mail;
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
#endif

	return pos;
}

/**
 * Remove the given mail from the given folder. It does not free the mail.
 *
 * @param folder the folder in which the mail resides.
 * @param mail the mail to be removed.
 */
static void folder_remove_mail_info(struct folder *folder, struct mail_info *mail)
{
	int i;
#if 0
	struct mail_info *submail;
#endif

	/* lock the folder, because we are going to remove something */
	folder_lock(folder);

	/* If mails info is not read_yet, the mail cannot be associated to this folder */
	if (!folder->mail_infos_loaded)
	{
		return;
	}

	/* free the sorted mail array */
	if (folder->sorted_mail_info_array)
	{
		free(folder->sorted_mail_info_array);
		folder->sorted_mail_info_array = NULL;
	}

	/* delete the indexfile if not already done */
	folder_indexfile_invalidate(folder);

#if 0
	for (i=0; i < folder->num_mails; i++)
	{
		if (folder->mail_info_array[i]->sub_thread_mail == mail)
		{
			folder->mail_info_array[i]->sub_thread_mail = mail->next_thread_mail;
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

		if (folder->mail_info_array[i]->next_thread_mail == mail)
		{
			folder->mail_info_array[i]->next_thread_mail = mail->next_thread_mail;
		}
	}

	if ((submail = mail->sub_thread_mail))
	{
		while (submail)
		{
			struct mail_info *next = submail->next_thread_mail;
			submail->next_thread_mail = NULL;
			submail = next;
		}
	}
	mail->sub_thread_mail = NULL;
	mail->next_thread_mail = NULL;
#endif

	for (i=0; i < folder->num_mails; i++)
	{
		if (folder->mail_info_array[i] == mail)
		{
			folder->num_mails--;

			for (; i < folder->num_mails; i++)
			{
				folder->mail_info_array[i] = folder->mail_info_array[i+1];
			}
		}
	}

	folder->num_index_mails--;
	if ((mail_info_get_status_type(mail) == MAIL_STATUS_UNREAD) && folder->unread_mails) folder->unread_mails--;
	if ((mail->flags & MAIL_FLAGS_NEW) && folder->new_mails) folder->new_mails--;
	if ((mail->flags & MAIL_FLAGS_PARTIAL) && folder->partial_mails) folder->partial_mails--;

	folder_unlock(folder);
}

/*****************************************************************************/

void folder_mark_mail_as_deleted(struct folder *folder, struct mail_info *mail)
{
	char *newfilename = mystrdup(mail->filename);
	char buf[256];

	if (!folder->is_imap) return;

	getcwd(buf, sizeof(buf));
	chdir(folder->path);

	if ((*newfilename == 'u') || (*newfilename == 'U')) *newfilename = 'd';
	if (!rename(mail->filename,newfilename))
	{
		free(mail->filename);
		mail->filename = newfilename;

		/* delete the indexfile if not already done */
		folder_indexfile_invalidate(folder);
	}

	chdir(buf);
}

/*****************************************************************************/

void folder_mark_mail_as_undeleted(struct folder *folder, struct mail_info *mail)
{
	char *newfilename = mystrdup(mail->filename);
	char buf[256];

	if (!folder->is_imap) return;

	getcwd(buf, sizeof(buf));
	chdir(folder->path);

	if ((*newfilename == 'd') || (*newfilename == 'D')) *newfilename = 'u';
	if (!rename(mail->filename,newfilename))
	{
		free(mail->filename);
		mail->filename = newfilename;

		/* delete the indexfile if not already done */
		folder_indexfile_invalidate(folder);
	}

	chdir(buf);
}

/*****************************************************************************/

void folder_replace_mail(struct folder *folder, struct mail_info *toreplace, struct mail_info *newmail)
{
	int i;

	folder_lock(folder);

	/* free the sorted mail array */
	if (folder->sorted_mail_info_array)
	{
		free(folder->sorted_mail_info_array);
		folder->sorted_mail_info_array = NULL;
	}

	/* Delete the indexfile if not already done */
	folder_indexfile_invalidate(folder);

	for (i=0; i < folder->num_mails; i++)
	{
		if (folder->mail_info_array[i] == toreplace)
		{
			folder->mail_info_array[i] = newmail;
			break;
		}
	}

	/* update the mail statistics */
	if ((mail_info_get_status_type(toreplace) == MAIL_STATUS_UNREAD) && folder->unread_mails) folder->unread_mails--;
	if ((toreplace->flags & MAIL_FLAGS_NEW) && folder->new_mails) folder->new_mails--;

	if (mail_info_get_status_type(newmail) == MAIL_STATUS_UNREAD) folder->unread_mails++;
	if (newmail->flags & MAIL_FLAGS_NEW) folder->new_mails++;

  folder_unlock(folder);
}

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

void folder_set_mail_status(struct folder *folder, struct mail_info *mail, int status_new)
{
	int i, mail_found = 0;
	/* first check the pending mail array */
	for (i=0;i<folder->num_pending_mails;i++)
	{
		if (folder->pending_mail_info_array[i]==mail)
		{
			mail_found = 1;
			break;
		}
	}
	if (!mail_found)
	{
		/* If the mail is not found check the mail_array */
		/* If mail infos are not read_yet, read them now */
		if (!folder->mail_infos_loaded)
			folder_read_mail_infos(folder,0);

		for (i=0;i<folder->num_mails;i++)
		{
			if (folder->mail_info_array[i]==mail)
			{
				mail_found = 1;
				break;
			}
		}
	}
	if (mail_found)
	{
		char *filename;

		if (status_new == mail->status)
			return;

		/* update the mail statistics */
		if (mail_info_get_status_type(mail) == MAIL_STATUS_UNREAD && folder->unread_mails) folder->unread_mails--;
		if ((mail->flags & MAIL_FLAGS_NEW) && folder->new_mails) folder->new_mails--;
		mail->flags &= ~MAIL_FLAGS_NEW;
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
			folder_indexfile_invalidate(folder);
		}
	}
}

/*****************************************************************************/

void folder_set_mail_flags(struct folder *folder, struct mail_info *mail, int flags_new)
{
	if (mail->flags == flags_new) return;

	if ((mail->flags & MAIL_FLAGS_PARTIAL) && !(flags_new & MAIL_FLAGS_PARTIAL))
	{
		if (folder->partial_mails) folder->partial_mails--;
	} else
	{
		if (!(mail->flags & MAIL_FLAGS_PARTIAL) && (flags_new & MAIL_FLAGS_PARTIAL))
			folder->partial_mails++;
	}

	mail->flags = flags_new;

	/* Delete the indexfile if not already done */
	folder_indexfile_invalidate(folder);
}

/*****************************************************************************/

int folder_count_signatures(char *def_signature)
{
	struct folder *f = folder_first();
	int use_count = 0;
	while (f)
	{
		if (mystrcmp(f->def_signature, def_signature) == 0)
		{
			use_count++;
		}
		f = folder_next(f);
	}
	return use_count;
}

/*****************************************************************************/

struct mail_info *folder_find_mail_by_filename(struct folder *folder, char *filename)
{
	int i;

	/* first check the pending mail array */
	for (i=0; i < folder->num_pending_mails; i++)
	{
		if (!mystricmp(folder->pending_mail_info_array[i]->filename,filename))
		{
			return folder->pending_mail_info_array[i];
		}
	}

	/* If the mail is not found check the mail_array */
	/* If mail infos are not read_yet, read them now */
	if (!folder->mail_infos_loaded)
		folder_read_mail_infos(folder,0);

	for (i=0; i < folder->num_mails; i++)
	{
		if (!mystricmp(folder->mail_info_array[i]->filename,filename))
		{
			return folder->mail_info_array[i];
		}
	}

	return NULL;
}

/*****************************************************************************/

struct mail_info *folder_imap_find_mail_by_uid(struct folder *folder, unsigned int uid)
{
	int i,l;
	char buf[20];

	if (!folder) return NULL;
	if (!folder->is_imap) return NULL;

	sprintf(buf,"%u",uid);
	l = strlen(buf);

	for (i=0; i < folder->num_mails; i++)
	{
		if (!mystrnicmp(buf,folder->mail_info_array[i]->filename + 1,l))
		{
			if (folder->mail_info_array[i]->filename[l+1] == '.' || folder->mail_info_array[i]->filename[l+1] == 0)
				return folder->mail_info_array[i];
		}
	}
	return NULL;
}

/*****************************************************************************/

int folder_is_filename_mail(const char *fn)
{
	if (fn[0] != '.')
		return 1;

	if (strcmp(".",fn) && strcmp("..",fn) && strcmp(".config", fn) && strcmp(".index", fn))
		return 1;

	return 0;
}

/*****************************************************************************/

struct folder_rescan_really_context
{
	struct coroutine_basic_context basic_context;

	/* Input parameters */

	const char *folder_path;
	const char *folder_name;

	/** Function to be called for a new mail. Returns 1 on success, 0 otherwise. */
	int (*mail_callback)(struct mail_info *m, void *udata);
	void *mail_callback_udata;
	void (*status_callback)(const char *txt);

	/** the optional progress monitor. It should have 101 work units. */
	struct progmon *pm;

	/* Actual context */
	DIR *dfd; /* directory descriptor */
	char path[380];

	struct string_list mail_filename_list;
	int number_of_mails;
	char buf[80];
	unsigned int last_ticks;
	unsigned int total_work_done;// = 0;
	unsigned int current_mail;

	int create;// = 1;
};

/**
 * Work horse for scanning a folder.
 *
 * @return coroutine return status.
 */
static coroutine_return_t folder_rescan_really(struct coroutine_basic_context *ctx)
{
	struct folder_rescan_really_context *c = (struct folder_rescan_really_context*)ctx;
	struct dirent *dptr; /* dir entry */
	struct string_node *snode;

	COROUTINE_BEGIN(c);

	c->create = 1;

	getcwd(c->path, sizeof(c->path));
	if (chdir(c->folder_path) == -1)
		goto out;

	if (!(c->dfd = opendir(SM_CURRENT_DIR)))
		goto out;

	c->last_ticks = time_reference_ticks();

	string_list_init(&c->mail_filename_list);
	c->number_of_mails = 0;

	if (c->status_callback)
	{
		sm_snprintf(c->buf,sizeof(c->buf),_("Scanning folder \"%s\""),c->folder_name);
		c->status_callback(c->buf);
		if (c->pm)
		{
			c->pm->working_on(c->pm, c->buf);
		}
	}

	while ((dptr = readdir(c->dfd)) != NULL)
	{
		char *name = dptr->d_name;

		if (!folder_is_filename_mail(name))
			continue;

		string_list_insert_tail(&c->mail_filename_list, name);
		c->number_of_mails++;
	}
	closedir(c->dfd);

	if (c->pm)
	{
		c->pm->work(c->pm, 1);
	}

	c->current_mail = 0;
	while ((snode = string_list_remove_head(&c->mail_filename_list)))
	{
		struct mail_info *m;

		if (c->status_callback || c->pm)
		{
			if (time_ms_passed(c->last_ticks) > 500)
			{
				sm_snprintf(c->buf,sizeof(c->buf),_("Reading mail %ld of %ld"),c->current_mail,c->number_of_mails);
				if (c->status_callback)
				{
					c->status_callback(c->buf);
				}

				if (c->pm)
				{
					int new_total_work_done;
					int work;

					new_total_work_done = c->current_mail * 100 / c->number_of_mails;;
					if ((work = new_total_work_done - c->total_work_done) > 0)
					{
						c->pm->working_on(c->pm, c->buf);
						c->pm->work(c->pm, work);

						c->total_work_done = new_total_work_done;
					}
				}
				c->last_ticks = time_reference_ticks();
			}
		}

		if (c->create && (m = mail_info_create_from_file(folder_mail_context, snode->string)))
		{
			c->create = c->mail_callback(m, c->mail_callback_udata);
		}

		free(snode->string);
		free(snode);

		c->current_mail++;

		if (!(c->current_mail % 512))
		{
			COROUTINE_YIELD(c);
		}
	}

	if (c->status_callback || c->pm)
	{
		sm_snprintf(c->buf,sizeof(c->buf),_("Folder \"%s\" scanned"),c->folder_name);

		if (c->status_callback)
		{
			c->status_callback(c->buf);
		}

		if (c->pm)
		{
			c->pm->working_on(c->pm, c->buf);
			c->pm->work(c->pm, 100 - c->total_work_done);
		}
	}

out:
	(void)1;
	COROUTINE_END(c);
}


/*****************************************************************************/

/**
 * Internal operation to dispose all mails of a folder.
 *
 * @param f
 */
static void folder_dispose_mails(struct folder *f)
{
	/* FIXME: We should also delete each mail */
	free(f->mail_info_array);
	free(f->sorted_mail_info_array);
	free(f->pending_mail_info_array);

	f->mail_info_array = f->sorted_mail_info_array = f->pending_mail_info_array = NULL;
	f->mail_info_array_allocated = 0;
	f->num_mails = 0;
	f->new_mails = 0;
	f->unread_mails = 0;
	f->pending_mail_info_array_allocated = 0;
	f->num_pending_mails = 0;
}

/*****************************************************************************/

/** Thread for various lengthy folder operations */
static thread_t folder_thread;

/*****************************************************************************/

struct folder_thread_mail_callback_data
{
	int num_mails;
	int mails_allocated;
	struct mail_info **mails;
};

/*****************************************************************************/

/**
 * Callback that is invoked for each successfully read in mail.
 *
 * @param m the mail the has been read in.
 * @param udata the user data.
 * @return 0 if further processing should be aborter, else 1.
 */
static int folder_thread_mail_callback(struct mail_info *m, void *udata)
{
	struct folder_thread_mail_callback_data *data = udata;

	if (thread_aborted())
	{
		return 0;
	}

	if (data->num_mails >= data->mails_allocated)
	{
		int new_mails_allocated = (data->mails_allocated * 2) + 8;
		struct mail_info **new_mails = (struct mail_info**)realloc(data->mails, new_mails_allocated * sizeof(*new_mails));
		if (!new_mails)
		{
			return 0;
		}
		data->mails = new_mails;
		data->mails_allocated = new_mails_allocated;
	}
	data->mails[data->num_mails++] = m;
	return 1;
}

/*****************************************************************************/

/**
 * Entry for the folder thread.
 *
 * @param udata
 * @return
 */
static int folder_thread_entry(void *udata)
{
	coroutine_scheduler_t sched;

	if (!(sched = coroutine_scheduler_new_custom(NULL, NULL)))
		return 0;

	if (thread_parent_task_can_contiue())
	{
		while (thread_wait(sched, NULL, NULL, 0));
	}
	coroutine_scheduler_dispose(sched);
	return 0;
}

/*****************************************************************************/

struct folder_thread_rescan_context
{
	struct coroutine_basic_context basic_context;

	/* Input parameters */
	char *folder_path;
	void (*status_callback)(const char *txt);
	void (*completed)(char *folder_path, void *udata);
	void *completed_udata;
	struct progmon *pm;

	/* Actual context */
	char *folder_name;
	struct folder_thread_mail_callback_data udata;

	/* Context of nested coroutine */
	struct folder_rescan_really_context *rescan_ctx;
};

/*****************************************************************************/


/**
 * Function that is called on the context of the main thread.
 *
 * @param folder_path
 * @param m
 * @param num_m
 */
static void folder_rescan_async_completed(struct folder_thread_rescan_context *ctx)
{
	struct folder *f;
	int i;

	char *folder_path;
	struct mail_info **m;
	int num_m;

	folder_path = ctx->folder_path;
	m = ctx->udata.mails;
	num_m = ctx->udata.num_mails;

	if (!(f = folder_find_by_path(folder_path)))
		return;

	folder_lock(f);
	folder_dispose_mails(f);

	f->mail_infos_loaded = 1; /* must happen before folder_add_mail() */
	f->num_index_mails = 0;
	f->partial_mails = 0;

	folder_prepare_for_additional_mails(f, num_m);

	for (i=0; i < num_m; i++)
	{
		folder_add_mail(f, m[i], 0);
	}

	folder_indexfile_invalidate(f);

	f->rescanning = 0;

	if (f->to_be_saved)
	{
		folder_save_index(f);
		f->to_be_saved = 0;
	}
	folder_unlock(f);

	ctx->completed(folder_path, ctx->completed_udata);
}

/*****************************************************************************/

/**
 * Rescan the folder. This function is callable from any context.
 *
 * @param ctx
 * @return a coroutine return value.
 */
static coroutine_return_t folder_thread_rescan_coroutine(struct coroutine_basic_context *ctx)
{
	struct folder_thread_rescan_context *c = (struct folder_thread_rescan_context*)ctx;
	struct folder_rescan_really_context *rescan_ctx = c->rescan_ctx;

	COROUTINE_BEGIN(c);

	{
		/* Retrieve name of the folder, set rescanning state and initialize context */
		struct folder *f;

		folders_lock();
		if ((f = folder_find_by_path(c->folder_path)))
		{
			folder_lock(f);
			c->folder_name = mystrdup(f->name);
			folder_unlock(f);
		}
		folders_unlock();

		if (!f)
		{
			goto bailout;
		}
	}

	if ((rescan_ctx = c->rescan_ctx = malloc(sizeof(*rescan_ctx))))
	{
		coroutine_t cor;

		memset(rescan_ctx, 0, sizeof(*rescan_ctx));

		rescan_ctx->folder_path = c->folder_path;
		rescan_ctx->folder_name = c->folder_name;
		rescan_ctx->mail_callback= folder_thread_mail_callback;
		rescan_ctx->mail_callback_udata = &c->udata;
		rescan_ctx->status_callback = c->status_callback;
		rescan_ctx->pm = c->pm;

		if ((cor = coroutine_add(ctx->scheduler, folder_rescan_really, &rescan_ctx->basic_context)))
		{
			COROUTINE_AWAIT_OTHER(c, cor);
		}

		free(rescan_ctx);
	}

	thread_call_function_sync(thread_get_main(), folder_rescan_async_completed, 1, c);
	free(c->udata.mails);
bailout:
	c->pm->done(c->pm);
	free(c->folder_path);
	free(c->folder_name);
	progmon_delete(c->pm);

	COROUTINE_END(c);
}

/*****************************************************************************/

/**
 * Function that is called when SimpleMail shuts down.
 *
 * @param udata
 */
static void folder_thread_cleanup(void *udata)
{
}

/*****************************************************************************/

/**
 * Ensure that the folder thread is running.
 *
 * @return 0 or 1, depending of the folder thread is up or not.
 */
static int folder_thread_ensure(void)
{
	if (!folder_thread)
	{
		if (!(folder_thread = thread_add("SimpleMail - Folder thread", folder_thread_entry, NULL)))
		{
			return 0;
		}
		atcleanup(folder_thread_cleanup, NULL);
	}

	return !!folder_thread;
}

/*****************************************************************************/

int folder_rescan_async(struct folder *folder, void (*status_callback)(const char *txt), void (*completed)(char *folder_path, void *udata), void *udata)
{
	char *folder_path;
	struct folder_thread_rescan_context *ctx;

	if (!folder_thread_ensure())
	{
		return 0;
	}

	if (!(folder_path = mystrdup(folder->path)))
	{
		return 0;
	}

	if (!(ctx = (struct folder_thread_rescan_context*)malloc(sizeof(*ctx))))
		return 0;
	memset(ctx, 0, sizeof(*ctx));
	ctx->basic_context.free_after_done = 1;
	ctx->folder_path = folder_path;
	ctx->status_callback = status_callback;
	ctx->completed = completed;
	ctx->completed_udata = udata;
	if ((ctx->pm = progmon_create()))
	{
		ctx->pm->begin(ctx->pm, 101, "Rescanning");
	}

	if (thread_call_coroutine(folder_thread, folder_thread_rescan_coroutine, &ctx->basic_context))
	{
		folder->rescanning = 1;
		callback_refresh_folder(folder);
		return 1;
	} else
	{
		free(ctx->folder_path);
		ctx->pm->done(ctx->pm);
		progmon_delete(ctx->pm);
	}
	return 0;
}

/**
 * Read the next mail info from the index.
 *
 * @param mc the mail context that shall be used when creating the mails or NULL.
 * @param fh the file handle of the index file.
 * @param sp the string pool that is used to resolve string references.
 * @return the mail info or NULL when something failed.
 */
static struct mail_info *folder_read_mail_info_from_index(mail_context *mc, FILE *fh, struct string_pool *sp)
{
	struct mail_info *m;
	int num_to = 0;
	int num_cc = 0;

	if (fread(&num_to,1,4,fh) != 4) return NULL;
	if (fread(&num_cc,1,4,fh) != 4) return NULL;

	if ((m = mail_info_create(mc)))
	{
		int pop3_id = -1;

		m->subject = (utf8*)fread_str(fh, NULL, 0, NULL);
		m->filename = fread_str(fh, NULL, 0, NULL);

		m->from_phrase = (utf8*)fread_str_no_null(fh, sp, NULL);
		m->from_addr = fread_str_no_null(fh, sp, NULL);

		/* Read the to list */
		if ((m->to_list = (struct address_list*)malloc(sizeof(struct address_list))))
			list_init(&m->to_list->list);

		while (num_to--)
		{
			char *realname = fread_str_no_null(fh, sp, NULL);
			char *email = fread_str_no_null(fh, sp, NULL);
			struct address *addr;

			if (m->to_list)
			{
				if ((addr = (struct address*)malloc(sizeof(struct address))))
				{
					addr->realname = realname;
					addr->email = email;
					list_insert_tail(&m->to_list->list, &addr->node);
				} /* should free realname and email on failure */
			}
		}

		/* Read the cc list */
		if ((m->cc_list = (struct address_list*)malloc(sizeof(struct address_list))))
			list_init(&m->cc_list->list);

		while (num_cc--)
		{
			char *realname = fread_str_no_null(fh, sp, NULL);
			char *email = fread_str_no_null(fh, sp, NULL);
			struct address *addr;

			if (m->cc_list)
			{
				if ((addr = (struct address*)malloc(sizeof(struct address))))
				{
					addr->realname = realname;
					addr->email = email;
					list_insert_tail(&m->cc_list->list, &addr->node);
				} /* should free realname and email on failure */
			}
		}

		m->pop3_server.str = fread_str_no_null(fh, sp, &pop3_id);
		if (pop3_id != -1)
		{
			const char *pop3_str = string_pool_get(sp, pop3_id);
			int our_pop3_id = string_pool_ref(m->context->sp, pop3_str);
			if (our_pop3_id != -1)
			{
				m->pop3_server.id = our_pop3_id;
				m->tflags |= MAIL_TFLAGS_POP3_ID;
			}
		} else
		{
			/* TODO: Compress string also here if possible */
		}
		m->message_id = fread_str_no_null(fh, sp, NULL);
		m->message_reply_id = fread_str_no_null(fh, sp, NULL);
		m->reply_addr = fread_str_no_null(fh, sp, NULL);

		fseek(fh,ftell(fh)%2,SEEK_CUR);
		fread(&m->size,1,sizeof(m->size),fh);
		fread(&m->seconds,1,sizeof(m->seconds),fh);
		fread(&m->received,1,sizeof(m->received),fh);
		fread(&m->flags,1,sizeof(m->flags),fh);
	}
	return m;
}

/**
 * Check whether the given file handle is a proper indexfile.
 *
 * @param fh
 * @return
 */
static int folder_indexfile_check(FILE *fh)
{
	struct folder_index_magic magic = {0};

	fread(&magic,1,sizeof(magic),fh);

	if (strncmp("SMFI", magic.magic, 4) != 0)
	{
		return 0;
	}

	if (magic.ver != FOLDER_INDEX_VERSION)
	{
		return 0;
	}

	return 1;
}

struct folder_index
{
	FILE *fh;
	int pending;
	int num_mails;
	int unread_mails;
};

/**
 * Open the index for the given folder.
 *
 * @param f
 * @return
 */
static struct folder_index *folder_index_open(struct folder *f)
{
	struct folder_index *fi;

	if (!(fi = (struct folder_index*)malloc(sizeof(*fi))))
	{
		return NULL;
	}
	memset(fi, 0, sizeof(*fi));

	if (!(fi->fh = folder_indexfile_open(f, "rb")))
	{
		goto bailout;
	}

	if (!folder_indexfile_check(fi->fh))
	{
		goto bailout;
	}

	if (fread(&fi->pending, 1, 4, fi->fh) != 4)
	{
		goto bailout;
	}
	if (fread(&fi->num_mails, 1, 4, fi->fh) != 4)
	{
		goto bailout;
	}
	if (fread(&fi->unread_mails, 1, 4, fi->fh) != 4)
	{
		goto bailout;
	}

	return fi;

bailout:
	if (fi->fh) fclose(fi->fh);
	free(fi);
	return NULL;
}

static void folder_index_close(struct folder_index *fi)
{
	if (!fi)
	{
		return;
	}
	fclose(fi->fh);
	free(fi);

}

/**
 * Load the string pool associated with the given folder.
 *
 * @param f
 * @return the string pool or NULL.
 */
static struct string_pool *folder_load_string_pool(struct folder *f)
{
	struct string_pool *sp;
	char *sp_name;

	if (!(sp = string_pool_create()))
		return NULL;

	if ((sp_name = folder_get_string_pool_name(f)))
	{
		/* Failure cases will be handled later when a string ref
		 * cannot be resolved */
		string_pool_load(sp, sp_name);
		free(sp_name);
	}
	return sp;

}

/**
 * Read all mail info from the already opened index file to the given folder.
 *
 * @param fi
 * @param sp
 * @param out_ptr
 */
static void folder_index_read_them_all(struct folder_index *fi, struct string_pool *sp, struct mail_info ***out_ptr)
{
	int i;
	int num_mails = fi->num_mails;
	struct mail_info **out;

	if (!(out = malloc(sizeof(struct mail_info *) * num_mails)))
	{
		*out_ptr = NULL;
		return;
	}

	for (i = 0; i < num_mails && !feof(fi->fh); i++)
	{
		struct mail_info *m;

		if ((m = folder_read_mail_info_from_index(folder_mail_context, fi->fh, sp)))
		{
			mail_identify_status(m);
			m->flags &= ~MAIL_FLAGS_NEW;
			out[i] = m;
		}
	}
	*out_ptr = out;
}

/**
 * Read the information of all mails of the given folder. This may
 * involve triggering index file or rescanning of the folder.
 *
 * @param folder
 * @param only_num_mails
 * @return 0 if an error occurred otherwise something else.
 */
static int folder_read_mail_infos(struct folder *folder, int only_num_mails)
{
	struct folder_index *fi;
	int mail_infos_read = 0;

	if (folder->special == FOLDER_SPECIAL_GROUP) return 0;

	if ((fi = folder_index_open(folder)))
	{
		unsigned int time_ref;
		int pending;

		time_ref = time_reference_ticks();
		pending = fi->pending;

		/* Read in the mail info if index is not marked as having pending mails
		   or if we know the pending mails. Also only do this if we do not
		   already know the number of mails */

		/* This whole if cause including needs a small rethought */
		if ((!pending || (pending && folder->num_pending_mails)) && (folder->num_index_mails == -1 || (!folder->mail_infos_loaded && !only_num_mails)))
		{
			int num_mails = fi->num_mails;

			if (!only_num_mails)
			{
				struct string_pool *sp;
				struct mail_info **mis;

				int i;

				if (!(sp = folder_load_string_pool(folder)))
					goto nosp;

				folder->mail_infos_loaded = 1; /* must happen before folder_add_mail() */
				mail_infos_read = 1;
				folder->unread_mails = 0;
				folder->new_mails = 0;
				folder_prepare_for_additional_mails(folder, num_mails + folder->num_pending_mails);

				if (pending)
				{
					SM_DEBUGF(10,("%ld mails within indexfile. %ld are pending\n",num_mails,folder->num_pending_mails));
				}

				folder_index_read_them_all(fi, sp, &mis);
				string_pool_delete(sp);

				if (mis)
				{
					for (i=0; i < num_mails; i++)
					{
						if (!mis[i])
						{
							continue;
						}
						folder_add_mail(folder, mis[i], 0);
					}
					free(mis);
				}

				if (folder->num_pending_mails)
				{
					/* Add pending mails (i.e., mails that have been added
					 * prior the loading of the folder) now */
					for (i=0;i<folder->num_pending_mails;i++)
						folder_add_mail(folder,folder->pending_mail_info_array[i],0);
					folder->num_pending_mails = 0;

					folder_index_close(fi);

					/* Two possibilities: Either we mark the indexfile as not uptodate (so it get's completely rewritten
					   if saving is requested or we append the pending stuff with the indexfile here. I chose
					   the first because it means less to do for me */
					folder_indexfile_delete(folder);
					folder->index_uptodate = 0;
					return 1;
				}

nosp:
				(void)1;
			} else
			{
				folder->num_index_mails = num_mails;
				folder->unread_mails = fi->unread_mails;
			}
		}

		SM_DEBUGF(10,("Index file of folder \"%s\" read after %d ms\n",folder->name,time_ms_passed(time_ref)));
		folder_index_close(fi);
	}

	if (only_num_mails)
		return 1;

	folder->index_uptodate = mail_infos_read;

	if (!mail_infos_read && !folder->rescanning)
	{
		folder_rescan_async(folder, NULL, callback_rescan_folder_completed, NULL);
	}
	return 1;
}

/**
 * Disposes the given folder node (deeply).
 *
 * @param node defines the entity to be freed. It is safe to pass NULL.
 */
static void folder_node_dispose(struct folder_node *node)
{
	int i;
	if (!node) return;
	for (i = 0; i < node->folder.num_mails; i++)
		mail_info_free(node->folder.mail_info_array[i]);
	free(node->folder.mail_info_array);
	free(node->folder.sorted_mail_info_array);
	free(node->folder.pending_mail_info_array);
	free(node->folder.imap_path);
	free(node->folder.imap_server);
	free(node->folder.imap_user);
	free(node->folder.path);
	free(node->folder.name);
	thread_dispose_semaphore(node->folder.sem);
	free(node);
}

/**
 * Adds a folder to the internal folder list.
 *
 * @param path defines the path of the folder (can be modified afterwards)
 * @return the new folder
 */
static struct folder *folder_add(char *path)
{
	struct folder_node *node;

	if (folder_find_by_path(path))
		return NULL;

	if (!(node = (struct folder_node*)malloc(sizeof(struct folder_node))))
		return NULL;

	/* Initialize everything with 0 */
	memset(node,0,sizeof(struct folder_node));
	node->folder.num_index_mails = -1;

	string_list_init(&node->folder.imap_all_folder_list);
	string_list_init(&node->folder.imap_sub_folder_list);

	/* create the directory if it doesn't exists */
	if (!sm_makedir(path))
		goto bailout;

	if (!(node->folder.sem = thread_create_semaphore()))
		goto bailout;

	if (!(node->folder.path = mystrdup(path)))
		goto bailout;

	node->folder.primary_sort = FOLDER_SORT_DATE;
	node->folder.secondary_sort = FOLDER_SORT_FROMTO;

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
		if (!mystricmp(folder_name,"Spam")) node->folder.special = FOLDER_SPECIAL_SPAM;
		folder_config_save(&node->folder);
	}
	folder_read_mail_infos(&node->folder,1);
	list_insert_tail(&folder_list,&node->node);
	return &node->folder;

bailout:
	folder_node_dispose(node);
	return NULL;
}
/*****************************************************************************/

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

/**
 * Creates a new folder group but does not add it to the internal
 * list.
 *
 * @param name the name of the group.
 * @return
 */
static struct folder_node *folder_create_group(char *name)
{
	struct folder_node *node;
	if ((node = (struct folder_node*)malloc(sizeof(struct folder_node))))
	{
		/* Initialize everything with 0 */
		memset(node,0,sizeof(struct folder_node));
		node->folder.name = mystrdup(name);
		node->folder.special = FOLDER_SPECIAL_GROUP;
		string_list_init(&node->folder.imap_all_folder_list);
		string_list_init(&node->folder.imap_sub_folder_list);

		if ((node->folder.sem = thread_create_semaphore()))
		{
			return node;
		}
		free(node->folder.name);
		free(node);
	}
	return NULL;
}

/*****************************************************************************/

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

/*****************************************************************************/

struct folder *folder_add_imap(struct folder *parent, char *imap_path)
{
	char *name;

	struct folder_node *node;
	struct folder_node *parent_node = (struct folder_node*)(((char*)parent)-sizeof(struct node));
	struct folder *f;

	if (!parent) return NULL;
	if (!parent->is_imap) return NULL;

	if ((f = folder_find_by_imap(parent->imap_user, parent->imap_server,imap_path))) return f;

	if (!(node = (struct folder_node*)malloc(sizeof(struct folder_node))))
		return NULL;

	/* Initialize everything with 0 */
	memset(node,0,sizeof(struct folder_node));

	name = sm_file_part(imap_path);
	node->folder.name = mystrdup(name);
	node->folder.parent_folder = parent;
	node->folder.path =  mycombinepath(parent->path,name);
	node->folder.special = FOLDER_SPECIAL_NO;
	node->folder.is_imap = 1;
	if (!(node->folder.imap_server = mystrdup(parent->imap_server)))
		goto bailout;
	if (!(node->folder.imap_user = mystrdup(parent->imap_user)))
		goto bailout;
	if (!(node->folder.imap_path = mystrdup(imap_path)))
		goto bailout;
	node->folder.imap_hierarchy_delimiter = ".";
	node->folder.num_index_mails = -1;
	string_list_init(&node->folder.imap_all_folder_list);
	string_list_init(&node->folder.imap_sub_folder_list);

	if (!(node->folder.sem = thread_create_semaphore()))
		goto bailout;

	if (!sm_makedir(node->folder.path))
		goto bailout;

	if (!folder_config_load(&node->folder))
	{
		folder_config_save(&node->folder);
	}

	folder_read_mail_infos(&node->folder,1);
	list_insert(&folder_list, &node->node, &parent_node->node);
	return &node->folder;

bailout:
	folder_node_dispose(node);
	return NULL;
}

/**
 * @brief Reparents all folders with parents @p old_p to have the parent @p new_p.
 *
 * @param old_p this one is going to be replaced.
 * @param new_p this one is the new one.
 */
static void folder_reparent_all(struct folder *old_p, struct folder *new_p)
{
	struct folder_node *node;

	/* Set a new parent to all the folders */
	node = (struct folder_node*)list_first(&folder_list);
	while (node)
	{
		if (node->folder.parent_folder == old_p)
			node->folder.parent_folder = new_p;
		node = (struct folder_node*)node_next(&node->node);
	}
}

/*****************************************************************************/

int folder_remove(struct folder *f)
{
  if (!folder_attempt_lock(f))
  {
  	sm_request(NULL,_("Can't delete folder because it is currently in use."),_("_Ok"));
  	return 0;
  }

	if (f->rescanning)
	{
		sm_request(NULL,_("Can't delete folder because it is currently being rescanned."),_("_Ok"));
		return 0;
	}

	if (f->special == FOLDER_SPECIAL_NO)
	{
		struct folder_node *node = (struct folder_node*)list_first(&folder_list);
		while (node)
		{
			if (&node->folder == f)
			{
				int ok;

				if (f->is_imap)
				{
					ok = sm_request(NULL,
							_("You are going to delete an IMAP folder.\n"
							  "This will remove the folder locally but not on the server.\n"
							  "The next time you connect to the server the folder will reappear."),
							_("_Delete it|_Cancel"));
				} else
				{
					ok = sm_request(NULL,
							_("Do you really want to delete the folder\nand all its mails?"),
							_("_Delete it|_Cancel"));
				}

				if (ok)
				{
					char buf[380];

					node_remove(&node->node);
					folder_delete_mails(f);
					sm_snprintf(buf,sizeof(buf),"%s.index",f->path);
					remove(buf);
					sm_snprintf(buf,sizeof(buf),"%s.config",f->path);
					remove(buf);
					remove(f->path);
					folder_unlock(f);
					thread_dispose_semaphore(node->folder.sem);
					free(node);
					folder_save_order();
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
				if (f->is_imap)
				{
					if (sm_request(NULL,
						_("You are going to delete an IMAP server.\n"
						  "Unless you don't remove it from the accounts setting page\n"
						  "it will reappear the next time SimpleMail is started."),
						_("_Delete it|_Cancel")))
					{
						struct folder_node *node2;
						char buf[380];

						/* delete the contents of the directory */
						mydeletedir(f->path);

						/* remove the IMAP server node from the list so it won't be deleted twice */
						node_remove(&node->node);

						/* remove all folders which are on the same imap server like the imap server folder */
						node2 = (struct folder_node*)list_first(&folder_list);
						while (node2)
						{
							struct folder *nf = &node2->folder;
							struct folder_node *next_node = (struct folder_node*)node_next(&node2->node);

							if (folder_on_same_imap_server(f,nf))
							{
								node_remove(&node2->node);
								free(node2);
							}

							node2 = next_node;
						}

						/* Get rid of the rest... */
						sm_snprintf(buf,sizeof(buf),"%s.index",f->path);
						remove(buf);
						sm_snprintf(buf,sizeof(buf),"%s.config",f->path);
						remove(buf);

						/* ...and the memory */
						folder_unlock(f);
						free(node);

						/* save the new order */
						folder_save_order();
						return 1;
					}
				} else
				{
					if (sm_request(NULL,
						_("Do you really want to delete this group?\nOnly the group entry is deleted,\nnot the folders inside the group."),
						_("_Delete it|_Cancel")))
					{
						folder_reparent_all(f,parent);
						node_remove(&node->node);
						folder_unlock(f);
						free(node);
						folder_save_order();
						rc = 1;
					}
				}
				break;
			}
			node = (struct folder_node*)node_next(&node->node);
		}

		if (!rc)
			folder_unlock(f);
		return rc;
	}
	folder_unlock(f);
	return 0;
}

/*****************************************************************************/

struct folder *folder_create_live_filter(struct folder *folder, utf8 *filter)
{
	struct folder *f;

	if (folder->is_imap) return NULL;

	if (!(f = (struct folder *)malloc(sizeof(struct folder))))
		return NULL;

	memset(f,0,sizeof(struct folder));

	if ((f->sem = thread_create_semaphore()))
	{
		f->num_index_mails = -1;
		f->filter = filter;
		f->ref_folder = folder;

		return f;
	}
	free(f);
	return NULL;
}

/*****************************************************************************/

void folder_delete_live_folder(struct folder *live_folder)
{
	thread_dispose_semaphore(live_folder->sem);
	free(live_folder);
}

/*****************************************************************************/

void folder_unlink_all(void)
{
	struct folder_node *node;
	while ((node = (struct folder_node*)list_remove_tail(&folder_list)));
}

/*****************************************************************************/

void folder_add_to_tree(struct folder *f,struct folder *parent)
{
	struct folder_node *fnode = (struct folder_node*)(((char*)f)-sizeof(struct node));

	f->parent_folder = parent;
	list_insert_tail(&folder_list,&fnode->node);
}

/**
 * Return the imap path of the folder specified by the path
 * by opening the config.
 *
 * @param folder_path
 * @return the imap path or NULL.
 */
static char *folder_config_get_imap_path(char *folder_path)
{
	char buf[256];
	FILE *fh;

	sm_snprintf(buf, sizeof(buf), "%s.config", folder_path);

	if ((fh = fopen(buf,"r")))
	{
		myreadline(fh,buf);

		if (!mystrnicmp("FICO",buf,4))
		{
			while (myreadline(fh,buf))
			{
				if (!mystrnicmp("IMapPath=",buf,9))
				{
					return mystrdup(&buf[9]);
				}
			}
		}
	}
	return NULL;
}

/**
 * Load the configuration for the folder.
 *
 * @param f the folder for which the configuration shall be loaded.
 * @return success or not.
 */
static int folder_config_load(struct folder *f)
{
	char buf[256];
	int rc = 0;
	FILE *fh;

	sm_snprintf(buf, sizeof(buf), "%s.config",f->path);

	string_list_clear(&f->imap_all_folder_list);
	string_list_clear(&f->imap_sub_folder_list);

	if ((fh = fopen(buf,"r")))
	{
		myreadline(fh,buf);

		if (!mystrnicmp("FICO",buf,4))
		{
			rc = 1;
			while (myreadline(fh,buf))
			{
				if (!mystrnicmp("Name=",buf,5))
				{
					if (f->name) free(f->name);
					f->name = mystrdup(&buf[5]);
				}
				else if (!mystrnicmp("Type=",buf,5)) f->type = atoi(&buf[5]);
				else if (!mystrnicmp("Special=",buf,8)) f->special = atoi(&buf[8]);
				else if (!mystrnicmp("PrimarySort=",buf,12)) f->primary_sort = atoi(&buf[12]);
				else if (!mystrnicmp("SecondarySort=",buf,14)) f->secondary_sort = atoi(&buf[14]);
				else if (!mystrnicmp("DefaultTo=",buf,10))
				{
					free(f->def_to);
					f->def_to = mystrdup(&buf[10]);
				}
				else if (!mystrnicmp("DefaultFrom=",buf,12))
				{
					free(f->def_from);
					f->def_from = mystrdup(&buf[12]);
				}
				else if (!mystrnicmp("DefaultReplyTo=",buf,15))
				{
					free(f->def_replyto);
					f->def_replyto = mystrdup(&buf[15]);
				}
				else if (!mystrnicmp("DefaultSignature=",buf,17))
				{
					free(f->def_signature);
					f->def_signature = mystrdup(&buf[17]);
				}
				else if (!mystrnicmp("IsIMap=",buf,7)) f->is_imap = atoi(&buf[7]);
				else if (!mystrnicmp("IMapUser=",buf,9))
				{
					free(f->imap_user);
					f->imap_user = mystrdup(&buf[9]);
				}
				else if (!mystrnicmp("IMapPath=",buf,9))
				{
					free(f->imap_path);
					f->imap_path = mystrdup(&buf[9]);
				}
				else if (!mystrnicmp("IMapServer=",buf,11))
				{
					free(f->imap_server);
					f->imap_server = mystrdup(&buf[11]);
				}
				else if (!mystrnicmp("IMapFolder=",buf,11))
				{
					string_list_insert_tail(&f->imap_all_folder_list,&buf[11]);
				}
				else if (!mystrnicmp("IMapSubFolder=",buf,14))
				{
					string_list_insert_tail(&f->imap_sub_folder_list,&buf[14]);
				}
				else if (!mystrnicmp("IMapDontUseUids=",buf,16))
				{
					f->imap_dont_use_uids = atoi(&buf[16]);
				}
				else if (!mystrnicmp("IMapUidNext=",buf,12))
				{
					f->imap_uid_next = strtoul(&buf[12],NULL,10);
				}
				else if (!mystrnicmp("IMapUidValidity=",buf,16))
				{
					f->imap_uid_validity = strtoul(&buf[16],NULL,10);
				}
				else if (!mystrnicmp("IMapDownloadMode=",buf,17))
				{
					f->imap_download = strtoul(&buf[17],NULL,10);

					/* At the moment, only two modes are supported.
					 * Everything which is not mode 1 is mode 0.
					 */
					if (f->imap_download != 1)
						f->imap_download = 0;
				}
			}
		}
		fclose(fh);
	}
	return rc;
}

/*****************************************************************************/

void folder_config_save(struct folder *f)
{
	char buf[256];
	FILE *fh;

	if (!f->path) return;
	if (f->special == FOLDER_SPECIAL_GROUP && !f->is_imap) return;

	sm_snprintf(buf, sizeof(buf), "%s.config",f->path);

	if ((fh = fopen(buf,"w")))
	{
		struct string_node *node;
		fputs("FICO\n",fh);
		fprintf(fh,"Name=%s\n",f->name);
		fprintf(fh,"Type=%d\n",f->type);
		fprintf(fh,"Special=%d\n",f->special);
		fprintf(fh,"PrimarySort=%d\n",f->primary_sort);
		fprintf(fh,"SecondarySort=%d\n",f->secondary_sort);
		fprintf(fh,"DefaultTo=%s\n", f->def_to?f->def_to:"");
		fprintf(fh,"DefaultFrom=%s\n", f->def_from?f->def_from:"");
		fprintf(fh,"DefaultReplyTo=%s\n", f->def_replyto?f->def_replyto:"");
		fprintf(fh,"DefaultSignature=%s\n",f->def_signature?f->def_signature:"");
		fprintf(fh,"IsIMap=%d\n",f->is_imap);
		fprintf(fh,"IMapUser=%s\n",f->imap_user?f->imap_user:"");
		fprintf(fh,"IMapPath=%s\n",f->imap_path?f->imap_path:"");
		fprintf(fh,"IMapServer=%s\n",f->imap_server?f->imap_server:"");
		if (f->imap_dont_use_uids)
			fprintf(fh,"IMapDontUseUids=1\n");
		if (f->imap_uid_next != 0)
			fprintf(fh,"IMapUidNext=%u\n",f->imap_uid_next);
		if (f->imap_uid_validity != 0)
			fprintf(fh,"IMapUidValidity=%u\n",f->imap_uid_validity);
		fprintf(fh,"IMapDownloadMode=%d\n",f->imap_download);

		node = string_list_first(&f->imap_all_folder_list);
		while (node)
		{
			fprintf(fh,"IMapFolder=%s\n",node->string);
			node = (struct string_node*)node_next(&node->node);
		}

		node = string_list_first(&f->imap_sub_folder_list);
		while (node)
		{
			fprintf(fh,"IMapSubFolder=%s\n",node->string);
			node = (struct string_node*)node_next(&node->node);
		}

		fclose(fh);
	}
}

/*****************************************************************************/

void folder_imap_set_folders(struct folder *folder, struct string_list *all_folders_list, struct string_list *sub_folders_list)
{
	struct string_node *node;

	if (all_folders_list)
	{
		string_list_clear(&folder->imap_all_folder_list);
		node = string_list_first(all_folders_list);
		while (node)
		{
			string_list_insert_tail(&folder->imap_all_folder_list,node->string);
			node = (struct string_node*)node_next(&node->node);
		}
	}

	if (sub_folders_list)
	{
		string_list_clear(&folder->imap_sub_folder_list);
		node = string_list_first(sub_folders_list);
		while (node)
		{
			string_list_insert_tail(&folder->imap_sub_folder_list,node->string);
			node = (struct string_node*)node_next(&node->node);
		}
	}
}

/*****************************************************************************/

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

/*****************************************************************************/

int folder_set(struct folder *f, char *newname, char *newpath, int newtype, char *newdefto, char *newdeffrom, char *newdefreplyto, char* newdefsignature, int prim_sort, int second_sort)
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

			if (refresh)
				folder_indexfile_invalidate(f);

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
			folder_indexfile_invalidate(f);
			rescan = 1;
		}
		f->type = newtype;
		changed = 1;
	}

	if (prim_sort != folder_get_primary_sort(f))
	{
		folder_set_primary_sort(f,prim_sort);
		changed = 1;
		refresh = 1;
	}

	if (second_sort != folder_get_secondary_sort(f))
	{
		folder_set_secondary_sort(f,second_sort);
		changed = 1;
		refresh = 1;
	}

	if (mystrcmp(newdefto,f->def_to) != 0)
	{
		free(f->def_to);
		f->def_to = mystrdup(newdefto);
		changed = 1;
	}

	if (mystrcmp(newdeffrom,f->def_from) != 0)
	{
		free(f->def_from);
		f->def_from = mystrdup(newdeffrom);
		changed = 1;
	}

	if (mystrcmp(newdefreplyto,f->def_replyto) != 0)
	{
		free(f->def_replyto);
		f->def_replyto = mystrdup(newdefreplyto);
		changed = 1;
	}

	if (mystrcmp(newdefsignature, f->def_signature) != 0)
	{
		free(f->def_signature);
		f->def_signature = mystrdup(newdefsignature);
		changed = 1;
	}

	/* Save the settings if the folder settings has been changed */
	if (changed) folder_config_save(f);

	if (rescan)
	{
		int i;

		/* free all kind of data */

		/* free the sorted mail array */
		if (f->sorted_mail_info_array)
		{
			free(f->sorted_mail_info_array);
			f->sorted_mail_info_array = NULL;
		}

		for (i=0;i < f->num_mails; i++)
			mail_info_free(f->mail_info_array[i]);

		if (f->mail_info_array)
		{
			free(f->mail_info_array);
			f->mail_info_array = NULL;
		}
		f->num_mails = 0;
		f->mail_info_array_allocated = 0;
		f->new_mails = 0;
		f->unread_mails = 0;
		f->num_index_mails = -1;

		folder_read_mail_infos(f,0);
	}

	return refresh;
}

/*****************************************************************************/

struct folder *folder_first(void)
{
	struct folder_node *node = (struct folder_node*)list_first(&folder_list);
	if (node) return &node->folder;
	return NULL;
}

/*****************************************************************************/

struct folder *folder_prev(struct folder *f)
{
	if (f)
	{
		struct folder_node *node = (struct folder_node*)(((char*)f)-sizeof(struct node));
		if ((node = (struct folder_node*)node_prev(&node->node)))
		{
			return &node->folder;
		}
	}
	return NULL;
}

/*****************************************************************************/

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

/*****************************************************************************/

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

/*****************************************************************************/

struct folder *folder_find_by_imap(char *user, char *server, char *path)
{
	struct folder *f = folder_first();
	while (f)
	{
		if (f->is_imap)
		{
			if (!mystricmp(server,f->imap_server) && !mystricmp(user,f->imap_user))
			{
				if (!path && !f->imap_path) return f;
				if (path && !(*path) && !f->imap_path) return f;
				if (!mystricmp(path,f->imap_path)) return f;
			}
		}
		f = folder_next(f);
	}
	return NULL;
}

/**
 * Find the folder by the given imap account.
 *
 * @param ac the account that is associated to the folder.
 * @return the root folder for the imap account or NULL if there is no such
 *  folder.
 */
static struct folder *folder_find_by_imap_account(struct account *ac)
{
	struct folder *f;

	/* Folder already been added? */
	f = folder_first();
	while (f)
	{
		if (!mystricmp(f->imap_server, ac->imap->name) &&
			!mystricmp(f->imap_user,ac->imap->login) &&
			f->special == FOLDER_SPECIAL_GROUP)
		{
			break;
		}
		f = folder_next(f);
	}
	return f;
}

/**
 * @return the main incoming folder.
 */
struct folder *folder_incoming(void)
{
	return folder_find_special(FOLDER_SPECIAL_INCOMING);
}

/**
 * @return the main outgoing folder.
 */
struct folder *folder_outgoing(void)
{
	return folder_find_special(FOLDER_SPECIAL_OUTGOING);
}

/**
 * @return the main sent folder.
 */
struct folder *folder_sent(void)
{
	return folder_find_special(FOLDER_SPECIAL_SENT);
}

/**
  * @return the main deleted folder.
 */
struct folder *folder_deleted(void)
{
	return folder_find_special(FOLDER_SPECIAL_DELETED);
}

/*****************************************************************************/

struct folder *folder_spam(void)
{
	return folder_find_special(FOLDER_SPECIAL_SPAM);
}

/*****************************************************************************/

struct folder *folder_find(int pos)
{
	struct folder_node *node = (struct folder_node*)list_find(&folder_list,pos);
	if (!node) return NULL;
	return &node->folder;
}

/*****************************************************************************/

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

/*****************************************************************************/

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

/**
 * Finds a group folder by name.
 *
 * @param name the name of the group folder.
 * @return the group folder or NULL
 */
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

/*****************************************************************************/

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

/*****************************************************************************/

struct folder *folder_find_by_file(char *filename)
{
	char buf[256];
	struct folder *f = folder_first();

	getcwd(buf, sizeof(buf));

	while (f)
	{
		chdir(f->path);
		if (sm_file_is_in_drawer(filename, f->path)) break;
		f = folder_next(f);
	}

	chdir(buf);
	return f;
}

/*****************************************************************************/

struct folder *folder_find_by_mail(struct mail_info *mail)
{
	struct folder_node *node = (struct folder_node*)list_first(&folder_list);
	while (node)
	{
		struct folder *folder = &node->folder;
		int i;
		for (i=0; i < folder->num_mails; i++)
		{
			if (folder->mail_info_array[i] == mail)
				return folder;
		}
		/* also check the pending mail array */
		for (i=0; i < folder->num_pending_mails; i++)
		{
			if (folder->pending_mail_info_array[i] == mail)
				return folder;
		}
		node = (struct folder_node *)node_next(&node->node);
	}
	return NULL;
}

/*****************************************************************************/

struct mail_info *folder_find_mail_by_position(struct folder *f, int position)
{
	void *handle = NULL;
	if (!f->sorted_mail_info_array) return NULL;
	if (position >= f->num_mails) return NULL;
	folder_next_mail_info(f,&handle); /* sort the stuff */
	if (f->sorted_mail_info_array) return f->sorted_mail_info_array[position];
	return f->mail_info_array[position];
}

/*****************************************************************************/

struct mail_info *folder_find_next_mail_info_by_filename(char *folder_path, char *mail_filename)
{
	void *handle = NULL;
	struct folder *f = folder_find_by_path(folder_path);
	struct mail_info *m;

	if (!f) return NULL;

	while ((m = folder_next_mail_info(f, &handle)))
	{
		if (!mystricmp(m->filename,mail_filename))
		{
			return folder_next_mail_info(f,&handle);
		}
	}
	return NULL;
}

/*****************************************************************************/

struct mail_info *folder_find_prev_mail_info_by_filename(char *folder_path, char *mail_filename)
{
	void *handle = NULL;
	struct folder *f = folder_find_by_path(folder_path);
	struct mail_info *lm = NULL;
	struct mail_info *m;

	if (!f) return NULL;

	while ((m = folder_next_mail_info(f, &handle)))
	{
		if (!mystricmp(m->filename,mail_filename))
		{
			return lm;
		}
		lm = m;
	}

	return NULL;
}

/*****************************************************************************/

struct mail_info *folder_find_best_mail_info_to_select(struct folder *folder)
{
	void *handle = NULL;
	struct mail_info *m;

	int relevant_types;

	if (folder->special == FOLDER_SPECIAL_OUTGOING) relevant_types = MAIL_STATUS_HOLD;
	else relevant_types = MAIL_STATUS_UNREAD;

	while ((m = folder_next_mail_info(folder, &handle)))
	{
		if (mail_info_get_status_type(m) == relevant_types) return m;
	}

	/* take the first mail */
	handle = NULL;
	m = folder_next_mail_info(folder, &handle);
	return m;
}

/*****************************************************************************/

int folder_get_index_of_mail(struct folder *f, struct mail_info *mail)
{
	int index = 0;
	void *handle = 0;
	struct mail_info *m;

	while ((m = folder_next_mail_info(f, &handle)))
	{
		if (m == mail) return index;
		index ++;
	}
	return -1;
}

/*****************************************************************************/

int folder_size_of_mails(struct folder *f)
{
	int size = 0;
	void *handle = NULL;
	struct mail_info *m;

	if (!f) return 0;

	while ((m = folder_next_mail_info(f, &handle)))
		size += m->size;
	return size;
}

/*****************************************************************************/

int folder_move_mail_array(struct folder *from_folder, struct folder *dest_folder, struct mail_info **mail_info_array, int num_mails)
{
	char *buf, *src_buf, *src_path_end_buf, *dest_buf, *dest_path_end_buf;
	char path[512];
	int path_changed = 0;
	int i = 0;

	if (!from_folder || !dest_folder) return 0;
	if (from_folder == dest_folder) return 1;
	if (from_folder->special == FOLDER_SPECIAL_GROUP ||
			dest_folder->special == FOLDER_SPECIAL_GROUP) return 0;

	if (!(buf = (char*)malloc(1024)))
		return 0;

	src_buf = buf;
	dest_buf = buf + 512;

	strcpy(src_buf,from_folder->path);
	strcpy(dest_buf,dest_folder->path);

	src_path_end_buf = src_buf + strlen(src_buf);
	dest_path_end_buf = dest_buf + strlen(dest_buf);

	for (i=0;i<num_mails;i++)
	{
		struct mail_info *mail = mail_info_array[i];

		sm_add_part(src_buf,mail->filename,512);

		/* Change the status of mails if moved to outgoing and had formerly a sent flag set */
		if (dest_folder->special == FOLDER_SPECIAL_OUTGOING && mail_info_get_status_type(mail) == MAIL_STATUS_SENT)
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

		if (rename(src_buf,dest_buf) != 0)
		{
			char *new_name;

			/* Renaming failed (probably because the name already existed in the dest dir.
			 * We need a new name, at first change to the dest dir if not already done */
			if (!path_changed)
			{
				getcwd(path,sizeof(path));
				if (chdir(dest_folder->path) == -1)
					break;
				path_changed = 1;
			}

			if ((new_name = mail_get_new_name(mail->status)))
			{
				if (rename(src_buf,new_name) == 0)
				{
					/* renaming was successfully */
					free(mail->filename);
					mail->filename = new_name;
				} else
				{
					free(new_name);
					break;
				}
			}
		}

		folder_remove_mail_info(from_folder,mail);
		folder_add_mail(dest_folder,mail,1);

		/* reset the path buffers */
		*src_path_end_buf = 0;
		*dest_path_end_buf = 0;
	}

	if (path_changed) chdir(path);

	free(buf);
	return i;
}

/*****************************************************************************/

int folder_move_mail(struct folder *from_folder, struct folder *dest_folder, struct mail_info *mail)
{
	return folder_move_mail_array(from_folder,dest_folder,&mail,1);
}

/*****************************************************************************/

int folder_delete_mail(struct folder *from_folder, struct mail_info *mail)
{
	char path[512];
	if (!from_folder) return 0;
	folder_remove_mail_info(from_folder,mail);
	strcpy(path,from_folder->path);
	sm_add_part(path,mail->filename,512);
	remove(path);
	mail_info_free(mail);
	return 1;
}

/**
 * Really delete all mails in the given folder.
 *
 * @param folder defines the folder from which to delete all mails.
 */
static void folder_delete_mails(struct folder *folder)
{
	int i;
	char path[256];

	if (!folder) return;

	getcwd(path, sizeof(path));
	if(chdir(folder->path) == -1) return;

	/* If mail infos are not read_yet, read them now */
	if (!folder->mail_infos_loaded)
		folder_read_mail_infos(folder,0);

	/* free the sorted mail array */
	if (folder->sorted_mail_info_array)
	{
		free(folder->sorted_mail_info_array);
		folder->sorted_mail_info_array = NULL;
	}

	for (i=0;i<folder->num_mails;i++)
	{
		remove(folder->mail_info_array[i]->filename);
		mail_info_free(folder->mail_info_array[i]);
	}

	for (i=0;i<folder->num_pending_mails;i++)
	{
		remove(folder->pending_mail_info_array[i]->filename);
		mail_info_free(folder->pending_mail_info_array[i]);
	}

	folder->num_mails = 0;
	folder->num_index_mails = 0;
	folder->unread_mails = 0;
	folder->new_mails = 0;
	folder->mail_info_array_allocated = 0;
	folder->num_pending_mails = 0;
	folder->pending_mail_info_array_allocated = 0;

	free(folder->mail_info_array);
	folder->mail_info_array = NULL;

	free(folder->pending_mail_info_array);
	folder->pending_mail_info_array = NULL;

	folder->index_uptodate = 0;

	chdir(path);
}

/*****************************************************************************/

void folder_delete_deleted(void)
{
	struct folder *f = folder_deleted();
	if (!f) return;
	if (!folder_attempt_lock(f)) return;
	folder_delete_mails(f);
	folder_unlock(f);
}

/**
 * Stores the header of the index file.
 *
 * @param f the folder for which the index file shall be written.
 * @param fh the file handle to write into.
 * @return 0 on failure, 1 on success
 */
static int folder_save_index_header(struct folder *f, FILE *fh)
{
	int ver = FOLDER_INDEX_VERSION;
	int pending = 0;

	fwrite("SMFI",1,4,fh);
	fwrite(&ver,1,4,fh);
	fwrite(&pending,1,4,fh);
	fwrite(&f->num_index_mails,1,4,fh);
	fwrite(&f->unread_mails,1,4,fh);
	return 1;
}

/*****************************************************************************/

static int string_pool_put_address_list(struct string_pool *sp, struct address_list *al)
{
	struct address *addr;

	addr = address_list_first(al);
	while (addr)
	{
		string_pool_ref(sp, addr->email);
		string_pool_ref(sp, addr->realname);
		addr = address_next(addr);
	}
	return 1;
}

/*****************************************************************************/

int folder_save_index(struct folder *f)
{
	FILE *fh;
	int append;
	struct string_pool *sp;

	if (!f->num_pending_mails && (!f->mail_infos_loaded || f->index_uptodate))
		return 0;

	if (f->rescanning)
	{
		f->to_be_saved = 1;
		return 0;
	}

	append = !!f->num_pending_mails;
	sp = NULL;

	if (!append && f->mail_info_array)
	{
		char *sp_name;

		if ((sp_name = folder_get_string_pool_name(f)))
		{
			if ((sp = string_pool_create()))
			{
				int i;

				for (i=0; i < f->num_mails; i++)
				{
					struct mail_info *m = f->mail_info_array[i];
					const char *pop3 = mail_get_pop3_server(m);

					if (m->from_addr) string_pool_ref(sp, m->from_addr);
					if (m->from_phrase) string_pool_ref(sp, m->from_phrase);
					if (m->to_list) string_pool_put_address_list(sp, m->to_list);
					if (m->cc_list) string_pool_put_address_list(sp, m->cc_list);
					if (m->reply_addr) string_pool_ref(sp, m->reply_addr);

					if (pop3) string_pool_ref(sp, pop3);
				}
				string_pool_save(sp, sp_name);
			}
			free(sp_name);
		}
	}

	if ((fh = folder_indexfile_open(f,append?"rb+":"wb")))
	{
		int i;
		struct mail_info **mail_info_array;
		int num_mails;

		if (append)
		{
			fseek(fh,0,SEEK_END);
			num_mails = f->num_pending_mails;
			mail_info_array = f->pending_mail_info_array;
		} else
		{
			folder_save_index_header(f,fh);
			num_mails = f->num_mails;
			mail_info_array = f->mail_info_array;
		}

		for (i=0; i < num_mails; i++)
		{
			struct mail_info *m = mail_info_array[i];
			int len = 0;
			int len_add;
			int num_to = 0;
			int num_cc = 0;
			struct address *to_addr = NULL;
			struct address *cc_addr = NULL;

			if (m->to_list)
			{
				num_to = address_list_length(m->to_list);
				to_addr = address_list_first(m->to_list);
			}

			if (m->cc_list)
			{
				num_cc = address_list_length(m->cc_list);
				cc_addr = address_list_first(m->cc_list);
			}

			fwrite(&num_to,1,4,fh);
			fwrite(&num_cc,1,4,fh);

			if (!(len_add = fwrite_str(fh, (char*)m->subject, NULL))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, m->filename, NULL))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, (char*)m->from_phrase, sp))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, m->from_addr, sp))) break;
			len += len_add;

			while (to_addr)
			{
				if (!(len_add = fwrite_str(fh, to_addr->realname, sp))) break;
				len += len_add;
				if (!(len_add = fwrite_str(fh, to_addr->email, sp))) break;
				len += len_add;
				to_addr = address_next(to_addr);
			}

			while (cc_addr)
			{
				if (!(len_add = fwrite_str(fh, cc_addr->realname, sp))) break;
				len += len_add;
				if (!(len_add = fwrite_str(fh, cc_addr->email, sp))) break;
				len += len_add;
				cc_addr = address_next(cc_addr);
			}

			if (!(len_add = fwrite_str(fh, mail_get_pop3_server(m), sp))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, m->message_id, NULL))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, m->message_reply_id, NULL))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, m->reply_addr, sp))) break;
			len += len_add;

			/* so that integervars are aligned */
			if (len % 2) fputc(0,fh);

			fwrite(&m->size,1,sizeof(m->size),fh);
			fwrite(&m->seconds,1,sizeof(m->seconds),fh);
			fwrite(&m->received,1,sizeof(m->received),fh);
			fwrite(&m->flags,1,sizeof(m->flags),fh);
		}

		if (append)
		{
			/* the pending mails are stored so we can free them */
			for (i=0;i<num_mails;i++)
				mail_info_free(mail_info_array[i]);

			f->num_pending_mails = 0;
			fseek(fh,0,SEEK_SET);
			folder_save_index_header(f,fh);
		}
		fclose(fh);
		f->index_uptodate = 1;
	} else
	{
		SM_DEBUGF(5,("Couldn't open index file for folder at path \"%s\" for writing\n",f->path));
	}

	if (sp)
	{
		string_pool_delete(sp);
	}

	return 1;
}

/*****************************************************************************/

void folder_save_all_indexfiles(void)
{
	struct folder *f = folder_first();
	while (f)
	{
		folder_save_index(f);
		f = folder_next(f);
	}
}

/*****************************************************************************/

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


/**
 * Sorts sorted_mail_info_array of the given folder with the folder-set
 * sorting options. folder->sorted_mail_info_array must be already allocated
 * and filled with mails to be sorted.
 *
 * @param f
 */
static void folder_sort_mails(struct folder *f)
{
	/* set the correct search function */
	mail_compare_set_sort_mode(f);

	if (compare_primary)
	{
		unsigned int time_ref = time_reference_ticks();
		if (compare_primary == mail_compare_date)
		{
			if (compare_primary_reverse) folder_sort_mails_by_date_rev(f->sorted_mail_info_array, f->num_mails);
			else folder_sort_mails_by_date(f->sorted_mail_info_array, f->num_mails);
		} else
		{
/*			struct mail_info **mails = f->sorted_mail_info_array;
			int num_mails = f->num_mails;
#define mail_compare_lt(a,b) (mail_compare(a,b) < 0)
			QSORT(struct mail_info *, mails, num_mails, mail_compare_lt);
*/			qsort(f->sorted_mail_info_array, f->num_mails, sizeof(struct mail*),mail_compare);
		}

		SM_DEBUGF(10,("Sorted mails in %d ms\n",time_ms_passed(time_ref)));
	}
}
/*****************************************************************************/

struct mail_info *folder_next_mail_info(struct folder *folder, void **handle)
{
	struct mail_info **mail_info_array;
	int *ihandle;

	ihandle = (int*)handle;

	if (folder->ref_folder)
	{
		struct mail_info *mi;
		utf8 *filter;

		/* If mail infos have not been loaded, determine number of mails
		 * before.
		 * TODO: Remove this as because of this these are no real live folders yet!
		 */
		if (!folder->mail_infos_loaded)
		{
			void *newhandle = NULL;
			int num_mails = 0;

			/* avoid endless loop */
			folder->mail_infos_loaded = 1;
			while ((mi = folder_next_mail_info(folder,&newhandle)))
				num_mails++;
			folder->num_mails = num_mails;
		}

		filter = folder->filter;

		while (1)
		{
			if (!(mi = folder_next_mail_info(folder->ref_folder,handle)))
				break;

			if (utf8stristr(mi->subject,filter))
				break;

			if (utf8stristr(mail_info_get_from_addr(mi),filter))
				break;

			if (utf8stristr(mail_info_get_from_phrase(mi),filter))
				break;

		}
		return mi;
	}

	/* If mail info haven't read yet, read it now */
	if (!folder->mail_infos_loaded)
		folder_read_mail_infos(folder,0);

	mail_info_array = folder->mail_info_array;
	if (!folder->sorted_mail_info_array && /* *((int**)handle) == 0 && */ folder->num_mails && folder->mail_info_array_allocated)
	{
		/* the array is not sorted, so sort it now */
		folder->sorted_mail_info_array = (struct mail_info**)malloc(sizeof(struct mail_info*)*folder->mail_info_array_allocated);
		if (folder->sorted_mail_info_array)
		{
			/* copy the mail pointers into the buffer */
			memcpy(folder->sorted_mail_info_array, folder->mail_info_array, sizeof(struct mail*)*folder->num_mails);

			folder_sort_mails(folder);

			mail_info_array = folder->sorted_mail_info_array;
		}
	}

	if (folder->sorted_mail_info_array) mail_info_array = folder->sorted_mail_info_array;
	return (struct mail_info*)(((*((int*)handle))<folder->num_mails)?(mail_info_array[(*ihandle)++]):NULL);
}
/* we define a macro for mail iterating, handle must be initialzed to NULL at start */
/*#define folder_next_mail(folder,handle) ( ((*((int*)handle))<folder->num_mails)?(folder->mail_array[(*((int*)handle))++]):NULL)*/

/*****************************************************************************/

struct mail_info **folder_get_mail_info_array(struct folder *folder)
{
	void *handle = NULL;

	/* start sort stuff */
	folder_next_mail_info(folder,&handle);
	if (folder->sorted_mail_info_array) return folder->sorted_mail_info_array;
	return folder->mail_info_array;
}

/*****************************************************************************/

/**
 * Helper function which returns 1 if a given mail has the given
 * property. If property is unknown 0 is returned.
 *
 * @param folder
 * @param mail
 * @param properties
 * @return
 */
static int folder_mail_info_has_property(struct folder *folder, struct mail_info *mail, int properties)
{
	if (!properties) return 1;
	if (properties & FOLDER_QUERY_MAILS_PROP_SPAM && (mail_info_is_spam(mail) || mail->flags & MAIL_FLAGS_AUTOSPAM))
		return 1;

	return 0;
}

/*****************************************************************************/

struct mail_info **folder_query_mails(struct folder *folder, int properties)
{
	int i,num;
	void *handle;
	struct mail_info *m;
	struct mail_info **array;

	if (!properties)
	{
		struct mail_info **src_array = folder_get_mail_info_array(folder);

		/* ensure that the array is sorted */
		num = folder->num_mails;

		if ((array = (struct mail_info**)malloc(sizeof(struct mail_info*)*(num+1))))
		{
			memcpy(array,src_array,sizeof(struct mail*)*num);
			array[num] = NULL;
			return array;
		}
	}

	num = 0;
	handle = NULL;
	while ((m = folder_next_mail_info(folder,&handle)))
	{
		if (folder_mail_info_has_property(folder,m,properties))
			num++;
	}

	if (!(array = (struct mail_info**)malloc(sizeof(struct mail_info*)*(num+1))))
		return NULL;

	i = 0;
	handle = NULL;

	while ((m = folder_next_mail_info(folder,&handle)))
	{
		if (folder_mail_info_has_property(folder,m,properties))
			array[i++] = m;
	}

	array[num] = NULL;
	return array;
}

/*****************************************************************************/

int folder_get_primary_sort(struct folder *folder)
{
	return folder->primary_sort;
}

/*****************************************************************************/

void folder_set_primary_sort(struct folder *folder, int sort_mode)
{
	if (folder->primary_sort != sort_mode)
	{
		folder->primary_sort = sort_mode;

		/* free the sorted mail array */
		if (folder->sorted_mail_info_array)
		{
			free(folder->sorted_mail_info_array);
			folder->sorted_mail_info_array = NULL;
		}
	}
}

/*****************************************************************************/

int folder_get_secondary_sort(struct folder *folder)
{
	return folder->secondary_sort;
}

/*****************************************************************************/

void folder_set_secondary_sort(struct folder *folder, int sort_mode)
{
	if (folder->secondary_sort != sort_mode)
	{
		folder->secondary_sort = sort_mode;

		/* free the sorted mail array */
		if (folder->sorted_mail_info_array)
		{
			free(folder->sorted_mail_info_array);
			folder->sorted_mail_info_array = NULL;
		}
	}
}

/*****************************************************************************/

int mail_matches_filter(struct folder *folder, struct mail_info *m,
											  struct filter *filter)
{
	struct filter_rule *rule = (struct filter_rule*)list_first(&filter->rules_list);
	struct mail_complete *mc = mail_complete_create(NULL);
	if (mc) mc->info = m;

	while (rule)
	{
		int take = 0;

		switch (rule->type)
		{
			case	RULE_FROM_MATCH:
						if (rule->u.from.from_pat)
						{
							int i = 0, flags = rule->flags;

							if (m->flags & MAIL_FLAGS_FROM_ADDR_ASCII7) flags |= SM_PATTERN_ASCII7;
							while (!take && rule->u.from.from_pat[i])
								take = sm_match_pattern(rule->u.from.from_pat[i++], m->from_addr, flags);

							if (!take)
							{
								i = 0;
								flags = rule->flags;
								if (m->flags & MAIL_FLAGS_FROM_ASCII7) flags |= SM_PATTERN_ASCII7;
								while (!take && rule->u.from.from_pat[i])
									take = sm_match_pattern(rule->u.from.from_pat[i++], m->from_phrase, flags);
							}
						}
						break;

			case	RULE_RCPT_MATCH:
						if (rule->u.rcpt.rcpt_pat)
						{
							int i = 0, flags = rule->flags;

							while (!take && rule->u.rcpt.rcpt_pat[i])
							{
								struct address *addr;
								addr = (struct address*)list_first(&m->to_list->list);
								while (!take && addr)
								{
									take = sm_match_pattern(rule->u.rcpt.rcpt_pat[i], addr->realname, flags);
									if (!take) take = sm_match_pattern(rule->u.rcpt.rcpt_pat[i], addr->email, flags);
									addr = (struct address*)node_next(&addr->node);
								}

								if (!take)
								{
									addr = (struct address*)list_first(&m->cc_list->list);
									while (!take && addr)
									{
										take = sm_match_pattern(rule->u.rcpt.rcpt_pat[i], addr->realname, flags);
										if (!take) take = sm_match_pattern(rule->u.rcpt.rcpt_pat[i], addr->email, flags);
										addr = (struct address*)node_next(&addr->node);
									}
								}
								i++;
							}
						}
						break;

			case	RULE_SUBJECT_MATCH:
						if (rule->u.subject.subject_pat)
						{
							int i = 0, flags = rule->flags;
							if (m->flags & MAIL_FLAGS_SUBJECT_ASCII7) flags |= SM_PATTERN_ASCII7;
							while (!take && rule->u.subject.subject_pat[i])
								take = sm_match_pattern(rule->u.subject.subject_pat[i++], m->subject, flags);
						}
						break;

			case	RULE_HEADER_MATCH:
						{
							if (mc && rule->u.header.name_pat)
							{
								struct header *header;

								mail_read_header_list_if_empty(mc);

								header = (struct header*)list_first(&mc->header_list);
								while (!take && header)
								{
									if (sm_match_pattern(rule->u.header.name_pat, (utf8*)header->name, SM_PATTERN_NOCASE|SM_PATTERN_NOPATT|SM_PATTERN_ASCII7))
									{
										if (header->contents)
										{
											utf8 *cont = NULL;
											parse_text_string(header->contents, &cont);

											if (cont)
											{
												int i = 0, flags = rule->flags;
												while (!take && rule->u.header.contents_pat[i])
													take = sm_match_pattern(rule->u.header.contents_pat[i++], cont, flags);
												free(cont);
											}
										}
									}
									header = (struct header*)node_next(&header->node);
								}
							}
						}
						break;

			case	RULE_BODY_MATCH:
						if (mc && rule->u.body.body)
						{
							if (mail_read_header_list_if_empty(mc))
							{
								struct mail_complete *text_part;

								mail_process_headers(mc);
								mail_read_contents(folder->path,mc);

								if ((text_part = mail_find_content_type(mc,"text","plain")))
								{
									void *decoded_data;
									int decoded_data_len;

									mail_decoded_data(text_part,&decoded_data,&decoded_data_len);
									take = filter_match_rule_len(&rule->u.body.body_parsed,(char*)decoded_data,decoded_data_len,rule->flags);
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

		if (!take && !filter->mode)
		{
			if (mc) mc->info = NULL; /* don't free the mail_info! */
			mail_complete_free(mc);
			return 0;
		}
		if (take && filter->mode)
		{
			if (mc) mc->info = NULL; /* don't free the mail_info! */
			mail_complete_free(mc);
			return 1;
		}

		rule = (struct filter_rule*)node_next(&rule->node);
	}

	if (mc) mc->info = NULL; /* don't free the mail_info! */
	mail_complete_free(mc);

	if (!filter->mode) return 1;
	return 0;
}

/*****************************************************************************/

struct filter *folder_mail_can_be_filtered(struct folder *folder, struct mail_info *m, int action)
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

/*****************************************************************************/

int folder_apply_filter(struct folder *folder, struct filter *filter)
{
	void *handle = NULL;
	struct mail_info *m;
	char path[512];

	getcwd(path, sizeof(path));
	if(chdir(folder->path) == -1) return 0;

	for (;;)
	{
		void *old_handle = handle;

		if (!(m = folder_next_mail_info(folder,&handle))) break;

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

/*****************************************************************************/

int folder_filter(struct folder *folder)
{
	void *handle = NULL;
	struct mail_info *m;
	char path[512];

	getcwd(path, sizeof(path));
	if(chdir(folder->path) == -1) return 0;

	for (;;)
	{
		void *old_handle = handle;
		struct filter *f;

		if (!(m = folder_next_mail_info(folder,&handle))) break;

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

/**
 * @brief Opens the file that stores the order of the folders.
 *
 * @param mode fopen() compatible open mode
 * @return the pointer to the file or NULL on failure.
 */
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

/**
 * @brief represents a single entry in the order file
 */
struct folder_order
{
	char *name; /**< the user presented name of the folder */
	char *path; /**< path on the local filesystem */
	int special; /**< its type */
	int parent; /**< index of the parent */
	int closed; /**< for groups, 1 if closed */
};

/**
 * @brief Traverse the contents of the order file using a callback
 *
 * @param folder_traverse_order_callback defines the function which is called for every order entry
 * @param user_data user data that is forwarded to the callback.
 */
static void folder_traverse_order_file(int (*folder_traverse_order_callback)(struct folder_order *order, void *user_data), void *user_data)
{
	FILE *fh;
	char *buf;
	struct folder_order order;

	if (!(fh = folder_open_order_file("r")))
		return;

	if (!(buf = (char*)malloc(1024)))
		goto bailout;

	/* name */
	order.name = buf;

	while ((fgets(buf,1024,fh)))
	{
		char *path;
		char *temp_buf;

		/* path */
		if (!(path = strchr(buf,'\t')))
			continue;
		*path++ = 0;
		order.path = path;

		/* special */
		if (!(temp_buf = strchr(path,'\t')))
			continue;
		*temp_buf++ = 0;
		order.special = atoi(temp_buf);

		/* parent */
		if (!(temp_buf = strchr(temp_buf,'\t')))
			continue;
		*temp_buf++ = 0;
		order.parent = atoi(temp_buf);

		/* closed */
		if ((temp_buf = strchr(temp_buf,'\t')))
			order.closed = atoi(temp_buf+1);
		else order.closed = 0;

		folder_traverse_order_callback(&order,user_data);
	}

	free(buf);
bailout:
	fclose(fh);
}

/**
 * @brief Remove the given entry and store it into a new list.
 *
 * @param order the actual entry
 * @param user_data the list where the entries are stored.
 * @return 1
 */
static int folder_load_order_traverse_orders_callback(struct folder_order *order, void *user_data)
{
	struct list *new_order_list = (struct list*)user_data;
	struct folder *new_folder;
	struct folder_node *new_folder_node;

	int parent = order->parent;

	if (order->special == FOLDER_SPECIAL_GROUP) new_folder = folder_find_group_by_name(order->name);
	else new_folder = folder_find_by_path(order->path);

	if (new_folder)
	{
		if (parent != -1)
			new_folder->parent_folder = &((struct folder_node*)list_find(new_order_list,parent))->folder;
		new_folder_node = find_folder_node_by_folder(new_folder);
		node_remove(&new_folder_node->node);
		list_insert_tail(new_order_list,&new_folder_node->node);
		new_folder->closed = order->closed;
	}
	return 1;
}

/*****************************************************************************/

void folder_load_order(void)
{
	struct list new_order_list;
	struct folder_node *folder_node;

	list_init(&new_order_list);
	folder_traverse_order_file(folder_load_order_traverse_orders_callback,&new_order_list);

	/* Move the nodes into the main folder list again */
	while ((folder_node = (struct folder_node*)list_remove_tail(&new_order_list)))
		list_insert(&folder_list, &folder_node->node, NULL);
}

/*****************************************************************************/

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

/*****************************************************************************/

char *folder_get_possible_path(void)
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

/*****************************************************************************/

void folder_create_imap(void)
{
	struct account *ac = (struct account*)list_first(&user.config.account_list);
	while (ac)
	{
		if (ac->recv_type && ac->imap && ac->imap->name)
		{
			struct folder *f;

			folders_lock();

			/* Folder already been added? */
			if (!(f = folder_find_by_imap_account(ac)))
			{
				char buf[128];
				int tries = 0;

				mystrlcpy(buf,ac->imap->name,sizeof(buf));

				while (folder_find_by_name(buf) && tries < 20)
				{
					sm_snprintf(buf,sizeof(buf),"%s-%d",ac->imap->name,tries);
					tries++;
				}

				if (tries < 20)
				{
					if ((f = folder_add_group(buf)))
					{
						f->is_imap = 1;
						f->imap_server = mystrdup(ac->imap->name);
						f->imap_user = mystrdup(ac->imap->login);
						f->path = mycombinepath(user.folder_directory,buf);
						if (f->path)
						{
							sm_makedir(f->path);
							folder_config_save(f);
						}
					}
				}
			} else
			{
				/* The folder has been added because a directory has been found. But it
				 * might be not considered as a group so we change it manually */
				f->special = FOLDER_SPECIAL_GROUP;
				f->is_imap = 1;
			}

			/* Now look into the directory for more folders */
			if (f)
			{
				char path[512];
				struct stat *st;
				DIR *dfd;

				getcwd(path, sizeof(path));

				if ((st = (struct stat*)malloc(sizeof(struct stat))))
				{
					if ((dfd = opendir(f->path)))
					{
						struct dirent *dptr; /* dir entry */

						while ((dptr = readdir(dfd)) != NULL)
						{
							char buf[320];

							if (!strcmp(dptr->d_name,".") || !strcmp(dptr->d_name,"..")) continue;
							mystrlcpy(buf,f->path,sizeof(buf));
							sm_add_part(buf,dptr->d_name,sizeof(buf));

							if (!stat(buf,st))
							{
								if (st->st_mode & S_IFDIR)
								{
									char *imap_path;

									/* If possible determine the true imap path that is stored in the config */
									if ((imap_path = folder_config_get_imap_path(buf)))
									{
										folder_add_imap(f, imap_path);
										free(imap_path);
									} else
									{
										folder_add_imap(f, sm_file_part(buf));
									}
								}
							}
						}
						closedir(dfd);
					}
					free(st);
				}
			}

			folders_unlock();
		}
		ac = (struct account*)node_next(&ac->node);
	}
}

/*****************************************************************************/

int folder_on_same_imap_server(struct folder *f1, struct folder *f2)
{
	if (!f1->is_imap || !f2->is_imap) return 0;
	return (!mystrcmp(f1->imap_server,f2->imap_server)) && (!mystrcmp(f1->imap_user,f2->imap_user));
}

/**
 * @brief callback for the folder_traverse_order_file() call in init_folders().
 *
 * It adds the folder to the global folder list as specified by the order file.
 *
 * @param order the actual entry
 * @param user_data is ignored here.
 * @return 1
 */
static int init_folders_traverse_orders_callback(struct folder_order *order, void *user_data)
{
	struct folder *new_folder;

	if (order->special == FOLDER_SPECIAL_GROUP)
	{
		new_folder = folder_add_group(order->name);
		new_folder->closed = order->closed;
		if (*order->path)
		{
			new_folder->path = mystrdup(order->path);
			folder_config_load(new_folder);
		}
	} else
	{
		new_folder = folder_add(order->path);
	}

	if (new_folder && order->parent != -1)
	{
		new_folder->parent_folder = folder_find(order->parent);
	}
	return 1;
}

/**
 * @brief Remove non-existent folders from the internal folder list.
 *
 * @note Doesn't arbitrate the access to folders.
 */
static void folder_fix(void)
{
	struct folder *f;
	struct stat *st;

	if (!(st = (struct stat*)malloc(sizeof(struct stat))))
		return;

	f = folder_first();
	while (f)
	{
		int remove = 0;
		struct folder *next;

		next = folder_next(f);

		if (f->path && *f->path)
		{
			if (stat(f->path,st)==0)
			{
				if (!(st->st_mode & S_IFDIR))
					remove = 1;
			} else remove = 1;
		}

		if (remove)
		{
			/* TODO: Refactor this in an own function */
			struct folder_node *f_node = find_folder_node_by_folder(f);
			if (f_node)
			{
				node_remove(&f_node->node);
				folder_reparent_all(f,f->parent_folder);
				thread_dispose_semaphore(f->sem);
				free(f_node);
			}
		}

		f = next;
	}

	free(st);
}

/*****************************************************************************/

int init_folders(void)
{
	DIR *dfd;
	struct dirent *dptr; /* dir entry */
	struct stat *st;
	int write_order = 0;

	if (!(folders_semaphore = thread_create_semaphore()))
		return 0;

	if (!(folder_mail_context = mail_context_create()))
	{
		thread_dispose_semaphore(folders_semaphore);
		return 0;
	}

	list_init(&folder_list);

	folder_traverse_order_file(init_folders_traverse_orders_callback,NULL);
	folder_fix();

	if (!(st = (struct stat*)malloc(sizeof(struct stat))))
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
	} else
	{
		SM_DEBUGF(10,("Unable to open directory %s\n",user.folder_directory));
	}

	free(st);

	if (!folder_incoming())
	{
		char *new_folder = mycombinepath(user.folder_directory,"incoming");
		if (new_folder)
		{
			folder_add(new_folder);
			free(new_folder);
			write_order = 1;
		}
	}

	if (!folder_outgoing())
	{
		char *new_folder = mycombinepath(user.folder_directory,"outgoing");
		if (new_folder)
		{
			folder_add(new_folder);
			free(new_folder);
			write_order = 1;
		}
	}

	if (!folder_sent())
	{
		char *new_folder = mycombinepath(user.folder_directory,"sent");
		if (new_folder)
		{
			folder_add(new_folder);
			free(new_folder);
			write_order = 1;
		}
	}

	if (!folder_deleted())
	{
		char *new_folder = mycombinepath(user.folder_directory,"deleted");
		if (new_folder)
		{
			folder_add(new_folder);
			free(new_folder);
			write_order = 1;
		}
	}

	if (!folder_spam())
	{
		char *new_folder = mycombinepath(user.folder_directory,"spam");
		if (new_folder)
		{
			folder_add(new_folder);
			free(new_folder);
			write_order = 1;
		}
	}

	if (!folder_incoming() || !folder_outgoing() || !folder_deleted() || !folder_sent() || !folder_spam())
		return 0;

	folder_outgoing()->type = FOLDER_TYPE_SEND;
	folder_sent()->type = FOLDER_TYPE_SEND;

	folder_create_imap();

	if (write_order)
		folder_save_order();

	return 1;
}

/*****************************************************************************/

void del_folders(void)
{
	struct folder_node *node;

	folder_save_all_indexfiles();
	thread_dispose_semaphore(folders_semaphore);

	while ((node = (struct folder_node*)list_remove_tail(&folder_list)))
		folder_node_dispose(node);

	mail_context_free(folder_mail_context);
}

/*****************************************************************************/

void folder_lock(struct folder *f)
{
	if (!f) return;
	thread_lock_semaphore(f->sem);
	if (f->ref_folder) folder_lock(f->ref_folder);
}

/*****************************************************************************/

int folder_attempt_lock(struct folder *f)
{
	int attempt;

	if (!f) return 0;

	attempt = thread_attempt_lock_semaphore(f->sem);
	if (attempt && f->ref_folder)
	{
		attempt = folder_attempt_lock(f->ref_folder);
		if (!attempt) thread_unlock_semaphore(f->sem);
	}
	return attempt;
}

/*****************************************************************************/

void folder_unlock(struct folder *f)
{
	if (!f) return;
	if (f->ref_folder) folder_unlock(f->ref_folder);
	thread_unlock_semaphore(f->sem);
}

/*****************************************************************************/

void folders_lock(void)
{
	thread_lock_semaphore(folders_semaphore);
}

/*****************************************************************************/

void folders_unlock(void)
{
	thread_unlock_semaphore(folders_semaphore);
}

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
** folder.c - Folder managment
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/dir.h> /* unix dir stuff */

#include "lists.h"
#include "folder.h"
#include "support.h"

static struct list folder_list;

struct folder_node
{
	struct node node;
	struct folder folder; /* this must follow! */
};

/******************************************************************
 Adds a new mail into the given folder
*******************************************************************/
int folder_add_mail(struct folder *folder, struct mail *mail)
{
	/* free the sorted mail array */
	if (folder->sorted_mail_array)
	{
		free(folder->sorted_mail_array);
		folder->sorted_mail_array = NULL;
	}

	if (folder->mail_array_allocated == folder->num_mails)
	{
		folder->mail_array_allocated += 50;
		folder->mail_array = realloc(folder->mail_array,folder->mail_array_allocated*sizeof(struct mail*));
	}
	if (folder->mail_array)
	{
		folder->mail_array[folder->num_mails++] = mail;
		return 1;
	}
	return 0;
}

/******************************************************************
 Removes a mail from the given folder
*******************************************************************/
static void folder_remove_mail(struct folder *folder, struct mail *mail)
{
	int i;

	/* free the sorted mail array */
	if (folder->sorted_mail_array)
	{
		free(folder->sorted_mail_array);
		folder->sorted_mail_array = NULL;
	}

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
}

/******************************************************************
 Replaces a mail with a new one (the replaced mail isn't freed)
 in the given folder
*******************************************************************/
void folder_replace_mail(struct folder *folder, struct mail *toreplace, struct mail *newmail)
{
	int i;

	/* free the sorted mail array */
	if (folder->sorted_mail_array)
	{
		free(folder->sorted_mail_array);
		folder->sorted_mail_array = NULL;
	}

	for (i=0; i < folder->num_mails; i++)
	{
		if (folder->mail_array[i] == toreplace)
		{
			folder->mail_array[i] = newmail;
			break;
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
int folder_read_mail_infos(struct folder *folder)
{
	DIR *dfd; /* directory descriptor */
	struct dirent *dptr; /* dir entry */

	char path[256];

	getcwd(path, sizeof(path));
	if(chdir(folder->path) == -1) return 0;

	dfd = opendir("");
	while ((dptr = readdir(dfd)) != NULL)
	{
		struct mail *m = mail_create_from_file(dptr->d_name);
		if (m)
		{
			folder_add_mail(folder,m);
		}
	}

	closedir(dfd);

	chdir(path);
	return 1;
}

/******************************************************************
 Adds a mail to the incoming folder (the mail not actually not
 copied)
*******************************************************************/
int folder_add_mail_incoming(struct mail *mail)
{
	struct folder *folder = folder_first(); /* currently this is the incoming folder */
	if (folder)
	{
		if (folder_add_mail(folder,mail))
		{
			return 1;
		}
	}
	return 0;
}

/******************************************************************
 Adds a folder to the internal folder list
*******************************************************************/
struct folder *folder_add(char *name, char *path)
{
	struct folder_node *node = (struct folder_node*)malloc(sizeof(struct folder_node));
	if (node)
	{
		/* Initialize everything with 0 */
		memset(node,0,sizeof(struct folder_node));
		/* create the directory if it doesn't exists */
		if (sm_makedir(path))
		{
			if ((node->folder.name = strdup(name)))
			{
				if ((node->folder.path = strdup(path)))
				{
					node->folder.primary_sort = FOLDER_SORT_DATE;
					node->folder.secondary_sort = FOLDER_SORT_AUTHOR;
/*
					node->folder.mail_array = NULL;
					node->folder.mail_array_allocated = node->folder.num_mails = 0;
*/
					/* for test reasons, later this will only be done if the folder gets activated
					   (in folder_next_mail()) */
					folder_read_mail_infos(&node->folder);

					list_insert_tail(&folder_list,&node->node);
					return &node->folder;
				}
			}
		}
	}
	return NULL;
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
 Returns the incoming folder (currently always the first folder)
*******************************************************************/
struct folder *folder_incoming(void)
{
	return folder_first();
}

/******************************************************************
 Returns the outgoing folder (currently always the 2nd folder)
*******************************************************************/
struct folder *folder_outgoing(void)
{
	return folder_find(1);
}

/******************************************************************
 Returns the sent folder (currently always the 3rd folder)
*******************************************************************/
struct folder *folder_sent(void)
{
	return folder_find(2);
}

/******************************************************************
 Returns the deleleted folder (currently always the 5th folder)
*******************************************************************/
struct folder *folder_deleted(void)
{
	return folder_find(4);
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
 Move a mail from source folder to a destination folder. 0 if
 the moving has failed
*******************************************************************/
int folder_move_mail(struct folder *from_folder, struct folder *dest_folder, struct mail *mail)
{
	char *buf;

	if (from_folder == dest_folder) return 1;

	if ((buf = (char*)malloc(512)))
	{
		char *src_buf = buf;
		char *dest_buf = buf + 256;

		strcpy(src_buf,from_folder->path);
		strcpy(dest_buf,dest_folder->path);
		sm_add_part(src_buf,mail->filename,256);
		sm_add_part(dest_buf,mail->filename,256);

		folder_remove_mail(from_folder,mail);
		folder_add_mail(dest_folder,mail);

		if (!rename(src_buf,dest_buf)) return 1;
	}
	return 0;
}

/******************************************************************
 Deletes a mail (move it to the delete drawer)
*******************************************************************/
int folder_delete_mail(struct folder *from_folder, struct mail *mail)
{
	struct folder *dest_folder = folder_deleted();
	if (dest_folder)
	{
		return folder_move_mail(from_folder,dest_folder,mail);
	}
	return 0;
}

/******************************************************************
 Really delete all mails in the delete folder
*******************************************************************/
void folder_delete_deleted(void)
{
	int i;
	struct folder *folder = folder_deleted();
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

	if (folder->mail_array)
	{
		free(folder->mail_array);
		folder->mail_array = NULL;
	}

	chdir(path);
}

/* to control the compare functions */
static int compare_primary_reverse;
static int (*compare_primary)(const struct mail *arg1, const struct mail *arg2, int reverse);
static int compare_secondary_reverse;
static int (*compare_secondary)(const struct mail *arg1, const struct mail *arg2, int reverse);

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
	return 0;
}

static int mail_compare_author(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	return mystricmp(arg1->author,arg2->author);
}

static int mail_compare_subject(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	return mystricmp(arg1->subject,arg2->subject);
}

static int mail_compare_reply(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	return 0;
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

/******************************************************************
 Returns the correct sorting function and fills the reverse pointer
*******************************************************************/
static void *get_compare_function(int sort_mode, int *reverse)
{
	if (sort_mode & FOLDER_SORT_REVERSE) *reverse = 1;
	else reverse = 0;

	switch (sort_mode & FOLDER_SORT_MODEMASK)
	{
		case	FOLDER_SORT_STATUS: return mail_compare_status;
		case	FOLDER_SORT_AUTHOR: return mail_compare_author;
		case	FOLDER_SORT_SUBJECT: return mail_compare_subject;
		case	FOLDER_SORT_REPLY: return mail_compare_reply;
		case	FOLDER_SORT_DATE: return mail_compare_date;
		case	FOLDER_SORT_SIZE: return mail_compare_size;
		case	FOLDER_SORT_FILENAME: return mail_compare_filename;
	}
	return NULL; /* thread */
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
	struct mail **mail_array = folder->mail_array;
	if (!folder->sorted_mail_array && *((int**)handle) == 0 && folder->num_mails)
	{
		/* the array is not sorted, so sort it now */
		folder->sorted_mail_array = (struct mail**)malloc(sizeof(struct mail*)*folder->num_mails);
		if (folder->sorted_mail_array)
		{
			/* copy the mail pointers into the buffer */
			memcpy(folder->sorted_mail_array, folder->mail_array, sizeof(struct mail*)*folder->num_mails);

			/* set the correct search function */
			compare_primary = (int (*)(const struct mail *, const struct mail *, int))get_compare_function(folder->primary_sort, &compare_primary_reverse);
			compare_secondary = (int (*)(const struct mail *, const struct mail *, int))get_compare_function(folder->secondary_sort, &compare_secondary_reverse);

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
 Initializes the default folders
*******************************************************************/
int init_folders(void)
{
	list_init(&folder_list);
	if (folder_add("Incoming","PROGDIR:.folders/income"))
	{
		if (folder_add("Outgoing","PROGDIR:.folders/outgoing"))
		{
			if (folder_add("Sent","PROGDIR:.folders/sent"))
			{
				if (folder_add("Archived","PROGDIR:.folders/archived"))
				{
					if (folder_add("Deleted", "PROGDIR:.folders/deleted"))
					{
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

/******************************************************************
 Removes all folders (before quit)
*******************************************************************/
void del_folders(void)
{
}

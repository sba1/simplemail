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
#include <sys/dir.h> /* unix dir stuff */
#include <sys/stat.h> /* state() */
#include <unistd.h>

#include "lists.h"
#include "folder.h"
#include "support.h"
#include "support_indep.h"

static struct list folder_list;

struct folder_node
{
	struct node node;
	struct folder folder; /* this must follow! */
};

static char *fread_str(FILE *fh);

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
void folder_delete_indexfile(struct folder *f)
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
 Adds a new mail into the given folder
*******************************************************************/
int folder_add_mail(struct folder *folder, struct mail *mail)
{
	int i;

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

	if (folder->mail_array_allocated == folder->num_mails)
	{
		folder->mail_array_allocated += 50;
		folder->mail_array = realloc(folder->mail_array,folder->mail_array_allocated*sizeof(struct mail*));
	}

	if (!folder->mail_array)
	{
		folder->mail_array_allocated = 0;
		return 0;
	}

	folder->mail_array[folder->num_mails++] = mail;
	if (mail_get_status_type(mail) == MAIL_STATUS_UNREAD) folder->unread_mails++;
	if (mail->flags & MAIL_FLAGS_NEW) folder->new_mails++;


	if (mail->message_id)
	{
		/* check if there is already an mail with the same message id, this would cause
  	   problems */
		for (i=0;i<folder->num_mails;i++)
		{
			struct mail *fm = folder->mail_array[i];
			if (!(mystricmp(mail->message_id,fm->message_id)))
			{
				free(mail->message_id);
				mail->message_id = NULL;
			}
		}
	}

	/* sort the mails for threads */
	if (mail->message_id)
	{
		for (i=0;i<folder->num_mails;i++)
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
		for (i=0;i<folder->num_mails;i++)
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

	return 1;
}

/******************************************************************
 Removes a mail from the given folder.
 (does not free it)
*******************************************************************/
static void folder_remove_mail(struct folder *folder, struct mail *mail)
{
	int i;
	struct mail *submail;

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

	if ((mail_get_status_type(mail) == MAIL_STATUS_UNREAD) && folder->unread_mails) folder->unread_mails--;
	if ((mail->flags & MAIL_FLAGS_NEW) && folder->new_mails) folder->new_mails--;
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

			if (status_new == mail->status) return;

			/* update the mail statistics */
			if (mail_get_status_type(mail) == MAIL_STATUS_UNREAD && folder->unread_mails) folder->unread_mails--;
			if ((status_new & MAIL_STATUS_MASK) == MAIL_STATUS_UNREAD) folder->unread_mails++;

			mail->status = status_new;
			if (!mail->filename) return;

			filename = mail_get_status_filename(mail->filename, status_new);

			if (strcmp(mail->filename,filename))
			{
				char buf[256];

				getcwd(buf, sizeof(buf));
				chdir(folder->path);

				rename(mail->filename,filename);
				free(mail->filename);
				mail->filename = filename;

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
int folder_read_mail_infos(struct folder *folder)
{
  int mail_infos_read = 0;

	FILE *fh = folder_open_indexfile(folder,"rb");
	if (fh)
	{
		char buf[4];
		fread(buf,1,4,fh);
		if (!strncmp("SMFI",buf,4))
		{
			int ver;
			fread(&ver,1,4,fh);
			if (ver == 1)
			{
				int num_mails;
				fread(&num_mails,1,4,fh);
				mail_infos_read = 1;
				while (num_mails--)
				{
					struct mail *m = mail_create();
					if (m)
					{
						char *buf;
						if ((buf = fread_str(fh)))
						{
							mail_add_header(m,"Subject",7,buf,strlen(buf));
							free(buf);
						}

						m->filename = fread_str(fh);

						if ((buf = fread_str(fh)))
						{
							mail_add_header(m,"From",4,buf,strlen(buf));
							free(buf);
						}

						if ((buf = fread_str(fh)))
						{
							mail_add_header(m,"To",2,buf,strlen(buf));
							free(buf);
						}

						if ((buf = fread_str(fh)))
						{
							mail_add_header(m,"Reply-To",8,buf,strlen(buf));
							free(buf);
						}

						fseek(fh,ftell(fh)%2,SEEK_CUR);
						fread(&m->size,1,sizeof(m->size),fh);
						fread(&m->seconds,1,sizeof(m->seconds),fh);
						fread(&m->flags,1,sizeof(m->flags),fh);
						mail_identify_status(m);
						mail_process_headers(m);
						folder_add_mail(folder,m);

						m->flags &= ~MAIL_FLAGS_NEW;
					}
				}
			}
		}
		fclose(fh);
	}

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
			while ((dptr = readdir(dfd)) != NULL)
			{
				struct mail *m;

				if (!strcmp(".",dptr->d_name) || !strcmp("..",dptr->d_name)) continue;

				if ((m = mail_create_from_file(dptr->d_name)))
				{
					folder_add_mail(folder,m);
				}
			}
			closedir(dfd);
		} else printf("%s\n",strerror(errno));

		chdir(path);
	}
	return 1;
}

/******************************************************************
 Adds a mail to the incoming folder (the mail not actually not
 copied). The mail will get a New Flag
*******************************************************************/
int folder_add_mail_incoming(struct mail *mail)
{
	struct folder *folder = folder_incoming(); /* currently this is the incoming folder */
	if (folder)
	{
		mail->flags |= MAIL_FLAGS_NEW;
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
static struct folder *folder_add(char *path)
{
	struct folder_node *node = (struct folder_node*)malloc(sizeof(struct folder_node));
	if (node)
	{
		/* Initialize everything with 0 */
		memset(node,0,sizeof(struct folder_node));
		/* create the directory if it doesn't exists */
		if (sm_makedir(path))
		{
			if ((node->folder.path = mystrdup(path)))
			{
				FILE *fh;
				char buf[256];

				node->folder.primary_sort = FOLDER_SORT_DATE;
				node->folder.secondary_sort = FOLDER_SORT_FROMTO;

				sprintf(buf,"%s.config",path);

				if ((fh = fopen(buf,"r")))
				{
					fclose(fh);
				} else
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
				}

				/* for test reasons, later this will only be done if the folder gets activated
				   (in folder_next_mail()) */
				folder_read_mail_infos(&node->folder);

				list_insert_tail(&folder_list,&node->node);
				return &node->folder;
			}
		}
	}
	return NULL;
}

/******************************************************************
 Test if the setting the foldersetting would require a reload
 (the mails would get disposed and reloaded)
*******************************************************************/
int folder_set_would_need_reload(struct folder *f, char *newname, char *newpath, int newtype)
{
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
}

/******************************************************************
 Set some folder attributes. Returns 1 if the folder must be
 refreshed in the gui.
*******************************************************************/
int folder_set(struct folder *f, char *newname, char *newpath, int newtype)
{
	int refresh = 0;
	int rescan = 0;
	if (newname && strcmp(f->name,newname))
	{
		if ((newname = mystrdup(newname)))
		{
			free(f->name);
			f->name = newname;
		}
	}

	if (newpath && strcmp(f->path,newpath))
	{
		if ((newpath = mystrdup(newpath)))
		{
			refresh = !!mystricmp(newpath,f->path);

			if (f->index_uptodate && refresh)
			{
				folder_delete_indexfile(f);
				f->index_uptodate = 0;
			}

			free(f->path);
			f->path = newpath;
			rescan = refresh;
		}
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
	}

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

		folder_read_mail_infos(f);
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
 Finds from a folder (which is given by its path) the mail's
 successor (which is given by the filename)
*******************************************************************/
struct mail *folder_find_next_mail_by_filename(char *folder_path, char *mail_filename)
{
	void *handle = NULL;
	struct folder *f = folder_find_by_path(folder_path);
	struct mail *m;
	int i = 0;

	while ((m = folder_next_mail(f, &handle)))
	{
		if (!mystricmp(m->filename,mail_filename))
		{
			return folder_next_mail(f,&handle);
		}
	}

	if (!f) return NULL;
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
	int i = 0;

	while ((m = folder_next_mail(f, &handle)))
	{
		if (!mystricmp(m->filename,mail_filename))
		{
			return lm;
		}
		lm = m;
	}

	if (!f) return NULL;
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

	if (from_folder == dest_folder) return 1;

	if ((buf = (char*)malloc(512)))
	{
		char *src_buf = buf;
		char *dest_buf = buf + 256;

		strcpy(src_buf,from_folder->path);
		strcpy(dest_buf,dest_folder->path);
		sm_add_part(src_buf,mail->filename,256);

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
		sm_add_part(dest_buf,mail->filename,256);

		folder_remove_mail(from_folder,mail);
		folder_add_mail(dest_folder,mail);

		if (!rename(src_buf,dest_buf)) return 1;
		free(buf);
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
	folder->mail_array_allocated = 0;

	if (folder->mail_array)
	{
		free(folder->mail_array);
		folder->mail_array = NULL;
	}

	chdir(path);
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
 Saved the index of an folder
*******************************************************************/
int folder_save_index(struct folder *f)
{
	FILE *fh = folder_open_indexfile(f,"wb");

	if (fh)
	{
		int i;
		int ver = 1;
		fwrite("SMFI",1,4,fh);
		fwrite(&ver,1,4,fh);
		fwrite(&f->num_mails,1,4,fh);

		for (i=0; i < f->num_mails; i++)
		{
			char *from = mail_find_header_contents(f->mail_array[i],"from");
			char *to = mail_find_header_contents(f->mail_array[i],"to");
			char *reply_to = mail_find_header_contents(f->mail_array[i],"reply-to");
			int len = 0;
			int len_add;

			if (!(len_add = fwrite_str(fh, f->mail_array[i]->subject))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, f->mail_array[i]->filename))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, from))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, to))) break;
			len += len_add;
			if (!(len_add = fwrite_str(fh, reply_to))) break;
			len += len_add;

			/* so that integervars are aligned */
			if (len % 2) fputc(0,fh);

			fwrite(&f->mail_array[i]->size,1,sizeof(f->mail_array[i]->size),fh);
			fwrite(&f->mail_array[i]->seconds,1,sizeof(f->mail_array[i]->seconds),fh);
			fwrite(&f->mail_array[i]->flags,1,sizeof(f->mail_array[i]->flags),fh);
		}
		fclose(fh);
	}

	return 1;
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

static int mail_compare_from(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	int rc = mystricmp(arg1->from,arg2->from);
	if (reverse) rc *= -1;
	return rc;
}

static int mail_compare_to(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	int rc = mystricmp(arg1->to,arg2->to);
	if (reverse) rc *= -1;
	return rc;
}

static int mail_compare_subject(const struct mail *arg1, const struct mail *arg2, int reverse)
{
	int rc = mystricmp(arg1->subject,arg2->subject);
	if (reverse) rc *= -1;
	return rc;
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
			compare_primary = (int (*)(const struct mail *, const struct mail *, int))get_compare_function(folder->primary_sort, &compare_primary_reverse, folder->type);
			compare_secondary = (int (*)(const struct mail *, const struct mail *, int))get_compare_function(folder->secondary_sort, &compare_secondary_reverse, folder->type);

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
/*
	stat()

	if (st.st_mode & S_IFDIR)
	{
	}
*/

#ifdef _AMIGA
	#define FOLDER_PATH "PROGDIR:.folders"
	char *folder_path = "PROGDIR:.folders";
#else
	#define FOLDER_PATH ".folders"
	char *folder_path = ".folders";
#endif
	DIR *dfd;
	struct dirent *dptr; /* dir entry */
	struct stat *st;

	list_init(&folder_list);

	if (!(st = malloc(sizeof(struct stat))))
		return 0;

	if ((dfd = opendir(folder_path)))
	{
		while ((dptr = readdir(dfd)) != NULL)
		{
			char buf[256];
			if (!strcmp(dptr->d_name,".") || !strcmp(dptr->d_name,"..")) continue;
			strcpy(buf,folder_path);
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
		folder_add(FOLDER_PATH "/incoming");

	if (!folder_outgoing())
		folder_add(FOLDER_PATH "/outgoing");

	if (!folder_sent())
		folder_add(FOLDER_PATH "/sent");

	if (!folder_deleted())
		folder_add(FOLDER_PATH "/deleted");

	if (!folder_incoming() || !folder_outgoing() || !folder_deleted() || !folder_sent())
		return 0;

	folder_outgoing()->type = FOLDER_TYPE_SEND;
	folder_sent()->type = FOLDER_TYPE_SEND;

	return 1;
}

/******************************************************************
 Removes all folders (before quit)
*******************************************************************/
void del_folders(void)
{
	struct folder *f = folder_first();
	while (f)
	{
		if (!f->index_uptodate) folder_save_index(f);
		f = folder_next(f);
	}
}


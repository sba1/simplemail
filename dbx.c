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
 * @file dbx.c
 *
 * Import and export functions for the dbx (Outlook Express) file format
 * The file format description is available from http://oedbx.aroh.de/
 */

#include "dbx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "folder.h"
#include "mail.h"
#include "simplemail.h"
#include "smintl.h"
#include "status.h"
#include "support_indep.h"

#include "subthreads.h"
#include "support.h"

/*****************************************************************************/

#define GetLong(ptr,off) ( (((unsigned char*)(ptr))[off]) | (((unsigned char*)(ptr))[off+1] << 8) | (((unsigned char*)(ptr))[off+2] << 16) | (((unsigned char*)(ptr))[off+3] << 24))

#define GetShort(ptr,off) ((((unsigned char*)(ptr))[off]) | (((unsigned char*)(ptr))[off+1] << 8))

/*****************************************************************************/

#define TREESIZE (0x18 + 0x264) /* header + body */

/*****************************************************************************/

/**
 * Read the message into out file
 */
static int dbx_read_message(FILE *fh, FILE *out, unsigned int addr)
{
	/* This can be static as this function is the leave of the recursion */
	static unsigned char buf[0x210];
	size_t size;

	while (addr)
	{
		if (fseek(fh,addr,SEEK_SET))
			return 0;

		if (fread(buf,1,sizeof(buf),fh) != sizeof(buf))
			return 0;

		size = GetLong(buf,8);
		addr = GetLong(buf,12);

		if (thread_aborted())
		{
			SM_DEBUGF(5,("User abort\n"));
			return 0;
		}

		if (fwrite(&buf[16],1,size,out) != size)
			return 0;
	}
	return 1;
}

/**
 * Read a indexed info message (containg the message)
 */
static int dbx_read_indexed_info(FILE *fh, unsigned int addr, unsigned int size)
{
	unsigned char *buf;
	char *mailfilename = NULL;
	FILE *mailout = NULL;

	unsigned char *data;
	unsigned char *body;
	int i;
	int rc = 0;
	int length_of_idxs;
	unsigned int num_of_idxs;
	unsigned int object_marker;
	unsigned char *entries[32];

	unsigned char *msg_entry;
	unsigned int message;

	int new_mail_status = MAIL_STATUS_UNREAD;

	if (size < 12) size = 12;

	if (!(buf = (unsigned char*)malloc(size)))
	{
		SM_DEBUGF(5,("Couldn't allocate %d bytes\n",size));
		return 0;
	}

	if (fseek(fh,addr,SEEK_SET))
	{
		SM_DEBUGF(5,("Unable to seek at %x\n",addr));
		goto out;
	}

	if (fread(buf,1,size,fh) != size)
	{
		SM_DEBUGF(5,("Unable to read %d bytes\n",size));
		goto out;
	}

	object_marker = GetLong(buf,0);
	if (object_marker != addr)
	{
		SM_DEBUGF(5,("Object marker didn't match\n"));
		goto out;
	}

	length_of_idxs = GetLong(buf, 4);
	num_of_idxs = buf[10];

	if (num_of_idxs > sizeof(entries)/sizeof(entries[0]))
	{
		SM_DEBUGF(5,("Too many indexes\n"));
		goto out;
	}

	/* Check if we have read enough data, if not we must read more */
	if (size - 12 < length_of_idxs)
	{
		unsigned char *newbuf;

		SM_DEBUGF(5,("read in %d bytes of object at 0x%x, but index length length is %d\n",size,addr,length_of_idxs));
		
		if (!(newbuf = (unsigned char*)malloc(length_of_idxs + 12)))
			goto out;

		memcpy(newbuf,buf,size);
		if (fread(&newbuf[size],1,length_of_idxs - size,fh) != length_of_idxs - size)
		{
			SM_DEBUGF(5,("Couldn't load more bytes\n"));
			free(newbuf);
			goto out;
		}

		free(buf);
		buf = newbuf;
	}
	

	body = buf + 12;
	data = body + num_of_idxs * 4;

	memset(entries,0,sizeof(entries));

	for (i=0;i<num_of_idxs;i++)
	{
		unsigned int idx = body[0];
		unsigned int addr = body[1] | (body[2] << 8) | (body[3] << 16);

		if ((idx & 0x7f) > sizeof(entries)/sizeof(entries[0]))
		{
			SM_DEBUGF(5,("Wrong index\n"));
			goto out;
		}

		if (idx & 0x80)
		{
			/* overwrite the body (enforce little endian) */
			body[0] = addr & 0xff;
			body[1] = (addr & 0xff00) >> 8;
			body[2] = (addr & 0xff0000) >> 16;
			body[3] = 0;

			/* is direct valiue */
			entries[idx & 0x7f] = body;
		} else
		{
			entries[idx] = data + addr;
		}
		body += 4;
	}

	/* Index number 1 points to flags */
	if (entries[1])
	{
		unsigned int flags = GetLong(entries[1],0);
		if (flags & (1UL << 19)) new_mail_status = MAIL_STATUS_REPLIED;
		else if (flags & (1UL << 7)) new_mail_status = MAIL_STATUS_READ;
	}

	/* Index number 4 points to the whole message */
	if (!(msg_entry = entries[4]))
	{
		SM_DEBUGF(5,("Did not find a messagen",size));	
		goto out;
	}

	/* extract the address */
	message = GetLong(msg_entry,0);

	/* Get mail name */
	if (!(mailfilename = mail_get_new_name(new_mail_status)))
	{
		SM_DEBUGF(5,("Couldn't get mail filename\n"));
		goto out;
	}

	/* Open mail file */
	if (!(mailout = fopen(mailfilename,"wb")))
	{
		SM_DEBUGF(5,("Couldn't open %s for output\n",mailfilename));
		goto out;
	}

	/* Write the message into the out file */
	if (!dbx_read_message(fh,mailout,message))
	{
		SM_DEBUGF(5,("Couldn't read dbx message\n"));
		fclose(mailout); mailout = NULL;
		remove(mailfilename);
		goto out;
	}

	thread_call_parent_function_async_string(callback_new_mail_arrived_filename, 2, mailfilename, 0);

	rc = 1;
out:
	if (mailout) fclose(mailout);
	free(mailfilename);
	free(buf);
	return rc;
}

/**
 * Read recursively a node within the tree. Every node can already contain
 * a value (message)
 */
static int dbx_read_node(FILE *fh, unsigned int addr, int *mail_accu)
{
	unsigned char *buf;
	unsigned char *body;
	unsigned int child;
	int entries;

	if (!(buf = (unsigned char*)malloc(TREESIZE))) return 0;

	if (fseek(fh,addr,SEEK_SET))
	{
		free(buf);
		return 0;
	}

	if (fread(buf,1,TREESIZE,fh) != TREESIZE)
	{
		free(buf);
		return 0;
	}

	/* object_marker = GetLong(buf,0); */
	child = GetLong(buf,8);
	entries = buf[17];

	body = &buf[0x18];

	while (entries--)
	{
		unsigned int value = GetLong(body,0);
		unsigned int chld = GetLong(body,4);
		unsigned int novals = GetLong(body,8);

		/* value points to a pointer to a message */
		if (value)
		{
			thread_call_function_async(thread_get_main(), status_set_mail, 2, *mail_accu, 1);
			thread_call_function_async(thread_get_main(), status_set_gauge,1, *mail_accu);

			*mail_accu = (*mail_accu) + 1;

			if (!(dbx_read_indexed_info(fh,value,novals)))
			{
				SM_DEBUGF(5,("Failed to read the indexed info\n"));
				free(buf);
				return 0;
			}
		}

		if (chld)
		{
			if (!(dbx_read_node(fh,chld,mail_accu)))
			{
				SM_DEBUGF(5,("Failed to read node at %p\n",chld));
				free(buf);
				return 0;
			}
		} 

		body += 12;
	}

	free(buf);

	if (child)
		return dbx_read_node(fh,child,mail_accu);

	return 1;
}

/***************************************************************************/

struct import_data
{
	char *filename;
	char *destdir;
	int in_folder;
};

/**
 * Entry point for the import subthread.
 *
 * @param data defines data of the process.
 * @return
 */
static int dbx_import_entry(struct import_data *data)
{
	char *filename;
	char *destdir;
	unsigned char *fileheader_buf;
	char head_buf[300];
	char path[380];
	int fsize;
	FILE *fh;

	if ((filename = mystrdup(data->filename)))
	{
		if ((destdir = mystrdup(data->destdir)))
		{
			struct folder *dest_folder;

			if (data->in_folder) dest_folder = folder_find_by_path(destdir);
			else dest_folder = NULL;

			if (dest_folder)
			{
				sm_snprintf(head_buf, sizeof(head_buf), _("Importing %s to %s"),filename,dest_folder->name);
			} else
			{
				sm_snprintf(head_buf, sizeof(head_buf), _("Importing %s"),filename);
			}

			if (thread_parent_task_can_contiue())
			{
				thread_call_function_async(thread_get_main(),status_init,1,0);
				thread_call_parent_function_async_string(status_set_title,1,_("SimpleMail - Importing a dbx file"));
				thread_call_parent_function_async_string(status_set_head,1,head_buf);
				thread_call_function_async(thread_get_main(),status_open,0);

				if ((fh = fopen(filename,"rb")))
				{
					getcwd(path,sizeof(path));
					if (chdir(destdir)!=-1)
					{
						fsize = myfsize(fh);
						thread_call_function_async(thread_get_main(),status_init_gauge_as_bytes,1,fsize);

						if ((fileheader_buf = (unsigned char*)malloc(0x24bc)))
						{
							if (fread(fileheader_buf,1,0x24bc,fh) == 0x24bc)
							{
								if ((fileheader_buf[0] == 0xcf && fileheader_buf[1] == 0xad &&
									 fileheader_buf[2] == 0x12 && fileheader_buf[3] == 0xfe) &&
									(fileheader_buf[4] == 0xc5 && fileheader_buf[5] == 0xfd &&
									 fileheader_buf[6] == 0x74 && fileheader_buf[7] == 0x6f))
								{
									int number_of_mails = GetLong(fileheader_buf,0xc4);
									int read_mails = 0;
									unsigned int root_node = GetLong(fileheader_buf,0x00e4);

									thread_call_function_async(thread_get_main(),status_init_mail, 1, number_of_mails);
									thread_call_function_async(thread_get_main(),status_init_gauge_as_bytes,1,number_of_mails); /* not really true */

									/* Start the the roor of the tree */
									dbx_read_node(fh, root_node, &read_mails);
								} else SM_DEBUGF(5,("File is not a dbx message file (header is %02x%02x%02x%02x %02x%02x%02x%02x)\n",fileheader_buf[0],fileheader_buf[1],fileheader_buf[2],fileheader_buf[3],fileheader_buf[4],fileheader_buf[5],fileheader_buf[6],fileheader_buf[7]));
							} else SM_DEBUGF(5,("Unable to read the file\n"));

							free(fileheader_buf);
						}
						chdir(path);
					}
					fclose(fh);
				}
				thread_call_function_async(thread_get_main(),status_close,0);
			}
			free(destdir);
		}
		free(filename);
	}
	return 0;
}

/*****************************************************************************/

int dbx_import_to_folder(struct folder *folder, char *filename)
{
	struct import_data data;

	data.filename = filename;
	data.destdir = folder?folder->path:folder_incoming()->path;
	data.in_folder = folder?1:0;

	return thread_start(THREAD_FUNCTION(dbx_import_entry),&data);
}


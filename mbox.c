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
** mbox.c - import and export functions for the mbox fileformat
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "debug.h"
#include "folder.h"
#include "mail.h"
#include "mbox.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"
#include "status.h"

#include "subthreads.h"
#include "support.h"

/**************************************************************************
 Checks wheather given string can be considered as any from line.
 Buf may not be NULL
**************************************************************************/
static int line_is_any_from(char *buf)
{
	unsigned char c;
	while ((c=*buf))
	{
		if (c != '>') break;
		buf++;
	}
	if (!c) return 0;
	if (!strncmp(buf,"From ",5)) return 1;
	return 0;
}

/*************************************************************************/

struct export_data
{
	char *filename;
	char *foldername;
};

/**************************************************************************
 Entry point for the export subthread
**************************************************************************/
static int export_entry(struct export_data *data)
{
	char *filename;
	char *foldername;

	if ((filename = mystrdup(data->filename)))
	{
		if ((foldername = mystrdup(data->foldername)))
		{
			if (thread_parent_task_can_contiue())
			{
				struct folder *f;

				/* lock folder list */
				folders_lock();
				f = folder_find_by_name(foldername);
				if (f)
				{
					FILE *fh;
					char head_buf[256];

					/* now lock the folder */
					folder_lock(f);
					/* unlock the folder list */
					folders_unlock();

					sm_snprintf(head_buf, sizeof(head_buf), _("Exporting folder %s to %s"),f->name,filename);
					thread_call_parent_function_async(status_init,1,0);
					thread_call_parent_function_async_string(status_set_title,1,_("SimpleMail - Exporting folder"));
					thread_call_parent_function_async_string(status_set_head,1,head_buf);
					thread_call_parent_function_async(status_open,0);

					if ((fh = fopen(filename,"w")))
					{
						void *handle = NULL;
						char buf[384];
						struct mail_info *m;
						char *file_buf;
						int max_size = 0;
						int size = 0;
						int mail_no = 1;

						while ((m = folder_next_mail(f, &handle)))
							max_size += m->size;

						thread_call_parent_function_async(status_init_gauge_as_bytes,1,max_size);
						thread_call_parent_function_async(status_init_mail, 1, f->num_mails);

						if ((file_buf = malloc(8192)))
						{
							char date_buf[128];

							getcwd(buf, sizeof(buf));
							chdir(f->path);

							handle = NULL;
							while ((m = folder_next_mail(f, &handle)))
							{
								FILE *in;

								static const char *mon_str[] = {"Jan","Feb","Mar","Apr","May","Jun", "Jul","Aug","Sep","Oct","Nov","Dec"};
								static const char *week_str[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
								struct tm tm;

								thread_call_parent_function_async(status_set_mail, 2, mail_no, m->size);

								sm_convert_seconds(m->received,&tm);
								sm_snprintf(date_buf,sizeof(date_buf),"%s %s %02d %02d:%02d:%02d %4d",week_str[tm.tm_wday],mon_str[tm.tm_mon-1],tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec,tm.tm_year+1900);

								fprintf(fh, "From %s %s\n",m->from_addr?m->from_addr:"",date_buf);

								in = fopen(m->filename,"r");
								if (in)
								{
									while (fgets(file_buf,8192,in))
									{
										if (line_is_any_from(file_buf)) fwrite(">",1,1,fh);
										size += fwrite(file_buf,1,strlen(file_buf),fh);
										thread_call_parent_function_async(status_set_gauge,1,size);
									}
									fclose(in);
								}
								fputs("\n",fh);
								mail_no++;
							}
							free(file_buf);
						}

						chdir(buf);

						fclose(fh);
					}
					thread_call_parent_function_async(status_close,0);

					folder_unlock(f);
				} else folders_unlock();
			}
			free(foldername);
		}
		free(filename);
	}
	return 0;
}

/**************************************************************************
 Export a given folder as a mbox file to the given filename
**************************************************************************/
int mbox_export_folder(struct folder *folder, char *filename)
{
	struct export_data data;

	data.filename = filename;
	data.foldername = folder->name;

	return thread_start(THREAD_FUNCTION(export_entry),&data);
}

/*************************************************************************/

struct import_data
{
	char *filename;
	char *destdir;
	int in_folder;
};

/**************************************************************************
 Entry point for the import subthread
**************************************************************************/
static int import_entry(struct import_data *data)
{
	char *filename,*mailfilename;
	char *destdir;
	char *line_buf;
	char head_buf[300];
	char path[380];
	int gauge,fsize,line_len,empty_line;
	FILE *fh,*mailfh;

	if ((filename = mystrdup(data->filename)))
	{
		if ((destdir = mystrdup(data->destdir)))
		{
			struct folder *dest_folder;
			int in_folder;

			in_folder = data->in_folder;
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
				thread_call_parent_function_async(status_init,1,0);
				thread_call_parent_function_async_string(status_set_title,1,_("SimpleMail - Importing a mbox file"));
				thread_call_parent_function_async_string(status_set_head,1,head_buf);
				thread_call_parent_function_async(status_open,0);

				if ((fh = fopen(filename,"r")))
				{
					getcwd(path,sizeof(path));
					if (chdir(destdir)!=-1)
					{
						fsize = myfsize(fh);
						thread_call_parent_function_async(status_init_gauge_as_bytes,1,fsize);

						if ((line_buf = malloc(8192)))
						{
							mailfilename = NULL;
							mailfh = NULL;
							gauge = empty_line = 0;

							while (fgets(line_buf,8192,fh))
							{
								line_len = strlen(line_buf);
								gauge += line_len;

								if (line_buf[0] == '\n' || (line_buf[0] == '\r' || line_buf[1] == '\n'))
								{
									if (empty_line && mailfh) fputs("\n",mailfh);
									empty_line = 1;
									continue;
								}

								if (!strncmp(line_buf, "From ",5))
								{
									if (mailfh)
									{
										fclose(mailfh);
										if (in_folder)
										{
											thread_call_parent_function_async_string(callback_new_mail_to_folder_by_file, 1, mailfilename);
										} else
										{
											thread_call_parent_function_async_string(callback_new_mail_arrived_filename, 2, mailfilename, 0);
										}
									}

									if (!(mailfilename = mail_get_new_name(MAIL_STATUS_UNREAD))) break;
									if (!(mailfh = fopen(mailfilename,"w"))) break;
									empty_line = 0;
									continue;
								}

								if (empty_line)
								{
									empty_line = 0;
									fputs("\n",mailfh);
								}
								fputs(line_buf + line_is_any_from(line_buf),mailfh);
								thread_call_parent_function_async(status_set_gauge,1,gauge);
							}

							if (mailfh)
							{
								if (empty_line) fputs("\n",mailfh);
								fclose(mailfh);
								if (in_folder)
								{
									thread_call_parent_function_async_string(callback_new_mail_to_folder_by_file, 1, mailfilename);
								} else
								{
									thread_call_parent_function_async_string(callback_new_mail_arrived_filename, 2, mailfilename, 0);
								}
							}

							free(line_buf);
						}
						chdir(path);
					}
					fclose(fh);
				}
				thread_call_parent_function_async(status_close,0);
			}
			free(destdir);
		}
		free(filename);
	}
	return 0;
}

/**************************************************************************
 Export a given a given file which must be a mbox file to a given folder.
 folder may be NULL which means that the mails are imported like fetching
 mails.
**************************************************************************/
int mbox_import_to_folder(struct folder *folder, char *filename)
{
	struct import_data data;

	data.filename = filename;
	data.destdir = folder?folder->path:folder_incoming()->path;
	data.in_folder = folder?1:0;

	return thread_start(THREAD_FUNCTION(import_entry),&data);
}


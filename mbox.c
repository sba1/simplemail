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

#include "folder.h"
#include "mail.h"
#include "mbox.h"
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
					char head_buf[300];

					/* now lock the folder */
					folder_lock(f);
					/* unlock the folder list */
					folders_unlock();

					sprintf(head_buf, _("Exporting folder %s to %s"),f->name,filename);
					thread_call_parent_function_async(status_init,1,0);
					thread_call_parent_function_async_string(status_set_title,1,_("SimpleMail - Exporting folder"));
					thread_call_parent_function_async_string(status_set_head,1,head_buf);
					thread_call_parent_function_async(status_open,0);

					if ((fh = fopen(filename,"w")))
					{
						void *handle = NULL;
						char buf[384];
						struct mail *m;
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
								sprintf(date_buf,"%s %s %02d %02d:%02d:%02d %4d",week_str[tm.tm_wday],mon_str[tm.tm_mon-1],tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec,tm.tm_year+1900);

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


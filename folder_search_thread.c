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

#include <stdlib.h>
#include <unistd.h>

#include "filter.h"
#include "folder.h"
#include "mail.h"
#include "support_indep.h"

#include "searchwnd.h"
#include "subthreads.h"
#include "support.h"
#include "timesupport.h"

/*****************************************************************************/

/**
 * The thread that is used for searching.
 */
static thread_t search_thread;

/*****************************************************************************/

/**
 * Clean the search thread pointer. Called from the search thread.
 */
static void folder_search_clean_thread(void)
{
	search_thread = NULL;
}

/**
 * @file
 */

struct search_msg
{
	int f_array_len;
	struct folder **f_array;
	struct search_options *sopt;
};

/**
 * Entry for the folder search thread
 *
 * @param msg the parameters
 */
static void folder_start_search_entry(struct search_msg *msg)
{
	struct folder **f_array = NULL;
	int f_array_len;
	struct filter *filter = NULL;
	struct search_options *sopt;
#define NUM_FOUND 100
	struct mail_info *found_array[NUM_FOUND];

	/* Used to reduce the amount of notifications sent to the parent task */
	unsigned int secs = sm_get_current_seconds();

	sopt = search_options_duplicate(msg->sopt);
	f_array_len = msg->f_array_len;
	if ((f_array = (struct folder**)malloc((f_array_len+1)*sizeof(struct folder*))))
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

		if (!(filter = filter_create_from_search_options(sopt)))
			goto fail;

		/* folder_apply_filter(f,filter); */ /* Not safe currently to be called from subthreads */
		{
			int folder_num = 0;
			int found_num = 0;
			struct mail_info *m;
			struct folder *f;
			char path[512];

			getcwd(path, sizeof(path));

			thread_call_parent_function_sync(NULL,search_enable_search, 0);

			while ((f = f_array[folder_num++]))
			{
				int i;

				if (chdir(f->path) == -1) break;

				for (i=0;i<f->num_mails;i++)
				{
					if (!(m = f->mail_info_array[i]))
						break;

					/* mail_matches_filter() is thread safe as long as no header search is used, like here */
					if (mail_matches_filter(f,m,filter))
					{
						unsigned int new_secs = sm_get_current_seconds();

						found_array[found_num++] = m;

						if (found_num == NUM_FOUND || new_secs != secs)
						{
							thread_call_parent_function_sync(NULL,search_add_result, 2, found_array, found_num);
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
				thread_call_parent_function_sync(NULL,search_add_result, 2, found_array, found_num);

			thread_call_parent_function_sync(NULL,folder_search_clean_thread, 0);
			thread_call_parent_function_sync(NULL,search_disable_search, 0);
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

/*****************************************************************************/

void folder_start_search(struct search_options *sopt)
{
	struct search_msg msg;
	struct folder *f;
	struct folder *start;
	struct folder *end;
	struct folder **array;
	int num;

	/* Only one thread allowed */
	if (search_thread)
		return;

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

	if ((array = (struct folder**)malloc((num+1)*sizeof(struct folder*))))
	{
		int i = 0;

		f = start;

		while (f && f != end)
		{
			if (f->special != FOLDER_SPECIAL_GROUP)
			{
				void *handle = NULL;
			  folder_next_mail_info(f,&handle);
				array[i++]  = f;
			}
			f = folder_next(f);
		}
		array[i] = NULL;

		msg.sopt = sopt;
		msg.f_array = array;
		msg.f_array_len = num;

		search_thread = thread_add("SimpleMail - Search Thread",
				THREAD_FUNCTION(&folder_start_search_entry),&msg);

		free(array);
	}
}

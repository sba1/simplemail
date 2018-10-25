/**
 * @file mailinfo_extractor.c
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#include "codesets.h"
#include "lists.h"
#include "folder.h"
#include "mail.h"
#include "mainwnd.h"
#include "subthreads.h"
#include "support_indep.h"

/**
 * @brief the lazy thread instance
 *
 * The main purpose of this thread is to execute...
 */
static thread_t lazy_thread;

/**
 * @brief Allows the storage of mail_info into a list.
 */
struct mail_info_node
{
	struct node node;
	struct mail_info *mail_info;
	char *folder_path;
};

/**
 * @brief List containing mail infos of which the excerpt should be read.
 *
 * Entries in this list are referenced!
 */
static struct list lazy_mail_list;

/**
 * @brief The semaphore protecting lazy_mail_list.
 */
static semaphore_t lazy_semaphore;


/**
 * @brief Finished the stuff done in lazy_thread_work and dereferences the mail.
 *
 * @param excerpt
 * @param mail
 *
 * @note Needs to be called at the context of the main task
 */
static void lazy_thread_work_finished(utf8 *excerpt, struct mail_info *mail)
{
	int refresh = 1;

	/* Don't refresh if excerpt was and is empty */
	if (excerpt && !*excerpt && (!mail->excerpt || !*mail->excerpt))
		refresh = 0;

	mail_info_set_excerpt(mail,excerpt);
	if (refresh)
		main_refresh_mail(mail);
	mail_dereference(mail);
}

/**
 * @brief The worker function of the lazy thread.
 *
 * This worker opens the mail and reads an excerpt. This is
 * then forwarded to the main task which in turn notifies
 * the view.
 *
 * @param path specifies the path which is going to be read here.
 * @param mail referenced mail
 *
 * @note needs to be called at the context of the lazy thread
 */
static void lazy_thread_work(char *path, struct mail_info *mail)
{
	char buf[380];

	struct mail_complete *mail_complete;
	utf8 *excerpt = NULL;

	if (mail->excerpt)
	{
		/* Just dereference the mail */
		thread_call_function_async(thread_get_main(),mail_dereference,1,mail);
		return;
	}

	getcwd(buf, sizeof(buf));
	chdir(path);

	if ((mail_complete = mail_complete_create_from_file(NULL, mail->filename)))
	{
		struct mail_complete *mail_text;
		mail_read_contents("",mail_complete);
		if ((mail_text = mail_find_initial(mail_complete)))
		{
			if (!mystricmp(mail_text->content_type,"text"))
			{
				void *cont; /* mails content */
				int cont_len;

				string str;

				mail_decoded_data(mail_text,&cont,&cont_len);

				/* Create the excerpt, i.e., the first 100 characters
				 * with no new lines and no quotation
				 */
				if (string_initialize(&str,100))
				{
					int i,j;
					int space = 0;
					int eol = 1;
					int ignore_line = 0;

					for (i=0,j=0;i<cont_len && j<100;)
					{
						unsigned char c = ((char*)cont)[i];
						int bytes;

						if (!c) break;

						if (c == 10)
						{
							eol = 1;
							space = 1;
							ignore_line = 0;
							i++;
							continue;
						}

						if (isspace(c))
						{
							i++;
							space = 1;
							continue;
						}

						if (c == '>' && eol)
							ignore_line = 1;

						eol = 0;

						if (space && !ignore_line)
						{
							string_append_char(&str,' ');
							j++;
							if (j == 100) break;
							space = 0;
						}

						bytes = utf8bytes(&((utf8*)cont)[i]);
						if (!ignore_line)
							string_append_part(&str,&((char*)cont)[i],bytes);
						i+=bytes;
					}
					excerpt = (utf8*)str.str;
				}
			}
		}
	}

	mail_complete_free(mail_complete);
	chdir(buf);

	/* This also dereferences the given mail */
	thread_call_function_async(thread_get_main(),lazy_thread_work_finished,2,excerpt,mail);
}

/**
 * @brief Entry for the lazy thread.
 *
 * @param user_data
 * @return
 */
static int lazy_entry(void *user_data)
{
	thread_parent_task_can_contiue();
	while (thread_wait(NULL,NULL,NULL,0))
	{
		/* Scan through the lazy mail list */
		while (1)
		{
			struct mail_info_node *mail_node;

			thread_lock_semaphore(lazy_semaphore);
			mail_node = (struct mail_info_node*)list_first(&lazy_mail_list);
			if (mail_node) node_remove(&mail_node->node);
			thread_unlock_semaphore(lazy_semaphore);
			if (!mail_node) break;
			lazy_thread_work(mail_node->folder_path,mail_node->mail_info);

			free(mail_node->folder_path);
			free(mail_node);
		}

	}
	return 1;
}

/*****************************************************************************/

void lazy_clean_list(void)
{
	struct mail_info_node *mail_node;

	if (!lazy_semaphore) return;

	thread_lock_semaphore(lazy_semaphore);

	while ((mail_node = (struct mail_info_node*)list_remove_tail(&lazy_mail_list)))
	{
		mail_dereference(mail_node->mail_info);
		free(mail_node->folder_path);
		free(mail_node);
	}

	thread_unlock_semaphore(lazy_semaphore);
}

/*****************************************************************************/

int simplemail_get_mail_info_excerpt_lazy(struct mail_info *mail)
{
	struct mail_info_node *node;
	struct folder *f = main_get_folder();
	char *folder_path;

	if (!lazy_thread)
	{
		if (!(lazy_semaphore = thread_create_semaphore()))
			return 0;

		if (!(lazy_thread = thread_add("SimpleMail - Mail Extractor",lazy_entry,NULL)))
		{
			thread_dispose_semaphore(lazy_semaphore);
			lazy_semaphore = NULL;
			return 0;
		}

		list_init(&lazy_mail_list);
	}

	if (!(folder_path = mystrdup(f->path)))
		return 0;
	if (!(node = (struct mail_info_node *)malloc(sizeof(*node))))
	{
		free(folder_path);
		return 0;
	}

	mail_reference(mail);
	node->mail_info = mail;
	node->folder_path = folder_path;

	thread_lock_semaphore(lazy_semaphore);
	list_insert_tail(&lazy_mail_list,&node->node);
	thread_unlock_semaphore(lazy_semaphore);

	thread_signal(lazy_thread);

	return 1;
}

/*****************************************************************************/

void cleanup_mailinfo_extractor(void)
{
	if (!lazy_semaphore)
		return;

	/* If there are some pending requests, clean them now */
	lazy_clean_list();
	thread_dispose_semaphore(lazy_semaphore);
}

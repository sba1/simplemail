/**
 * @file progmon.c
 */

#include "codesets.h"
#include "debug.h"
#include "lists.h"
#include "progmon.h"
#include "simplemail.h"
#include "subthreads.h"

#include <stdlib.h>
#include <string.h>

/** @brief contains all active progress monitors */
static struct list progmon_list;

/** @brief total work of all active progress monitors */
static int progmon_total_work;

/** @brief total work work_done of all all active progress monitors */
static int progmon_total_work_done;

/** @brief number of active progress monitors */
static int progmon_number_of_actives;

/** @brief arbitrate access to global data */
static semaphore_t progmon_list_sem;

struct progmon_default
{
	struct progmon pm;

	utf8 *name;
	utf8 *working_on;
	unsigned int work;
	unsigned int work_done;
};

static void progmon_begin(struct progmon *pm, unsigned int work, const utf8 *txt)
{
	struct progmon_default *pmd = (struct progmon_default*)pm;

	SM_ENTER;

	pmd->name = utf8dup(txt);
	pmd->work = work;

	SM_DEBUGF(10,("%s\n",txt));
	thread_lock_semaphore(progmon_list_sem);
	list_insert_tail(&progmon_list,&pm->node);
	progmon_total_work += work;
	progmon_number_of_actives++;
	thread_unlock_semaphore(progmon_list_sem);

	if (thread_get() != thread_get_main())
		thread_call_function_async(thread_get_main(),simplemail_update_progress_monitors,0);
	else
		simplemail_update_progress_monitors();

	SM_LEAVE;
}

static void progmon_working_on(struct progmon *pm, const utf8 *txt)
{
	struct progmon_default *pmd = (struct progmon_default*)pm;
	utf8 *working_on_txt;

	working_on_txt = utf8dup(txt);
	thread_lock_semaphore(progmon_list_sem);
	if (pmd->working_on) free(pmd->working_on);
	pmd->working_on = working_on_txt;
	thread_unlock_semaphore(progmon_list_sem);

	if (thread_get() != thread_get_main())
		thread_call_function_async(thread_get_main(),simplemail_update_progress_monitors,0);
	else
		simplemail_update_progress_monitors();
}

static void progmon_work(struct progmon *pm, unsigned int done)
{
	struct progmon_default *pmd = (struct progmon_default*)pm;

	SM_ENTER;

	thread_lock_semaphore(progmon_list_sem);
	pmd->work_done += done;
	progmon_total_work_done += done;
	thread_unlock_semaphore(progmon_list_sem);

	if (thread_get() != thread_get_main())
		thread_call_function_async(thread_get_main(),simplemail_update_progress_monitors,0);
	else
		simplemail_update_progress_monitors();

	SM_LEAVE;
}

static int progmon_canceled(struct progmon *pm)
{
	return 0;
}

static void progmon_done(struct progmon *pm)
{
	thread_lock_semaphore(progmon_list_sem);
	node_remove(&pm->node);
	progmon_total_work -= ((struct progmon_default*)pm)->work;
	progmon_total_work_done -= ((struct progmon_default*)pm)->work_done;
	progmon_number_of_actives--;
	thread_unlock_semaphore(progmon_list_sem);

	if (thread_get() != thread_get_main())
		thread_call_function_async(thread_get_main(),simplemail_update_progress_monitors,0);
	else
		simplemail_update_progress_monitors();
}

/**
 * Returns the total work.
 *
 * @return
 */
unsigned int progmon_get_total_work(void)
{
	unsigned int tw;

	thread_lock_semaphore(progmon_list_sem);
	tw = progmon_total_work;
	thread_unlock_semaphore(progmon_list_sem);

	return tw;
}

/**
 * Returns the total work work_done.
 *
 * @return
 */
unsigned int progmon_get_total_work_done(void)
{
	unsigned int twd;

	thread_lock_semaphore(progmon_list_sem);
	twd = progmon_total_work_done;
	thread_unlock_semaphore(progmon_list_sem);

	return twd;
}

/**
 * Return number of active monitors.
 *
 * @return
 */
unsigned int progmon_get_number_of_actives(void)
{
	unsigned int noa;

	thread_lock_semaphore(progmon_list_sem);
	noa = progmon_number_of_actives;
	thread_unlock_semaphore(progmon_list_sem);

	return noa;
}

/**
 * Creates a new progress monitor.
 *
 * @return
 */
struct progmon *progmon_create(void)
{
	struct progmon_default *pm;

	if ((pm = (struct progmon_default*)malloc(sizeof(*pm))))
	{
		memset(pm,0,sizeof(*pm));
		pm->pm.begin = progmon_begin;
		pm->pm.working_on = progmon_working_on;
		pm->pm.work = progmon_work;
		pm->pm.canceled = progmon_canceled;
		pm->pm.done = progmon_done;
	}

	return &pm->pm;
}

/**
 * Deletes the given progress monitor.
 *
 * @param pm
 */
void progmon_delete(struct progmon *pm)
{
	struct progmon_default *pmd = (struct progmon_default*)pm;
	if (pmd)
	{
		free(pmd->working_on);
		free(pmd->name);
	}
	free(pm);
}

/**
 * Scans available progress monitors.
 *
 * @param callback
 * @param udata
 */
int progmon_scan(void (*callback)(struct progmon_info *, void *udata), void *udata)
{
	int num;
	struct progmon_info info;
	struct progmon_default *pm;

	num = 0;

	thread_lock_semaphore(progmon_list_sem);
	pm = (struct progmon_default*)list_first(&progmon_list);
	while (pm)
	{
		info.name = pm->name;
		info.work = pm->work;
		info.work_done = pm->work_done;
		info.working_on = pm->working_on;

		callback(&info,udata);

		pm = (struct progmon_default*)node_next(&pm->pm.node);
		num++;
	}
	thread_unlock_semaphore(progmon_list_sem);

	return num;
}


/**
 *
 * @return
 */
int progmon_init(void)
{
	int rc;

	SM_ENTER;

	list_init(&progmon_list);

	if ((progmon_list_sem = thread_create_semaphore()))
	{
		rc = 1;
	} else rc = 0;

	SM_RETURN(rc,"%ld");
	return rc;
}

/**
 * Deinitializes the prog monitors.
 */
void progmon_deinit(void)
{
	SM_ENTER;

	if (progmon_list_sem)
		thread_dispose_semaphore(progmon_list_sem);

	SM_LEAVE;
}

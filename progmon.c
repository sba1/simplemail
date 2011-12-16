/**
 * @file progmon.c
 */

#include "debug.h"
#include "lists.h"
#include "progmon.h"
#include "subthreads.h"

#include <stdlib.h>

static struct list progmon_list;
static semaphore_t progmon_list_sem;

struct progmon_default
{
	struct progmon pm;
};

static void progmon_begin(struct progmon *pm, unsigned int work, char *txt)
{
}

static void progmon_working_on(struct progmon *pm, char *txt)
{
}

static void progmon_work(struct progmon *pm, unsigned int done)
{
}

static int progmon_canceled(struct progmon *pm)
{
	return 0;
}

static void progmon_done(struct progmon *pm)
{
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
		pm->pm.begin = progmon_begin;
		pm->pm.working_on = progmon_working_on;
		pm->pm.work = progmon_work;
		pm->pm.canceled = progmon_canceled;
		pm->pm.done = progmon_done;

		thread_lock_semaphore(progmon_list_sem);
		list_insert_tail(&progmon_list,&pm->pm.node);
		thread_unlock_semaphore(progmon_list_sem);
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
	thread_lock_semaphore(progmon_list_sem);
	node_remove(&pm->node);
	thread_unlock_semaphore(progmon_list_sem);
	free(pm);
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

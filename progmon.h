/**
 * @file progmon.h
 */

#ifndef SM__PROGMON_H
#define SM__PROGMON_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

#ifndef SM__CODESETS_H
#include "codesets.h"
#endif

struct progmon
{
	/**
	 * Embedded node.
	 */
	struct node node;

	/**
	 * Notifies the progress monitor that a task is about
	 * to be started.
	 *
	 * @param work amount of work
	 * @param txt some useful text describing the task
	 */
	void (*begin)(struct progmon *pm, unsigned int work, const utf8 *txt);

	/**
	 * Notifies the progess monitor that we are now
	 * working on the given specific problem.
	 * @param txt
	 */
	void (*working_on)(struct progmon *pm, const utf8 *txt);

	/**
	 * Notifies the progess monitor that the given amount
	 * of work has been done.
	 *
	 * @param done
	 */
	void (*work)(struct progmon *pm, unsigned int done);

	/**
	 * @return if the operation should be canceled or not.
	 */
	int (*canceled)(struct progmon *pm);

	/**
	 * Notifies the progress monitor that the task
	 * has been completed.
	 */
	void (*done)(struct progmon *pm);
};

struct progmon_info
{
	/** A unique id */
	int id;

	utf8 *name;
	utf8 *working_on;
	unsigned int work;
	unsigned int work_done;

	/** Is the progress monitor cancelable */
	int cancelable;
};

/**
 * Progmon cancel callback type.
 *
 * @param udata supplied user data
 * @return whether the cancel operation was successful
 */
typedef int (*progmon_cancel_callback_t)(void *udata);

/**
 * Create a new progress monitor with an ability to cancel.
 *
 * @param cancel_callback callback that is invoked when use intends to cancel the job.
 * @param cancel_callback_data the user data supplied to the callback.
 * @return the new progress monitor handle.
 */
struct progmon *progmon_create_cancelable(progmon_cancel_callback_t cancel_callback, void *cancel_callback_data);

/**
 * Creates a new progress monitor.
 *
 * @return the new progress monitor handle.
 */
struct progmon *progmon_create(void);

/**
 * Deletes the given progress monitor.
 *
 * @param pm
 */
void progmon_delete(struct progmon *pm);

/**
 * Returns the total work.
 *
 * @return
 */
unsigned int progmon_get_total_work(void);

/**
 * Returns the total work work_done.
 *
 * @return
 */
unsigned int progmon_get_total_work_done(void);

/**
 * Return number of active monitors.
 *
 * @return
 */
unsigned int progmon_get_number_of_actives(void);

/**
 * Scans available progress monitors.
 *
 * @param callback
 * @param udata
 */
int progmon_scan(void (*callback)(struct progmon_info *, void *udata), void *udata);

/**
 * Cancel the progmon with the given id.
 *
 * @param id id of the progmon (as in progmon_info) to be cancelled.
 * @return whether the cancel operation was successfully initiated
 */
int progmon_cancel(int id);

/**
 * Initializes the progmon component.
 * @return
 */
int progmon_init(void);

/**
 * Deinitializes the prog monitors.
 */
void progmon_deinit(void);

#endif

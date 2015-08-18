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
};

struct progmon *progmon_create(void);
void progmon_delete(struct progmon *pm);

unsigned int progmon_get_total_work(void);
unsigned int progmon_get_total_work_done(void);
unsigned int progmon_get_number_of_actives(void);

int progmon_scan(void (*callback)(struct progmon_info *, void *udata), void *udata);

int progmon_init(void);
void progmon_deinit(void);

#endif

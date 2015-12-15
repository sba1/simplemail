/**
 * @file coroutines_platform.h
 *
 * API for platform specific implementations.
 */

#ifndef SM__COROUTINES_PLATFORM_H
#define SM__COROUTINES_PLATFORM_H

struct coroutine_scheduler_platform;
typedef struct coroutine_scheduler_platform *coroutine_scheduler_platform_t;

/**
 * Creates platform specific coroutine scheduler support.
 *
 * @return instance of platform specific scheduler
 */
coroutine_scheduler_platform_t coroutine_scheduler_platform_new(void);

#endif

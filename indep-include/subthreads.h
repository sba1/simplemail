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
 * @file subthreads.h
 *
 * Various functions to call functions on a context of a different thread.
 * This file can be compiled with a C++ compiler, in which some compile-time
 * checks are performed.
 */

#ifndef SM__SUBTHREADS_H
#define SM__SUBTHREADS_H

#ifndef SM__COROUTINES_H
#include "coroutines.h"
#endif

#if __cplusplus >= 201103L
/* This is for the support to let the compiler check that the types of the
 * various varargs functions are correct. We implement some of them on our own
 * template classes so avoid the need to include too many files.
 */
#include <type_traits>

namespace simplemail
{

template<typename... As>
struct tuple {
};

template<typename A, typename... As>
struct tuple<A, As...> {
};

template<>
struct tuple<> {
};

template<typename A, typename B>
struct is_convertible;

template<typename A, typename... As, typename B, typename... Bs>
struct is_convertible<tuple<A, As...>, tuple<B, Bs...>>
{
    static constexpr bool convertible = is_convertible<tuple<As...>,tuple<Bs...>>::convertible && std::is_convertible<B, A>::value;
};

template<>
struct is_convertible<tuple<>, tuple<>>
{
    static constexpr bool convertible = true;
};
}

#endif

#define THREAD_FUNCTION(x) ((int (*)(void*))x)

/**
 * Initialize the thread system
 *
 * @return 0 on failure, 1 on success.
 */
int init_threads(void);

/**
 * Cleanup the thread system. Will abort every thread.
 *
 * @todo there should be a separate abort_threads() function which
 *   aborts all threads (and disallows the creation of new ones)
 */
void cleanup_threads(void);

/* thread handling */
struct thread_s;
typedef struct thread_s * thread_t; /* opaque type */

struct future_s;
typedef struct future_s * future_t;

/**
 * Informs the parent task that it can continue, i.e., that thread_add() or thread_start()
 * can return.
 *
 * @return 0 on failure, 1 on success.
 */
int thread_parent_task_can_contiue(void);


/**
 * Start the default thread with the given entry point. This function will be removed in the future
 * as it is too inflexible.
 *
 * @param entry the entry point
 * @param udata the user data passed to the entry point
 * @return 0 on failure (e.g., default thread already exists) or 1 on success.
 */
int thread_start(int (*entry)(void*), void *udata);


/**
 * Runs a given function in a newly created thread under the given name which
 * is linked into a internal list.
 *
 * @param thread_name
 * @param entry
 * @param eudata
 * @return
 */
thread_t thread_add(const char *thread_name, int (*entry)(void *), void *eudata);


/**
 * @brief Waits until a signal has been sent and calls timer_callback
 * periodically.
 *
 * It's possible to execute functions on the threads context while this
 * function is executed via thread_call_function_xxx()
 *
 * @param sched a scheduler for coroutines
 * @param timer_callback function that is called periodically
 * @param timer_data some data that is passed as the first argument
 *        to the callback
 * @param millis the periodic time span
 * @return 0 if the function finished due to an abort request (or failure),
 *         1 if due to a call to thread_signal().
 */
int thread_wait(coroutine_scheduler_t sched, void (*timer_callback(void*)), void *timer_data, int millis);

/**
 * @brief Aborts the given thread.
 *
 * @param thread_to_abort the thread to be aborted. Specify NULL for the default
 * subthread.
 *
 * @note This functions doesn't wait until the given thread has been finished.
 * It just requests the abortion.
 */
void thread_abort(thread_t thread);

/**
 * @brief Signals the given thread.
 *
 * @param thread_to_signal the thread to be signaled. Specify NULL for the default
 * subthread.
 */
void thread_signal(thread_t thread_to_signal);

/**
 * @brief Check if somebody sent an abort signal.
 *
 * @return 1 if an abort signal was sent.
 */
int thread_aborted(void);

/**
 * Call a function in the context of the given thread in a synchronous manner,
 * i.e., this call will return if the called function will return or if an
 * error occurred.
 *
 * @param thread the thread in which the given function shall be called. It is
 *  okay if this is the current thread.
 * @param function
 * @param argcount
 * @return
 */
int thread_call_function_sync(thread_t thread, void *function, int argcount, ...);

#if __cplusplus >= 201103L
template<typename R, typename... A, typename... B>
int thread_call_function_sync(thread_t thread, R (*function)(A...), int argcount, B... args)
{
	using namespace simplemail;
	static_assert(sizeof...(A) == sizeof...(B));
	static_assert(is_convertible<tuple<A...>, tuple<B...>>::convertible == true);

	return thread_call_function_sync(thread, (void *)function, argcount, args...);
}
#endif

/**
 * @brief Call a function in the context of the given thread in an asynchronous manner.
 *
 * @param thread the thread in which context the function is executed.
 * @param function the function to be executed.
 * @param argcount number of function parameters
 * @return whether the call was successfully forwarded.
 */
int thread_call_function_async_(thread_t thread, void *function, int argcount, ...);

#if __cplusplus > 201703L

template<int N, typename R, typename... A, typename... B>
static inline int thread_call_function_async_2(thread_t thread, R (*function)(A...), int argcount, B... args)
{
	using namespace simplemail;
	static_assert(N == sizeof...(B));
	static_assert(sizeof...(A) == sizeof...(B));
	static_assert(is_convertible<tuple<A...>, tuple<B...>>::convertible == true);

	return thread_call_function_async_(thread, (void *)function, argcount, args...);
}
#define thread_call_function_async(thread, function, argcount, ...) \
		thread_call_function_async_2<argcount>(thread, function, argcount __VA_OPT__(,) __VA_ARGS__)

#elif __cplusplus >= 201103L

template<typename R, typename... A, typename... B>
static inline int thread_call_function_async(thread_t thread, R (*function)(A...), int argcount, B... args)
{
	using namespace simplemail;
	static_assert(sizeof...(A) == sizeof...(B));
	static_assert(is_convertible<tuple<A...>, tuple<B...>>::convertible == true);

	return thread_call_function_async_(thread, (void *)function, argcount, args...);
}

#else

#define thread_call_function_async thread_call_function_async_

#endif

/**
 * @brief Call a function in the context of the given thread in an asynchronous manner
 *  with the possibility to retrieve the result.
 *
 * @param future_t out parameter that is filled when the call was successful and
 *  with which you can get access to the result.
 * @param thread the thread in which context the function is executed.
 * @param function the function to be executed.
 * @param argcount number of function parameters
 * @return whether the call was successfully submitted
 */
int thread_call_function_async_future(future_t *future_t, thread_t thread, void *function, int arcount, ...);

/**
 * Call a function in context of the parent task in a sychron manner. The contents of
 * success is set to 1, if the call was successful otherwise to 0.
 * success may be NULL. If success would be 0, the call returns 0 as well.
 *
 * @param success
 * @param function
 * @param argcount
 * @return
 */
int thread_call_parent_function_sync(int *success, void *function, int argcount, ...);

#if __cplusplus >= 201103L
template<typename R, typename... A, typename... B>
int thread_call_parent_function_sync(int *success, R (function)(A...), int argcount, B... args)
{
	using namespace simplemail;
	static_assert(sizeof...(A) == sizeof...(B));
	static_assert(is_convertible<tuple<A...>, tuple<B...>>::convertible == true);

	return thread_call_parent_function_sync(success, (void *)function, argcount, args...);

}
#endif

/**
 * Call the given function asynchronous in the context of the parent thread
 * and duplicate the first argument which is threaded at a string.
 *
 * @param function
 * @param argcount
 * @return
 */
int thread_call_parent_function_async_string(void *function, int argcount, ...);

#if __cplusplus >= 201103L
template<typename R, typename... A, typename... B>
static inline int thread_call_parent_function_async_string(R (*function)(const char *, A...), int argcount, const char *str, B... args)
{
	using namespace simplemail;
	static_assert(sizeof...(A) == sizeof...(B));
	static_assert(is_convertible<tuple<A...>, tuple<B...>>::convertible == true);

	return thread_call_parent_function_async_string((void *)function, argcount, str, args...);
}
#endif


/**
 * @brief Call a coroutine on the context of the given thread.
 *
 * The thread must run a coroutine scheduler.
 *
 * @param thread the thread
 * @param coroutine the coroutine to execute
 * @param ctx the initialized context that is used by the given coroutine.
 * @return whether the submission of the coroutine call was successful.
 */
int thread_call_coroutine(thread_t thread, coroutine_entry_t coroutine, struct coroutine_basic_context *ctx);

/**
 * @brief Call the function synchronous in the context of the parent task.
 *
 * Also calls the timer_callback on the calling process context periodically.
 *
 * @param timer_callback
 * @param timer_data
 * @param millis
 * @param function
 * @param argcount
 * @return
 */
int thread_call_parent_function_sync_timer_callback(void (*timer_callback)(void*), void *timer_data, int millis, void *function, int argcount, ...);

#if __cplusplus >= 201103L
template<typename R, typename... A, typename... B>
static inline int thread_call_parent_function_sync_timer_callback(void (*timer_callback)(void*), void *timer_data, int millis, R (*function)(A...), int argcount, B... args)
{
	using namespace simplemail;
	static_assert(sizeof...(A) == sizeof...(B));
	static_assert(is_convertible<tuple<A...>, tuple<B...>>::convertible == true);

	return thread_call_parent_function_sync_timer_callback(timer_callback, timer_data, millis, (void *)function, argcount, args...);
}
#endif

/**
 * Pushes a function call in the function queue of the callers task context.
 *
 * @param function
 * @param argcount
 * @return 1 for success else 0.
 */
int thread_push_function(void *function, int argcount, ...);

/**
 * Pushes a function call in the function queue of the callers task context
 * but only after a given amount of time.
 *
 * @param millis
 * @param function
 * @param argcount
 * @return return 1 for success else 0.
 */
int thread_push_function_delayed(int millis, void *function, int argcount, ...);

#if __cplusplus >= 201103L
template<typename R, typename... A, typename... B>
static inline int thread_push_function_delayed(int millis, R (*function)(A...), int argcount, B... args)
{
	using namespace simplemail;
	static_assert(sizeof...(A) == sizeof...(B));
	static_assert(is_convertible<tuple<A...>, tuple<B...>>::convertible == true);

	return thread_push_function_delayed(millis, function, argcount, args...);
}
#endif

/**
 * Return the main (UI) thread.
 *
 * @return
 */
thread_t thread_get_main(void);

/**
 * Return the current thread.
 *
 * @return
 */
thread_t thread_get(void);

/* semaphore handling */
struct semaphore_s;
typedef struct semaphore_s * semaphore_t; /* opaque type */

/**
 * Create a new semaphore.
 *
 * @return the semaphore or NULL for an error.
 */
semaphore_t thread_create_semaphore(void);

/**
 * Dispose the given semaphore.
 *
 * @param sem defines the semaphore to be disposed. NULL is accepted.
 */
void thread_dispose_semaphore(semaphore_t sem);
int thread_attempt_lock_semaphore(semaphore_t sem);
void thread_lock_semaphore(semaphore_t sem);
void thread_unlock_semaphore(semaphore_t sem);

#endif

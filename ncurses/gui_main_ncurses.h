/**
 * @file gui_main_ncurses.h
 */

#ifndef SM__GUI_MAIN_NCURSES_H
#define SM__GUI_MAIN_NCURSES_H

#include "lists.h"

struct gui_resize_listener
{
	struct node n;

	void *udata;
	void (*callback)(void *udata);
};

/**
 * Add a listener that is invoked when the window is resized.
 *
 * @param listener the listener to add
 * @param callback the callback that is called
 * @param udata the udata is passed to the callback
 */
void gui_add_resize_listener(struct gui_resize_listener *listener, void (*callback)(void *arg), void *udata);

/**
 * Remove the given resize listener.
 *
 * @param listener the listener to be removed.
 */
void gui_remove_resize_listener(struct gui_resize_listener *listener);

/** The global screen */
extern struct screen gui_screen;

#endif

/**
 * @file gui_main_ncurses.h
 */

#ifndef SM__GUI_MAIN_NCURSES_H
#define SM__GUI_MAIN_NCURSES_H

#include "lists.h"

struct gui_key_listener
{
	struct node n;
	char ch;

	void (*callback)(void);
};

/**
 * Registers the given listener to call the given function on the given key event.
 *
 * @param listener the storage of the listener
 * @param ch the character
 * @param callback the function that is called on the event that ch is pressed.
 */
void gui_add_key_listener(struct gui_key_listener *listener, char ch, void (*callback)(void));

/**
 * Remove the previously added listener.
 *
 * @param listener defines the listener that shall be removed.
 */
void gui_remove_key_listener(struct gui_key_listener *listener);

#endif

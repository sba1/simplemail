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
 * @file readwnd.h
 */
#ifndef SM__READWND_H
#define SM__READWND_H

struct folder;
struct mail_info;

/**
 * Opens a read window. Returns the number of the read window or -1
 * for an error. You can specify the number of the window which to
 * use or -1 for a random one.
 *
 * @param folder the folder were the mail is located
 * @param mail the mail for which the read window should be opened
 * @param window the actual window
 * @return 1 on success, 0 on failure
 */
int read_window_open(const char *folder, struct mail_info *mail, int window);

/**
 * Activate a read window
 *
 * @param num the read window to be activated
 */
void read_window_activate(int num);

/**
 * Closes a read window
 *
 * @param num the number of the read window to be closed.
 */
void read_window_close(int num);

/**
 * Deallocates all resources associated with any read window.
 */
void read_window_cleanup(void);

/**
 * Returns the displayed mail of the given window
 *
 * @param num the window number of the window for which the mail should be
 *  returned
 * @return the mail
 */
struct mail_complete *read_window_get_displayed_mail(int num);

/**
 * Refresh state of the prev/next buttons
 *
 * @param f
 */
void read_refresh_prevnext_button(struct folder *f);

#endif

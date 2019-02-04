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
 * @file gui_main_amiga.h
 */

#ifndef GUI_MAIN_AMIGA_H
#define GUI_MAIN_AMIGA_H

/**
 * Returns the directory, in which the images are stored.
 *
 * @return
 */
const char *gui_get_images_directory(void);

/**
 * Execute the main event loop.
 */
void loop(void);

/**
 * Shows the application (uniconifies it).
 */
void app_show(void);

/**
 * Hides the application (iconifies it).
 */
void app_hide(void);

/**
 * Quits the application.
 */
void app_quit();

#endif

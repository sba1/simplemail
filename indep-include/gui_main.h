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
 * @file gui_main.h
 */

#ifndef GUI_MAIN_H
#define GUI_MAIN_H

/**
 * The application should present itself as busy.
 */
void app_busy(void);

/**
 * The application should present itself as ready/unbusy.
 */
void app_unbusy(void);

/**
 * Parse the command line arguments
 *
 * @param argc
 * @param argv
 * @return
 */
int gui_parseargs(int argc, char *argv[]);

/**
 * Initialize the GUI
 *
 * @return 1 on success, 0 otherwise.
 */
int gui_init(void);

/**
 * The GUI loop.
 */
void gui_loop(void);

/**
 * Frees the GUI.
 */
void gui_deinit(void);

/**
 * Execute an ARexx script
 * @param filename name of the script to be executed.
 * @return 1 if script is being executed, 0 otherwise
 */
int gui_execute_arexx(char *filename);

#endif


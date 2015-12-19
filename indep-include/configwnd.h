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
 * @file configwnd.h
 */

#ifndef SM__CONFIGWND_H
#define SM__CONFIGWND_H

/**
 * Open the config window
 */
void open_config(void);

/**
 * Close and dispose the config window
 */
void close_config(void);

/**
 * Refreshes the folders
 */
void config_refresh_folders(void);

/**
 * Set the state of the receive failed indicator. This is used for account
 * testing.
 *
 * @param state
 */
void config_accounts_set_recv_failed_state(int state);

/**
 * Set the state of the send failed indicator. This is used for account testing.
 *
 * @param state
 */
void config_accounts_set_send_failed_state(int state);

/**
 * Informs the config window that accounts can be tested or not-
 *
 * @param tested whether accounts can be tested or not.
 */
void config_accounts_can_be_tested(int tested);

/**
 * Inform the config window that the fingerprint of the given server has been
 * changed.
 *
 * @param server name of the server for which the fingerprint has been updated.
 * @param fingerprint the new fingerprint of the server.
 */
void config_accounts_update_fingerprint(const char *server, const char *fingerprint);

#endif

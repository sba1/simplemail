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
 * @file trans.h
 */

#ifndef SM__TRANS_H
#define SM__TRANS_H

struct account;
struct mail_info;

/**
 * Download mails from all accounts.
 *
 * @param called_by_auto whether this was triggered by some automatic mechanism.
 * @return 1 on success, 0 on failure.
 */
int mails_dl(int called_by_auto);

/**
 * Download all mails from a single account.
 *
 * @param ac the account from which to download.
 * @return 1 on success, 0 on failure.
 */
int mails_dl_single_account(struct account *ac);

/**
 * Uploads all mails tof the incoming folder hat can be uploaded.
 *
 * @return 1 on success, 0 on failure.
 */
int mails_upload(void);

/**
 * Uploads a single mail.
 *
 * @param mi the mail info describing the mail to send.
 * @return 1 on success, 0 on failure.
 * @note Assumes that the chdir is the dir where m->filename is located
 */
int mails_upload_single(struct mail_info *mi);

/**
 * Callback type for mails_test_account().
 *
 * @param success whether the test was successful, i.e., the config is likely
 *  to work as given.
 */
typedef void (*account_tested_callback_t)(int success);

/**
 * Tests whether logging in into the given account works. This is an async call.
 *
 * @param ac account to test.
 * @param callback
 * @return 1 if job has been submitted, 0 otherwise.
 */
int mails_test_account(struct account *ac, account_tested_callback_t callback);


#endif

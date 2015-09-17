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
 * @file spam.h
 */

#ifndef SM__SPAM_H
#define SM__SPAM_H

struct folder;
struct mail_info;

/**
 * Init the spam identification subsystem.
 *
 * @return whether this succeeded or not
 */
int spam_init(void);

/**
 * Reverses spam_init().
 */
void spam_cleanup(void);

/**
 * Train the classifier with an example of a spam mail.
 *
 * @param folder
 * @param mail
 * @return
 */
int spam_feed_mail_as_spam(struct folder *folder, struct mail_info *mail);

/**
 * Train the classifier with an example of a ham mail.
 *
 * @param folder the folder in which the training mail is located
 * @param mail the example mail.
 * @return
 */
int spam_feed_mail_as_ham(struct folder *folder, struct mail_info *mail);

/**
 * Determines whether a mail is spam or not. white and black are string
 * arrays (created via the array_xxx() functions) and stand for white
 * and black lists.
 *
 * @param folder_path the folder path of the mail to check
 * @param to_check_mail the mail to check
 * @param white arrays created via array_xxx() for the white list
 * @param black arrays created via array_xxx() for the black list
 * @return 1 if the mail is spam, 0 otherwise
 */
int spam_is_mail_spam(char *folder_path, struct mail_info *to_check_mail, char **white, char **black);

/**
 * Returns the number of mails classified as spam
 *
 * @return number of mails classified as spam
 */
unsigned int spam_num_of_spam_classified_mails(void);

/**
 * Returns the number of mails classified as ham.
 *
 * @return number of mails classified as ham
 */
unsigned int spam_num_of_ham_classified_mails(void);

/**
 * Resets the ham statistics
 */
void spam_reset_ham(void);

/**
 * Resets the spam statistics
 */
void spam_reset_spam(void);


#endif

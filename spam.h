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
#ifndef SM__SPAM_H
#define SM__SPAM_H

int spam_init(void);
void spam_cleanup(void);
int spam_feed_mail_as_spam(struct folder *folder, struct mail *mail);
int spam_feed_mail_as_ham(struct folder *folder, struct mail *mail);
int spam_is_mail_spam(struct folder *folder, struct mail *to_check_mail);

unsigned int spam_num_of_spam_classified_mails(void);
unsigned int spam_num_of_ham_classified_mails(void);

void spam_reset_ham(void);
void spam_reset_spam(void);


#endif

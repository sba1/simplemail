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
#ifndef SM__ADDRESSBOOK_H
#define SM__ADDRESSBOOK_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

#define ADDRESSBOOK_ENTRY_GROUP 0
#define ADDRESSBOOK_ENTRY_PERSON 1
#define ADDRESSBOOK_ENTRY_LIST 2

/* the addressbook structure (read only outside from addressbook.c! */
struct addressbook_entry
{
	struct node node; /* embedded node structure */
	int type; /* see above */

	union
	{
		struct
		{
			char *alias;
			char *description;

			struct list list; /* more addressbook entries */
		} group;

		struct
		{
			char *alias;
			char *realname;
			char *pgpid;
			char *homepage;
			char *street;
			char *city;
			char *country;
			char *phone;
			char *description;
			unsigned int dob; /* days since 1900 */
			int num_emails; /* number of email addresses */
			char **emails; /* array of email addresses */
		} person;

		struct
		{
			char *alias;
			char *replyaddress;
			char *nameofml;
			char *description;
			int num_members;
			char **members;
		} list;
	};
};

void addressbook_set_description(struct addressbook_entry *entry, char *desc);
void addressbook_set_alias(struct addressbook_entry *entry, char *alias);
int addressbook_person_add_email(struct addressbook_entry *entry, char *email);

void init_addressbook(void);
void cleanup_addressbook(void);
struct addressbook_entry *addressbook_create_person(char *realname, char *email);
struct addressbook_entry *addressbook_new_person(struct addressbook_entry *list, char *realname, char *email);
struct addressbook_entry *addressbook_create_group(void);
struct addressbook_entry *addressbook_new_group(struct addressbook_entry *list);
struct addressbook_entry *addressbook_duplicate_entry(struct addressbook_entry *entry);
void addressbook_free_entry(struct addressbook_entry *entry);
char *addressbook_get_address_str(struct addressbook_entry *entry);
char *addressbook_get_expand_str(char *unexpand);
char *addressbook_complete_address(char *address);

struct addressbook_entry *addressbook_first(struct addressbook_entry *group);
struct addressbook_entry *addressbook_next(struct addressbook_entry *entry);

#endif

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

/*
** addressbook.h
*/

#ifndef SM__ADDRESSBOOK_H
#define SM__ADDRESSBOOK_H

#ifndef SM__LISTS_H
#include "lists.h"
#endif

struct mail;

#define ADDRESSBOOK_ENTRY_GROUP 0
#define ADDRESSBOOK_ENTRY_PERSON 1
#define ADDRESSBOOK_ENTRY_LIST 2

struct address_snail_phone
{
	char *title;
	char *organization;
	char *street;
	char *city;
	char *zip;
	char *state;
	char *country;
	char *phone1;
	char *phone2;
	char *mobil;
	char *fax;
};

/* the addressbook structure (read-only outside from addressbook.c)! */
struct addressbook_entry
{
	struct node node; /* embedded node structure */
	int type; /* see above */

	char *alias;
	char *description;

	union
	{
		struct
		{
			struct list list; /* more addressbook entries */
		} group;

		struct
		{
			char *realname;
			char *pgpid;
			char *homepage;
			char *notepad;
			char *portrait; /* filename to a picture of this person */

			struct address_snail_phone priv;
			struct address_snail_phone work;

			int dob_day; /* day of birth */
			int dob_month; /* month of birth */
			int dob_year; /* year of birth */

			int sex; /* 0 unspecifed, 1 female, 2 male */
			int num_emails; /* number of email addresses */
			char **emails; /* array of email addresses */
		} person;

		struct
		{
			char *replyaddress;
			char *nameofml;
			int num_members;
			char **members;
		} list;
	} u;
};


void addressbook_set_description(struct addressbook_entry *entry, char *desc);
void addressbook_set_alias(struct addressbook_entry *entry, char *alias);
int addressbook_person_add_email(struct addressbook_entry *entry, char *email);

void init_addressbook(void);
void cleanup_addressbook(void);
int addressbook_load(void);
void addressbook_save(void);

void addressbook_insert_tail(struct addressbook_entry *entry, struct addressbook_entry *new_entry);
struct addressbook_entry *addressbook_create_person(char *realname, char *email);
struct addressbook_entry *addressbook_new_person(struct addressbook_entry *list, char *realname, char *email);
struct addressbook_entry *addressbook_create_group(void);
struct addressbook_entry *addressbook_new_group(struct addressbook_entry *list);
struct addressbook_entry *addressbook_duplicate_entry(struct addressbook_entry *entry);
void addressbook_free_entry(struct addressbook_entry *entry);
char *addressbook_get_realname(char *email);
char *addressbook_get_portrait(char *email);
char *addressbook_download_portrait(char *email);
char *addressbook_get_address_str(struct addressbook_entry *entry);
char *addressbook_get_expand_str(char *unexpand);
struct addressbook_entry *addressbook_find_entry_by_address(char *addr);
struct addressbook_entry *addressbook_get_entry_from_mail_header(struct mail *m, char *header);
char *addressbook_completed_by_entry(char *part, struct addressbook_entry *entry, int *type_ptr);
char *addressbook_complete_address(char *address);

struct addressbook_entry *addressbook_first(struct addressbook_entry *group);
struct addressbook_entry *addressbook_next(struct addressbook_entry *entry);

#endif


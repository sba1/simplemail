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

#ifndef SM__CODESETS_H
#include "codesets.h"
#endif

struct mail;

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

struct addressbook_group
{
	struct node node;
	utf8 *name;
	utf8 *description;
};

struct addressbook_entry_new
{
	struct node node;

	char *alias;
	char *description;

	char *realname;
	char *notepad;
	char *pgpid;
	char *homepage;
	char *portrait; /* filename to a picture of this person */

	struct address_snail_phone priv;
	struct address_snail_phone work;

	int dob_day; /* day of birth */
	int dob_month; /* month of birth */
	int dob_year; /* year of birth */

	int sex; /* 0 unspecifed, 1 female, 2 male */

	char **email_array; /* NULL terminated array of emails (use array_xxx() functions) */
	char **group_array; /* NULL terminated array of names of groups (use array_xxx() functions) */
};

/* Entry functions */
struct addressbook_entry_new *addressbook_first_entry(void);
struct addressbook_entry_new *addressbook_next_entry(struct addressbook_entry_new *entry);
struct addressbook_entry_new *addressbook_duplicate_entry_new(struct addressbook_entry_new *entry);
void addressbook_free_entry_new(struct addressbook_entry_new *entry);
char *addressbook_get_entry_completing_part(struct addressbook_entry_new *entry, char *part, int *type_ptr);
struct addressbook_entry_new *addressbook_add_entry(char *realname);
struct addressbook_entry_new *addressbook_add_entry_duplicate(struct addressbook_entry_new *entry);

/* Group functions */
struct addressbook_group *addressbook_first_group(void);
struct addressbook_group *addressbook_next_group(struct addressbook_group *grp);
struct addressbook_group *addressbook_duplicate_group(struct addressbook_group *srcgrp);
void addressbook_free_group(struct addressbook_group *grp);
struct addressbook_group *addressbook_find_group_by_name(utf8 *name);
struct addressbook_group *addressbook_add_group(utf8 *name);
struct addressbook_group *addressbook_add_group_duplicate(struct addressbook_group *group);

/* init and io */
int read_line(FILE *fh, char *buf);
void init_addressbook(void);
void cleanup_addressbook(void);
int addressbook_load(void);
void addressbook_save(void);
void addressbook_save_as(char *filename);
int addressbook_import_sm(char *filename);
int addressbook_import_yam(char *filename);
int addressbook_import_file(char *filename, int append);

/* general */
char *addressbook_get_expanded(char *unexpand);
struct addressbook_entry_new *addressbook_find_entry_by_address(char *email);
struct addressbook_entry_new *addressbook_find_entry_by_alias(char *alias);
struct addressbook_entry_new *addressbook_find_entry_by_realname(char *realname);
char **addressbook_get_array_of_email_addresses(void);
char *addressbook_download_portrait(char *email);
char *addressbook_complete_address(char *address);

#endif


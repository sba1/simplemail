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
 * @file
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

	int sex; /* 0 unspecified, 1 female, 2 male */

	char **email_array; /* NULL terminated array of emails (use array_xxx() functions) */
	char **group_array; /* NULL terminated array of names of groups (use array_xxx() functions) */
};

/* Entry functions */
/**
 * Returns the first address book entry.
 */
struct addressbook_entry_new *addressbook_first_entry(void);

/**
 * Returns the next address book entry.
 */
struct addressbook_entry_new *addressbook_next_entry(struct addressbook_entry_new *entry);

/**
 * Create a duplicate of the given address book entry. The duplicate is not
 * added to the list.
 */
struct addressbook_entry_new *addressbook_duplicate_entry_new(struct addressbook_entry_new *entry);

/**
 * Free the memory associated with the given address book entry.
 */
void addressbook_free_entry_new(struct addressbook_entry_new *entry);

/**
 * Returns the rest of the a partial string with respect to the given entry.
 *
 * @param entry the entry
 * @param part a prefix string that should be completed by some field of the
 *  entry.
 * @param type_ptr will be filled with 0 if the alias has been completed, 1 for the
 *  real name and all greater than 1 the email.
 *
 * @return pointer to the remaining characters or NULL if no match was found.
 *  You must not change the contents of the result!
 */
char *addressbook_get_entry_completing_part(struct addressbook_entry_new *entry, char *part, int *type_ptr);

/**
 * Adds an entry into the address book.
 *
 * @param real name
 * @return the reference to the entry that have just been added.
 */
struct addressbook_entry_new *addressbook_add_entry(const char *realname);

/**
 * Add a duplicate of the given address book entry.
 *
 * @param entry the entry to be duplicated
 * @return the duplicated entry
 */
struct addressbook_entry_new *addressbook_add_entry_duplicate(struct addressbook_entry_new *entry);

/* Group functions */

/**
 * Returns the first address book group.
 */
struct addressbook_group *addressbook_first_group(void);

/**
 * Returns the next address book group.
 *
 * @param grp
 * @return
 */
struct addressbook_group *addressbook_next_group(struct addressbook_group *grp);

/**
 * Duplicates a given address book group.
 *
 * @param srcgrp
 * @return
 */
struct addressbook_group *addressbook_duplicate_group(struct addressbook_group *srcgrp);

/**
 * Free all memory associated to the given address book group.
 *
 * @param grp
 */
void addressbook_free_group(struct addressbook_group *grp);

/**
 * Find an address book group by the given name.
 *
 * @param name
 * @return
 */
struct addressbook_group *addressbook_find_group_by_name(const utf8 *name);

/**
 * Add a new address book group with the given name. Duplicates are allowed.
 *
 * @param name specifies the name of the address book group to be created.
 * @return the reference to the newly created address book group.
 */
struct addressbook_group *addressbook_add_group(const utf8 *name);

/**
 * Adds a duplicate of the given address book group.
 *
 * @param group the group to be added.
 * @return the reference to the newly created group.
 */
struct addressbook_group *addressbook_add_group_duplicate(struct addressbook_group *group);

/* init and io */
/**
 * Initializes the addressbook
 */
int init_addressbook(void);

/**
 * Cleanups the addressbook. Oposite of init_addressbook().
 */
void cleanup_addressbook(void);

/**
 * Clears the addressbook without cleaning it up completely.
 */
void addressbook_clear(void);

/**
 * Load the addressbook. Returns 0 for an error.
 *
 * @return
 */
int addressbook_load(void);

/**
 * Saved the addressbook to the default file
 */
void addressbook_save(void);

/**
 * Saves the addressbok to a given file.
 *
 * @param filename
 */
void addressbook_save_as(char *filename);

/**
 * Loads the Addressbook as the xml format
 *
 * @param filename
 * @return
 */
int addressbook_import_sm(char *filename);

/**
 * Import addressbook entries from YAM.
 *
 * @param filename
 * @return
 */
int addressbook_import_yam(char *filename);

/**
 * Add entries from a specified file. Set append to 1 if the addressbook
 * should be appended. filename mybe NULL which means that the default
 * filename is used. Returns 1 for success.
 * TODO: replace addressbook_load().
 *
 * @param filename
 * @param append
 * @return
 */
int addressbook_import_file(char *filename, int append);

/* general */

/**
 * This function returns an expanded string consisting of phrases and email
 * addresses. It uses the address book for that purpose and performs syntax
 * checks (uses some parse functions). If NULL is returned something had failed.
 *
 * @param unexpand the string to expand, i.e,, a comma separated list of phrases,
 *  email addresses, or both.
 *
 * @return the expanded string.
 */
char *addressbook_get_expanded(char *unexpand);

/**
 * Finds the first address book entry that contains the given address.
 *
 * @param email the mail address that is associated to the address book entry
 *  to be found.
 * @return the first address book entry or NULL if no such entry exists.
 */
struct addressbook_entry_new *addressbook_find_entry_by_address(const char *email);

/**
 * Finds the first address book entry that has the given alias.
 *
 * @param alias the alias of the address book entry to be found.
 * @return the first address book entry or NULL if no such entry exists.
 */
struct addressbook_entry_new *addressbook_find_entry_by_alias(const char *alias);

/**
 * Finds the first address book entry that has the given real name..
 *
 * @param realname
 * @return the first address book entry or NULL if no such entry exists.
 */
struct addressbook_entry_new *addressbook_find_entry_by_realname(const char *realname);

/**
 * Returns a string array of all addresses within the addressbook.
 * array must be free'd with array_free() when no longer used.
 *
 * @return
 */
char **addressbook_get_array_of_email_addresses(void);

/**
 * Returns a path to the portrait of the given e-mail. The returned string
 * is allocated with malloc().
 *
 * @param email
 * @return
 */
char *addressbook_download_portrait(char *email);

/**
 * Completes an groupname/alias/realname/e-mail address of the addressbook
 *
 * @param address
 * @return
 */
char *addressbook_complete_address(char *address);

/*****************************************************************************/

typedef enum
{
	ACNT_GROUP,
	ACNT_ALIAS,
	ACNT_REALNAME,
	ACNT_EMAIL
} addressbook_completion_node_type;

struct addressbook_completion_list
{
	struct list l;

	/** Defines if the completion list is complete */
	int complete;
};

struct addressbook_completion_node
{
	struct node n;

	addressbook_completion_node_type type;

	/** The complete string */
	char *complete;
};

/**
 * Completes an groupname/alias/realname/e-mail address of the addressbook
 *
 * @param address
 * @return
 */
struct addressbook_completion_list *addressbook_complete_address_full(char *address);

/**
 * Frees the list returned by addressbook_complete_address_full().
 *
 * @param cl
 */
void addressbook_completion_list_free(struct addressbook_completion_list *cl);

#endif


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
#ifndef SM__FILTER_H
#define SM__FILTER_H

#include <stdio.h>

#ifndef SM__LISTS_H
#include "lists.h"
#endif

#ifndef SM__BAYERMOORE_H
#include "boyermoore.h"
#endif

#define RULE_FROM_MATCH				0
#define RULE_RCPT_MATCH				1
#define RULE_SUBJECT_MATCH			2
#define RULE_HEADER_MATCH			3
#define RULE_ATTACHMENT_MATCH	4
#define RULE_STATUS_MATCH			5
#define RULE_BODY_MATCH				6

#define RULE_STATUS_NEW			0
#define RULE_STATUS_READ			1
#define RULE_STATUS_UNREAD		2
#define RULE_STATUS_REPLIED	3
#define RULE_STATUS_FORWARDED 4
#define RULE_STATUS_PENDING   5
#define RULE_STATUS_SENT			6

/**
 * @brief A processed filter rule.
 */
struct filter_rule_parsed
{
	struct boyermoore_context *bm_context;
	char *parsed; /* result from sm_parse_pattern() */
};

struct filter_rule
{
	struct node node; /* embedded node structure */
	int type; /* type of the rule */
	int flags; /* flags for the pattern matching rules (see indep-include/support.h/SM_PATTERN_#?) */
	union
	{
		struct {
			char **from; /* NULL terminated array */
			char **from_pat; /* NULL terminated array, result from sm_parse_pattern() */
		} from;
		struct {
		  char **rcpt; /* NULL terminated array */
		  char **rcpt_pat; /* NULL terminated array, result from sm_parse_pattern() */
		} rcpt;
		struct {
			char **subject; /* NULL terminated array */
			char **subject_pat; /* NULL terminated array, result from sm_parse_pattern() */
		} subject;
		struct {
			char *name;
			char *name_pat; /* result from sm_parse_pattern() */
			char **contents; /* NULL terminated array */
			char **contents_pat; /* NULL terminated array, result from sm_parse_pattern() */
		} header;
		struct {
			char *body; /* string */
			struct filter_rule_parsed body_parsed;
		} body;
		struct {
			int status;
		} status;
	} u;
};

struct filter_action
{
	struct node node; /* embedded node structure */
	int type; /* type of that action */
};

#define FILTER_FLAG_REQUEST	(1<<0)
#define FILTER_FLAG_NEW			(1<<1)
#define FILTER_FLAG_SENT			(1<<2)
#define FILTER_FLAG_REMOTE		(1<<3)

struct filter
{
	struct node node; /* embedded node structure */
	char *name; /* name of the filter */
	int flags; /* activity flags of the filter */
	int mode; /* 0=and  -  1=or */
	struct list rules_list; /* list of rules */

	char *dest_folder; /* Move the mail into the given folder */
	int use_dest_folder; /* If set to 1 */

	char *sound_file; /* Play a sound file */
	int use_sound_file; /* If set to 1 */

	char *arexx_file; /* Execute ARexx Script */
	int use_arexx_file; /* If set to 1 */

  int search_filter; /* filter is a search filter (used for the search function) */

	struct list action_list; /* list of actions */
};

/**
 * Creates a new filter instance with default values.
 *
 * @return
 */
struct filter *filter_create(void);

/**
 * Duplicates a filter instance.
 *
 * @param filter
 * @return
 */
struct filter *filter_duplicate(struct filter *filter);

/**
 * Disposes the filter and all associated rules.
 *
 * @param f
 */
void filter_dispose(struct filter *filter);

/**
 * Deinitializes the parsed filter rule. Does not free the rule!
 *
 * @param p
 */
void filter_deinit_rule(struct filter_rule_parsed *p);

/**
 * Initializes the given filter rule for the given str with flags.
 *
 * @param p
 * @param str
 * @param flags
 */
void filter_init_rule(struct filter_rule_parsed *p, char *str, int flags);

/**
 * Match the given str againt the parsed filter.
 *
 * @param p
 * @param str should be 0-byte terminated even if strl is given.
 * @param strl length of the string.
 * @param flags
 * @return
 */
int filter_match_rule_len(struct filter_rule_parsed *p, char *str, int strl, int flags);

/**
 * Adds the given rule to the filter.
 *
 * @param filter
 * @param fr
 */
void filter_add_rule(struct filter *filter, struct filter_rule *fr);

/**
 * Creates and adds a rule of the given type to the given filter.
 *
 * @param filter
 * @param type
 * @return
 */
struct filter_rule *filter_create_and_add_rule(struct filter *filter, int type);

/**
 * Adds the copy of the given text to the filter rule if rule
 * is a text rule.
 *
 * @param r
 * @param text
 */
void filter_rule_add_copy_of_string(struct filter_rule *fr, char *text);

/**
 * Create a filter rule that matches the given set of strings.
 *
 * @param strings
 * @param num_strings
 * @param flags
 * @return
 */
struct filter_rule *filter_rule_create_from_strings(char **strings, int num_strings, int flags);

/**
 * Create a filter rule from common sorted recipients.
 *
 * @param addresses an array to an array of sorted recipients.
 * @param num_addresses length of addresses.
 * @param type
 * @return
 */

struct filter_rule *filter_rule_create_from_common_sorted_recipients(char ***addresses, unsigned int num_addresses);

enum filter_rule_create_type
{
	FRCT_RECEPIENTS,
	FRCT_SUBJECT,
	FRCT_FROM
};

struct mail_info;

/**
 * Create filter rule of the specific type from a mail iteration.
 *
 * @param get_first_mail_info
 * @param get_next_mail_info
 * @param num_mails
 * @param type
 * @return
 */
struct filter_rule *filter_rule_create_from_mail_iterator(enum filter_rule_create_type type, int num_mails,
												  struct mail_info * (*get_first_mail_info)(void *handle, void *userdata),
												  struct mail_info * (*get_next_mail_info)(void *handle, void *userdata),
												  void *userdata);

/**
 * Return the num-th rule of the given filter.
 *
 * @param filter
 * @param num
 * @return
 */
struct filter_rule *filter_find_rule(struct filter *filter, int num);

/**
 * Remove the rule from its filter.
 *
 * @param fr
 */
void filter_remove_rule(struct filter_rule *fr);


/**
 * Preprocesses the pattern of all rules of the given filter
 *
 * @param f the filter of which the rules should be processed
 */
void filter_parse_filter_rules(struct filter *f);

/**
 * Preparse the pattern of all filters.
 */
void filter_parse_all_filters(void);

/**
 * Returns the num-th action of the given filter.
 *
 * @param filter
 * @param num
 * @return
 */
struct filter_action *filter_find_action(struct filter *filter, int num);

/**
 * Clear the global configuration filter list.
 */
void filter_list_clear(void);

/**
 * Adds a duplicate of the filter to the global filter list
 *
 * @param f
 */
void filter_list_add_duplicate(struct filter *);

/**
 * Get the first filter of the global filter list.
 *
 * @return
 */
struct filter *filter_list_first(void);

/**
 * Returns the next filter of the given filter.
 *
 * @param f
 * @return
 */
struct filter *filter_list_next(struct filter *f);

/**
 * Returns whether the global filter list contains any remote filters.
 *
 * @return
 */
int filter_list_has_remote(void);

/**
 * Loads the filter list from the already fopen()'ed filehandle.
 *
 * @param fh
 */
void filter_list_load(FILE *fh);

/**
 * Saves the filter list into the given handle that has been opened with
 * fopen() before.
 *
 * @param fh
 */
void filter_list_save(FILE *fh);

struct search_options
{
	char *folder; /* NULL means all folders */
	char *from;
	char *to;
	char *subject;
	char *body;
};

/**
 * Duplicates the given search option.
 *
 * @param so
 * @return
 */
struct search_options *search_options_duplicate(struct search_options *so);

/**
 * Frees all memory associated with the given search options.
 *
 * @param so
 */
void search_options_free(struct search_options *so);

/**
 * Create a filter from search options.
 *
 * @param sopt the search options from which to create the filter.
 *
 * @return the created filter
 */
struct filter *filter_create_from_search_options(struct search_options *sopt);


#endif

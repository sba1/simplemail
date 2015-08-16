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

struct filter *filter_create(void);
struct filter *filter_duplicate(struct filter *filter);
void filter_dispose(struct filter *filter);

void filter_deinit_rule(struct filter_rule_parsed *p);
void filter_init_rule(struct filter_rule_parsed *p, char *str, int flags);
int filter_match_rule_len(struct filter_rule_parsed *p, char *str, int strl, int flags);

void filter_add_rule(struct filter *filter, struct filter_rule *fr);
struct filter_rule *filter_create_and_add_rule(struct filter *filter, int type);
void filter_rule_add_copy_of_string(struct filter_rule *fr, char *text);
struct filter_rule *filter_rule_create_from_strings(char **strings, int num_strings, int flags);
struct filter_rule *filter_rule_create_from_common_sorted_recipients(char ***addresses, unsigned int num_addresses);

enum filter_rule_create_type
{
	FRCT_RECEPIENTS,
	FRCT_SUBJECT,
	FRCT_FROM
};

struct mail_info;
struct filter_rule *filter_rule_create_from_mail_iterator(enum filter_rule_create_type type, int num_mails,
												  struct mail_info * (*get_first_mail_info)(void *handle, void *userdata),
												  struct mail_info * (*get_next_mail_info)(void *handle, void *userdata),
												  void *userdata);
struct filter_rule *filter_find_rule(struct filter *filter, int num);
char *filter_get_rule_string(struct filter_rule *rule);
void filter_remove_rule(struct filter_rule *fr);
void filter_parse_filter_rules(struct filter *f);
void filter_parse_all_filters(void);

struct filter_action *filter_find_action(struct filter *filter, int num);

void filter_list_clear(void);
void filter_list_add_duplicate(struct filter *);
struct filter *filter_list_first(void);
struct filter *filter_list_next(struct filter *f);
int filter_list_has_remote(void);
void filter_list_load(FILE *fh);
void filter_list_save(FILE *fh);

struct search_options
{
	char *folder; /* NULL means all folders */
	char *from;
	char *to;
	char *subject;
	char *body;
};

struct search_options *search_options_duplicate(struct search_options *so);
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

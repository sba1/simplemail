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

#ifndef SM__LISTS_H
#include "lists.h"
#endif

#define RULE_FROM_MATCH				0
#define RULE_SUBJECT_MATCH			1
#define RULE_HEADER_MATCH			2
#define RULE_ATTACHMENT_MATCH	3
#define RULE_STATUS_MATCH			4

#define RULE_STATUS_NEW			0
#define RULE_STATUS_READ			1
#define RULE_STATUS_UNREAD		2
#define RULE_STATUS_REPLIED	3
#define RULE_STATUS_FORWARDED 4
#define RULE_STATUS_PENDING   5
#define RULE_STATUS_SENT			6

struct filter_rule
{
	struct node node; /* embedded node structure */
	int type; /* type of the rule */
	union
	{
		struct {
			char **from; /* NULL terminated array */
		} from;
		struct {
			char **subject; /* NULL terminated array */
		} subject;
		struct {
			char *name;
			char **contents; /* NULL terminated array */
		} header;
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

struct filter
{
	struct node node; /* embedded node structure */
	char *name; /* name of the filter */
	int flags; /* filter is active */
	int mode; /* 0=and  -  1=or */
	struct list rules_list; /* list of rules */

	char *dest_folder; /* Move the mail into the given folder */
	int use_dest_folder; /* If set to 1 */

	struct list action_list; /* list of actions */
};

struct filter *filter_create(void);
struct filter *filter_duplicate(struct filter *filter);
void filter_dispose(struct filter *filter);

struct filter_rule *filter_create_and_add_rule(struct filter *filter, int type);
struct filter_rule *filter_find_fule(struct filter *filter, int num);
char *filter_get_rule_string(struct filter_rule *rule);
void filter_remove_rule(struct filter_rule *fr);

struct filter_action *filter_find_action(struct filter *filter, int num);

void filter_list_clear(void);
void filter_list_add_duplicate(struct filter *);
struct filter *filter_list_first(void);
struct filter *filter_list_next(struct filter *f);
void filter_list_load(FILE *fh);
void filter_list_save(FILE *fh);

#endif

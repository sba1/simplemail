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
** composewnd.h
*/

#ifndef COMPOSEWND_H
#define COMPOSEWND_H

struct compose_args
{
//	char *to_str; /* the initial to string */
	struct mail *to_change; /* this mail is changed (can be freed afterwards) might be NULL */

	struct mail *ref_mail;	/* the status of this mail is changed after successful editing */
	int action;						/* how the status will be changed */
};

#define COMPOSE_ACTION_NEW     0
#define COMPOSE_ACTION_EDIT    1
#define COMPOSE_ACTION_REPLY   2
#define COMPOSE_ACTION_FORWARD 3

void compose_window_open(struct compose_args *args);

#endif

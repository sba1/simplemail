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
** pop3.h
*/

#ifndef POP3_H
#define POP3_H

struct pop3_server
{
	struct node node;

	char *name;
	unsigned int port;
	char *login;
	char *passwd;
	int del; /* 1 if downloaded mails should be deleted */
};

int pop3_dl(struct list *pop_list, char *dest_dir, int receive_preselection, int receive_size);
int pop3_login_only(struct pop3_server *);

struct pop3_server *pop_malloc(void);
struct pop3_server *pop_duplicate(struct pop3_server *pop);
void pop_free(struct pop3_server *pop);

#endif
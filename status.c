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
** status.c
*/

#include <stdio.h>

#include "estimate.h"
#include "smintl.h"
#include "status.h"

#include "statuswnd.h"

static int is_open;

/******************************************************************
 Initialize the window with as the given type
*******************************************************************/
void status_init(int type)
{
	status_mail_list_clear();
}

/******************************************************************
 Opens the status window. Type indicates the type of the status
 window
*******************************************************************/
int status_open(void)
{
	return statuswnd_open(1);
}

/******************************************************************
 ...
*******************************************************************/
void status_close(void)
{
	statuswnd_close();
}

/******************************************************************
 Set the title of the status window
*******************************************************************/
void status_set_title(char *title)
{
	statuswnd_set_title(title);
}

/******************************************************************
 Set the status line
*******************************************************************/
void status_set_line(char *str)
{
}

/******************************************************************
 Set the status line for connecting to a server
*******************************************************************/
void status_set_connect_to_server(char *server)
{
	static char buf[400];
	sprintf(buf,_("Connecting to server %s..."),server);
	status_set_line(buf);
}

static struct estimate gauge_est;
static int gauge_maximal;
static int gauge_value;

/******************************************************************
 Sets the gauge to a maximal value and tell that it's a bytes
 gauge
*******************************************************************/
void status_init_gauge_as_bytes(int maximal)
{
	estimate_init(&gauge_est,gauge_maximal/1024);
	gauge_maximal = maximal;
	gauge_value = 0;
}

/******************************************************************
 Sets the gauge current value
*******************************************************************/
void status_set_gauge(int value)
{
	gauge_value = value;
	estimate_calc_remaining(&gauge_est,value/1024);
}

static int mail_maximal;
static int mail_current; /* starts at 1 */

/******************************************************************
 Initialize the number of mails to download
*******************************************************************/
void status_init_mail(int maximal)
{
	mail_maximal = maximal;
	mail_current = 0;
}

/******************************************************************
 Set the current mail
*******************************************************************/
void status_set_mail(int current)
{
	mail_current = current;
}

/******************************************************************
 Insert a mail into the status mail list which turns it into
 a mail selection window
*******************************************************************/
void status_mail_list_insert(int mno, int mflags, int msize)
{
}

/******************************************************************
 Set some additional infos to the given mail. The mail has to be
 inserted before.
*******************************************************************/
void status_mail_list_set_info(int mno, char *from, char *subject, char *date)
{
}

/******************************************************************
 Returns mail flags for the given mail. Returns -1 if mail is not
 in the mail selection
*******************************************************************/
int status_mail_list_get_flags(int mno)
{
	return -1;
}

/******************************************************************
 Clears the mail list
*******************************************************************/
void status_mail_list_clear(void)
{
}

/******************************************************************
 Freezes the mail list.
*******************************************************************/
void status_mail_list_freeze(void)
{
}

/******************************************************************
 Thaws the mail list.
*******************************************************************/
void status_mail_list_thaw(void)
{
}

/******************************************************************
 Wait's for user interaction. Returns 0 for negative respone
 (aborting) otherwise another value
*******************************************************************/
int status_wait(void)
{
	return 0;
}

/******************************************************************
 Returns 0 if user has aborted the statistic listing
*******************************************************************/
int status_more_statitics(void)
{
	return 0;
}

/******************************************************************
 Check if the current transfering should be aborted
*******************************************************************/
int status_check_abort(void)
{
	return 0;
}


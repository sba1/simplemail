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
#include <stdlib.h>
#include <string.h>

#include "estimate.h"
#include "smintl.h"
#include "status.h"
#include "support.h"
#include "support_indep.h"

#include "statuswnd.h"

static int is_open;
static struct estimate gauge_est;
static int gauge_maximal;
static int gauge_value;
static int mail_maximal;
static int mail_current; /* starts at 1 */
static int mail_current_size; /* size of current mail */
static char *status_text;

/******************************************************************
 Returns the status text
*******************************************************************/
static char *status_get_status(void)
{
	static char buf[512];
	static char time_buf[256];

	int time = estimate_calc_remaining(&gauge_est,gauge_value/1024);
	if (time < 60) sprintf(time_buf,_("%d s"),time);
	else sprintf(time_buf,_("%d min %d s"),time/60,time%60);

	if (status_text) sprintf(buf,"%s\n%s %s",status_text,_("Time remaining:"),time_buf);
	else return time_buf;
	return buf;
}

static char *status_get_time_str(void)
{
	static char time_buf[64];
	int time = estimate_calc_remaining(&gauge_est,gauge_value/1024);

	if (time < 60) sprintf(time_buf,_("%d s"),time);
	else sprintf(time_buf,_("%d min %d s"),time/60,time%60);

	return time_buf;
}

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
 Set the status line for connecting to a server
*******************************************************************/
void status_set_connect_to_server(char *server)
{
	static char buf[300];
	sprintf(buf,_("Connecting to server %s..."),server);
	statuswnd_set_head(buf);
}

/******************************************************************
 Sets the gauge to a maximal value and tell that it's a bytes
 gauge
*******************************************************************/
void status_init_gauge_as_bytes(int maximal)
{
	estimate_init(&gauge_est,maximal/1024);
	gauge_maximal = maximal;
	gauge_value = 0;
	statuswnd_init_gauge(maximal);
}

/******************************************************************
 Sets the gauge current value
*******************************************************************/
void status_set_gauge(int value)
{
	static char status_buf[100];
	static char gauge_buf[256];
	static int last_seconds;
	int seconds = sm_get_current_seconds();

	gauge_value = value;

	if (last_seconds == seconds) return;

	sprintf(gauge_buf,_("%d KB / %d KB (time left: %s)"), gauge_value / 1024, gauge_maximal / 1024, status_get_time_str());

	statuswnd_set_gauge(value);
	statuswnd_set_gauge_text(gauge_buf);

	sprintf(status_buf,_("Processing mail %d (%d KB) of %d"),mail_current,mail_current_size / 1024, mail_maximal);
	statuswnd_set_status(status_buf);

	last_seconds = seconds;
}

/******************************************************************
 Set the status line
*******************************************************************/
void status_set_line(char *str)
{
	if (status_text) free(status_text);
	status_text = mystrdup(str);
	statuswnd_set_status(status_get_status());
}

void status_set_status(char *str)
{
	static char *status_text;
	free(status_text);
	status_text = mystrdup(str);
	statuswnd_set_status(status_text);
}

/******************************************************************
 Set the head of the status window
*******************************************************************/
void status_set_head(char *head)
{
	static char *status_head;
	free(status_head);
	status_head = mystrdup(head);
	statuswnd_set_head(status_head);
}

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
void status_set_mail(int current, int current_size)
{
//	static char buf[100];
	mail_current = current;
	mail_current_size = current_size;

//	sprintf(buf,_("%d mails to go"), mail_maximal - mail_current + 1);
//	statuswnd_set_gauge_text(buf);
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


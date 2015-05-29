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
 * @file status.c
 *
 * @note TODO: This status stuff needs an overhaul.
 */

#include "status.h"

#include <stdio.h>
#include <stdlib.h>

#include "atcleanup.h"
#include "estimate.h"
#include "smintl.h"
#include "support_indep.h"

#include "mainwnd.h"
#include "statuswnd.h"
#include "subthreads.h"
#include "support.h"
#include "timesupport.h"

static struct estimate gauge_est;
static int gauge_maximal;
static int gauge_value;
static int mail_maximal;
static int mail_current; /* starts at 1 */
static int mail_current_size; /* size of current mail */
static char *status_text;
static char *status_head;
static char *status_line;

static int status_cleanup_registered;

/**
 * Cleanup resources.
 *
 * @param user_data
 */
static void status_cleanup(void *user_data)
{
	free(status_line);
	free(status_text);
	free(status_head);
}

/******************************************************************
 Returns the status text
*******************************************************************/
static char *status_get_status(char *msg)
{
	static char buf[128];
	static char time_buf[64];

	int time = estimate_calc_remaining(&gauge_est,gauge_value/1024);
	if (time < 60) sm_snprintf(time_buf,sizeof(time_buf),_("%d s"),time);
	else sm_snprintf(time_buf,sizeof(time_buf),_("%d min %d s"),time/60,time%60);

	if (msg) sm_snprintf(buf,sizeof(buf),"%s\n%s %s",msg,_("Time remaining:"),time_buf);
	else return time_buf;
	return buf;
}

static char *status_get_time_str(void)
{
	static char time_buf[64];
	int time = estimate_calc_remaining(&gauge_est,gauge_value/1024);

	if (time < 60) sm_snprintf(time_buf,sizeof(time_buf),_("%d s"),time);
	else sm_snprintf(time_buf,sizeof(time_buf),_("%d min %d s"),time/60,time%60);

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
 Opens the status window.
*******************************************************************/
int status_open(void)
{
	return statuswnd_open(1);
}

/******************************************************************
 Opens the status window but doesn't not activate it.
*******************************************************************/
int status_open_notactivated(void)
{
	return statuswnd_open(0);
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
void status_set_title(const char *title)
{
	statuswnd_set_title(title);
}

/******************************************************************
 Set the title of the status window
*******************************************************************/
void status_set_title_utf8(const char *title)
{
	statuswnd_set_title_utf8(title);
}

/******************************************************************
 Set the status line for connecting to a server
*******************************************************************/
void status_set_connect_to_server(const char *server)
{
	static char buf[80];
	sm_snprintf(buf,sizeof(buf),_("Connecting to server %s..."),server);
	statuswnd_set_status(buf);
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

	sm_snprintf(gauge_buf,sizeof(gauge_buf),_("%d KB / %d KB (time left: %s)"), gauge_value / 1024, gauge_maximal / 1024, status_get_time_str());

	statuswnd_set_gauge(value);
	statuswnd_set_gauge_text(gauge_buf);

	sm_snprintf(status_buf,sizeof(status_buf),_("Processing mail %d (%d KB) of %d"),mail_current,mail_current_size / 1024, mail_maximal);
	statuswnd_set_status(status_buf);

	last_seconds = seconds;
}

/******************************************************************
 Set the status line
*******************************************************************/
void status_set_line(char *str)
{
	free(status_line);
	status_line = mystrdup(str);
	statuswnd_set_status(status_get_status(status_line));

	if (!status_cleanup_registered)
	{
		atcleanup(status_cleanup,NULL);
		status_cleanup_registered = 1;
	}
}

/**
 * Sets the status. Must only be called in context of the main thread.
 *
 * @param str the status text
 */
static void status_set_status_really(const char *str)
{
	free(status_text);
	status_text = mystrdup(str);
	statuswnd_set_status(status_text);
	main_set_status_text(status_text);

	if (!status_cleanup_registered)
	{
		atcleanup(status_cleanup,NULL);
		status_cleanup_registered = 1;
	}
}

/**
 * Set the status text. It is safe to call this function from
 * arbitrary threads.
 *
 * @param str the status text.
 */
void status_set_status(const char *str)
{
	thread_call_function_sync(thread_get_main(), status_set_status_really, 1, str);
}

/******************************************************************
 Set the head of the status window
*******************************************************************/
void status_set_head(const char *head)
{
	free(status_head);
	status_head = mystrdup(head);
	statuswnd_set_head(status_head);

	if (!status_cleanup_registered)
	{
		atcleanup(status_cleanup,NULL);
		status_cleanup_registered = 1;
	}
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
	mail_current = current;
	mail_current_size = current_size;
}

/******************************************************************
 Insert a mail into the status mail list which turns it into
 a mail selection window
*******************************************************************/
void status_mail_list_insert(int mno, int mflags, int msize)
{
	statuswnd_mail_list_insert(mno,mflags,msize);
}


/******************************************************************
 Set the flags of the given mail number
*******************************************************************/
void status_mail_list_set_flags(int mno, int mflags)
{
	statuswnd_mail_list_set_flags(mno,mflags);
}

/******************************************************************
 Set some additional infos to the given mail. The mail has to be
 inserted before.
*******************************************************************/
void status_mail_list_set_info(int mno, char *from, char *subject, char *date)
{
	statuswnd_mail_list_set_info(mno,from,subject,date);
}

/******************************************************************
 Returns mail flags for the given mail. Returns -1 if mail is not
 in the mail selection
*******************************************************************/
int status_mail_list_get_flags(int mno)
{
	return statuswnd_mail_list_get_flags(mno);
}

/******************************************************************
 Clears the mail list
*******************************************************************/
void status_mail_list_clear(void)
{
	statuswnd_mail_list_clear();
}

/******************************************************************
 Freezes the mail list.
*******************************************************************/
void status_mail_list_freeze(void)
{
	statuswnd_mail_list_freeze();
}

/******************************************************************
 Thaws the mail list.
*******************************************************************/
void status_mail_list_thaw(void)
{
	statuswnd_mail_list_thaw();
}

/******************************************************************
 Wait's for user interaction. Returns 0 for negative respone
 (aborting) otherwise another value
*******************************************************************/
int status_wait(void)
{
	return statuswnd_wait();
}

/******************************************************************
 Returns wheather the current server should be skipped
*******************************************************************/
int status_skipped(void)
{
	return statuswnd_skipped();
}

/******************************************************************
 Returns 0 if user has aborted the statistic listing
*******************************************************************/
int status_more_statistics(void)
{
	return statuswnd_more_statistics();
}

/******************************************************************
 Check if the current transfering should be aborted
*******************************************************************/
int status_check_abort(void)
{
	return 0;
}


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

#include "timesupport.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libraries/locale.h>

#include <proto/intuition.h>
#include <proto/utility.h>

#include "configuration.h"
#include "support.h"

#include "amigasupport.h"

/*****************************************************************************/

unsigned int sm_get_seconds(int day, int month, int year)
{
	struct ClockData cd;
	if (year < 1978) return 0;
	cd.sec = cd.min = cd.hour = 0;
	cd.mday = day;
	cd.month = month;
	cd.year = year;
	return Date2Amiga(&cd);
}

/*****************************************************************************/

int sm_get_gmt_offset(void)
{
	extern struct Locale *DefaultLocale;
	if (DefaultLocale)
	{
		return -DefaultLocale->loc_GMTOffset + (user.config.dst * 60);
	}
	return (user.config.dst * 60);
}

/*****************************************************************************/

unsigned int sm_get_current_seconds(void)
{
	ULONG mics,secs;
	CurrentTime(&secs,&mics);
	return secs;
}

/*****************************************************************************/

unsigned int sm_get_current_micros(void)
{
	ULONG mics,secs;
	CurrentTime(&secs,&mics);
	return mics;
}

/*****************************************************************************/

char *sm_get_date_long_str(unsigned int seconds)
{
	static char buf[128];
	SecondsToStringLong(buf,seconds);
	return buf;
}

/*****************************************************************************/

char *sm_get_date_long_str_utf8(unsigned int seconds)
{
	static char buf[128];
	char *utf8;

	SecondsToStringLong(buf,seconds);

	if ((utf8 = (char*)utf8create(buf, user.config.default_codeset?user.config.default_codeset->name:NULL)))
	{
		strcpy(buf,utf8);
		free(utf8);
	}

	return buf;
}

/*****************************************************************************/

char *sm_get_date_str(unsigned int seconds)
{
	static char buf[128];
	SecondsToDateString(buf,seconds);
	return buf;
}

/*****************************************************************************/

char *sm_get_time_str(unsigned int seconds)
{
	static char buf[128];
	SecondsToTimeString(buf,seconds);
	return buf;
}

/*****************************************************************************/

void sm_convert_seconds(unsigned int seconds, struct tm *tm)
{
	struct ClockData cd;
	Amiga2Date(seconds,&cd);
	tm->tm_sec = cd.sec;
	tm->tm_min = cd.min;
	tm->tm_hour = cd.hour;
	tm->tm_mday = cd.mday;
	tm->tm_mon = cd.month - 1;
	tm->tm_year = cd.year - 1900;
	tm->tm_wday = cd.wday;
}


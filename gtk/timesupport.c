#include "timesupport.h"


#include <glib.h>
#include <sys/time.h>
#include <time.h>

/*****************************************************************************/

unsigned int sm_get_seconds(int day, int month, int year)
{
	struct tm tm;
	time_t t;

	if (year < 1978) return 0;

	tm.tm_sec = 0;
	tm.tm_min = 0;
	tm.tm_hour = 0;
	tm.tm_mday = day;
	tm.tm_mon = month - 1;
	tm.tm_year = year - 1900;
	tm.tm_isdst = -1;

	t = mktime(&tm);

	tm.tm_sec = 0;
	tm.tm_min = 0;
	tm.tm_hour = 0;
	tm.tm_mday = 1;
	tm.tm_mon = 0;
	tm.tm_year = 78;
	tm.tm_isdst = 0;
	t -= mktime(&tm);
	return (unsigned int)t;
}

/*****************************************************************************/

int sm_get_gmt_offset(void)
{
	struct timezone tz;
	struct timeval tv;

	gettimeofday(&tv,&tz);
	return tz.tz_minuteswest;
}

/*****************************************************************************/

unsigned int sm_get_current_seconds(void)
{
	struct timezone tz;
	struct timeval tv;

	gettimeofday(&tv,&tz);
	return tv.tv_sec;
}

/*****************************************************************************/

unsigned int sm_get_current_micros(void)
{
	struct timezone tz;
	struct timeval tv;

	gettimeofday(&tv,&tz);
	return tv.tv_usec;
}

/*****************************************************************************/

char *sm_get_date_long_str(unsigned int seconds)
{
	static char buf[128];

	return buf;
}

/*****************************************************************************/

char *sm_get_date_long_str_utf8(unsigned int seconds)
{
	return sm_get_date_long_str(seconds);
}

/*****************************************************************************/

char *sm_get_date_str(unsigned int seconds)
{
	static char buf[128];
	GDate *date;
	struct tm tm;
	time_t t = seconds;

	tm.tm_sec = 0;
	tm.tm_min = 0;
	tm.tm_hour = 0;
	tm.tm_mday = 1;
	tm.tm_mon = 0;
	tm.tm_year = 78;
	tm.tm_isdst = 0;
	t += mktime(&tm);

	date = g_date_new();
	g_date_set_time_t(date,t);
	g_date_strftime(buf,sizeof(buf),"%x",date);
	g_date_free(date);

	return buf;
}

/*****************************************************************************/

char *sm_get_time_str(unsigned int seconds)
{
	static char buf[128];
	return buf;
}

/*****************************************************************************/

void sm_convert_seconds(unsigned int seconds, struct tm *tm)
{
	time_t t = seconds;
	localtime_r(&t, tm);
}


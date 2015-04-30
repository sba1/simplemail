/**
 * @file
 */
#ifndef SM__TIMESUPPORT_H
#define SM__TIMESUPPORT_H

struct tm;

/**
 * Returns the seconds since 1978
 *
 * @param day the day
 * @param month the month
 * @param year the year
 * @return the seconds since 1978.
 */
unsigned int sm_get_seconds(int day, int month, int year);

/**
 * Returns the GMT offset in minutes.
 *
 * @return the offset from GMT in minutes.
 */
int sm_get_gmt_offset(void);

/**
 * Returns the current seconds since 1978
 *
 * @return the second since 1978.
 */
unsigned int sm_get_current_seconds(void);

/**
 * Returns the current micro seconds.
 *
 * @return the current micro seconds.
 */
unsigned int sm_get_current_micros(void);

/**
 * Convert seconds since 1978 to a tm
 *
 * @param seconds the seconds since 1978 to be converted
 * @param tm the tm to which the result is written
 */
void sm_convert_seconds(unsigned int seconds, struct tm *tm);

/**
 * Convert seconds since 1978 to a long form string. The returned string is
 * static, i.e., no two threads should call this function simultaneously.
 *
 * @param seconds since 1978
 * @param tm
 */
char *sm_get_date_long_str(unsigned int seconds);

/**
 * Convert seconds since 1978 to a long form UTF8 string. The returned string is
 * static, i.e., no two threads should call this function simultaneously.
 *
 * @param seconds the seconds since 1978
 * @return the string containing the data in long format
 */
char *sm_get_date_long_str_utf8(unsigned int seconds);


/**
 * Convert seconds since 1978 to a date string. The returned string is static.
 *
 * @param seconds the seconds since 1978
 * @return the string containing the date
 */
char *sm_get_date_str(unsigned int seconds);

/**
 * Convert seconds since 1978 to a time string. The returned string is static.
 *
 * @param seconds the seconds since 1978
 * @return the string containing the time
 */
char *sm_get_time_str(unsigned int seconds);

#endif

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
** support.c
*/

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "errorwnd.h"
#include "subthreads.h"
#include "support.h"
#include "support_indep.h"

/******************************************************************
 Creates a directory including all necessaries parent directories.
 Nothing will happen if the directory already exists
*******************************************************************/
int sm_makedir(char *path)
{
	int rc;

#define MKDIR_FLAGS 0xffff //O_READ|O_WRITE

	if (mkdir(path,MKDIR_FLAGS) == 0) return 1;
	if (errno == EEXIST) return 1;

	rc = 0;

	if (errno == ENOENT)
	{
		char *buf = (char*)malloc(strlen(path)+1);
		if (buf)
		{
			char *buf2;
			char *last_buf = NULL;
			int times = 0; /* how many paths has been splitted? */

			rc = 1;

			strcpy(buf,path);

			/* split every path */
			while ((buf2 = sm_path_part(buf)))
			{
				/* path cannot be be splitted more */
				if (buf2 == last_buf) break;
				times++;

				/* clear the '/' sign */
				*buf2 = 0;

				/* try if directory can be created */
				if (mkdir(buf,MKDIR_FLAGS) == 0) break; /* yes, so leave the loop */
				if (errno != ENOENT) break;
				last_buf = buf2;
			}

			/* put back the '/' sign and create the directories */
			while(times--)
			{
				buf[strlen(buf)] = '/';
				if (mkdir(buf,MKDIR_FLAGS) != 0) break;
			}

			free(buf);
		}
	}
	return rc;
}

/******************************************************************
 Returns the seconds since 1978
*******************************************************************/
unsigned int sm_get_seconds(int day, int month, int year)
{
	struct tm tm;
	time_t t;

	if (year < 1978) return 0;

	tm.tm_sec = 0;
	tm.tm_min = 0;
	tm.tm_hour = 0;
	tm.tm_mday = day;
	tm.tm_mon = month;
	tm.tm_year = year;
	tm.tm_isdst = -1;

	t = mktime(&tm);

	tm.tm_sec = 0;
	tm.tm_min = 0;
	tm.tm_hour = 0;
	tm.tm_mday = 1;
	tm.tm_mon = 0;
	tm.tm_year = 1978;
	tm.tm_isdst = 0;
	
	t -= mktime(&tm);
	return (unsigned int)t;
}

/******************************************************************
 Returns the GMT offset in minutes
*******************************************************************/
int sm_get_gmt_offset(void)
{
	struct timezone tz;
	struct timeval tv;

	gettimeofday(&tv,&tz);
	return tz.tz_minuteswest;
}

/******************************************************************
 Add a filename component to the drawer
*******************************************************************/
int sm_add_part(char *drawer, const char *filename, int buf_size)
{
	int drawer_len = strlen(drawer);

	if (drawer_len)
	{
		if (drawer[drawer_len-1] != '/')
		{
			if (drawer_len + 1 >= buf_size) return 0;
			drawer[drawer_len] = '/';
			drawer_len++;
		}
	}
	mystrlcpy(&drawer[drawer_len],filename,buf_size - drawer_len);
	return 1;
}

/******************************************************************
 Return the file component of a path
*******************************************************************/
char *sm_file_part(char *filename)
{
	int filename_len = strlen(filename);
	if (!filename_len) return filename;
	if (filename[filename_len-1] == '/') filename_len--;
	while (filename_len)
	{
		if (filename[filename_len-1] == '/') break;
		filename_len--;
	}
	return &filename[filename_len];
}

/******************************************************************
 Return the character after the last path component
*******************************************************************/
char *sm_path_part(char *filename)
{
	char *file_part = sm_file_part(filename);
	if (file_part == filename) return filename;
	return file_part-1;
}

/******************************************************************
 Opens a requester. Returns 0 if the rightmost gadgets is pressed
 otherwise the position of the gadget from left to right
*******************************************************************/
int sm_request(char *title, char *text, char *gadgets, ...)
{
	printf("sm_request()\n");
	return 0;
}

/******************************************************************
 Tells an error message
*******************************************************************/
void tell(char *str)
{
	printf("%s\n",str);
	error_add_message(str);
}

/******************************************************************
 Tells an error message from a subtask
*******************************************************************/
void tell_from_subtask(char *str)
{
	thread_call_parent_function_sync(tell,1,str);
}




















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
** support.c - functions which are used in gui independed parts but
**             which uses things which are not ansi c or unix compatible
*/

#include <string.h>

#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/utility.h>

#include "support.h"

/******************************************************************
 Creates a directory including all necessaries parent directories.
 Nothing will happen if the directory already exists
 (should be in a sperate file, because of many many AmigaOS
 functions but I'm too lazy atm ;-) )
*******************************************************************/
int sm_makedir(char *path)
{
	int rc;
	BPTR lock = Lock(path,ACCESS_READ);
	if (lock)
	{
		UnLock(lock);
		return 1;
	}

	if ((lock = CreateDir(path)))
	{
		UnLock(lock);
		return 1;
	}

	rc = 0;

	if (IoErr() == 205) /* Object not found */
	{
		char *buf = (char*)AllocVec(strlen(path)+1,MEMF_CLEAR|MEMF_REVERSE);
		if (buf)
		{
			char *buf2;
			char *last_buf = NULL;
			int times = 0; /* how many paths has been splitted? */

			rc = 1;

			strcpy(buf,path);

			/* split every path */
			while ((buf2 = PathPart(buf)))
			{
				/* path cannot be be splitted more */
				if (buf2 == last_buf) break;
				times++;

				/* clear the '/' sign */
				*buf2 = 0;

				/* try if directory can be created */
				if ((lock = CreateDir(buf)))
				{
					/* yes, so leave the loop */
					UnLock(lock);
					break;
				}
				if (IoErr() != 205) break;
				last_buf = buf2;
			}

			/* put back the '/' sign and create the directories */
			while(times--)
			{
				buf[strlen(buf)] = '/';

				if ((lock = CreateDir(buf))) UnLock(lock);
				else
				{
					/* an error has happened */
					rc = 0;
					break;
				}
			}

			FreeVec(buf);
		}
	}
	return rc;
}

/******************************************************************
 Returns the seconds since 1978
*******************************************************************/
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

/******************************************************************
 Add a filename component to the drawer
*******************************************************************/
int sm_add_part(char *drawer, const char *filename, int buf_size)
{
	AddPart(drawer,filename,buf_size);
	return 1;
}

/******************************************************************
 Compares a string case insensitive. Accepts NULL pointers
*******************************************************************/
int mystricmp(const char *str1, const char *str2)
{
	if (!str1)
	{
		if (str2) return -1;
		return 0;
	}

	if (!str2) return 1;

	return Stricmp(str1,str2);
}

/******************************************************************
 Compares a string case insensitive and n characters.
 Accepts NULL pointers
*******************************************************************/
int mystrnicmp(const char *str1, const char *str2, int n)
{
	if (!str1)
	{
		if (str2) return -1;
		return 0;
	}

	if (!str2) return 1;

	return Strnicmp(str1,str2,n);
}

/******************************************************************
 returns the length of a string. Accepts NULL pointer (returns 0
 then)
*******************************************************************/
unsigned int mystrlen(const char *str)
{
	if (!str) return 0;
	return strlen(str);
}

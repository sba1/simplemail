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
 * @file taglines.c
 */

#include "taglines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "support_indep.h"

#include "timesupport.h"

#define TAGLINES_INDEX_VERSION 0

static int taglines_num;
static int *taglines_positions;
static char *taglines_filename;
static char *taglines_indexname;

/**
 * Returns a random number uniformly distributed in [0,max-1]
 *
 * @param max defines the upper bound of the random number
 * @return the random number
 */
static int sm_random(int max)
{
	static int x=0;

	if(!x)
	{
		srand(sm_get_current_seconds());
		x = 1;
	}

	return (rand() % max);
}

/**
 * Returns a random tagline that is allocated via malloc().
 *
 * @return a random tagline. Free via free().
 */
static char *get_tagline(void)
{
	FILE *fh;
	int nr;
	char *tagline = NULL;

	if (!taglines_filename || !taglines_indexname ||!taglines_num) return NULL;

	nr = sm_random(taglines_num);

	if ((fh = fopen(taglines_filename,"rb")))
	{
		int len = taglines_positions[nr+1] - taglines_positions[nr];

		if ((tagline = (char *)malloc(len+1)))
		{
			fseek(fh,taglines_positions[nr],SEEK_SET);
			fread(tagline,1,len,fh);
			tagline[len-4]=0;
		}
		fclose(fh);
	}

	return tagline;
}

/*****************************************************************************/

char *taglines_add_tagline(char *buf)
{
	long len = 0;
	char *fmt;

	if ((fmt = strstr(buf,"%t")))
	{
		char *tagline;

		if ((tagline = get_tagline()))
		{
			char *new_buf;
			len = strlen(buf) + strlen(tagline) - 2;
			if ((new_buf = (char *)malloc(len+1)))
			{
				strncpy(new_buf,buf,fmt-buf);
				new_buf[fmt-buf]=0;
				strcat(new_buf,tagline);
				strcat(new_buf,buf + (fmt-buf) + 2);
				free(buf);
				buf = new_buf;
			}
			free(tagline);
		}
	}

	if ((fmt = strstr(buf,"%e")))
	{
		FILE *fh;
		if ((fh = fopen("ENV:Signature","rb")))
		{
			char *new_text;
			unsigned int len = myfsize(fh);

			if ((new_text = (char *)malloc(len+1)))
			{
				char *new_buf;
				fread(new_text,len,1,fh);
				new_text[len]=0;
				len = strlen(buf) + strlen(new_text) - 2;
				if ((new_buf = (char *)malloc(len+1)))
				{
					strncpy(new_buf,buf,fmt-buf);
					new_buf[fmt-buf]=0;
					strcat(new_buf,new_text);
					strcat(new_buf,buf + (fmt-buf) + 2);
					free(buf);
					buf = new_buf;
				}

				free(new_text);
			}
			fclose(fh);
		}
	}

	return buf;
}

/**
 * Create a tagine index file for a given tagkline file.
 *
 * @param filename
 * @param indexname
 */
static void taglines_create_index(char *filename, char *indexname)
{
	FILE *tagfh, *indexfh;
	char *buf;

	if (!(buf = (char *)malloc(512))) return;

	if ((tagfh = fopen(filename,"r")))
	{
		if ((indexfh = fopen(indexname,"wb")))
		{
			int pos = 0;
			int ver = TAGLINES_INDEX_VERSION;

			fwrite("SMTI",1,4,indexfh);
			fwrite(&ver,1,4,indexfh);

			while (myreadline(tagfh,buf))
			{
				if (strcmp(buf, "%%") == 0)
				{
					fwrite(&pos,1,4,indexfh);
					pos = ftell(tagfh);
				}
			}

			fclose(indexfh);
		}
		fclose(tagfh);
	}
	free(buf);
}

/*****************************************************************************/

void taglines_cleanup(void)
{
	free(taglines_filename);
	free(taglines_indexname);
	free(taglines_positions);
	taglines_filename = taglines_indexname = NULL;
	taglines_positions = NULL;
	taglines_num = 0;
}

/*****************************************************************************/

void taglines_init(char *filename)
{
	FILE *fh;
	char *indexname;
	unsigned int fh_size;

	taglines_cleanup();

	if (!(indexname = (char *)malloc(mystrlen(filename)+10)))
		return;

	if (!(filename = mystrdup(filename)))
	{
		free(indexname);
		return;
	}

	strcpy(indexname,filename);
	strcat(indexname,".index");

	if (myfiledatecmp(filename,indexname)>0)
	{
		taglines_create_index(filename,indexname);
	}

	taglines_indexname = NULL;
	taglines_filename = NULL;

	if ((fh = fopen(indexname,"rb")))
	{
		char buf[4];

		fread(buf,1,4,fh);
		if (!strncmp("SMTI",buf,4))
		{
			int ver;
			fread(&ver,1,4,fh);
			if (ver == TAGLINES_INDEX_VERSION)
			{
				fh_size = myfsize(fh);
				if ((taglines_positions = (int *)malloc(fh_size - 8 + sizeof(int))))
				{
					fread(taglines_positions,1,fh_size - 8,fh);
					taglines_num = (fh_size - 8) / sizeof(int);
					taglines_positions[taglines_num] = fh_size;
					taglines_indexname = indexname;
					taglines_filename = filename;
				}
			}
		}

		fclose(fh);
	}

	if (!taglines_indexname) free(indexname);
	if (!taglines_filename) free(filename);
}

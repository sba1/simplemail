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
 * @file archdebug.c
 */

#include <stdlib.h>

#include <dos/doshunks.h>
#include <libraries/iffparse.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include "support_indep.h"

#include "archdebug.h"
#include "debug.h"
#include "support.h"

#if defined(__SASC) && defined(DEBUG_RESTRACK)
#include <dos.h>

#define MAX_ADDR 32

struct bt
{
	int addr_used;
	ULONG addr[MAX_ADDR];
};

struct debug_info
{
	unsigned int offset;
	unsigned int line;
	unsigned char *filename;
};

static struct debug_info *debug_info_array;
static int debug_info_count;
static int debug_info_allocated;

static int debug_info_cmp(const void *o1, const void *o2)
{
	struct debug_info *i1 = (struct debug_info*)o1;
	struct debug_info *i2 = (struct debug_info*)o2;

	return (int)i1->offset - (int)i2->offset;
}

static void arch_debug_info_add(struct debug_info *info)
{
	if (debug_info_count == debug_info_allocated)
	{
		if (debug_info_allocated == 0)
			debug_info_allocated = 1000;

		debug_info_allocated *= 2;

		debug_info_array = realloc(debug_info_array,debug_info_allocated*sizeof(struct debug_info));
	}

	if (debug_info_array)
		debug_info_array[debug_info_count++] = *info;
}

extern ULONG *__code_base;
extern ULONG __code_size;

#endif


struct bt *arch_debug_get_bt(void)
{
	struct bt *bt = NULL;
	struct Process *this;

#if defined(__SASC) && defined(DEBUG_RESTRACK)

	/* Note that we assume that SimpleMail consists only of one hunk */

	ULONG *stackptr = (ULONG*)getreg(REG_A7);

	int cnt = 0;

	if (!(bt = (struct bt*)malloc(sizeof(*bt))))
		return NULL;

	this = (struct Process*)FindTask(NULL);

	while ((void*)stackptr < this->pr_Task.tc_SPUpper && cnt < MAX_ADDR)
	{
		ULONG addr = *stackptr;
		if (addr >= ((ULONG)__code_base) && addr < ((ULONG)__code_base) + __code_size)
		{
			bt->addr[cnt] = addr;
			cnt++;
		}
		stackptr++;
	}
	bt->addr_used = cnt;

#endif
	return bt;
}

void arch_debug_free_bt(struct bt *bt)
{
	free(bt);
}

static int arch_debug_loaded = 0;

static void arch_debug_load(void)
{
#if defined(__SASC) && defined(DEBUG_RESTRACK)
	BPTR fh;
	char prog_name[64];
	char *buf = NULL;
	ULONG val;
	int buf_size, buf_pos;
	int table_size;
	int hunk;
	int hunk_type;
	int first_hunk;
	int last_hunk;
	int guard;

	if (arch_debug_loaded) return;

	arch_debug_loaded = 1;

	if (!(GetProgramName(prog_name,sizeof(prog_name))))
		return;

	if (!(fh = Open(prog_name,MODE_OLDFILE)))
		goto bailout;

	Seek(fh,0,OFFSET_END);
	buf_size = Seek(fh,0,OFFSET_BEGINNING);

	if (!(buf = (char*)malloc(buf_size)))
		goto bailout;

	if (Read(fh,buf,buf_size) != buf_size)
		goto bailout;

	buf_pos = 0;

#define CHECK_EOF {if (buf_pos >= buf_size) goto bailout;}
#define GET_ULONG {CHECK_EOF; val = (*(ULONG*)&buf[buf_pos]); buf_pos+=4;}
	GET_ULONG;
	if (val != HUNK_HEADER)
		goto bailout;

	GET_ULONG;
	if (val != 0)
		goto bailout;

	GET_ULONG; table_size = val;
	GET_ULONG; first_hunk = val;
	GET_ULONG; last_hunk = val;

	buf_pos += table_size * 4;
	CHECK_EOF;

	for (guard = 0, hunk = first_hunk;hunk <= last_hunk && guard < 1000; guard++)
	{
		SM_DEBUGF(20,("Scanning hunk %d\n",hunk));

		GET_ULONG; hunk_type = val & 0x3FFFFFFF;

		switch (hunk_type)
		{
			case	HUNK_DEBUG:
					{
						int lwd;
						int next_hunk_buf_pos;

						ULONG hunk_size;
						ULONG base_offset;

						GET_ULONG; hunk_size = val;
						next_hunk_buf_pos = buf_pos + hunk_size * 4;
						GET_ULONG; base_offset = val;

						for (lwd = 0; lwd < 20 && lwd < hunk_size; lwd++)
						{
							GET_ULONG; /* type of debug info */

							switch (val)
							{
								case	MAKE_ID('S','R','C',' '):
										break;

								case	MAKE_ID('L','I','N','E'):
										{
											int name_size;
											char *name;

											GET_ULONG; name_size = val;

											name = mystrdup((char*)&buf[buf_pos]);

											buf_pos += name_size * 4;

											while(buf_pos < next_hunk_buf_pos && guard < 1000)
											{
												ULONG line, junk;

												struct debug_info dbi;

												GET_ULONG; line = val;
												GET_ULONG; junk = val;

												dbi.filename = name;
												dbi.line = line & 0xffffff;
												dbi.offset = junk + base_offset;
												arch_debug_info_add(&dbi);
											}
										}
										break;
							}
						}
						buf_pos = next_hunk_buf_pos;
					}
					break;

			case	HUNK_CODE:
			case	HUNK_DATA:
			case	HUNK_NAME:
					{
						ULONG hunk_size;
						GET_ULONG; hunk_size = val;
						buf_pos += hunk_size * 4;
					}
					break;

			case	HUNK_RELOC32:
			case	HUNK_SYMBOL:
					{
						ULONG num_relocs;
						do
						{
							GET_ULONG; num_relocs = val;

							if (num_relocs)
							{
								/* Skip the number of relocs plus the hunk to which they refer */
								buf_pos += num_relocs * 4 + 4;
							}
						} while (num_relocs);
					}
					break;

			case	HUNK_END:
					hunk++;
					break;

			default:
					goto bailout;
		}
	}

	if (debug_info_count)
	{
		struct debug_info *di;
		struct debug_info dummy;

		dummy.offset = 0;
		dummy.line = 0;
		dummy.filename = "";
		arch_debug_info_add(&dummy);

		dummy.offset = 0x7fffff;
		arch_debug_info_add(&dummy);

		qsort(debug_info_array,debug_info_count, sizeof(debug_info_array[0]), debug_info_cmp);
	}

bailout:
	free(buf);
	if (fh) Close(fh);
#endif
}

/**
 * Turns the backtrace into a string that can be displayed
 *
 * @param bt
 * @return
 */
char *arch_debug_bt2string(struct bt *bt)
{
#if defined(__SASC) && defined(DEBUG_RESTRACK)
	string str;
	char buf[120];
	int i;
	int last_line = -1;
	char *last_file = NULL;

	if (!bt) return NULL;

	if (!(string_initialize(&str,200)))
		return NULL;

	/* Load debug infos if not done yet */
	arch_debug_load();

	string_append(&str,"Backtrace\n");

	for (i=0;i<bt->addr_used;i++)
	{
		ULONG offset = (ULONG)bt->addr[i] - (ULONG)__code_base;

		sm_snprintf(buf,sizeof(buf),"\t\t%p (offset %p",bt->addr[i],offset);
		string_append(&str,buf);


		if (debug_info_count > 0)
		{
			struct debug_info *di;
			int offset = (ULONG)bt->addr[i] - (ULONG)__code_base;

			BIN_SEARCH(debug_info_array, 0, debug_info_count-1,
					(debug_info_array[m].offset <= offset) ?
					(offset < debug_info_array[m+1].offset?0:(1)):(-1),
					di);

			if (di && (di->filename != last_file || di->line != last_line))
			{
				string_append(&str,", ");
				sm_snprintf(buf,sizeof(buf),"%s/%d",di->filename,di->line);
				string_append(&str,buf);
				last_file = di->filename;
				last_line = di->line;
			}
		}

		string_append(&str,")\n");
	}

	return str.str;
#else
	return NULL;
#endif
}

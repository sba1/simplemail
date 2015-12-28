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

#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <CUnit/Basic.h>

/*****************************************************************************/

/* @Test */
void test_logging_init_and_dispose(void)
{
	CU_ASSERT(logg_init() == 1);
	logg_dispose();
}

/*****************************************************************************/

/* @Test */
void test_logging_only_dispose(void)
{
	logg_dispose();
}

/*****************************************************************************/

/* @Test */
void test_one_entry(void)
{
	CU_ASSERT(logg_init() == 1);
	logg(INFO, 0, __FILE__,__PRETTY_FUNCTION__, __LINE__, "LOG", LAST);
	logg_dispose();
}

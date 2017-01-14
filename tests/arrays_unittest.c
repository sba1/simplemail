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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <CUnit/Basic.h>

#include "arrays.h"

/*****************************************************************************/

/* @Test */
void test_arrays(void)
{
	struct array a;

	CU_ASSERT(array_init(&a) == 1);

	CU_ASSERT_EQUAL(array_add(&a, (void*)1), 0);
	CU_ASSERT_EQUAL(array_add(&a, (void*)2), 1);
	CU_ASSERT_EQUAL(array_add(&a, (void*)3), 2);

	array_deinit(&a);
}

/*****************************************************************************/

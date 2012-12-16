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
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include <CUnit/Basic.h>

#include "configuration.h"
#include "smock.h"

/*******************************************************/

FILE *fopen(const char *fname, const char *mode)
{
	return (FILE*)mock(fname,mode);
}

/*******************************************************/

/* @Test */
void test_save_config_with_failing_fopen(void)
{
	user.config_filename = "test.cfg";
	mock_always_returns(fopen,0);
	save_config();
	mock_clean();
}

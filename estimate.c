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
** estimate.c
*/

#include "estimate.h"

#include "support.h"

void estimate_init(struct estimate *est, unsigned int new_max_value)
{
	est->init_seconds = sm_get_current_seconds();
	est->init_micros = sm_get_current_micros();

	est->max_value = new_max_value;
}

unsigned int estimate_calc(struct estimate *est,unsigned int value)
{
	unsigned int seconds = sm_get_current_seconds();
	unsigned int micros = sm_get_current_micros();

	seconds -= est->init_seconds;
	if (micros < est->init_micros)
	{
		seconds--;
		micros += 1000000;
	}
	micros -= est->init_micros;

	if (!value) return 0xffffffff;

	return seconds * est->max_value / value;
}

unsigned int estimate_calc_remaining(struct estimate *est,unsigned int value)
{
	unsigned int seconds = sm_get_current_seconds();
	unsigned int micros = sm_get_current_micros();

	seconds -= est->init_seconds;
	if (micros < est->init_micros)
	{
		seconds--;
		micros += 1000000;
	}
	micros -= est->init_micros;

	if (!value) return 0xffffffff;

	return seconds * est->max_value / value - seconds;
}

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
 * @file estimate.c
 */

#include "estimate.h"

#include <stdlib.h>

#include "timesupport.h"

/*****************************************************************************/

void estimate_init(struct estimate *est, unsigned int new_max_value)
{
	est->init_seconds = sm_get_current_seconds();
	est->init_micros = sm_get_current_micros();

	est->max_value = new_max_value;
}

/*****************************************************************************/

/**
 * Estimate the end time when we now when given amount of work has already been processed.
 *
 * @param est the previously initialized estimator
 * @param value the work that has already been processed
 * @param now_seconds stores the now time in seconds.
 * @return the end time (in seconds)
 */
static int estimate_calc_absolute(struct estimate *est, unsigned int value, unsigned int *now_seconds)
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

	if (now_seconds) *now_seconds = seconds;

	if (!value) return 0xffffffff;

	return seconds * est->max_value / value;

}

/*****************************************************************************/

unsigned int estimate_calc(struct estimate *est,unsigned int value)
{
	return estimate_calc_absolute(est, value, NULL);
}

/*****************************************************************************/

unsigned int estimate_calc_remaining(struct estimate *est,unsigned int value)
{
	unsigned int now_seconds;
	unsigned int end_seconds;

	if (!value) return 0xffffffff;

	end_seconds = estimate_calc_absolute(est, value, &now_seconds);
	return end_seconds - now_seconds;
}

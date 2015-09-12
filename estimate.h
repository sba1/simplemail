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
 * @file estimate.h
 */

#ifndef SM__ESTIMATE_H
#define SM__ESTIMAGE_H

struct estimate
{
	unsigned int init_seconds;
	unsigned int init_micros;
	unsigned int max_value;
};

/**
 * Initialize the work "estimator" for the given max work.
 *
 * @param est the estimator to be initialized
 * @param new_max_value the work that must be done maximally
 */
void estimate_init(struct estimate *est, unsigned int new_max_value);

/**
 * Estimate the end time when we now when given amount of work has already been processed.
 *
 * @param est the previously initialized estimator
 * @param value the work that has already been processed
 * @return the end time (in seconds)
 */
unsigned int estimate_calc(struct estimate *est,unsigned int value);

/**
 * Estimate the number of seconds still remaining when a given amount of work has already been processed.
 *
 * @param est the previously initialized estimator
 * @param value the work that has already been processed
 * @return the remaining time (in seconds)
 */
unsigned int estimate_calc_remaining(struct estimate *est,unsigned int value);

#endif

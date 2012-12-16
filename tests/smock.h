/**
 * smock.h - A very simple mocking implementation for ANSI C.
 * Copyright (C) 2012  Sebastian Bauer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file smock.h
 *
 * @author Sebastian Bauer
 *
 * @note API inspired by cgreen (http://www.lastcraft.com/cgreen.php).
 */
#ifndef SM__MOCK_H
#define SM__MOCK_H

enum mock_type
{
	MOCK_DUMMY,
	MOCK_RETURN
};

struct mock_entry
{
	struct mock_entry *next;

	/** Name of the function this entry is responsible for */
	const char *func;

	enum mock_type type;

	union
	{
		struct
		{
			int always;
			intptr_t rc;
		} ret;
	} u;
};

static struct mock_entry _dummy_first = {NULL,"",MOCK_DUMMY};

/**
 * Inserts the given entry.
 *
 * @param e
 */
static void _mock_insert(struct mock_entry *e)
{
	struct mock_entry *p = &_dummy_first;

	while (p->next)
		p = p->next;
	p->next = e;
}

/**
 * This is the general mock function. It is called, whenever
 * a mocked function is called.
 *
 * @param func
 * @param args
 * @return
 */
static intptr_t mock_(const char *func, const char *args)
{
	struct mock_entry *p;
	struct mock_entry *e;

	p = &_dummy_first;
	e = p->next;

	while (e)
	{
		if (!strcmp(func,e->func))
		{
			intptr_t rc = e->u.ret.rc;

			if (!e->u.ret.always)
			{
				p->next = e->next;
				free(e);
			}
			return rc;
		}
		p = e;
		e = e->next;
	}


	fprintf(stderr,"No mocking instructions found for \"%s\"!\n",func);
	exit(-1);
}

/**
 * The given function shall always return rc.
 *
 * @param func
 * @param rc
 */
static void _mock_returns(const char *func, intptr_t rc, int always)
{
	struct mock_entry *e;

	if (!(e = (struct mock_entry*)malloc(sizeof(*e))))
	{
		fprintf(stderr,"Not enough memory!\n");
		exit(-1);
	}

	memset(e,0,sizeof(*e));
	e->func = func;
	e->u.ret.always = always;
	e->u.ret.rc = rc;

	_mock_insert(e);
}

/*******************************************************/

/* The following macros define the public interface */

/**
 * The macro that shall be used in the function body of the
 * mocked function.
 */
#define mock(...) mock_(__func__, #__VA_ARGS__)

/**
 * The given function shall return the given return value
 * once.
 */
#define mock_returns(f,rc) _mock_returns(#f,fc, 1)

/**
 * The given function shall always return the given value.
 */
#define mock_always_returns(f,rc) _mock_returns(#f,rc, 1)

/**
 * Clean up function.
 */
static inline mock_clean(void)
{
	struct mock_entry *e, *n;

	e = _dummy_first.next;

	while (e)
	{
		n = e->next;
		free(e);
		e = n;
	}

	_dummy_first.next = NULL;
}

/*******************************************************/

#endif

/**
 * @file
 */

#include <CUnit/Basic.h>

#include "imap_helper.h"

#include "imap.h"
#include "support_indep.h"

/******************************************************************************/

/* @Test */
void test_imap_get_result(void)
{
	char *res;
	char dest[200];
	int dest_size = sizeof(dest);

	res = imap_get_result(" ANSWER1 ANSWER2 (ANSWER3a ANSWER3b) \"ANSWER4\"",dest,dest_size);
	CU_ASSERT_STRING_EQUAL(dest, "ANSWER1");

	res = imap_get_result(res,dest,dest_size);
	CU_ASSERT_STRING_EQUAL(dest, "ANSWER2");

	res = imap_get_result(res,dest,dest_size);
	CU_ASSERT_STRING_EQUAL(dest, "ANSWER3a ANSWER3b");

	res = imap_get_result(res,dest,dest_size);
	CU_ASSERT_STRING_EQUAL(dest, "ANSWER4");
}

/* @Test */
void test_imap_get_result_too_small(void)
{
	char *res;
	char dest[2];
	int dest_size = sizeof(dest);

	res = imap_get_result(" 1ANSWER 2ANSWER (3ANSWER)",dest,dest_size);
	CU_ASSERT_STRING_EQUAL(dest, "1");

	res = imap_get_result(res,dest,dest_size);
	CU_ASSERT_STRING_EQUAL(dest, "2");

	res = imap_get_result(res,dest,dest_size);
	CU_ASSERT_STRING_EQUAL(dest, "3");
}

/******************************************************************************/

struct mock_connection
{
	char **writes;
	int currently_expected_write;

	char **responses;
	int currently_expected_response;
	int currently_expected_response_done;

	string cur_write_string;
};

/**
 * Expect that someone writes the given string to the mocked connection, and if
 * so, give back the given response..
 *
 * @param c
 * @param fmt
 * @param response
 */
static void expect_write(struct mock_connection *c, const char *fmt, const char *response)
{
	c->writes = array_add_string(c->writes, fmt);
	c->responses = array_add_string(c->responses, response);
}

static int mock_read(struct connection *c, void *buf, size_t len)
{
	struct mock_connection *m = (struct mock_connection *)c->udata;

	char *response = m->responses[m->currently_expected_response];
	int response_len = strlen(response);
	int todo = response_len - m->currently_expected_response_done;
	int tocpy = todo < len ? todo : len;

	memcpy(buf, &response[m->currently_expected_response_done], todo);
	m->currently_expected_response_done += todo;
	return todo;
}

static int mock_write(struct connection *c, void *buf, size_t len)
{
	struct mock_connection *m = (struct mock_connection *)c->udata;

	char str[len + 1];
	memcpy(str, buf, len);
	str[len] = 0;

	string_append(&m->cur_write_string, str);

	if (!strncmp(m->cur_write_string.str, m->writes[m->currently_expected_write], strlen(m->writes[m->currently_expected_write])))
	{
		m->currently_expected_response = m->currently_expected_write;
		m->currently_expected_write++;
	}
	return len;
}

static struct mock_connection *mock(struct connection *c)
{
	struct mock_connection *m;

	if (!(m = malloc(sizeof(*m))))
		return NULL;
	memset(m, 0, sizeof(*m));
	c->udata = m;
	c->read = mock_read;
	c->write = mock_write;

	if (!string_initialize(&m->cur_write_string, 0))
		return NULL;
	m->currently_expected_response = -1;
	return m;
}

/******************************************************************************/

/* @Test */
void test_imap_login(void)
{
	struct connection *c;
	struct mock_connection *m;
	struct imap_server *imap;

	c = tcp_create_connection();
	CU_ASSERT(c != NULL);

	m = mock(c);
	CU_ASSERT(m != NULL);

	/* Prepare imap data */
	imap = imap_malloc();
	CU_ASSERT(imap != NULL);
	imap->name = "imap.simplemail.sf.net";
	imap->login = "login";
	imap->passwd = "???";

	expect_write(m, "0000 LOGIN login ???\r\n", "0000 OK\r\n");
	CU_ASSERT(imap_login(c, imap) == 1);
	tcp_disconnect(c);
}

/******************************************************************************/

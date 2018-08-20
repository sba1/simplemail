/**
 * @file
 */

#include "support_indep.h"
#include "tcp.h"


struct mock_connection
{
	char **writes;
	int currently_expected_write;

	char **responses;
	int currently_expected_response;
	int currently_expected_response_done;
	int currently_expected_all_of_response_is_done;

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


/**
 * Inject a read into the mock communication stream.
 *
 * @param c
 * @param response
 */
static void inject_read(struct mock_connection *c, const char *response)
{
	c->responses = array_add_string(c->responses, response);
	c->currently_expected_response = 0;
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

	if (todo == 0)
	{
		if (m->currently_expected_all_of_response_is_done)
		{
			fprintf(stderr, "Read something without something being expected\n");
			exit(1);
		}
		m->currently_expected_all_of_response_is_done = 1;
	} else
	{
		m->currently_expected_all_of_response_is_done = 0;
	}
	return todo;
}

static int mock_write(struct connection *c, void *buf, size_t len)
{
	struct mock_connection *m = (struct mock_connection *)c->udata;

	char str[len + 1];
	memcpy(str, buf, len);
	str[len] = 0;

	string_append(&m->cur_write_string, str);

	if (array_length(m->writes) <= m->currently_expected_write)
	{
		fprintf(stderr, "Unexpected write of \"%s\"\n", (char*)buf);
		exit(1);
	}

	printf("%d %s\n", m->currently_expected_write, m->writes[m->currently_expected_write]);
	if (!strncmp(m->cur_write_string.str, m->writes[m->currently_expected_write], strlen(m->writes[m->currently_expected_write])))
	{
		m->currently_expected_response = m->currently_expected_write;
		m->currently_expected_write++;
		m->currently_expected_response_done = 0;
		string_crop(&m->cur_write_string, 0, 0);
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

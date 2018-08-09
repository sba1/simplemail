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

/******************************************************************************/

/* @Test */
void test_imap_send_simple_command(void)
{
	struct connection *c;
	struct mock_connection *m;

	imap_reset_command_counter();

	c = tcp_create_connection();
	CU_ASSERT(c != NULL);

	m = mock(c);
	CU_ASSERT(m != NULL);

	expect_write(m, "0000 QUIT\r\n", "0000 OK\r\n");
	CU_ASSERT(imap_send_simple_command(c, "QUIT") == 1);

	expect_write(m, "0001 QUIT\r\n", "0001 NO\r\n");
	CU_ASSERT(imap_send_simple_command(c, "QUIT") == 0);
}

/******************************************************************************/

/* @Test */
void test_imap_wait_login(void)
{
	struct connection *c;
	struct mock_connection *m;
	struct imap_server *imap;

	imap_reset_command_counter();

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

	inject_read(m, "0000 OK\r\n0001 NO\r\n0002 OK");

	CU_ASSERT(imap_wait_login(c, imap) != 0);
	CU_ASSERT(imap_wait_login(c, imap) == 0);
	CU_ASSERT(imap_wait_login(c, imap) == 0); /* Last misses newline, so it should fail */
}

/******************************************************************************/

/* @Test */
void test_imap_login(void)
{
	struct connection *c;
	struct mock_connection *m;
	struct imap_server *imap;

	imap_reset_command_counter();

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

	expect_write(m, "0001 LOGIN login ??\r\n", "0001 NO\r\n");
	CU_ASSERT(imap_login(c, imap) == 0);

	tcp_disconnect(c);
}

/******************************************************************************/

/* @Test */
void test_imap_get_folders(void)
{
	struct connection *c;
	struct mock_connection *m;

	struct string_list *folders;
	struct string_node *s;

	imap_reset_command_counter();

	c = tcp_create_connection();
	CU_ASSERT(c != NULL);

	m = mock(c);
	CU_ASSERT(m != NULL);

	expect_write(m, "0000 LIST \"\" *\r\n", "0000 OK\r\n");

	expect_write(m, "0001 LSUB \"\" *\r\n", "0001 OK\r\n");
	expect_write(m, "0002 STATUS INBOX (MESSAGES)\r\n", "0002 OK\r\n");

	expect_write(m, "0003 LIST \"\" *\r\n",
			" * LIST () \"/\" \"inbox\"\r\n"
			" * LIST () \"/\" \"sent\"\r\n"
			"0003 OK\r\n");

	folders = imap_get_folders(c, 1);
	CU_ASSERT(folders != NULL);
	CU_ASSERT(string_list_first(folders) == NULL);
	string_list_free(folders);

	/* Here we expect an INBOX folder at least */
	folders = imap_get_folders(c, 0);
	CU_ASSERT(folders != NULL);
	CU_ASSERT(string_list_first(folders) != NULL);
	CU_ASSERT_STRING_EQUAL(string_list_first(folders)->string, "INBOX");
	string_list_free(folders);

	/* Here we expect an inbox folder at least */
	folders = imap_get_folders(c, 1);
	CU_ASSERT(folders != NULL);
	CU_ASSERT(string_list_first(folders) != NULL);
	CU_ASSERT_STRING_EQUAL(string_list_first(folders)->string, "inbox");
	s = string_list_remove_head(folders);
	free(s->string);
	free(s);
	CU_ASSERT_STRING_EQUAL(string_list_first(folders)-> string, "sent");
	string_list_free(folders);
}

/******************************************************************************/

static void test_imap_set_status_static(const char *str)
{
}

static void test_imap_set_status(const char *str)
{
}

/* @Test */
void test_imap_select_mailbox(void)
{
	struct connection *c;
	struct mock_connection *m;

	struct string_list *folders;
	struct string_node *s;

	struct imap_select_mailbox_args args = {0};
	struct remote_mailbox *rm;

	imap_reset_command_counter();

	c = tcp_create_connection();
	CU_ASSERT(c != NULL);

	m = mock(c);
	CU_ASSERT(m != NULL);

	expect_write(m, "0000 EXAMINE \"INBOX\"\r\n", "0000 OK\r\n");

	/* Taken literally from the RC3501 */
	expect_write(m, "0001 EXAMINE \"INBOX\"\r\n",
			"* 172 EXISTS\r\n"
			"* 1 RECENT\r\n"
			"* OK [UNSEEN 12]\r\n"
			"* OK [UIDVALIDITY 3857529045]\r\n"
			"* OK [UIDNEXT 4392]\r\n"
			"* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n"
			"* OK [PERMANENTFLAGS (\\Deleted \\Seen \\*)] Limited\r\n"
			"0001 OK [READ-WRITE] SELECT completed\r\n");

	args.conn = c;
	args.path = "INBOX";
	args.set_status = test_imap_set_status;
	args.set_status_static = test_imap_set_status_static;
	args.writemode = 0;

	rm = imap_select_mailbox(&args);
	CU_ASSERT(rm != NULL);
	CU_ASSERT(rm->num_of_remote_mail == 0);

	rm = imap_select_mailbox(&args);
	CU_ASSERT(rm != NULL);
	CU_ASSERT(rm->num_of_remote_mail == 172);
	CU_ASSERT(rm->uid_next == 4392);
	CU_ASSERT(rm->uid_validity == 3857529045);
}

/******************************************************************************/

/* @Test */
void test_get_remote_mails(void)
{
	struct connection *c;
	struct mock_connection *m;

	struct imap_get_remote_mails_args args = {0};
	struct remote_mailbox *rm;
	int empty_folder;

	imap_reset_command_counter();

	c = tcp_create_connection();
	CU_ASSERT(c != NULL);

	m = mock(c);
	CU_ASSERT(m != NULL);

	expect_write(m, "0000 EXAMINE \"INBOX\"\r\n",
			"* 4 EXISTS\r\n"
			"* 1 RECENT\r\n"
			"* OK [UNSEEN 1]\r\n"
			"* OK [UIDVALIDITY 3857529045]\r\n"
			"* OK [UIDNEXT 4392]\r\n"
			"* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n"
			"* OK [PERMANENTFLAGS (\\Deleted \\Seen \\*)] Limited\r\n"
			"0000 OK [READ-WRITE] SELECT completed\r\n");
	expect_write(m, "0001 FETCH 1:4 (UID FLAGS RFC822.SIZE BODY[HEADER.FIELDS (FROM DATE SUBJECT TO CC)])\r\n",
			" * 1 FETCH (UID 1 RFC822.SIZE 1234)\r\n"
			" * 2 FETCH (UID 2 RFC822.SIZE 8888)\r\n"
			" * 3 FETCH (UID 3 RFC822.SIZE 2222)\r\n"
			" * 4 FETCH (UID 4 RFC822.SIZE 4321)\r\n"
			"0001 OK\r\n");

	args.conn = c;
	args.path = "INBOX";
	args.writemode = 0;
	args.headers = 1;
	args.set_status = test_imap_set_status_static;
	args.set_status_static = test_imap_set_status;

	rm = imap_get_remote_mails(&empty_folder, &args);
	CU_ASSERT(rm != NULL);
	CU_ASSERT(rm->num_of_remote_mail == 4);

	CU_ASSERT_EQUAL(rm->remote_mail_array[0].size, 1234);
	CU_ASSERT_EQUAL(rm->remote_mail_array[1].size, 8888);
	CU_ASSERT_EQUAL(rm->remote_mail_array[2].size, 2222);
	CU_ASSERT_EQUAL(rm->remote_mail_array[3].size, 4321);
}

/**
 * @file
 */

#include <CUnit/Basic.h>

#include "imap_helper.h"

#include "imap.h"
#include "mail.h"
#include "support_indep.h"

#include "mock_connection.c"

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

	mock_free(m);
	tcp_disconnect(c);
}

/******************************************************************************/

/**
 * Create a test imap server instance.
 *
 * @return the test imap instance.
 */
static struct imap_server *create_test_imap_server(void)
{
	struct imap_server *imap;

	if (!(imap = imap_malloc()))
	{
		return NULL;
	}

	imap->name = "imap.simplemail.sf.net";
	imap->login = "login";
	imap->passwd = "???";
	return imap;
}

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
	imap = create_test_imap_server();
	CU_ASSERT(imap != NULL);

	inject_read(m, "0000 OK\r\n0001 NO\r\n0002 OK");

	CU_ASSERT(imap_wait_login(c, imap) != 0);
	CU_ASSERT(imap_wait_login(c, imap) == 0);
	CU_ASSERT(imap_wait_login(c, imap) == 0); /* Last misses newline, so it should fail */

	free(imap);
	mock_free(m);
	tcp_disconnect(c);
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
	imap = create_test_imap_server();
	CU_ASSERT(imap != NULL);

	expect_write(m, "0000 LOGIN login ???\r\n", "0000 OK\r\n");
	expect_write(m, "0001 LOGIN login ???\r\n", "0001 NO\r\n");

	CU_ASSERT(imap_login(c, imap) == 1);
	CU_ASSERT(imap_login(c, imap) == 0);

	free(imap);
	mock_free(m);
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

	mock_free(m);
	tcp_disconnect(c);
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
	free(rm);

	rm = imap_select_mailbox(&args);
	CU_ASSERT(rm != NULL);
	CU_ASSERT(rm->num_of_remote_mail == 172);
	CU_ASSERT(rm->uid_next == 4392);
	CU_ASSERT(rm->uid_validity == 3857529045);

	free(rm->remote_mail_array);
	free(rm);
	mock_free(m);
	tcp_disconnect(c);
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

	rm = imap_get_remote_mails(&args);
	CU_ASSERT(rm != NULL);
	CU_ASSERT(rm->num_of_remote_mail == 4);

	CU_ASSERT_EQUAL(rm->remote_mail_array[0].uid, 1);
	CU_ASSERT_EQUAL(rm->remote_mail_array[0].size, 1234);

	CU_ASSERT_EQUAL(rm->remote_mail_array[1].uid, 2);
	CU_ASSERT_EQUAL(rm->remote_mail_array[1].size, 8888);

	CU_ASSERT_EQUAL(rm->remote_mail_array[2].uid, 3);
	CU_ASSERT_EQUAL(rm->remote_mail_array[2].size, 2222);

	CU_ASSERT_EQUAL(rm->remote_mail_array[3].uid, 4);
	CU_ASSERT_EQUAL(rm->remote_mail_array[3].size, 4321);

	free(rm->remote_mail_array);
	free(rm);
	mock_free(m);
	tcp_disconnect(c);
}

/******************************************************************************/

static void test_imap_download_mail_callback(struct mail_info *m, void *userdata)
{
}

/* @Test */
void test_imap_download_mail()
{
	struct connection *c;
	struct mock_connection *m;
	struct mail_info *mi;

	const char *mail_text =
		"From: Sebastian Bauer <mail@sebastianbauer.info>\r\n"
		"Date: Sa, 25 Aug 2018 10:06:36 +0200\r\n";

	char expected_read_buf[1024];

	int success;
	FILE *f;

	imap_reset_command_counter();

	c = tcp_create_connection();
	CU_ASSERT(c != NULL);

	m = mock(c);
	CU_ASSERT(m != NULL);

	mi = mail_info_create(NULL);
	CU_ASSERT(mi != NULL);

	snprintf(expected_read_buf, sizeof(expected_read_buf),
			"* 1 * ({%d}\r\n"
			"%s"
			")\r\n"
			"0000 OK\r\n", strlen(mail_text), mail_text);

	expect_write(m, "0000 UID FETCH 1 RFC822\r\n", expected_read_buf);

	mi->filename = strdup("u00001");
	success = imap_really_download_mail(c, "/tmp/", mi, test_imap_download_mail_callback, NULL);
	CU_ASSERT(success != 0);

	f = fopen("/tmp/u00001", "r");
	CU_ASSERT(f != NULL);
	fclose(f);
	mail_info_free(mi);
	mock_free(m);
	tcp_disconnect(c);
}

/******************************************************************************/

#include "codesets.h"
#include "configuration.h"
#include "folder.h"
#include "progmon.h"
#include "subthreads.h"

void main_set_progress(unsigned int max_work, unsigned int work)
{
}

void main_hide_progress(void)
{
}

void test_imap_new_uids(unsigned int uid_validity, unsigned int uid_next, char *user, char *server, char *path)
{
}

void test_imap_new_mails_arrived(int num_filenames, char **filenames, char *user, char *server, char *path)
{
}

/* @Test */
void test_imap_really_download_mails()
{
	struct imap_download_mails_options options = {0};

	struct connection *c;
	struct mock_connection *m;

	int num_mails;

	char tempdir[] = "SimpleMail-tmp-XXXXXX";

	struct folder *f;

	char path[1024];
	FILE *fh;

	CU_ASSERT(mkdtemp(tempdir) != NULL);

	CU_ASSERT(codesets_init() != 0);
	CU_ASSERT(init_threads() != 0);
	user.folder_directory = tempdir;
	CU_ASSERT(init_folders() != 0);
	CU_ASSERT(progmon_init() != 0);

	/* Let the real test begin */

	imap_reset_command_counter();

	c = tcp_create_connection();
	CU_ASSERT(c != NULL);

	m = mock(c);
	CU_ASSERT(m != NULL);

	options.imap_folder = "INBOX";
	options.imap_server = create_test_imap_server();
	CU_ASSERT(options.imap_server != NULL);

	f = folder_add_imap_server(options.imap_server->name, options.imap_server->name, options.imap_server->login);
	CU_ASSERT(f != NULL);

	f = folder_add_imap(f, "INBOX");
	CU_ASSERT(f != NULL);

	options.imap_local_path = f->path;
	options.uid_options.imap_dont_use_uids = 1;
	options.callbacks.set_status = test_imap_set_status;
	options.callbacks.set_status_static = test_imap_set_status_static;
	options.callbacks.new_uids = test_imap_new_uids;
	options.callbacks.new_mails_arrived = test_imap_new_mails_arrived;

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

	num_mails = imap_really_download_mails(c, &options);
	CU_ASSERT(num_mails == 4);

	snprintf(path, sizeof(path), "%s/u1", f->path);
	CU_ASSERT((fh = fopen(path, "rb")) != NULL);
	fclose(fh);

	snprintf(path, sizeof(path), "%s/u2", f->path);
	CU_ASSERT((fh = fopen(path, "rb")) != NULL);
	fclose(fh);

	snprintf(path, sizeof(path), "%s/u3", f->path);
	CU_ASSERT((fh = fopen(path, "rb")) != NULL);
	fclose(fh);

	snprintf(path, sizeof(path), "%s/u4", f->path);
	CU_ASSERT((fh = fopen(path, "rb")) != NULL);
	fclose(fh);

	del_folders();
	cleanup_threads();
	free(options.imap_server);
	mock_free(m);
	tcp_disconnect(c);
}

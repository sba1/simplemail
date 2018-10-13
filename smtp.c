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
 * @file smtp.c
 */

#include "smtp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "account.h"
#include "codecs.h"
#include "codesets.h"
#include "debug.h"
#include "hmac_md5.h"
#include "lists.h"
#include "pop3.h"
#include "simplemail.h"
#include "smintl.h"
#include "support_indep.h"
#include "tcp.h"

#include "subthreads.h"
#include "support.h"
#include "tcpip.h"

/*****************************************************************************/

struct smtp_connection
{
	struct connection *conn;
	char *server_name;
	char *fingerprint;
	int flags;			/* ESMTP flags */
	int auth_flags; /* Supported AUTH methods */
};

/*****************************************************************************/

static char *buf_init(void)
{
	char *rc;
	
	rc = (char *)malloc(1100);
	if(rc)
		rc[0] = 0;
	
	return rc;
}

static void buf_free(char *buf)
{
	free(buf);
}

/**
 * Send a smtp command and evaluate the result. If cmd is NULL the send
 * phase is skipped, which means that the result phase is still evaluated.
 * If an error occurs -1 is returned, else the error code of the smtp
 * command.
 *
 * @param conn
 * @param cmd
 * @param args
 * @return
 */
static int smtp_send_cmd(struct smtp_connection *conn, char *cmd, char *args)
{
	int rc;
	long count;
	int ready = 0;
	char *buf;
	char send_buf[300];
	
	rc  = -1;

	if (cmd != NULL)
	{
		if(args != NULL)
			sm_snprintf(send_buf, sizeof(send_buf), "%s %s\r\n", cmd, args);
		else
			sm_snprintf(send_buf, sizeof(send_buf), "%s\r\n", cmd);

		count = tcp_write(conn->conn, send_buf, strlen(send_buf));
		
		if (count != strlen(send_buf))
			return -1;
	}

	while (!ready && (buf = tcp_readln(conn->conn)))
	{
		if(buf[3] == ' ')
		{
			ready = 1;
			rc = atol(buf);
		} else
		{
			if(buf[3] != '-')
				break;
		}
	}
	
	return rc;
}

/**
 * Service ready?
 *
 * @param conn the already established smtp connection
 * @return ready (1) or not ready (0)
 */
static int smtp_service_ready(struct smtp_connection *conn)
{
	if (smtp_send_cmd(conn, NULL, NULL) != SMTP_SERVICE_READY)
	{
		tell_from_subtask(N_("Service not ready."));
		return 0;
	}
	return 1;
}

/**
 * Send the HELO command
 *
 * @param conn the already established smtp connection.
 * @param account the account used for the connection.
 * @return 1 success, 0 otherwise
 */
static int smtp_helo(struct smtp_connection *conn, struct account *account)
{
	char dom[512];

	if (!account->smtp->ip_as_domain)
	{
		/* TODO: use the parse functions for this */
		char *domain = strchr(account->email,'@');
		if (domain)
		{
			domain++;
			mystrlcpy(dom,domain,sizeof(dom));
		} else dom[0] = 0;
	}
	else if(tcp_gethostname(dom, sizeof(dom)) != 0)
	{
		return 0;
	}

	if (smtp_send_cmd(conn,"HELO",dom) != SMTP_OK) return 0;

	return 1;
}

/**
 * Send the FROM command
 *
 * @param conn the already established smtp connection
 * @param account the account defining the from address
 * @return 1 on success, 0 otherwise.
 */
static int smtp_from(struct smtp_connection *conn, struct account *account)
{
	int rc;
	string send_str;

	if (!string_initialize(&send_str, 200))
		return 0;

	rc = 0;

	if (!(string_append(&send_str,"FROM:<"))) goto out;
	if (isascii7(account->email))
	{
		if (!(string_append(&send_str,account->email))) goto out;
	} else
	{
		utf8 *puny;

		if (!(puny = encode_address_puny(account->email)))
			goto out;

		if (!(string_append(&send_str,puny)))
		{
			free(puny);
			goto out;
		}
	}
	if (!(string_append(&send_str,">"))) goto out;

	if (smtp_send_cmd(conn, "MAIL", send_str.str) == SMTP_OK) rc = 1;

out:
	free(send_str.str);
	return rc;
}

/**
 * Send the RCPT command.
 *
 * @param conn the already established smtp connection
 * @param account the account defining the recipients
 * @param om the description.
 * @return 1 on success, 0 otherwise.
 */
static int smtp_rcpt(struct smtp_connection *conn, struct account *account, struct outmail *om)
{
	int i,rc;
	string send_str;

	if (!string_initialize(&send_str, 200))
		return 0;

	rc = 1;

	for (i = 0; om->rcp[i] != NULL; i++)
	{
		int res;

		string_crop(&send_str,0,0);
		if (!(string_append(&send_str,"TO:<")))
		{
			rc = 0;
			goto out;
		}

		if (isascii7(om->rcp[i]))
		{
			if (!string_append(&send_str,om->rcp[i]))
			{
				rc = 0;
				goto out;
			}
		} else
		{
			utf8 *puny;

			if (!(puny = encode_address_puny(om->rcp[i])))
			{
				rc = 0;
				goto out;
			}

			if (!(string_append(&send_str,puny)))
			{
				rc = 0;
				free(puny);
				goto out;
			}

			free(puny);
		}

		if (!string_append(&send_str,">"))
		{
			rc = 0;
			goto out;
		}

		res = smtp_send_cmd(conn, "RCPT", send_str.str);

		if (res != SMTP_OK && res != SMTP_OK_FORWARD)
		{
			rc = 0;
			break;
		}
	}
out:
	free(send_str.str);
	return rc;
}

/**
 * Perform the data phase.
 *
 * @param conn the already established smtp connection
 * @param account the account for whic the data phase should be processed.
 * @param mailfile the name of the mail that should be send.
 * @param cur_mail_size the number of bytes already sent (should accumulate
 *  data of previously sent mails)
 * @return 1 on success, 0 on failure.
 */
static int smtp_data(struct smtp_connection *conn, struct account *account, char *mailfile, int cur_mail_size, struct smtp_send_callbacks *callbacks)
{
	int rc = 0;
	char *buf;

	buf = buf_init();
	if(buf != NULL)
	{
		FILE *fp;

		if(( fp = fopen(mailfile, "r") ))
		{
			unsigned int size;

			size = myfsize(fp);

			if(SMTP_SEND_MAIL == smtp_send_cmd(conn, "DATA", NULL))
			{
				long z,bytes_send=0,last_bytes_send=0;
				char convert8bit = 0;

				rc = 1;
				z = ((z = size / 100) >1)?z:1;

				for(;;) /* header */
				{
					long count;

					if(!fgets(buf,999,fp)) /* read error or EOF in header */
					{
						rc = 0;
						break;
					}
					if(!strchr(buf,'\n')) /* line too long? */
					{
						rc = 0;
						break;
					}
					if (!mystrnicmp(buf, "Bcc:", 4))
					{
						/* Do not send the Bcc Header field! */
						continue;
					}
					if(!mystricmp(buf,"Content-Transfer-Encoding: 8bit\n"))
					{
						if(!(conn->flags & ESMTP_8BITMIME))
						{
							convert8bit = 1;
							strcpy(buf,"Content-Transfer-Encoding: quoted-printable\n");
						}
					}

					count = tcp_write(conn->conn, buf, strlen(buf)-1);
					if(count != strlen(buf)-1)
					{
						rc = 0;
						break;
					}
					bytes_send += count+1;

					if(2 != tcp_write(conn->conn, "\r\n", 2))
					{
						rc = 0;
						break;
					}

					if((last_bytes_send%z) != (bytes_send%z))
					{
						callbacks->set_gauge(cur_mail_size + bytes_send);
					}
					last_bytes_send = bytes_send;

					if(1 == strlen(buf)) break;
				}

				if(1 == rc) for(;;) /* body */
				{
					int len = 0;
					char *buf2;
					unsigned char c;

					if(!fgets(buf,998,fp)) /* read error or EOF */
					{
						if(!feof(fp)) rc = 0;
						break;
					}

					/* the last line could be have a missing newline character */
					for (buf2 = buf;(c = *buf2);buf2++)
					{
						if (c == '\n' || c == '\r') break;
						len++;
					}

					if(convert8bit)
					{
						long count;
						char qp[4];
						int pos = 0, linepos;

						if(!mystrnicmp(buf,"From ",5))
						{
							sprintf(qp,"=%02X",buf[0]);
							if(3 != tcp_write(conn->conn, qp, 3))
							{
								rc = 0;
								break;
							}
							count = tcp_write(conn->conn, buf+1, 4);
							if(4 != count)
							{
								rc = 0;
								break;
							}
							bytes_send += 5;
							pos = 5;
						} else if('.' == buf[0])
						{
							if(3 != tcp_write(conn->conn, "=2E", 3))
							{
								rc = 0;
								break;
							}
							bytes_send++;
							pos = 1;
						}

						linepos = pos;
						while(pos < len)
						{
							char useqp=('=' == buf[pos] || ' ' > buf[pos] || '~' < buf[pos]);

							if((pos == len-1) && (' ' == buf[pos])) useqp=1;

							if(linepos >= 75-2*useqp)
							{
								if(3 != tcp_write(conn->conn, "=\r\n", 3))
								{
									rc = 0;
									break;
								}
								linepos = 0;
							}
							if(useqp)
							{
								sprintf(qp,"=%02X",buf[pos]);
								if(3 != tcp_write(conn->conn, qp, 3))
								{
									rc = 0;
									break;
								}
								pos++;
								linepos += 3;
								bytes_send++;
							} else {
								if(1 != tcp_write(conn->conn, buf+pos, 1))
								{
									rc = 0;
									break;
								}
								pos++;
								linepos++;
								bytes_send++;
							}
						}

						if(2 != tcp_write(conn->conn, "\r\n", 2))
						{
							rc = 0;
							break;
						}
						bytes_send++;
					} else {
						if('.' == buf[0]) if(1 != tcp_write(conn->conn, ".", 1))
						{
							rc = 0;
							break;
						}
						if (len != tcp_write(conn->conn, buf, len))
						{
							rc = 0;
							break;
						}
						bytes_send += strlen(buf);
						if(2 != tcp_write(conn->conn, "\r\n", 2))
						{
							rc = 0;
							break;
						}
					}

					if((last_bytes_send%z) != (bytes_send%z))
					{
						callbacks->set_gauge(cur_mail_size + bytes_send);
					}
					last_bytes_send = bytes_send;
				}

				if(rc == 1)
				{
					if(smtp_send_cmd(conn, "\r\n.", NULL) != SMTP_OK) /* \r\n is done by the function */
					{
						rc = 0;
					}
				}
			} else {
				tell_from_subtask(N_("DATA-Cmd failed."));
			}

			fclose(fp);
		}

		buf_free(buf);
	}

	return rc;
}

/**
 * Send the EHLO command (for ESMTP servers.
 *
 * @param conn the already established smtp connection
 * @param account the account for which auth should be processed.
 * @return 1 on success, 0 otherwise.
 */
static int esmtp_ehlo(struct smtp_connection *conn, struct account *account)
{
	char dom[512];
	char *answer;

	conn->flags = 0;
	conn->auth_flags  = 0;

	if (!account->smtp->ip_as_domain)
	{
		/* TODO: use the parse functions for this */
		char *domain = strchr(account->email,'@');
		if (domain)
		{
			domain++;
			mystrlcpy(dom,domain,sizeof(dom));
		} else dom[0] = 0;
	}
	else if(tcp_gethostname(dom, sizeof(dom)) != 0)
	{
		return 0;
	}

	tcp_write(conn->conn, "EHLO ",5);
	if (account->smtp->secure && !tcp_secure(conn->conn))
	{
		/* don't send any private date until connection is secure */
		tcp_write(conn->conn, "simplemail.sourceforge.net",26);
	} else 
	{
		tcp_write(conn->conn, dom, strlen(dom));
	}
	tcp_write(conn->conn, "\r\n", 2);
	tcp_flush(conn->conn);

	do
	{
		answer = tcp_readln(conn->conn);
		if (!answer) return 0;

		if (strstr(answer, "ENHANCEDSTATUSCODES")) conn->flags |= ESMTP_ENHACEDSTATUSCODES;
		else if (strstr(answer, "8BITMIME")) conn->flags |= ESMTP_8BITMIME;
		else if (strstr(answer, "ONEX")) conn->flags |= ESMTP_ONEX;
		else if (strstr(answer, "ETRN")) conn->flags |= ESMTP_ETRN;
		else if (strstr(answer, "XUSR")) conn->flags |= ESMTP_XUSR;
		else if (strstr(answer, "PIPELINING")) conn->flags |= ESMTP_PIPELINING;
		else if (strstr(answer, "STARTTLS")) conn->flags |= ESMTP_STARTTLS;
		else if (strstr(answer, "AUTH"))
		{
			conn->flags |= ESMTP_AUTH;

			if (strstr(answer, "PLAIN")) conn->auth_flags |= AUTH_PLAIN;
			if (strstr(answer, "LOGIN")) conn->auth_flags |= AUTH_LOGIN;
			if (strstr(answer, "DIGEST-MD5")) conn->auth_flags |= AUTH_DIGEST_MD5;
			if (strstr(answer, "CRAM-MD5")) conn->auth_flags |= AUTH_CRAM_MD5;
		}
	} while (answer[3] != ' ');

	return atoi(answer)==SMTP_OK;
}

static int esmtp_auth_cram(struct smtp_connection *conn, struct account *account)
{
	static char cram_str[] = "AUTH CRAM-MD5\r\n";
	char *line;
	int rc;
	char *challenge;
	unsigned int challenge_len = (unsigned int)-1; /* for decode_base64 */
	unsigned long digest[4]; /* 16 chars */
	char buf[512];
	char *encoded_str;

	char *login = account->smtp->auth_login;
	char *password = account->smtp->auth_password;

	tcp_write(conn->conn, cram_str,sizeof(cram_str)-1);

	if (!(line = tcp_readln(conn->conn))) return 0;
	rc = atoi(line);
	if (rc != 334) return 0;

	if (!(challenge = decode_base64((unsigned char*)(line+4),strlen(line+4),&challenge_len)))
		return 0;

	hmac_md5((unsigned char*)challenge,strlen(challenge),(unsigned char*)password,strlen(password),(unsigned char*)digest);
	free(challenge);
	sm_snprintf(buf,sizeof(buf),"%s %08lx%08lx%08lx%08lx",login,
					digest[0],digest[1],digest[2],digest[3]);

	encoded_str = encode_base64((unsigned char*)buf,strlen(buf));
	if (!encoded_str) return 0;
	tcp_write(conn->conn,encoded_str,strlen(encoded_str));
	tcp_write(conn->conn,"\r\n",2);
	free(encoded_str);

	if (!(line = tcp_readln(conn->conn))) return 0;
	rc = atoi(line);

	return rc == 235;
}

/**
 * Authenticate via AUTH commands. It tries all supported
 * methods, starting at the strongest.
 *
 * @param conn the already established smtp connection
 * @param account the account for which auth should be processed.
 * @return 1 on success, 0 otherwise.
 */
static int esmtp_auth(struct smtp_connection *conn, struct account *account)
{
	int flags, success;
	char *buf, prep[1024];

	flags = conn->auth_flags;

  if (flags & AUTH_CRAM_MD5)
	{
		SM_DEBUGF(10,("Trying AUTH CRAM-MD5\n"));
		if (esmtp_auth_cram(conn, account))
			return 1;
	}
	
	if (flags & AUTH_LOGIN)
	{
		SM_DEBUGF(10,("Trying AUTH LOGIN\n"));

		if (smtp_send_cmd(conn, "AUTH", "LOGIN") == 334)
		{
			mystrlcpy(prep, account->smtp->auth_login, sizeof(prep));

			if ((buf = encode_base64((unsigned char*)prep, strlen(prep))))
			{
				if (smtp_send_cmd(conn, buf, NULL) == 334)
				{
					free(buf);

					mystrlcpy(prep,account->smtp->auth_password,sizeof(prep));

					if ((buf = encode_base64((unsigned char*)prep, strlen(prep))))
					{
						success = smtp_send_cmd(conn, buf, NULL) == 235;

						free(buf);

						return success;
					}
				}
				free(buf);
			}
		}
	}

	if (flags & AUTH_PLAIN)
	{
		int ll = strlen(account->smtp->auth_login);
		int pl = strlen(account->smtp->auth_password);

		SM_DEBUGF(10,("Trying AUTH PLAIN\n"));

		if (ll + pl < sizeof(prep)-3)
		{
			prep[0] = 0;
			strcpy(&prep[1], account->smtp->auth_login);
			strcpy(&prep[2 + ll], account->smtp->auth_password);

			if ((buf = encode_base64((unsigned char*)prep, ll + pl + 2)))
			{
				if (smtp_send_cmd(conn, "AUTH PLAIN", buf) == 235)
				{
					free(buf);
					return 1;
				}
				free(buf);
			}
		}
	}

	SM_DEBUGF(5,("Authentication failed\n"));
	return 0;
}

/**
 * Login into the (e)smtp server. After a successful call you
 * can send the mails.
 *
 * @param conn the already established smtp connection
 * @param account the account for which the login should be processed.
 * @return 0 on error, 1 on success
 */
static int smtp_login(struct smtp_connection *conn, struct account *account, struct smtp_send_callbacks *callbacks)
{
	if (!smtp_service_ready(conn)) return 0;

	callbacks->set_status_static(_("Sending EHLO..."));
	if (!esmtp_ehlo(conn,account))
	{
		if (tcp_error_code() == TCP_INTERRUPTED) return 0;

		callbacks->set_status_static(_("Sending HELO..."));
		if (!smtp_helo(conn,account))
		{
			if (tcp_error_code() != TCP_INTERRUPTED) tell_from_subtask(N_("HELO failed"));
			return 0;
		}
	}

	if (account->smtp->secure)
	{
		if (!(conn->flags & ESMTP_STARTTLS))
		{
			tell_from_subtask(N_("Connection could not be made secure because the SMTP server doesn't seem to support it!"));
			return 0;
		}

		callbacks->set_status_static(_("Sending STARTTLS..."));
		if ((smtp_send_cmd(conn,"STARTTLS",NULL)!=SMTP_SERVICE_READY))
		{
			if (tcp_error_code() != TCP_INTERRUPTED) tell_from_subtask(N_("STARTTLS failed. Connection could not be made secure."));
			return 0;
		}

		if (!(tcp_make_secure(conn->conn, conn->server_name, conn->fingerprint)))
		{
			tell_from_subtask(N_("Connection could not be made secure."));
			return 0;
		}

		callbacks->set_status_static(_("Sending secured EHLO..."));
		if (!esmtp_ehlo(conn,account))
		{
			if (tcp_error_code() == TCP_INTERRUPTED) return 0;

			callbacks->set_status_static(_("Sending secured HELO..."));
			if (!smtp_helo(conn,account))
			{
				if (tcp_error_code() != TCP_INTERRUPTED) tell_from_subtask(N_("HELO failed"));
				return 0;
			}
		}
	}

	if (account->smtp->auth)
	{
		callbacks->set_status_static(_("Sending AUTH..."));
		if (!esmtp_auth(conn,account))
		{
			if (tcp_error_code() != TCP_INTERRUPTED) tell_from_subtask(N_("AUTH failed. User couldn't be authenticated. Please recheck your settings."));
			return 0;
		}
	}

	return 1;
}

/**
 * Count the number of mails which belongs to the given account
 *
 * @param account
 * @param om
 * @return
 */
static int count_mails(struct account *account, struct outmail **om)
{
	int amm=0;
	int i=0;

	while (om[i])
	{
		if (!mystricmp(account->email,om[i]->from))
			amm++;
		i++;
	}
	return amm;
}

/**
 * Count the number of mails within the given om which belongs to the given
 * account.
 *
 * @param account
 * @param om
 * @return
 */
static int count_mails_size(struct account *account, struct outmail **om)
{
	int size=0;
	int i=0;

	while (om[i])
	{
		if (!mystricmp(account->email,om[i]->from))
			size += om[i]->size;
		i++;
	}
	return size;
}

/**
 * Send all the mails which belongs to the account now.
 *
 * @param conn
 * @param account
 * @param om
 * @return
 */
static int smtp_send_mails(struct smtp_connection *conn, struct account *account, struct outmail **om, struct smtp_send_callbacks *callbacks)
{
	int i,j,amm,max_mail_size_sum,mail_size_sum = 0;

	amm = count_mails(account,om);
	max_mail_size_sum = count_mails_size(account,om);

	callbacks->init_mail(amm);
	callbacks->init_gauge_as_bytes(max_mail_size_sum);

	for (i=0,j=0;om[i] && j < amm;i++)
	{
		if (mystricmp(account->email,om[i]->from)) continue;

		j++;
		callbacks->set_mail(j,om[i]->size); /* starts from 1 */

		callbacks->set_status_static(_("Sending FROM..."));

		if (!smtp_from(conn,account))
		{
			if (tcp_error_code() != TCP_INTERRUPTED) tell_from_subtask(N_("FROM failed."));
			callbacks->mail_has_not_been_sent(om[i]->mailfile);
			return 0;
		}

		callbacks->set_status_static(_("Sending RCPT..."));
		if (!smtp_rcpt(conn,account, om[i]))
		{
			if (tcp_error_code() != TCP_INTERRUPTED) tell_from_subtask(N_("RCPT failed."));
			callbacks->mail_has_not_been_sent(om[i]->mailfile);
			return 0;
		}

		callbacks->set_status_static(_("Sending DATA..."));

		if (!smtp_data(conn,account, om[i]->mailfile, mail_size_sum, callbacks))
		{
			if (tcp_error_code() != TCP_INTERRUPTED) tell_from_subtask(N_("DATA failed."));
			callbacks->mail_has_not_been_sent(om[i]->mailfile);
			return 0;
		}

		mail_size_sum += om[i]->size;
		callbacks->set_gauge(mail_size_sum);

		/* no error while mail sending, so it can be moved to the "Sent" folder now */
		callbacks->mail_has_been_sent(om[i]->mailfile);
	}
	return 1;
}

/**
 * Send the QUIT command
 *
 * @param conn
 * @return
 */
static int smtp_quit(struct smtp_connection *conn)
{
	return (smtp_send_cmd(conn, "QUIT", NULL) == SMTP_OK);
}

/**
 * Connect to the smtp server as specified in the given account.
 *
 * @param conn the connection that will be filed. It's a out parameter.
 * @param ac the account describing the smtp server to be connected.
 * @return 1 on success, 0 on error.
 */
static int smtp_connect(struct smtp_connection *conn, struct account *ac, struct smtp_send_callbacks *callbacks)
{
	struct connect_options connect_opts = {0};
	char head_buf[100];
	int error_code;

	if (ac->account_name)
	{
		if (callbacks->set_title_utf8) callbacks->set_title_utf8(ac->account_name);
	} else
	{
		if (callbacks->set_title) callbacks->set_title(ac->smtp->name);
	}

	if (ac->smtp->pop3_first)
	{
		/* Connect to the pop3 server first */

		struct pop3_dl_callbacks pop3_callbacks = {0};

		pop3_callbacks.set_status_static = callbacks->set_status_static;

		if (callbacks->set_head)
		{
			sm_snprintf(head_buf,sizeof(head_buf),_("Sending mails to %s, connecting to %s first"),ac->smtp->name,ac->pop->name);
			callbacks->set_head(head_buf);
		}

		callbacks->set_status_static(_("Log into POP3 Server...."));
		pop3_login_only(ac->pop, &pop3_callbacks);
	}

	sm_snprintf(head_buf,sizeof(head_buf),_("Sending mails to %s"),ac->smtp->name);
	if (callbacks->set_head) callbacks->set_head(head_buf);
	if (callbacks->set_connect_to_server) callbacks->set_connect_to_server(ac->smtp->name);

	/* Make a possible fingerprint available to tcp_connect() */
	connect_opts.fingerprint = ac->smtp->fingerprint;
	connect_opts.use_ssl = ac->smtp->ssl && !ac->smtp->secure;

	if (!(conn->conn = tcp_connect(ac->smtp->name, ac->smtp->port,&connect_opts,&error_code)))
	{
		char message[380];
		sm_snprintf(message,sizeof(message),_("Unable to connect to server %s: %s"),ac->smtp->name,tcp_strerror(error_code));
		tell_from_subtask(message);
		return 0;
	}

	conn->server_name = ac->smtp->name;
	conn->fingerprint = ac->smtp->fingerprint;

	if (!smtp_login(conn,ac,callbacks))
	{
		char message[380];

		tcp_disconnect(conn->conn);

		sm_snprintf(message,sizeof(message),_("Unable to login into server %s"),ac->smtp->name);
		tell_from_subtask(message);
		return 0;
	}

	return 1;
}

/*****************************************************************************/

int smtp_login_only(struct account *ac, struct smtp_send_callbacks *callbacks)
{
	int rc;
	struct smtp_connection conn;

	rc = 0;

	if (!open_socket_lib())
		return 0;


	if (!(rc = smtp_connect(&conn, ac, callbacks)))
	{
		goto bailout;
	}

	smtp_quit(&conn);

	tcp_disconnect(conn.conn);
	rc = 1;

bailout:
	close_socket_lib();
	return rc;
}

/*****************************************************************************/

int smtp_send_really(struct smtp_send_options *options)
{
	int rc = 0;
	struct smtp_send_callbacks *callbacks = &options->callbacks;

	struct list *account_list = options->account_list;
	struct outmail **outmail = options->outmail;

	if (open_socket_lib())
	{
		struct account *account;

		for (account = (struct account*)list_first(account_list);account;account = (struct account*)node_next(&account->node))
		{
			struct smtp_connection conn;

			if (count_mails(account,outmail)==0) continue;

			if (smtp_connect(&conn, account, callbacks))
			{
				rc = smtp_send_mails(&conn,account,outmail,callbacks);

				if (thread_aborted())
				{
					callbacks->set_status_static("Aborted - Sending QUIT...");
					smtp_quit(&conn);
					callbacks->set_status_static("Aborted - Disconnecting...");
					tcp_disconnect(conn.conn);

					if (callbacks->skip_server())
						continue;

					break;
				}
			} else
			{
				if (thread_aborted() && !callbacks->skip_server())
				{
					callbacks->set_status_static(_("Aborted"));
					break;
				}
			}
		}

		close_socket_lib();
	}
	else
	{
		tell_from_subtask(N_("Cannot open bsdsocket.library. Please start a TCP/IP-Stack."));
	}

	return rc;
}

/*****************************************************************************/

struct outmail **create_outmail_array(int amm)
{
	int memsize;
	struct outmail **newom;

	memsize = (amm+1)*sizeof(struct outmail*) + sizeof(struct outmail)*amm;
	if ((newom = (struct outmail**)malloc(memsize)))
	{
		int i;
		struct outmail *out = (struct outmail*)(((char*)newom)+sizeof(struct outmail*)*(amm+1));

		memset(newom,0,memsize);

		for (i=0;i<amm;i++)
			newom[i] = out++;
	}
	return newom;
}

/*****************************************************************************/

struct outmail **duplicate_outmail_array(struct outmail **om_array)
{
	int amm = 0;
	struct outmail **newom_array;

	while (om_array[amm]) amm++;

	if ((newom_array = create_outmail_array(amm)))
	{
		int i;

		for (i=0;i<amm;i++)
		{
			newom_array[i]->from = mystrdup(om_array[i]->from);
			newom_array[i]->mailfile = mystrdup(om_array[i]->mailfile);
			newom_array[i]->rcp = array_duplicate(om_array[i]->rcp);
			newom_array[i]->size = om_array[i]->size;
		}
	}
	return newom_array;
}

/*****************************************************************************/

void free_outmail_array(struct outmail **om_array)
{
	struct outmail *om;
	int i = 0;
	while ((om = om_array[i++]))
	{
		if (om->from) free(om->from);
		if (om->rcp) array_free(om->rcp);
		if (om->mailfile) free(om->mailfile);
	}
	free(om_array);
}

/*****************************************************************************/

struct smtp_server *smtp_malloc(void)
{
	struct smtp_server *smtp = (struct smtp_server*)malloc(sizeof(struct smtp_server));
	if (smtp)
	{
		memset(smtp,0,sizeof(struct smtp_server));
		smtp->port = 25;
	}
	return smtp;
}

/*****************************************************************************/

struct smtp_server *smtp_duplicate(struct smtp_server *smtp)
{
	struct smtp_server *new_smtp = smtp_malloc();
	*new_smtp = *smtp;
	new_smtp->name = mystrdup(new_smtp->name);
	new_smtp->fingerprint = mystrdup(new_smtp->fingerprint);
	new_smtp->auth_login = mystrdup(new_smtp->auth_login);
	new_smtp->auth_password = mystrdup(new_smtp->auth_password);

	return new_smtp;
}

/*****************************************************************************/

void smtp_free(struct smtp_server *smtp)
{
	if (smtp->auth_password) free(smtp->auth_password);
	if (smtp->auth_login) free(smtp->auth_login);
	if (smtp->fingerprint) free(smtp->fingerprint);
	if (smtp->name) free(smtp->name);
	free(smtp);
}


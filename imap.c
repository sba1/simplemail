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
** imap.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include "account.h"
#include "codesets.h"
#include "mail.h"
#include "folder.h"
#include "lists.h"
#include "parse.h"
#include "simplemail.h"
#include "smintl.h"
#include "subthreads.h"
#include "support_indep.h"
#include "status.h"
#include "tcp.h"

#include "support.h"
#include "tcpip.h"

#include "imap.h"

#ifdef _AMIGA
#undef printf
#endif

#define MIN(a,b) ((a)<(b)?(a):(b))

static int val;

struct remote_mail
{
	unsigned int uid;
	unsigned int size;

	/* only if headers are requested  */
  char *headers;
};

struct local_mail
{
	unsigned int uid;
	unsigned int todel;
};

/**************************************************************************
 Returns the local mail array of a given folder. 0 for failure (does not
 change the contents of the ptrs in that case). Free the array with free()
 as sonn as no longer needed
**************************************************************************/
static int get_local_mail_array(struct folder *folder, struct local_mail **local_mail_array_ptr, int *num_of_mails_ptr, int *num_of_todel_mails_ptr)
{
	struct local_mail *local_mail_array;
	int num_of_mails, num_of_todel_mails;
	void *handle = NULL;
	int i,success = 0;

	folder_lock(folder);
	folder_next_mail(folder,&handle);

	num_of_mails = folder->num_mails;
	num_of_todel_mails = 0;
	
	if ((local_mail_array = malloc(sizeof(*local_mail_array) * num_of_mails)))
	{
		/* fill in the uids of the mails */
		for (i=0;i < num_of_mails;i++)
		{
			if (folder->mail_array[i])
			{
				local_mail_array[i].uid = atoi(folder->mail_array[i]->filename + 1);
				local_mail_array[i].todel = mail_is_marked_as_deleted(folder->mail_array[i]);
				num_of_todel_mails += !!local_mail_array[i].todel;
			} else
			{
				local_mail_array[i].uid = 0;
				local_mail_array[i].todel = 0;
			}
		}
		success = 1;
		*local_mail_array_ptr = local_mail_array;
		*num_of_mails_ptr = num_of_mails;
		*num_of_todel_mails_ptr = num_of_todel_mails;
	}
	folder_unlock(folder);
	return success;
}

/**************************************************************************
 Free's the given name list
**************************************************************************/
static void imap_free_name_list(struct list *list)
{
	string_list_clear(list);
	free(list);
}

/**************************************************************************
 Writes the next word into the dest buffer but not more than dest_size
**************************************************************************/
static char *imap_get_result(char *src, char *dest, int dest_size)
{
	char c;
	char delim = 0;

  dest_size--;

	dest[0] = 0;
	if (!src) return NULL;

	while ((c = *src))
	{
		if (!isspace((unsigned char)c))
			break;
		src++;
	}

	if (c)
	{
		int i = 0;
		int b = 0;
		int incr_delim = 0;

		if (c == '(') { incr_delim = c; delim = ')';}
		else if (c== '"') delim = '"';

		if (delim)
		{
			src++;
			b++;
		}

		while ((c = *src))
		{
			if (c == incr_delim) 
			{
				b++;
			} else
			if (c == delim && !(--b))
			{
				src++;
				break;
			}

			if (!delim)
			{
				if (isspace((unsigned char)c)) break;
			}

			if (i<dest_size)
				dest[i++] = c;
			src++;
		}
		dest[i] = 0;
		return src;
	}
	return NULL;
}

/**************************************************************************
 Create back a RFC822 Adress Field from an Address part of an envelope
**************************************************************************/
#if 0
static char *imap_build_address_header(char *str)
{
	char buf[360];
	char name[100];
	char nil[100];
	char user[100];
	char domain[100];

	if (imap_get_result(str,buf,sizeof(buf)))
	{
		char *addr;
		int addr_len;
		int use_name = 0;
		char *temp = buf;

		if (strncmp(temp,"NIL",3)) use_name = 1;

		temp = imap_get_result(temp,name,sizeof(name));
		temp = imap_get_result(temp,nil,sizeof(nil));
		temp = imap_get_result(temp,user,sizeof(user));
		temp = imap_get_result(temp,domain,sizeof(domain));

		addr_len = (use_name?(strlen(name)+6):0) + strlen(user) + strlen(domain) + 10;

		if ((addr = malloc(addr_len)))
		{
			char *fmt;
			if (use_name)
			{
				if (needs_quotation(name)) fmt = "\"%s\" <%s@%s>";
				else fmt = "%s <%s@%s>";
				sm_snprintf(addr,addr_len,fmt,name,user,domain);
			} else
			{
				sm_snprintf(addr,addr_len,"%s@%s",user,domain);
			}
			return addr;
		}
	}
	return NULL;
}
#endif

/**************************************************************************
 Send a simple imap command only for checking for success/failure
**************************************************************************/
int imap_send_simple_command(struct connection *conn, char *cmd)
{
	char send[200];
	char tag[20];
	char buf[380];
	char *line;
	int success;

	/* Now really remove the message */
	sprintf(tag,"%04x",val++);
	sm_snprintf(send,sizeof(send),"%s %s\r\n",tag,cmd);
	tcp_write(conn,send,strlen(send));
	tcp_flush(conn);

	success = 0;
	while ((line = tcp_readln(conn)))
	{
		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,"OK"))
				success = 1;
			break;
		}
	}
	return success;
}

/**************************************************************************
 
**************************************************************************/
static int imap_wait_login(struct connection *conn, struct imap_server *server)
{
	char *line;
	char buf[100];

	/* At the moment the loop is not necessary */
	while ((line = tcp_readln(conn)))
	{
		line = imap_get_result(line,buf,sizeof(buf));
		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,"OK")) return 1;
		else return 0;
	}
	return 0;
}

/**************************************************************************
 
**************************************************************************/
static int imap_login(struct connection *conn, struct imap_server *server)
{
	char *line;
	char tag[20];
	char send[200];
	char buf[100];

	sprintf(tag,"%04x",val++);
	sprintf(send,"%s LOGIN %s %s\r\n", tag, server->login, server->passwd);
	tcp_write(conn,send,strlen(send));
	tcp_flush(conn);

	while ((line = tcp_readln(conn)))
	{
		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,"OK"))
			{
				return 1;
			}
			break;
		}
	}
	return 0;
}

/**************************************************************************
 Returns wheather succesful.
 The given path stays in selected/examine state

 Path must be is UTF8 encoded
**************************************************************************/
static int imap_get_remote_mails(struct connection *conn, char *path, int writemode, int headers, struct remote_mail **remote_mail_array_ptr, int *num_ptr)
{
	/* get number of remote mails */
	char *line;
	char tag[20];
	char send[200];
	char buf[2048];
	char status_buf[200];
	int num_of_remote_mails = 0;
	int success = 0;
	struct remote_mail *remote_mail_array = NULL;

	path = utf8toiutf7(path,strlen(path));
	if (!path) return 0;

	sm_snprintf(status_buf,sizeof(status_buf),_("Examining folder %s"),path);
	thread_call_parent_function_sync(status_set_status,1,status_buf);

	sprintf(tag,"%04x",val++);
	sm_snprintf(send,sizeof(send),"%s %s \"%s\"\r\n",tag,writemode?"SELECT":"EXAMINE",path);
	tcp_write(conn,send,strlen(send));
	tcp_flush(conn);

	while ((line = tcp_readln(conn)))
	{
		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,"OK"))
				success = 1;
			break;
		} else
		{
			/* untagged */
			char first[200];
			char second[200];

			line = imap_get_result(line,first,sizeof(first));
			line = imap_get_result(line,second,sizeof(second));

			if (!mystricmp("EXISTS",second))
			{
				num_of_remote_mails = atoi(first);
			}
		}
	}

	if (success)
	{
		sm_snprintf(status_buf,sizeof(status_buf),_("Identified %d mails in %s"),num_of_remote_mails,path);
		thread_call_parent_function_sync(status_set_status,1,status_buf);

		if (!num_of_remote_mails)
		{
			*num_ptr = 0;
			*remote_mail_array_ptr = NULL;
			return 1;
		}

		if ((remote_mail_array = malloc(sizeof(struct remote_mail)*num_of_remote_mails)))
		{
			memset(remote_mail_array,0,sizeof(struct remote_mail)*num_of_remote_mails);

			sprintf(tag,"%04x",val++);
			sm_snprintf(send,sizeof(send),"%s FETCH %d:%d (UID RFC822.SIZE%s)\r\n",tag,1,num_of_remote_mails,headers?" BODY[HEADER.FIELDS (FROM DATE SUBJECT TO CC)]":"");
			tcp_write(conn,send,strlen(send));
			tcp_flush(conn);
			
			success = 0;
	
			while ((line = tcp_readln(conn)))
			{
				line = imap_get_result(line,buf,sizeof(buf));
				if (!mystricmp(buf,tag))
				{
					line = imap_get_result(line,buf,sizeof(buf));
					if (!mystricmp(buf,"OK")) success = 1;
					break;
				} else
				{
					/* untagged */
					unsigned int msgno;
					int is_mail = 0;
					unsigned int uid = 0;
					unsigned int size = 0;
					char *headers = NULL;
					char msgno_buf[100];
					char stuff_buf[1024];
					char cmd_buf[1024];
					char *temp;
					int i;

					line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
					line = imap_get_result(line,cmd_buf,sizeof(cmd_buf));
					imap_get_result(line,stuff_buf,sizeof(stuff_buf)); /* don't update the line because BODY[HEADER.FIELDS] would be skipped and because it is parsed diffently */

					msgno = (unsigned int)atoi(msgno_buf);
					temp = stuff_buf;

					for (i=0;i<3;i++)
					{
						temp = imap_get_result(temp,cmd_buf,sizeof(cmd_buf));
						if (!mystricmp(cmd_buf,"UID"))
						{
							temp = imap_get_result(temp,cmd_buf,sizeof(cmd_buf));
							uid = atoi(cmd_buf);
							is_mail = 1;
						}
						else if (!mystricmp(cmd_buf,"RFC822.SIZE"))
						{
							temp = imap_get_result(temp,cmd_buf,sizeof(cmd_buf));
							size = atoi(cmd_buf);
						}
						else if (!mystrnicmp(cmd_buf,"BODY",4))
						{
							char *temp_ptr;
							int todownload;

							if ((temp_ptr = strchr(line,'{'))) /* } - avoid bracket checking problems */
							{
								temp_ptr++;
								todownload = atoi(temp_ptr);
							} else todownload = 0;

							if (todownload)
							{
								int pos = 0;

								headers = malloc(todownload+1);
								headers[todownload]=0;

								while (todownload)
								{
									int dl;
									dl = tcp_read(conn,headers + pos,todownload);
									if (dl == -1 || !dl) break;
									todownload -= dl;
									pos += dl;
								}
							}
						}
					}

					if (msgno <= num_of_remote_mails && msgno > 0 && is_mail)
					{
						remote_mail_array[msgno-1].uid = uid;
						remote_mail_array[msgno-1].size = size;
						remote_mail_array[msgno-1].headers = headers;
					}
				}
			}
		}
	} else thread_call_parent_function_async(status_set_status,1,N_("Failed examining the folder"));
	if (!success)
	{
		free(remote_mail_array);
	} else
	{
		*remote_mail_array_ptr = remote_mail_array;
		*num_ptr = num_of_remote_mails;
	}
	free(path);
	return success;
}

/**************************************************************************
 Returns a list with string_node nodes which describes the folder names
**************************************************************************/
static struct list *imap_get_folders(struct connection *conn, struct imap_server *server, int all)
{
	char *line;
	char tag[20];
	char send[200];
	char buf[100];

	struct list *list = malloc(sizeof(struct list));
	if (!list) return NULL;
	list_init(list);

	sprintf(tag,"%04x",val++);
	sprintf(send,"%s %s \"\" *\r\n",tag,all?"LIST":"LSUB");
	tcp_write(conn,send,strlen(send));
	tcp_flush(conn);

	while ((line = tcp_readln(conn)))
	{
		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,"OK"))
			{
				return list;
			}
			break;
		} else
		{
			char *utf_name;

			/* command */
			line = imap_get_result(line,buf,sizeof(buf));

			/* read flags */
			line = imap_get_result(line,buf,sizeof(buf));

			/* read delim */
			line = imap_get_result(line,buf,sizeof(buf));

			/* read name */
			line = imap_get_result(line,buf,sizeof(buf));

			if ((utf_name = iutf7ntoutf8(buf, strlen(buf))))
			{
				string_list_insert_tail(list,utf_name);
				free(utf_name);
			}
		}
	}

	imap_free_name_list(list);
	return NULL;
}

/**************************************************************************
 
**************************************************************************/
static int imap_synchonize_folder(struct connection *conn, struct imap_server *server, char *imap_path)
{
	struct folder *folder;
	int success = 0;
	folders_lock();
	if ((folder = folder_find_by_imap(server->name, imap_path)))
	{
		int num_of_local_mails;
		int num_of_todel_local_mails;
		struct local_mail *local_mail_array;
		char path[380];

		/* Get the local mails */
		if (!get_local_mail_array(folder, &local_mail_array, &num_of_local_mails, &num_of_todel_local_mails))
		{
			folder_unlock(folder);
			folders_unlock();
			return 0;
		}

		/* Change the current directory to the to be synchronized folder */
		folder_lock(folder);
		getcwd(path, sizeof(path));
		if (chdir(folder->path) == -1)
		{
			free(local_mail_array);
			folder_unlock(folder);
			folders_unlock();
			return 0;
		}
		folder_unlock(folder);

		folders_unlock();

		if (local_mail_array)
		{
			/* get number of remote mails */
			char *line;
			char tag[20];
			char send[200];
			char buf[100];
			char status_buf[200];
			int num_of_remote_mails = 0;
			struct remote_mail *remote_mail_array = NULL;

			if (num_of_todel_local_mails)
			{
				if (imap_get_remote_mails(conn, folder->imap_path, 1, 0, &remote_mail_array, &num_of_remote_mails))
				{
					int i,j;
					for (i = 0 ; i < num_of_local_mails; i++)
					{
						if (mail_is_marked_as_deleted(folder->mail_array[i]))
						{
							for (j = 0; j < num_of_remote_mails; j++)
							{
								if (local_mail_array[i].uid == remote_mail_array[j].uid)
								{
									sprintf(tag,"%04x",val++);
									sprintf(send,"%s STORE %d +FLAGS.SILENT (\\Deleted)\r\n",tag,j+1);
									tcp_write(conn,send,strlen(send));
									tcp_flush(conn);

									success = 0;
									while ((line = tcp_readln(conn)))
									{
										line = imap_get_result(line,buf,sizeof(buf));
										if (!mystricmp(buf,tag))
										{
											line = imap_get_result(line,buf,sizeof(buf));
											if (!mystricmp(buf,"OK"))
												success = 1;
											break;
										}
									}
									if (!success) break;
								}
							}
						}
					}

					if (success)
					{
						sprintf(tag,"%04x",val++);
						sprintf(send,"%s EXPUNGE\r\n",tag);
						tcp_write(conn,send,strlen(send));
						tcp_flush(conn);

						success = 0;
						while ((line = tcp_readln(conn)))
						{
							line = imap_get_result(line,buf,sizeof(buf));
							if (!mystricmp(buf,tag))
							{
								line = imap_get_result(line,buf,sizeof(buf));
								if (!mystricmp(buf,"OK"))
									success = 1;
								break;
							}
						}
					}
					free(remote_mail_array);
					remote_mail_array = NULL;
				}
			}

			if (imap_get_remote_mails(conn, folder->imap_path, 0, 0, &remote_mail_array, &num_of_remote_mails))
			{
				int i,j;
				int msgtodl;
				unsigned int max_todl_bytes = 0;
				unsigned int accu_todl_bytes = 0; /* this represents the exact todl bytes according to the RFC822.SIZE */
				unsigned int todl_bytes = 0;

				/* delete mails which are not on server but localy */
				for (i = 0 ; i < num_of_local_mails; i++)
				{
					unsigned int local_uid = local_mail_array[i].uid;
					for (j = 0 ; j < num_of_remote_mails; j++)
					{
						if (local_uid == remote_mail_array[j].uid)
						{
							local_uid = 0;
							break;
						}
					}
					if (local_uid)
					{
						thread_call_parent_function_sync(callback_delete_mail_by_uid,3,server->name,imap_path,local_uid);
					}
				}

				for (msgtodl = 1;msgtodl <= num_of_remote_mails;msgtodl++)
				{
					/* check if the mail already exists */
					int does_exist = 0;
					for (i=0; i < num_of_local_mails;i++)
					{
						if (local_mail_array[i].uid == remote_mail_array[msgtodl-1].uid)
							does_exist = 1;
					}
					if (does_exist) continue;

					max_todl_bytes += remote_mail_array[msgtodl-1].size;
				}

				thread_call_parent_function_async(status_init_gauge_as_bytes,1,max_todl_bytes);

				if (!num_of_remote_mails) success = 1;

				for (msgtodl = 1;msgtodl <= num_of_remote_mails;msgtodl++)
				{
					/* check if the mail already exists */
					int does_exist = 0;
					for (i=0; i < num_of_local_mails;i++)
					{
						if (local_mail_array[i].uid == remote_mail_array[msgtodl-1].uid)
							does_exist = 1;
					}
					if (does_exist)
					{
						success = 1;
						continue;
					}

					accu_todl_bytes += remote_mail_array[msgtodl-1].size;

					sprintf(status_buf,_("Fetching mail %d from server"),msgtodl);
					thread_call_parent_function_sync(status_set_status,1,status_buf);

					sprintf(tag,"%04x",val++);
					sprintf(send,"%s FETCH %d RFC822\r\n",tag,msgtodl);
					tcp_write(conn,send,strlen(send));
					tcp_flush(conn);

					success = 0;
					while ((line = tcp_readln(conn)))
					{
						line = imap_get_result(line,buf,sizeof(buf));
						if (!mystricmp(buf,tag))
						{
							line = imap_get_result(line,buf,sizeof(buf));
							if (!mystricmp(buf,"OK"))
								success = 1;
							break;
						} else
						{
							/* untagged */
							if (buf[0] == '*')
							{
								char msgno_buf[200];
								char *temp_ptr;
								int msgno;
								int todownload;
								line++;

								line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
								msgno = atoi(msgno_buf); /* ignored */

								/* skip the fetch command */
								line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
								if ((temp_ptr = strchr(line,'{'))) /* } - avoid bracket checking problems */
								{
									temp_ptr++;
									todownload = atoi(temp_ptr);
								} else todownload = 0;

								if (todownload)
								{
									FILE *fh;
									char filename_buf[60];
									sprintf(filename_buf,"u%d",remote_mail_array[msgtodl-1].uid); /* u means unchanged */

									if ((fh = fopen(filename_buf,"w")))
									{
										while (todownload)
										{
											char buf[204];
											int dl;
											dl = tcp_read(conn,buf,MIN((sizeof(buf)-4),todownload));

											if (dl == -1 || !dl) break;
											todl_bytes = MIN(accu_todl_bytes,todl_bytes + dl);
											thread_call_parent_function_async(status_set_gauge, 1, todl_bytes);
											fwrite(buf,1,dl,fh);
											todownload -= dl;
										}
										fclose(fh);
										thread_call_parent_function_sync(callback_new_imap_mail_arrived, 3, filename_buf, server->name, imap_path);
									}
								}
							}
						}
					}

					if (!success) break;
					todl_bytes = accu_todl_bytes;
					/* TODO: should be enforced */
					thread_call_parent_function_async(status_set_gauge, 1, todl_bytes);
				}
			}
		}

		chdir(path);

		free(local_mail_array);
	} else folders_unlock();
	return success;
}

/**************************************************************************
 
**************************************************************************/
void imap_synchronize_really(struct list *imap_list, int called_by_auto)
{
	if (open_socket_lib())
	{
		struct imap_server *server = (struct imap_server*)list_first(imap_list);

		for( ;server; server = (struct imap_server*)node_next(&server->node))
		{
			struct connection *conn;
			char head_buf[100];

			sprintf(head_buf,_("Synchronizing mails with %s"),server->name);
			thread_call_parent_function_async_string(status_set_head, 1, head_buf);
			thread_call_parent_function_async_string(status_set_title, 1, server->name);
			thread_call_parent_function_async_string(status_set_connect_to_server, 1, server->name);

			/* Ask for the login/password */
/*			if (server->ask)
			{
				char *password = malloc(512);
				char *login = malloc(512);

				if (password && login)
				{
					int rc;

					if (server->login) mystrlcpy(login,server->login,512);
					password[0] = 0;

					if ((rc = thread_call_parent_function_sync(sm_request_login,4,server->name,login,password,512)))
					{
						server->login = mystrdup(login);
						server->passwd = mystrdup(password);
					}
				}

				free(password);
				free(login);
			}*/

			if ((conn = tcp_connect(server->name, server->port, server->ssl)))
			{
				thread_call_parent_function_async(status_set_status,1,N_("Waiting for login..."));
				if (imap_wait_login(conn,server))
				{
					thread_call_parent_function_async(status_set_status,1,N_("Login..."));
					if (imap_login(conn,server))
					{
						struct list *folder_list;
						thread_call_parent_function_async(status_set_status,1,N_("Login successful"));
						thread_call_parent_function_async(status_set_status,1,N_("Checking for folders"));

						if ((folder_list = imap_get_folders(conn,server,0)))
						{
							struct string_node *node;

							/* add the folders */
							node = (struct string_node*)list_first(folder_list);
							while (node)
							{
								thread_call_parent_function_sync(callback_add_imap_folder,2,server->name,node->string);
								node = (struct string_node*)node_next(&node->node);
							}
							thread_call_parent_function_sync(callback_refresh_folders,0);

							/* sync the folders */
							node = (struct string_node*)list_first(folder_list);
							while (node)
							{
								if (!(imap_synchonize_folder(conn, server, node->string)))
									break;
								node = (struct string_node*)node_next(&node->node);
							}

							imap_free_name_list(folder_list);
						}
					} else thread_call_parent_function_async(status_set_status,1,N_("Login failed!"));
				}
				tcp_disconnect(conn);

				if (thread_aborted()) break;
			} else
			{
				if (thread_aborted()) break;
				else tell_from_subtask((char*)tcp_strerror(tcp_error_code()));
			}

			/* Clear the preselection entries */
			thread_call_parent_function_sync(status_mail_list_clear,0);
		}
		close_socket_lib();

//		thread_call_parent_function_async(callback_number_of_mails_downloaded,1,nummails);
	}
	else
	{
		tell_from_subtask(N_("Cannot open the bsdsocket.library!"));
	}
	
}

struct imap_entry_msg
{
	struct list *imap_list;
	int called_by_auto;
};

/**************************************************************************
 Entrypoint for the fetch mail process
**************************************************************************/
static int imap_entry(struct imap_entry_msg *msg)
{
	struct list imap_list;
	struct imap_server *imap;
	int called_by_auto = msg->called_by_auto;

	list_init(&imap_list);
	imap = (struct imap_server*)list_first(msg->imap_list);

	while (imap)
	{
		if (imap->name)
		{
			struct imap_server *new_imap = imap_duplicate(imap);
			if (new_imap)
				list_insert_tail(&imap_list,&new_imap->node);
		}
		imap = (struct imap_server*)node_next(&imap->node);
	}

	if (thread_parent_task_can_contiue())
	{
		thread_call_parent_function_async(status_init,1,0);
		if (called_by_auto) thread_call_parent_function_async(status_open_notactivated,0);
		else thread_call_parent_function_async(status_open,0);

		imap_synchronize_really(&imap_list, called_by_auto);

		thread_call_parent_function_async(status_close,0);
	}
	return 0;

}

/**************************************************************************
 Synchronizes the given imap servers
**************************************************************************/
int imap_synchronize(struct list *imap_list, int called_by_auto)
{
	struct imap_entry_msg msg;
	msg.imap_list = imap_list;
	msg.called_by_auto = called_by_auto;
	return thread_start(THREAD_FUNCTION(&imap_entry),&msg);
}

struct imap_get_folder_list_entry_msg
{
	struct imap_server *server;
	void (*callback)(struct imap_server *, struct list *, struct list *);
};

/**************************************************************************
 
**************************************************************************/
static void imap_get_folder_list_really(struct imap_server *server, void (*callback)(struct imap_server *, struct list *, struct list *))
{
	if (open_socket_lib())
	{
		struct connection *conn;
		char head_buf[100];

		sprintf(head_buf,_("Reading folders of %s"),server->name);
		thread_call_parent_function_async_string(status_set_head, 1, head_buf);
		thread_call_parent_function_async_string(status_set_title, 1, server->name);
		thread_call_parent_function_async_string(status_set_connect_to_server, 1, server->name);

		if ((conn = tcp_connect(server->name, server->port, server->ssl)))
		{
			thread_call_parent_function_async(status_set_status,1,N_("Waiting for login..."));
			if (imap_wait_login(conn,server))
			{
				thread_call_parent_function_async(status_set_status,1,N_("Login..."));
				if (imap_login(conn,server))
				{
					struct list *all_folder_list;
					thread_call_parent_function_async(status_set_status,1,N_("Reading folders..."));
					if ((all_folder_list = imap_get_folders(conn,server,1)))
					{
						struct list *sub_folder_list;
						if ((sub_folder_list = imap_get_folders(conn,server,0)))
						{
							thread_call_parent_function_sync(callback,3,server,all_folder_list,sub_folder_list);
							imap_free_name_list(sub_folder_list);
						}
						imap_free_name_list(all_folder_list);
					}
				}
			}
			tcp_disconnect(conn);
		}

		close_socket_lib();
	}
}

/**************************************************************************
 Entrypoint for the get folder list mail process
**************************************************************************/
static int imap_get_folder_list_entry(struct imap_get_folder_list_entry_msg *msg)
{
	struct imap_server *server = imap_duplicate(msg->server);
	void (*callback)(struct imap_server *, struct list *, struct list *) = msg->callback;

	if (thread_parent_task_can_contiue())
	{
		thread_call_parent_function_async(status_init,1,0);
		thread_call_parent_function_async(status_open,0);

		imap_get_folder_list_really(server,callback);

		thread_call_parent_function_async(status_close,0);
	}
	return 0;
}

/**************************************************************************
 Returns the list of all folders of the imap server
**************************************************************************/
int imap_get_folder_list(struct imap_server *server, void (*callback)(struct imap_server *, struct list *, struct list *))
{
	struct imap_get_folder_list_entry_msg msg;
	msg.server = server;
	msg.callback = callback;
	return thread_start(THREAD_FUNCTION(&imap_get_folder_list_entry),&msg);
}

/**************************************************************************
 NOTE: list elements are removed!
**************************************************************************/
static void imap_submit_folder_list_really(struct imap_server *server, struct list *list)
{
	if (open_socket_lib())
	{
		struct connection *conn;
		char head_buf[100];

		sprintf(head_buf,_("Submitting subscribed folders to %s"),server->name);
		thread_call_parent_function_async_string(status_set_head, 1, head_buf);
		thread_call_parent_function_async_string(status_set_title, 1, server->name);
		thread_call_parent_function_async_string(status_set_connect_to_server, 1, server->name);

		if ((conn = tcp_connect(server->name, server->port, server->ssl)))
		{
			thread_call_parent_function_async(status_set_status,1,N_("Waiting for login..."));
			if (imap_wait_login(conn,server))
			{
				thread_call_parent_function_async(status_set_status,1,N_("Login..."));
				if (imap_login(conn,server))
				{
					struct list *all_folder_list;
					thread_call_parent_function_async(status_set_status,1,N_("Reading folders..."));
					if ((all_folder_list = imap_get_folders(conn,server,1)))
					{
						struct list *sub_folder_list;
						thread_call_parent_function_async(status_set_status,1,N_("Reading subscribed folders..."));
						if ((sub_folder_list = imap_get_folders(conn,server,0)))
						{
							char *line;
							char tag[20];
							char send[200];
							char buf[100];
							struct string_node *node;

							node = (struct string_node*)list_first(list);
							while (node)
							{
								if (!string_list_find(sub_folder_list,node->string))
								{
									int success = 0;
									char *path = utf8toiutf7(node->string,strlen(node->string));
									if (path)
									{
										/* subscribe this folder */
										sprintf(tag,"%04x",val++);
										sprintf(send,"%s SUBSCRIBE \"%s\"\r\n",tag,path);
										tcp_write(conn,send,strlen(send));
										tcp_flush(conn);
	
										while ((line = tcp_readln(conn)))
										{
											line = imap_get_result(line,buf,sizeof(buf));
											if (!mystricmp(buf,tag))
											{
												line = imap_get_result(line,buf,sizeof(buf));
												if (!mystricmp(buf,"OK"))
												{
													success = 1;
												} else
												{
													tell_from_subtask(N_("Subscribing folders failed!"));
												}
												break;
											}
										}
									}
									free(path);
								}
								node = (struct string_node*)node_next(&node->node);
							}

							node = (struct string_node*)list_first(sub_folder_list);
							while (node)
							{
								if (!string_list_find(list,node->string))
								{
									int success = 0;
									char *path = utf8toiutf7(node->string,strlen(node->string));
									if (path)
									{
										/* subscribe this folder */
										sprintf(tag,"%04x",val++);
										sprintf(send,"%s UNSUBSCRIBE \"%s\"\r\n",tag,path);
										tcp_write(conn,send,strlen(send));
										tcp_flush(conn);

										while ((line = tcp_readln(conn)))
										{
											line = imap_get_result(line,buf,sizeof(buf));
											if (!mystricmp(buf,tag))
											{
												line = imap_get_result(line,buf,sizeof(buf));
												if (!mystricmp(buf,"OK"))
												{
													success = 1;
												} else
												{
													tell_from_subtask(N_("Unsubscribing folders failed!"));
												}
												break;
											}
										}
										free(path);
									}
								}
								node = (struct string_node*)node_next(&node->node);
							}
							
							imap_free_name_list(sub_folder_list);
						}
						imap_free_name_list(all_folder_list);
					}
				}
			}
			tcp_disconnect(conn);
		}

		close_socket_lib();
	}
}

struct imap_submit_folder_list_entry_msg
{
	struct imap_server *server;
	struct list *list;
};


/**************************************************************************
 Entrypoint for the get folder list mail process
**************************************************************************/
static int imap_submit_folder_list_entry(struct imap_submit_folder_list_entry_msg *msg)
{
	struct imap_server *server = imap_duplicate(msg->server);
	struct list list;
	struct string_node *node;
	
	list_init(&list);
	node = (struct string_node*)list_first(msg->list);
	while (node)
	{
		string_list_insert_tail(&list,node->string);
		node = (struct string_node*)node_next(&node->node);
	}
	

	if (thread_parent_task_can_contiue())
	{
		thread_call_parent_function_async(status_init,1,0);
		thread_call_parent_function_async(status_open,0);

		imap_submit_folder_list_really(server,&list);

		thread_call_parent_function_async(status_close,0);
	}
	string_list_clear(&list);
	return 0;
}

/**************************************************************************
 Submit the given list as subscribed to the server
**************************************************************************/
int imap_submit_folder_list(struct imap_server *server, struct list *list)
{
	struct imap_submit_folder_list_entry_msg msg;
	msg.server = server;
	msg.list = list;
	return thread_start(THREAD_FUNCTION(&imap_submit_folder_list_entry),&msg);
}

/**************************************************************************
 malloc() a imap_server and initializes it with default values.
 TODO: rename all imap identifiers to imap
**************************************************************************/
struct imap_server *imap_malloc(void)
{
	struct imap_server *imap;
	if ((imap = (struct imap_server*)malloc(sizeof(*imap))))
	{
		memset(imap,0,sizeof(struct imap_server));
		imap->port = 143;
	}
	return imap;
}

/**************************************************************************
 Duplicates a imap_server
**************************************************************************/
struct imap_server *imap_duplicate(struct imap_server *imap)
{
	struct imap_server *new_imap = imap_malloc();
	if (new_imap)
	{
		new_imap->name = mystrdup(imap->name);
		new_imap->login = mystrdup(imap->login);
		new_imap->passwd = mystrdup(imap->passwd);
		new_imap->port = imap->port;
		new_imap->active = imap->active;
		new_imap->ssl = imap->ssl;
	}
	return new_imap;
}

/**************************************************************************
 Frees a imap_server completly
**************************************************************************/
void imap_free(struct imap_server *imap)
{
	if (imap->name) free(imap->name);
	if (imap->login) free(imap->login);
	if (imap->passwd) free(imap->passwd);
	free(imap);
}

/**************************************************************************
 Checks if a new connection is needed
**************************************************************************/
static int imap_new_connection_needed(struct imap_server *srv1, struct imap_server *srv2)
{
	if (!srv1 && !srv2) return 0;
	if (!srv1) return 1;
	if (!srv2) return 1;

  return strcmp(srv1->name,srv2->name) || (srv1->port != srv2->port) || strcmp(srv1->login,srv2->login) ||
         strcmp(srv1->passwd,srv2->passwd) || (srv1->ssl != srv2->ssl);
}


/***** IMAP Thread *****/

static thread_t imap_thread;
static struct connection *imap_connection;
static int imap_socket_lib_open;
static struct imap_server *imap_server;
static char *imap_folder;
static char *imap_local_path;

static void imap_thread_really_download_mails(void)
{
	char path[380];
	struct remote_mail *remote_mail_array;
	int num_remote_mails;

	if (!imap_connection) return;

	getcwd(path, sizeof(path));
	if (chdir(imap_local_path) == -1) return;

	if (imap_get_remote_mails(imap_connection, imap_folder, 0, 1, &remote_mail_array, &num_remote_mails))
	{
		int i,j;
		int num_of_local_mails;
		int num_of_todel_local_mails;
		struct local_mail *local_mail_array;
		struct folder *local_folder;

		folders_lock();
		if ((local_folder = folder_find_by_imap(imap_server->name, imap_folder)))
		{
			if (get_local_mail_array(local_folder, &local_mail_array, &num_of_local_mails, &num_of_todel_local_mails))
			{
				folders_unlock();

				/* delete mails which are not on server but localy */
				for (i = 0 ; i < num_of_local_mails; i++)
				{
					unsigned int local_uid = local_mail_array[i].uid;
					for (j = 0 ; j < num_remote_mails; j++)
					{
						if (local_uid == remote_mail_array[j].uid)
						{
							local_uid = 0;
							break;
						}
					}
					if (local_uid)
					{
						thread_call_parent_function_sync(callback_delete_mail_by_uid,3,imap_server->name,imap_folder,local_uid);
					}
				}

				for (i=0;i<num_remote_mails;i++)
				{
					char filename_buf[60];
					FILE *fh;

					int does_exist = 0;
					for (j=0; j < num_of_local_mails;j++)
					{
						if (local_mail_array[j].uid == remote_mail_array[i].uid)
						{
							does_exist = 1;
							break;
						}
					}
					if (does_exist) continue;

					sprintf(filename_buf,"u%d",remote_mail_array[i].uid); /* u means unchanged */

					/* Store as a partial mail */
					if ((fh = fopen(filename_buf,"w")))
					{
						fprintf(fh,"X-SimpleMail-Partial: yes\n");
						fprintf(fh,"X-SimpleMail-Size: %d\n",remote_mail_array[i].size);
						if (remote_mail_array[i].headers) fputs(remote_mail_array[i].headers,fh);
						fclose(fh);

						thread_call_parent_function_sync(callback_new_imap_mail_arrived, 3, filename_buf, imap_server->name, imap_folder);
					}
				}
			} else folders_unlock();
		} else folders_unlock();
		free(remote_mail_array);
	}
	chdir(path);
}

static void imap_thread_really_connect_to_server(void)
{
	if (imap_server)
	{
		char status_buf[256];

		if (!imap_socket_lib_open)
		 imap_socket_lib_open = open_socket_lib();
		if (!imap_socket_lib_open) return;

		if (imap_connection)
			tcp_disconnect(imap_connection);

		/* Display "Connecting" - status message */
		sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Connecting..."));
		thread_call_parent_function_async_string(status_set_status,1,status_buf);

		if ((imap_connection = tcp_connect(imap_server->name, imap_server->port, imap_server->ssl)))
		{
			if (imap_wait_login(imap_connection,imap_server))
			{
				/* Display "Loggin in" - status message */
				sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Loggin in..."));
				thread_call_parent_function_async_string(status_set_status,1,status_buf);

				if (imap_login(imap_connection,imap_server))
				{
					struct list *folder_list;

					/* Display "Retrieving mail" - status message */
					sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Retrieving mail folders..."));
					thread_call_parent_function_async_string(status_set_status,1,status_buf);

					/* We have now connected to the server, check for the folders at first */
					folder_list = imap_get_folders(imap_connection, imap_server, 0);
					if (folder_list)
					{
						struct string_node *node;
						/* add the folders */
						node = (struct string_node*)list_first(folder_list);
						while (node)
						{
							thread_call_parent_function_sync(callback_add_imap_folder,2,imap_server->name,node->string);
							node = (struct string_node*)node_next(&node->node);
						}
						thread_call_parent_function_sync(callback_refresh_folders,0);

						imap_free_name_list(folder_list);

						imap_thread_really_download_mails();

						/* Display "Connected" - status message */
						sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Connected"));
						thread_call_parent_function_async_string(status_set_status,1,status_buf);
					}
				} else
				{
					sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Loggin in failed. Check Username and Password for this account"));
					thread_call_parent_function_async_string(status_set_status,1,status_buf);
				}
			}
		} else
		{
			/* Display "Failed to connect" - status message */
			sm_snprintf(status_buf,sizeof(status_buf),"%s: %s",imap_server->name, _("Connecting to the server failed"));
			thread_call_parent_function_async_string(status_set_status,1,status_buf);
		}
	}
}

static int imap_thread_connect_to_server(struct imap_server *server, char *folder, char *local_path)
{
	if (imap_new_connection_needed(imap_server,server))
	{
		if (imap_server) imap_free(imap_server);
		if ((imap_server = imap_duplicate(server)))
		{
			if (imap_folder) free(imap_folder);
			imap_folder = mystrdup(folder);

			if (imap_local_path) free(imap_local_path);
			imap_local_path = mystrdup(local_path);

			return thread_push_function(imap_thread_really_connect_to_server, 0);
		}
	} else
	{
		if (imap_folder) free(imap_folder);
		imap_folder = mystrdup(folder);

		if (imap_local_path) free(imap_local_path);
		imap_local_path = mystrdup(local_path);

		return thread_push_function(imap_thread_really_download_mails, 0);
	}
}

static int imap_thread_download_mail(struct folder *f, struct mail *m)
{
	char send[200];
	char tag[20];
	char buf[380];
	char *line;
	int uid;
	int success;

	if (!imap_server) return 0;
	if (!imap_connection) return 0;

	uid = atoi(m->filename + 1);

	sprintf(tag,"%04x",val++);
	sprintf(send,"%s UID FETCH %d RFC822\r\n",tag,uid);
	tcp_write(imap_connection,send,strlen(send));
	tcp_flush(imap_connection);

	success = 0;
	while ((line = tcp_readln(imap_connection)))
	{
		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,"OK"))
				success = 1;
			break;
		} else
		{
			/* untagged */
			if (buf[0] == '*')
			{
				char msgno_buf[200];
				char *temp_ptr;
				int msgno;
				int todownload;
				line++;

				line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
				msgno = atoi(msgno_buf); /* ignored */

				/* skip the fetch command */
				line = imap_get_result(line,msgno_buf,sizeof(msgno_buf));
				if ((temp_ptr = strchr(line,'{'))) /* } - avoid bracket checking problems */
				{
					temp_ptr++;
					todownload = atoi(temp_ptr);
				} else todownload = 0;

				if (todownload)
				{
					FILE *fh;

					mystrlcpy(buf,f->path,sizeof(buf));
					sm_add_part(buf,m->filename,sizeof(buf));

					if ((fh = fopen(buf,"w")))
					{
						while (todownload)
						{
							int dl;
							dl = tcp_read(imap_connection,buf,MIN((sizeof(buf)-4),todownload));

							if (dl == -1 || !dl) break;
							fwrite(buf,1,dl,fh);
							todownload -= dl;
						}
						fclose(fh);
					}
				}
			}
		}
	}
	return success;
}

/**************************************************************************
	Move a mail from one folder into another one
**************************************************************************/
static int imap_thread_move_mail(struct mail *mail, struct folder *src_folder, struct folder *dest_folder)
{
	char send[200];
	char tag[20];
	char buf[380];
	char *line;
	int uid;
	int msgno = -1;
	int success;

	if (!imap_server) return 0;
	if (!imap_connection) return 0;

	sm_snprintf(send,sizeof(send),"SELECT \"%s\"\r\n",src_folder->imap_path);
	if (!imap_send_simple_command(imap_connection,send)) return 0;

	success = 0;
	uid = atoi(mail->filename + 1);

	sprintf(tag,"%04x",val++);
	sprintf(send,"%s SEARCH UID %d\r\n",tag,uid);
	tcp_write(imap_connection,send,strlen(send));
	tcp_flush(imap_connection);

	while ((line = tcp_readln(imap_connection)))
	{
		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,"OK"))
				success = 1;
			break;
		} else
		{
			/* untagged */
			char first[200];
			char second[200];

			line = imap_get_result(line,first,sizeof(first));
			line = imap_get_result(line,second,sizeof(second));

			msgno = atoi(second);
		}
	}

	if (success && msgno != -1)
	{
		success = 0;

		/* Copy the message */
		sprintf(send,"COPY %d %s\r\n",msgno,dest_folder->imap_path);
		success = imap_send_simple_command(imap_connection,send);

		if (success)
		{
			sprintf(send,"STORE %d +FLAGS.SILENT (\\Deleted)\r\n",msgno);
			success = imap_send_simple_command(imap_connection,send);

			if (success)
			{
				success = imap_send_simple_command(imap_connection,"EXPUNGE");
			}
		}
	}

	return success;
}

/**************************************************************************
 Delete a mail permanently. Thread version.
**************************************************************************/
static int imap_thread_delete_mail_by_filename(char *filename, struct folder *folder)
{
	char send[200];
	char tag[20];
	char buf[380];
	char *line;
	int uid;
	int msgno = -1;
	int success;

	if (!imap_server) return 0;
	if (!imap_connection) return 0;

	sm_snprintf(send,sizeof(send),"SELECT \"%s\"\r\n",folder->imap_path);
	if (!imap_send_simple_command(imap_connection,send)) return 0;

	success = 0;
	uid = atoi(filename + 1);

	sprintf(tag,"%04x",val++);
	sprintf(send,"%s SEARCH UID %d\r\n",tag,uid);
	tcp_write(imap_connection,send,strlen(send));
	tcp_flush(imap_connection);

	while ((line = tcp_readln(imap_connection)))
	{
		line = imap_get_result(line,buf,sizeof(buf));
		if (!mystricmp(buf,tag))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,"OK"))
				success = 1;
			break;
		} else
		{
			/* untagged */
			char first[200];
			char second[200];

			line = imap_get_result(line,first,sizeof(first));
			line = imap_get_result(line,second,sizeof(second));

			msgno = atoi(second);
		}
	}

	if (success && msgno != -1)
	{
		sprintf(send,"STORE %d +FLAGS.SILENT (\\Deleted)\r\n",msgno);
		success = imap_send_simple_command(imap_connection,send);

		if (success)
		{
			success = imap_send_simple_command(imap_connection,"EXPUNGE");
		}
	}

	return success;
}

/**************************************************************************
 Store a mail. Thread version.
**************************************************************************/
static int imap_thread_append_mail(struct mail *mail, char *source_dir, char *dest_imap_path)
{
	char send[200];
	char buf[380];
	char *line;
	int success;
	FILE *fh,*tfh;
	char tag[20],path[380];
	char line_buf[1200];
	int filesize;

	if (!imap_server) return 0;
	if (!imap_connection) return 0;

	/* At first copy the mail to a temporary location because we may store the emails which only has a \n ending */
	if (!(tfh = tmpfile())) return 0;
	if (chdir(source_dir) == -1)
	{
		fclose(tfh);
		return 0;
	}

	if (!(fh = fopen(mail->filename,"r")))
	{
		chdir(path);
		fclose(tfh);
		return 0;
	}

	while (fgets(line_buf,sizeof(line_buf)-2,fh))
	{
		int len = strlen(line_buf);
		if (len > 2 && line_buf[len-2] != '\r' && line_buf[len-1] == '\n')
		{
			line_buf[len-1] = '\r';
			line_buf[len] = '\n';
			line_buf[len+1]=0;
		} else if (len == 1 && line_buf[0] == '\n')
		{
			line_buf[0] = '\r';
			line_buf[1] = '\n';
			line_buf[2] = 0;
		}
		fputs(line_buf,tfh);
	}
	fclose(fh);
	chdir(path);

	/* Now upload the mail */
	filesize = ftell(tfh);
	fseek(tfh,0,SEEK_SET);
	sprintf(tag,"%04x",val++);
	sm_snprintf(send,sizeof(send),"%s APPEND %s {%d}\r\n",tag,dest_imap_path,filesize);
	tcp_write(imap_connection,send,strlen(send));
	tcp_flush(imap_connection);

	line = tcp_readln(imap_connection);
	if (line[0] != '+')
	{
		fclose(tfh);
		return 0;
	}

	success = 1;
	while (filesize>0)
	{
		int read = fread(line_buf,1,sizeof(line_buf),tfh);
		if (!read || read == -1)
		{
			success = 0;
			break;
		}
		tcp_write(imap_connection,line_buf,read);
		filesize -= read;
	}
	fclose(tfh);

	if (success)
	{
		tcp_write(imap_connection,"\r\n",2);
		success = 0;
		while ((line = tcp_readln(imap_connection)))
		{
			line = imap_get_result(line,buf,sizeof(buf));
			if (!mystricmp(buf,tag))
			{
				line = imap_get_result(line,buf,sizeof(buf));
				if (!mystricmp(buf,"OK"))
					success = 1;
				break;
			}
		}
	}
	return success;
}

/**************************************************************************
 The entry point for the imap thread. It just go into the wait state and
 then frees all resources when finished
**************************************************************************/
static void imap_thread_entry(void *test)
{
	if (thread_parent_task_can_contiue())
	{
		thread_wait(NULL,NULL,0);

		if (imap_connection)
		{
			tcp_disconnect(imap_connection);
			imap_connection = NULL;
		}

		if (imap_socket_lib_open)
		{
			close_socket_lib();
			imap_socket_lib_open = 0;
		}
	}
}

/**************************************************************************
 Let the imap thread connect to the imap server represented by the folder.
**************************************************************************/
void imap_thread_connect(struct folder *folder)
{
	struct imap_server *server = account_find_imap_server_by_folder(folder);
	if (!server) return;

	if (!imap_thread)
	{
		imap_thread = thread_add("SimpleMail - IMAP thread", THREAD_FUNCTION(&imap_thread_entry),NULL);
	}

	if (imap_thread)
	{
		thread_call_function_sync(imap_thread, imap_thread_connect_to_server,3,server,folder->imap_path, folder->path);
	}
}

/**************************************************************************
 Download the given mail from the imap server using 
***************************************************************************/
int imap_download_mail(struct folder *f, struct mail *m)
{
	if (!imap_thread) return 0;
	if (!(m->flags & MAIL_FLAGS_PARTIAL)) return 0;

	if (thread_call_function_sync(imap_thread, imap_thread_download_mail, 2, f, m))
	{
		folder_set_mail_flags(f,m, (m->flags & (~MAIL_FLAGS_PARTIAL)));
		return 1;
	}
	return 0;
}

/**************************************************************************
 Moves the mail from a source folder to a destination folder
***************************************************************************/
int imap_move_mail(struct mail *mail, struct folder *src_folder, struct folder *dest_folder)
{
	if (!imap_thread) return 0;
	if (!src_folder->is_imap || !dest_folder->is_imap) return 0;
	if (thread_call_function_sync(imap_thread, imap_thread_move_mail, 3, mail, src_folder, dest_folder))
	{
		return 1;
	}
	return 0;
}

/**************************************************************************
 Deletes a mail from the server
***************************************************************************/
int imap_delete_mail_by_filename(char *filename, struct folder *folder)
{
	if (!imap_thread) return 0;
	if (!folder->is_imap) return 0;

	if (thread_call_function_sync(imap_thread, imap_thread_delete_mail_by_filename, 2, filename, folder))
	{
		return 1;
	}
	return 0;
}

/**************************************************************************
 Store the given mail in the given folder of an imap server
***************************************************************************/
int imap_append_mail(struct mail *mail, char *source_dir, char *dest_imap_path)
{
	if (!imap_thread) return 0;

	if (thread_call_function_sync(imap_thread, imap_thread_append_mail, 3, mail, source_dir, dest_imap_path))
	{
		return 1;
	}
	return 0;
}

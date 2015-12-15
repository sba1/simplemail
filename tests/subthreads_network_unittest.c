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

#include "subthreads.h"
#include "coroutines.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <CUnit/Basic.h>

/****************************************************************************/

static void listen_for_connection(void)
{
	static int fd;
	int err;

	struct sockaddr_in local_addr = {0};
	socklen_t local_addrlen = sizeof(local_addr);

	struct sockaddr_in remote_addr = {0};
	socklen_t remote_addrlen = sizeof(remote_addr);

	int rc;

	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(1234);
	local_addr.sin_addr.s_addr = INADDR_ANY;

	printf("Listen\n");

	fd = socket(AF_INET, SOCK_STREAM, 0);
	CU_ASSERT(fd != 0);

	rc = bind(fd, (struct sockaddr*)&local_addr, local_addrlen);
	CU_ASSERT(rc == 0);

	CU_ASSERT(listen(fd, 50) == 0);

	CU_ASSERT(fcntl(fd, F_SETFL, O_NONBLOCK) == 0);

	AWAIT_READ(fd);

	int rrr = accept(fd, (struct sockaddr*)&remote_addr, &remote_addrlen);
	printf("%d\n",rrr);
}

/* @Test */
void test_network_thread(void)
{
	CU_ASSERT(init_threads() != 0);

	thread_t network = thread_add_network("Network");
	CU_ASSERT(network != NULL);

	thread_call_function_sync(network, listen_for_connection, 0);

	cleanup_threads();
}

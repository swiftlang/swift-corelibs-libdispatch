/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

// A read source on a socket with only an error pending should fire, like
// EVFILT_READ does on so_error, so that recv() can report the error. On epoll
// the error arrives as EPOLLERR without EPOLLIN or EPOLLHUP.

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include <dispatch/dispatch.h>

#include <bsdtests.h>
#include "dispatch_test.h"

#define ERR_TIMEOUT (10 * NSEC_PER_SEC)

// Returns a nonblocking UDP socket connected to a closed port on the loopback
// address, after one send(). The ICMP port unreachable reply leaves
// ECONNREFUSED pending on the socket with nothing to read.
static int
refused_udp_socket(void)
{
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
	};
	socklen_t len = sizeof(addr);

	// Bind a socket to get a free port, then close it so the port is closed.
	int probe = socket(AF_INET, SOCK_DGRAM, 0);
	if (probe == -1 ||
			bind(probe, (struct sockaddr *)&addr, sizeof(addr)) == -1 ||
			getsockname(probe, (struct sockaddr *)&addr, &len) == -1) {
		test_errno("probe socket", errno, 0);
		test_stop();
	}
	close(probe);

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1 ||
			connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		test_errno("connect", errno, 0);
		test_stop();
	}
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
	test_long("send", (long)send(fd, "x", 1, 0), 1);
	return fd;
}

int
main(void)
{
	dispatch_test_start("Dispatch read source on a socket with an error");

	int fd = refused_udp_socket();
	dispatch_semaphore_t done = dispatch_semaphore_create(0);
	dispatch_queue_t q = dispatch_queue_create("read_err", NULL);
	dispatch_source_t ds = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
			(uintptr_t)fd, 0, q);
	__block int recv_errno = 0;
	dispatch_source_set_event_handler(ds, ^{
		char buf[16];
		if (recv(fd, buf, sizeof(buf), 0) == -1 && errno != EAGAIN) {
			recv_errno = errno;
			dispatch_source_cancel(ds);
		}
	});
	dispatch_source_set_cancel_handler(ds, ^{
		close(fd);
		dispatch_release(ds);
		dispatch_semaphore_signal(done);
	});
	dispatch_resume(ds);

	long timedout = dispatch_semaphore_wait(done,
			dispatch_time(DISPATCH_TIME_NOW, ERR_TIMEOUT));
	test_long("read source fired and was cancelled", timedout, 0);
	if (!timedout) {
		test_long("recv errno", recv_errno, ECONNREFUSED);
	}
	dispatch_release(q);

	test_stop();
}

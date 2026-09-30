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

// Read sources on hung-up fds should behave like EV_EOF on kevent: readers
// see the remaining byte count (0 at EOF) and keep firing until cancelled.
// On epoll these are delivered as EPOLLHUP (SR-9033).

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dispatch/dispatch.h>

#include <bsdtests.h>
#include "dispatch_test.h"

#define HUP_TIMEOUT (10 * NSEC_PER_SEC)

static void
set_nonblock(int fd)
{
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

// Returns a nonblocking read end whose writer has already closed, with `len`
// bytes of `buf` left unread in the pipe.
static int
hungup_pipe(const char *buf, size_t len)
{
	int p[2];
	if (pipe(p) == -1) {
		test_errno("pipe", errno, 0);
		test_stop();
	}
	set_nonblock(p[0]);
	if (len) {
		test_long("pipe write", (long)write(p[1], buf, len), (long)len);
	}
	close(p[1]);
	return p[0];
}

static bool
wait_done(dispatch_semaphore_t sema, const char *desc)
{
	long timedout = dispatch_semaphore_wait(sema,
			dispatch_time(DISPATCH_TIME_NOW, HUP_TIMEOUT));
	test_long(desc, timedout, 0);
	return !timedout;
}

// A handler that cancels on data == 0 && read() == 0 gets called and cancels.
static void
test_eof_cancel(void)
{
	int fd = hungup_pipe(NULL, 0);
	dispatch_semaphore_t done = dispatch_semaphore_create(0);
	dispatch_queue_t q = dispatch_queue_create("eof_cancel", NULL);
	dispatch_source_t ds = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
			(uintptr_t)fd, 0, q);
	dispatch_source_set_event_handler(ds, ^{
		char buf[64];
		if (dispatch_source_get_data(ds) == 0 &&
				read(fd, buf, sizeof(buf)) == 0) {
			dispatch_source_cancel(ds);
		}
	});
	dispatch_source_set_cancel_handler(ds, ^{
		close(fd);
		dispatch_release(ds);
		dispatch_semaphore_signal(done);
	});
	dispatch_resume(ds);
	wait_done(done, "eof_cancel: cancelled");
	dispatch_release(q);
}

// Bytes still in the pipe at hangup are reported first, then 0 at EOF.
static void
test_buffered(void)
{
	int fd = hungup_pipe("hello", 5);
	dispatch_semaphore_t done = dispatch_semaphore_create(0);
	dispatch_queue_t q = dispatch_queue_create("buffered", NULL);
	dispatch_source_t ds = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
			(uintptr_t)fd, 0, q);
	__block int calls = 0;
	__block unsigned long first_data = ~0ul;
	__block long total = 0;
	dispatch_source_set_event_handler(ds, ^{
		unsigned long data = dispatch_source_get_data(ds);
		if (calls++ == 0) first_data = data;
		char buf[64];
		ssize_t n = read(fd, buf, data && data < sizeof(buf) ? data : sizeof(buf));
		if (n > 0) {
			total += n;
		} else if (data == 0 && n == 0) {
			dispatch_source_cancel(ds);
		}
	});
	dispatch_source_set_cancel_handler(ds, ^{
		close(fd);
		dispatch_release(ds);
		dispatch_semaphore_signal(done);
	});
	dispatch_resume(ds);
	if (wait_done(done, "buffered: cancelled")) {
		test_long("buffered: first data", (long)first_data, 5);
		test_long("buffered: bytes read", total, 5);
	}
	dispatch_release(q);
}

// An uncancelled source at EOF keeps firing, once per rearm.
static void
test_refire(void)
{
	int fd = hungup_pipe(NULL, 0);
	dispatch_semaphore_t done = dispatch_semaphore_create(0);
	dispatch_queue_t q = dispatch_queue_create("refire", NULL);
	dispatch_source_t ds = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
			(uintptr_t)fd, 0, q);
	__block int calls = 0;
	dispatch_source_set_event_handler(ds, ^{
		if (++calls == 5) dispatch_source_cancel(ds);
	});
	dispatch_source_set_cancel_handler(ds, ^{
		close(fd);
		dispatch_release(ds);
		dispatch_semaphore_signal(done);
	});
	dispatch_resume(ds);
	wait_done(done, "refire: fired 5 times");
	dispatch_release(q);
}

int
main(void)
{
	dispatch_test_start("Dispatch read sources on hung-up fds");

	test_eof_cancel();
	test_buffered();
	test_refire();

	test_stop();
}

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
// On epoll these are delivered as EPOLLHUP (SR-9033). On Windows the pipe
// monitor thread exits at EOF, so for pipes the 0 byte event is only delivered
// once.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <WinSock2.h>
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <dispatch/dispatch.h>

#include <bsdtests.h>
#include "dispatch_test.h"

#define HUP_TIMEOUT (10 * NSEC_PER_SEC)

struct hup_kind {
	const char *name;
	// Returns a read end whose writer has already closed, with `len` bytes of
	// `buf` left unread.
	dispatch_fd_t (*make)(const char *buf, size_t len);
	long (*read)(dispatch_fd_t fd, char *buf, size_t len);
	void (*close)(dispatch_fd_t fd);
	bool refires;
};

#if defined(_WIN32)
static void
pipe_write_and_close(HANDLE hWrite, const char *buf, size_t len)
{
	if (len) {
		DWORD dwWritten = 0;
		WriteFile(hWrite, buf, (DWORD)len, &dwWritten, NULL);
		test_long("pipe write", (long)dwWritten, (long)len);
	}
	CloseHandle(hWrite);
}

static dispatch_fd_t
hungup_pipe(const char *buf, size_t len)
{
	HANDLE hRead, hWrite;
	if (!CreatePipe(&hRead, &hWrite, NULL, 0)) {
		test_long("CreatePipe", (long)GetLastError(), ERROR_SUCCESS);
		test_stop();
	}
	pipe_write_and_close(hWrite, buf, len);
	return (dispatch_fd_t)hRead;
}

static dispatch_fd_t
hungup_message_pipe(const char *buf, size_t len)
{
	wchar_t name[64];
	static int counter = 0;
	swprintf(name, sizeof(name) / sizeof(name[0]),
			L"\\\\.\\pipe\\dispatch_read_hup_%lu_%d", GetCurrentProcessId(),
			counter++);
	HANDLE hRead = CreateNamedPipeW(name,
			PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, /* nMaxInstances */ 1,
			/* nOutBufferSize */ 0x1000, /* nInBufferSize */ 0x1000,
			/* nDefaultTimeOut */ 0, /* lpSecurityAttributes */ NULL);
	if (hRead == INVALID_HANDLE_VALUE) {
		test_ptr_not("CreateNamedPipe", hRead, INVALID_HANDLE_VALUE);
		test_stop();
	}
	HANDLE hWrite = CreateFileW(name, GENERIC_WRITE, /* dwShareMode */ 0,
			/* lpSecurityAttributes */ NULL, OPEN_EXISTING,
			/* dwFlagsAndAttributes */ 0, /* hTemplateFile */ NULL);
	if (hWrite == INVALID_HANDLE_VALUE) {
		test_ptr_not("CreateFile", hWrite, INVALID_HANDLE_VALUE);
		test_stop();
	}
	pipe_write_and_close(hWrite, buf, len);
	return (dispatch_fd_t)hRead;
}

static long
pipe_read(dispatch_fd_t fd, char *buf, size_t len)
{
	DWORD dwRead = 0;
	if (!ReadFile((HANDLE)fd, buf, (DWORD)len, &dwRead, NULL)) {
		return GetLastError() == ERROR_BROKEN_PIPE ? 0 : -1;
	}
	return (long)dwRead;
}

static void
pipe_close(dispatch_fd_t fd)
{
	CloseHandle((HANDLE)fd);
}

static dispatch_fd_t
hungup_socket(const char *buf, size_t len)
{
	SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == INVALID_SOCKET) {
		test_long("socket", WSAGetLastError(), 0);
		test_stop();
	}
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	int addrlen = sizeof(addr);
	if (bind(listener, (struct sockaddr *)&addr, addrlen) ||
			listen(listener, 1) ||
			getsockname(listener, (struct sockaddr *)&addr, &addrlen)) {
		test_long("bind/listen", WSAGetLastError(), 0);
		test_stop();
	}
	SOCKET writer = socket(AF_INET, SOCK_STREAM, 0);
	if (writer == INVALID_SOCKET ||
			connect(writer, (struct sockaddr *)&addr, addrlen)) {
		test_long("connect", WSAGetLastError(), 0);
		test_stop();
	}
	SOCKET reader = accept(listener, NULL, NULL);
	if (reader == INVALID_SOCKET) {
		test_long("accept", WSAGetLastError(), 0);
		test_stop();
	}
	closesocket(listener);
	if (len) {
		test_long("socket send", send(writer, buf, (int)len, 0), (long)len);
	}
	shutdown(writer, SD_SEND);
	closesocket(writer);
	return (dispatch_fd_t)reader;
}

static long
socket_read(dispatch_fd_t fd, char *buf, size_t len)
{
	return recv((SOCKET)fd, buf, (int)len, 0);
}

static void
socket_close(dispatch_fd_t fd)
{
	closesocket((SOCKET)fd);
}

static const struct hup_kind kinds[] = {
	{"pipe", hungup_pipe, pipe_read, pipe_close, false},
	{"message pipe", hungup_message_pipe, pipe_read, pipe_close, false},
	{"socket", hungup_socket, socket_read, socket_close, true},
};
#else
static void
set_nonblock(int fd)
{
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

// Returns a nonblocking read end whose writer has already closed, with `len`
// bytes of `buf` left unread in the pipe.
static dispatch_fd_t
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

static long
pipe_read(dispatch_fd_t fd, char *buf, size_t len)
{
	return (long)read(fd, buf, len);
}

static void
pipe_close(dispatch_fd_t fd)
{
	close(fd);
}

static const struct hup_kind kinds[] = {
	{"pipe", hungup_pipe, pipe_read, pipe_close, true},
};
#endif

static bool
wait_done(dispatch_semaphore_t sema, const struct hup_kind *kind,
		const char *desc)
{
	long timedout = dispatch_semaphore_wait(sema,
			dispatch_time(DISPATCH_TIME_NOW, HUP_TIMEOUT));
	char msg[128];
	snprintf(msg, sizeof(msg), "%s: %s", kind->name, desc);
	test_long(msg, timedout, 0);
	return !timedout;
}

// A handler that cancels on data == 0 && read() == 0 gets called and cancels.
static void
test_eof_cancel(const struct hup_kind *kind)
{
	dispatch_fd_t fd = kind->make(NULL, 0);
	dispatch_semaphore_t done = dispatch_semaphore_create(0);
	dispatch_queue_t q = dispatch_queue_create("eof_cancel", NULL);
	dispatch_source_t ds = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
			(uintptr_t)fd, 0, q);
	dispatch_source_set_event_handler(ds, ^{
		char buf[64];
		if (dispatch_source_get_data(ds) == 0 &&
				kind->read(fd, buf, sizeof(buf)) == 0) {
			dispatch_source_cancel(ds);
		}
	});
	dispatch_source_set_cancel_handler(ds, ^{
		kind->close(fd);
		dispatch_release(ds);
		dispatch_semaphore_signal(done);
	});
	dispatch_resume(ds);
	wait_done(done, kind, "eof_cancel: cancelled");
	dispatch_release(q);
}

// Bytes still buffered at hangup are reported first, then 0 at EOF.
static void
test_buffered(const struct hup_kind *kind)
{
	dispatch_fd_t fd = kind->make("hello", 5);
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
		long n = kind->read(fd, buf,
				data && data < sizeof(buf) ? data : sizeof(buf));
		if (n > 0) {
			total += n;
		} else if (data == 0 && n == 0) {
			dispatch_source_cancel(ds);
		}
	});
	dispatch_source_set_cancel_handler(ds, ^{
		kind->close(fd);
		dispatch_release(ds);
		dispatch_semaphore_signal(done);
	});
	dispatch_resume(ds);
	if (wait_done(done, kind, "buffered: cancelled")) {
		char msg[128];
		snprintf(msg, sizeof(msg), "%s: buffered: first data", kind->name);
		test_long(msg, (long)first_data, 5);
		snprintf(msg, sizeof(msg), "%s: buffered: bytes read", kind->name);
		test_long(msg, total, 5);
	}
	dispatch_release(q);
}

// An uncancelled source at EOF keeps firing, once per rearm.
static void
test_refire(const struct hup_kind *kind)
{
	dispatch_fd_t fd = kind->make(NULL, 0);
	dispatch_semaphore_t done = dispatch_semaphore_create(0);
	dispatch_queue_t q = dispatch_queue_create("refire", NULL);
	dispatch_source_t ds = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
			(uintptr_t)fd, 0, q);
	__block int calls = 0;
	dispatch_source_set_event_handler(ds, ^{
		if (++calls == 5) dispatch_source_cancel(ds);
	});
	dispatch_source_set_cancel_handler(ds, ^{
		kind->close(fd);
		dispatch_release(ds);
		dispatch_semaphore_signal(done);
	});
	dispatch_resume(ds);
	wait_done(done, kind, "refire: fired 5 times");
	dispatch_release(q);
}

int
main(void)
{
	dispatch_test_start("Dispatch read sources on hung-up fds");
#if defined(_WIN32)
	WSADATA wsa;
	int err = WSAStartup(MAKEWORD(2, 2), &wsa);
	if (err != 0) {
		fprintf(stderr, "WSAStartup failed with %d\n", err);
		test_stop();
	}
#endif

	for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
		test_eof_cancel(&kinds[i]);
		test_buffered(&kinds[i]);
		if (kinds[i].refires) {
			test_refire(&kinds[i]);
		}
	}

	test_stop();
}

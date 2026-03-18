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

#include <dispatch/dispatch.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#include <pthread.h>
#include <unistd.h>
#endif
#if defined(__OpenBSD__) || defined(__FreeBSD__)
#include <pthread_np.h>
#endif

#include <bsdtests.h>

#if HAVE_PTHREAD_SET_NAME_NP && !HAVE_PTHREAD_SETNAME_NP
#define pthread_setname_np pthread_set_name_np
#endif
#if HAVE_PTHREAD_GET_NAME_NP && !HAVE_PTHREAD_GETNAME_NP
#define pthread_getname_np pthread_get_name_np
#endif

#include <bsdtests.h>
#include "dispatch_test.h"

#define ITERATIONS 10

static void
set_current_thread_name(const char *name)
{
#if defined(__APPLE__)
	pthread_setname_np(name);
#else
	pthread_setname_np(pthread_self(), name);
#endif
}

int
main(void)
{
	dispatch_test_start("Dispatch Thread Name");

	const char *parent_name = "TestParentName";
	set_current_thread_name(parent_name);

	dispatch_group_t group = dispatch_group_create();
	assert(group);

	dispatch_queue_t q = dispatch_get_global_queue(0, 0);

	for (int i = 0; i < ITERATIONS; i++) {
		dispatch_group_async(group, q, ^{
			char thread_name[64] = {0};
			pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));
			fprintf(stderr, "Worker thread name: '%s'\n", thread_name);
			test_long("worker thread name must not be empty",
					strlen(thread_name) > 0, 1);
			test_long("worker thread must not inherit parent name",
					strcmp(thread_name, parent_name) != 0, 1);
		});
	}

	test_group_wait(group);
	dispatch_release(group);

	test_stop();
	return 0;
}

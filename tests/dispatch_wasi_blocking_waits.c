/*
 * This source file is part of the Swift.org open source project
 *
 * Copyright (c) 2026 Apple Inc. and the Swift project authors
 *
 * Licensed under Apache License v2.0 with Runtime Library Exception
 *
 * See https://swift.org/LICENSE.txt for license information
 * See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
 *
 */
/*
 * A blocking wait on the sole WASI thread runs no queued work. So a wait
 * returns at once when nothing has to run for it, a timed wait that nothing
 * can satisfy sleeps to its deadline and times out, and a dispatch_sync on
 * an idle queue runs its block on the caller's frame.
 */
#include <dispatch/dispatch.h>
#include <stdint.h>

#include <bsdtests.h>
#include "dispatch_test.h"

int
main(void)
{
	dispatch_test_start("Dispatch WASI Blocking Waits");

	dispatch_group_t group = dispatch_group_create();
	long result = dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
	test_long("dispatch_group_wait(FOREVER) on an empty group returns 0",
			result, 0);

	dispatch_semaphore_t signaled = dispatch_semaphore_create(1);
	result = dispatch_semaphore_wait(signaled, DISPATCH_TIME_FOREVER);
	test_long("dispatch_semaphore_wait(FOREVER) with a count returns 0",
			result, 0);
	dispatch_semaphore_signal(signaled);

	dispatch_semaphore_t unsignaled = dispatch_semaphore_create(0);
	uint64_t start = dispatch_time(DISPATCH_TIME_NOW, 0);
	result = dispatch_semaphore_wait(unsignaled,
			dispatch_time(DISPATCH_TIME_NOW, 250 * NSEC_PER_MSEC));
	long elapsed_ms = (long)((dispatch_time(DISPATCH_TIME_NOW, 0) - start) /
			NSEC_PER_MSEC);
	test_long("timed semaphore wait with no signal times out", !result, 0);
	test_long_greater_than_or_equal("timed semaphore wait lasts at least "
			"its 250 ms timeout", elapsed_ms, 250);
	test_long_less_than("timed semaphore wait returns soon after its "
			"timeout", elapsed_ms, 2000);

	dispatch_queue_t queue = dispatch_queue_create("wasi.blocking.sync",
			DISPATCH_QUEUE_SERIAL);
	__block long sync_ran = 0;
	dispatch_sync(queue, ^{ sync_ran = 1; });
	test_long("dispatch_sync on an idle queue runs its block inline",
			sync_ran, 1);

	dispatch_release(group);
	dispatch_release(signaled);
	dispatch_release(unsignaled);
	dispatch_release(queue);
	test_stop();
	return 0;
}

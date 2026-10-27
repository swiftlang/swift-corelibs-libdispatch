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
 * A timed semaphore wait on the sole WASI thread runs no queued work. With
 * nobody left to signal it, it sleeps to its own 200 ms deadline and reports
 * a timeout. A 10 s timer on the same serial queue must not extend that
 * wait, so the wait must end well before the timer.
 */
#include <dispatch/dispatch.h>
#include <stdint.h>

#include <bsdtests.h>
#include "dispatch_test.h"

int
main(void)
{
	dispatch_test_start("Dispatch WASI Timed Wait");

	dispatch_queue_t queue = dispatch_queue_create("wasi.timed.wait",
			DISPATCH_QUEUE_SERIAL);
	dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC),
			queue, ^{ });
	dispatch_async(queue, ^{
		uint64_t start = dispatch_time(DISPATCH_TIME_NOW, 0);
		long result = dispatch_semaphore_wait(semaphore,
				dispatch_time(DISPATCH_TIME_NOW, 200 * NSEC_PER_MSEC));
		long elapsed_ms = (long)((dispatch_time(DISPATCH_TIME_NOW, 0) -
				start) / NSEC_PER_MSEC);
		test_long("timed semaphore wait in a work item times out",
				!result, 0);
		test_long_greater_than_or_equal("timed semaphore wait sleeps to "
				"its 200 ms deadline", elapsed_ms, 200);
		test_long_less_than("timed semaphore wait does not sleep to the "
				"10 s timer", elapsed_ms, 5000);
		test_stop();
	});
	dispatch_main();
}

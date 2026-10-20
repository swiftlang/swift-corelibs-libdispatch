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
 * An indefinite semaphore wait inside a work item crashes at once with
 * "Deadlock: semaphore wait on single-threaded WASI". A wait runs no queued
 * work, so nothing can ever signal the semaphore. A 10 s timer on the same
 * queue must not turn the crash into a sleep. The work item prints a marker
 * before it waits, so the crash provably comes from the wait.
 */
#include <dispatch/dispatch.h>
#include <stdio.h>

int
main(void)
{
	dispatch_queue_t queue = dispatch_queue_create("wasi.nested.wait",
			DISPATCH_QUEUE_SERIAL);
	dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC),
			queue, ^{ });
	dispatch_async(queue, ^{
		puts("nested wait started");
		fflush(stdout);
		dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
	});
	// dispatch_main() runs the queued item; its endless wait must crash
	dispatch_main();
}

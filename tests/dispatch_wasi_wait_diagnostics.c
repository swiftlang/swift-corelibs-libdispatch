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
 * An indefinite-wait deadlock diagnostic names the public call that
 * blocked: a dispatch_sync that waits behind queued work, inside a work
 * item. dispatch_wasi_nested_wait.c covers the dispatch_semaphore_wait case.
 *
 * Built with WAIT_DIAGNOSTICS_TOP_LEVEL, it covers a wait outside any drain
 * step: a semaphore wait in main() that queued work would satisfy. A wait
 * runs no queued work on one thread, so it crashes instead.
 */
#include <dispatch/dispatch.h>
#include <stdio.h>

int
main(void)
{
#if WAIT_DIAGNOSTICS_TOP_LEVEL
	dispatch_semaphore_t sema = dispatch_semaphore_create(0);
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT,
			0), ^{
		dispatch_semaphore_signal(sema);
	});
	puts("top-level wait starting");
	fflush(stdout);
	dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
	puts("top-level wait returned");
	return 1;
#else
	dispatch_queue_t global = dispatch_get_global_queue(
			DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
	dispatch_queue_t queue = dispatch_queue_create("wasi.wait.diag",
			DISPATCH_QUEUE_SERIAL);
	dispatch_async(global, ^{
		puts("nested sync starting");
		fflush(stdout);
		dispatch_async(queue, ^{});
		dispatch_sync(queue, ^{});
	});
	dispatch_main();
#endif
}

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
 * dispatch_async never runs its block before the call returns: a poke only
 * records pending work, and dispatch_main() runs it. Threaded platforms do
 * not promise this; a worker thread can run the block before dispatch_async
 * returns. Work queued from a running item runs in a later drain step, in
 * FIFO order within one queue.
 */
#include <dispatch/dispatch.h>

#include <bsdtests.h>
#include "dispatch_test.h"

static long order[4];
static long order_count;

static void
push(long value)
{
	order[order_count++] = value;
}

int
main(void)
{
	dispatch_test_start("Dispatch WASI Deferred Submission");

	dispatch_queue_t global = dispatch_get_global_queue(
			DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
	__block long root_ran = 0, main_ran = 0;
	dispatch_async(global, ^{ root_ran = 1; });
	dispatch_async(dispatch_get_main_queue(), ^{ main_ran = 1; });
	test_long("dispatch_async to a root queue does not run the block "
			"before it returns", root_ran, 0);
	test_long("dispatch_async to the main queue does not run the block "
			"before it returns", main_ran, 0);

	dispatch_async(global, ^{
		push(1);
		dispatch_async(global, ^{ push(3); });
		dispatch_async(global, ^{
			push(4);
			test_long("the root item queued at top level ran", root_ran, 1);
			test_long("the main-queue item queued at top level ran",
					main_ran, 1);
			test_long("items queued from a running item run after it",
					order[0], 1);
			test_long("a root item queued before them runs first",
					order[1], 2);
			test_long("items queued from a running item keep FIFO order",
					order[2], 3);
			test_stop();
		});
	});
	dispatch_async(global, ^{ push(2); });
	dispatch_main();
}

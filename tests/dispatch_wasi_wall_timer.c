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
 * A dispatch_after with a dispatch_walltime() deadline fires under
 * dispatch_main() no earlier than its 20 ms deadline. Wall-clock deadlines
 * stay in their own clock domain and are checked at every wake.
 */
#include <dispatch/dispatch.h>
#include <stdint.h>

#include <bsdtests.h>
#include "dispatch_test.h"

int
main(void)
{
	dispatch_test_start("Dispatch WASI Wall Timer");

	dispatch_queue_t queue = dispatch_queue_create("wasi.wall.timer",
			DISPATCH_QUEUE_SERIAL);
	uint64_t start = dispatch_time(DISPATCH_TIME_NOW, 0);
	dispatch_after(dispatch_walltime(NULL, 20 * NSEC_PER_MSEC), queue, ^{
		long elapsed_ms = (long)((dispatch_time(DISPATCH_TIME_NOW, 0) -
				start) / NSEC_PER_MSEC);
		test_long_greater_than_or_equal("walltime timer fires no earlier "
				"than 20 ms", elapsed_ms, 20);
		test_long_less_than("walltime timer fires well before 2 s",
				elapsed_ms, 2000);
		test_stop();
	});
	dispatch_main();
}

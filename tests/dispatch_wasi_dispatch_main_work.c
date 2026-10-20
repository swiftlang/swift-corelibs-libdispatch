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
 * dispatch_main() runs main-queue work scheduled with dispatch_after: the
 * timer fires on the sole WASI thread and its block runs on the main queue,
 * where it stops the test. Without that work dispatch_main() would crash
 * idle (see dispatch_wasi_dispatch_main_idle.c).
 */
#include <dispatch/dispatch.h>

#include <bsdtests.h>
#include "dispatch_test.h"

int
main(void)
{
	dispatch_test_start("Dispatch WASI Main Queue Work");

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_MSEC),
			dispatch_get_main_queue(), ^{
		dispatch_test_current("dispatch_after work runs on the main "
				"queue", dispatch_get_main_queue());
		test_stop();
	});
	dispatch_main();
}

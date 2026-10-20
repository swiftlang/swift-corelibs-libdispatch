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
 * dispatch_main() with no work, no timers, and no sources can never make
 * progress on the sole WASI thread. It must crash with a named diagnostic
 * instead of parking forever.
 */
#include <dispatch/dispatch.h>

int
main(void)
{
	dispatch_main();
}

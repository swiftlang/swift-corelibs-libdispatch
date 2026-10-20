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
 * A lane that keeps re-posting to itself must not hold the sole thread. A
 * step runs one item of the lane, then re-enqueues the lane. So while the
 * lane stays busy, a 10 ms dispatch_after, a LOW global item, and a
 * main-queue item all run. The lanes:
 *  1. a plain serial queue
 *  2. a serial queue that targets another serial queue
 *  3. a workloop
 *  4. a serial queue that targets the main queue
 *  5. a data source that merges data into itself from its handler, on an
 *     overcommit root queue
 * Each queue lane must keep FIFO order. Driven by dispatch_main().
 */
#include <dispatch/dispatch.h>
#include <dispatch/private.h>
#include <stdint.h>

#include <bsdtests.h>
#include "dispatch_test.h"

#define LAST_PHASE 5
// The test fails when a phase runs longer than this. A fair drain finishes
// each phase soon after its 10 ms timer fires.
#define LANE_TIME_LIMIT_NS (2 * NSEC_PER_SEC)
// The lane reads the clock once every 4096 items, to keep each item cheap.
#define CLOCK_CHECK_MASK 0xfff

static const char *const phase_names[LAST_PHASE + 1] = {
	NULL,
	"plain serial lane",
	"lane targeting a serial queue",
	"workloop lane",
	"lane targeting the main queue",
	"self-triggering data source",
};

static dispatch_queue_t lane;
static dispatch_source_t source;
static int phase;
static long timer_fired, low_ran, main_ran;
static unsigned long lane_count, last_seq;
// lane items that did not run right after the item submitted before them
static long out_of_order;
static uint64_t phase_start_ns;

static void
mark(void *context)
{
	*(long *)context = 1;
}

static void start_phase(int next_phase);

static void
check_phase(void)
{
	const char *name = phase_names[phase];
	if (phase != LAST_PHASE) {
		test_long_format(out_of_order, 0,
				"%s runs its items in FIFO order", name);
	}
	test_long_format(timer_fired, 1,
			"10 ms dispatch_after runs while the %s is busy", name);
	test_long_format(low_ran, 1,
			"LOW global item runs while the %s is busy", name);
	test_long_format(main_ran, 1,
			"main-queue item runs while the %s is busy", name);
}

// Returns true when the phase ended.
static bool
lane_step(void)
{
	lane_count++;
	if (timer_fired && low_ran && main_ran) {
		check_phase();
		if (phase == LAST_PHASE) {
			test_stop();
		}
		return true;
	}
	if ((lane_count & CLOCK_CHECK_MASK) == 0 &&
			dispatch_time(DISPATCH_TIME_NOW, 0) - phase_start_ns >
			LANE_TIME_LIMIT_NS) {
		// the lane held the thread: the check for each item that did
		// not run fails
		check_phase();
		test_stop();
	}
	return false;
}

static void
lane_item(void *context)
{
	unsigned long seq = (unsigned long)(uintptr_t)context;
	if (seq != last_seq + 1) {
		out_of_order++;
	}
	last_seq = seq;
	if (lane_step()) {
		// the system keeps the lane alive until this item returns
		dispatch_release(lane);
		start_phase(phase + 1);
		return;
	}
	dispatch_async_f(lane, (void *)(uintptr_t)(seq + 1), lane_item);
}

static void
source_handler(void *context)
{
	(void)context;
	if (lane_step()) {
		dispatch_source_cancel(source);
		dispatch_release(source);
		return;
	}
	dispatch_source_merge_data(source, 1);
}

static void
start_phase(int next_phase)
{
	phase = next_phase;
	timer_fired = low_ran = main_ran = 0;
	lane_count = last_seq = 0;
	out_of_order = 0;
	phase_start_ns = dispatch_time(DISPATCH_TIME_NOW, 0);
	dispatch_queue_t global = dispatch_get_global_queue(
			DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
	dispatch_queue_t low = dispatch_get_global_queue(
			DISPATCH_QUEUE_PRIORITY_LOW, 0);
	dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW,
			10 * NSEC_PER_MSEC), global, &timer_fired, mark);
	dispatch_async_f(low, &low_ran, mark);
	dispatch_async_f(dispatch_get_main_queue(), &main_ran, mark);

	switch (phase) {
	case 1:
		lane = dispatch_queue_create("wasi.serial_lane",
				DISPATCH_QUEUE_SERIAL);
		break;
	case 2: {
		dispatch_queue_t outer = dispatch_queue_create(
				"wasi.serial_lane.outer",
				DISPATCH_QUEUE_SERIAL);
		lane = dispatch_queue_create_with_target(
				"wasi.serial_lane.inner", DISPATCH_QUEUE_SERIAL,
				outer);
		dispatch_release(outer);
		break;
	}
	case 3:
		lane = (dispatch_queue_t)dispatch_workloop_create(
				"wasi.serial_lane.workloop");
		break;
	case 4:
		lane = dispatch_queue_create_with_target(
				"wasi.serial_lane.main", DISPATCH_QUEUE_SERIAL,
				dispatch_get_main_queue());
		break;
	default:
		source = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_ADD, 0, 0,
				dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT,
				DISPATCH_QUEUE_OVERCOMMIT));
		dispatch_source_set_event_handler_f(source, source_handler);
		dispatch_resume(source);
		dispatch_source_merge_data(source, 1);
		return;
	}
	dispatch_async_f(lane, (void *)(uintptr_t)1, lane_item);
}

int
main(void)
{
	dispatch_test_start("Dispatch WASI Serial Lane Fairness");

	start_phase(1);
	dispatch_main();
}

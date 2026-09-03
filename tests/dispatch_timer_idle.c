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

#include <dispatch/dispatch.h>

#include <bsdtests.h>
#include "dispatch_test.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <time.h>

#define OBSERVATION_SECONDS 0.25
#define MAX_IDLE_CPU_RATIO 0.20

static _Atomic int timer_fired;
static volatile uint64_t spin_sink;

static void
monotonic_now(struct timespec *value)
{
	if (clock_gettime(CLOCK_MONOTONIC, value) != 0) {
		perror("clock_gettime");
		exit(EXIT_FAILURE);
	}
}

static double
seconds_between(struct timespec start, struct timespec end)
{
	return (double)(end.tv_sec - start.tv_sec) +
		(double)(end.tv_nsec - start.tv_nsec) / (double)NSEC_PER_SEC;
}

static double
timeval_seconds(struct timeval value)
{
	return (double)value.tv_sec +
		(double)value.tv_usec / (double)USEC_PER_SEC;
}

static double
process_cpu_seconds(void)
{
	struct rusage usage;

	if (getrusage(RUSAGE_SELF, &usage) != 0) {
		perror("getrusage");
		exit(EXIT_FAILURE);
	}
	return timeval_seconds(usage.ru_utime) + timeval_seconds(usage.ru_stime);
}

static double
measure_busy_cpu_ratio(void)
{
	struct timespec wall_start, wall_now;
	double cpu_start, cpu_end;

	monotonic_now(&wall_start);
	cpu_start = process_cpu_seconds();
	do {
		spin_sink++;
		monotonic_now(&wall_now);
	} while (seconds_between(wall_start, wall_now) < OBSERVATION_SECONDS);
	cpu_end = process_cpu_seconds();

	return (cpu_end - cpu_start) / seconds_between(wall_start, wall_now);
}

static void
unexpected_timer_fire(void *context)
{
	(void)context;
	atomic_store_explicit(&timer_fired, 1, memory_order_relaxed);
}

static void
observe_idle_timer(double *wall_seconds, double *cpu_seconds)
{
	struct timespec wall_start, wall_end;
	struct timespec remaining = {
		.tv_sec = 0,
		.tv_nsec = (long)(OBSERVATION_SECONDS * (double)NSEC_PER_SEC),
	};
	dispatch_source_t timer;
	double cpu_start;
	int sleep_result;

	timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
			dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
	test_ptr_notnull("dispatch_source_create", timer);
	dispatch_source_set_event_handler_f(timer, unexpected_timer_fire);
	dispatch_source_set_timer(timer,
			dispatch_time(DISPATCH_TIME_NOW, 3600 * NSEC_PER_SEC),
			DISPATCH_TIME_FOREVER, 0);
	dispatch_resume(timer);

	monotonic_now(&wall_start);
	cpu_start = process_cpu_seconds();
	do {
		sleep_result = nanosleep(&remaining, &remaining);
	} while (sleep_result == -1 && errno == EINTR);
	if (sleep_result != 0) {
		perror("nanosleep");
		exit(EXIT_FAILURE);
	}
	*cpu_seconds = process_cpu_seconds() - cpu_start;
	monotonic_now(&wall_end);
	*wall_seconds = seconds_between(wall_start, wall_end);

	dispatch_source_cancel(timer);
	dispatch_release(timer);
}

int
main(void)
{
	double busy_cpu_ratio, idle_wall_seconds, idle_cpu_seconds;
	double idle_cpu_ratio, relative_cpu_ratio;

	dispatch_test_start("Dispatch idle timer CPU usage");

	busy_cpu_ratio = measure_busy_cpu_ratio();
	observe_idle_timer(&idle_wall_seconds, &idle_cpu_seconds);
	idle_cpu_ratio = idle_cpu_seconds / idle_wall_seconds;
	relative_cpu_ratio = idle_cpu_ratio / busy_cpu_ratio;

	fprintf(stderr, "idle=%f busy=%f relative=%f\n",
			idle_cpu_ratio, busy_cpu_ratio, relative_cpu_ratio);
	test_long("timer fired", atomic_load_explicit(&timer_fired,
			memory_order_relaxed), 0);
	test_double_less_than("idle timer CPU ratio", relative_cpu_ratio,
			MAX_IDLE_CPU_RATIO);
	test_stop();
	return 0;
}

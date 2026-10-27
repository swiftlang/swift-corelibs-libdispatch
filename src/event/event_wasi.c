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

#include "internal.h"
#if DISPATCH_EVENT_BACKEND_WASI

#include <time.h>

#if !DISPATCH_USE_MGR_THREAD
#error unsupported configuration
#endif

// wasm32-wasip1-threads builds with the atomics feature, and -pthread also
// defines _REENTRANT. Plain wasip1 defines neither.
#if defined(__wasm_atomics__) || defined(_REENTRANT)
#error the WASI event backend assumes a single-threaded (wasip1) target
#endif

// Single-threaded WASI has no worker or manager thread. A poke only records
// pending work. The sole thread drains it in dispatch_main(). A blocking
// wait runs no work (shims/lock.c).
// One drain step runs the first phase with work: due timers, the manager
// queue, the main queue, one root item.
// Main and root steps alternate. Roots are served round-robin, so QoS orders
// only the first pass.
// Timers use the shared heap. Only the nearest deadline per clock is kept.

struct _dispatch_wasi_timeout_s {
	uint64_t dwt_deadline; // in the timer's own clock domain
	bool dwt_armed;
};

static struct _dispatch_wasi_timeout_s
		_dispatch_wasi_timeout[DISPATCH_CLOCK_COUNT];

static bool _dispatch_wasi_root_pending[DISPATCH_ROOT_QUEUE_COUNT];
static size_t _dispatch_wasi_next_root = DISPATCH_ROOT_QUEUE_COUNT - 1;
static bool _dispatch_wasi_main_pending;
static bool _dispatch_wasi_mgr_pending;
static bool _dispatch_wasi_main_ran_last;
// A poke inside a drain step records work only. The running drain takes it.
static bool _dispatch_wasi_draining;

DISPATCH_ALWAYS_INLINE
static inline bool
_dispatch_wasi_root_work_pending(void)
{
	for (size_t i = 0; i < countof(_dispatch_wasi_root_pending); i++) {
		if (_dispatch_wasi_root_pending[i]) return true;
	}
	return false;
}

#pragma mark dispatch_unote_t

// Descriptor and signal sources are not implemented. Registering one
// crashes, because a cancelled source would leave the sole thread waiting
// forever. Timer and data sources do not register here.
bool
_dispatch_unote_register_muxed(dispatch_unote_t du)
{
	DISPATCH_CLIENT_CRASH(du._du->du_filter,
			"Descriptor and signal sources unsupported on WASI");
}

void
_dispatch_unote_resume_muxed(dispatch_unote_t du)
{
	// unreachable: _dispatch_unote_register_muxed never succeeds
	DISPATCH_INTERNAL_CRASH(du._du->du_filter,
			"Resume of an unregistered muxed unote");
}

bool
_dispatch_unote_unregister_muxed(dispatch_unote_t du)
{
	// unreachable: _dispatch_unote_register_muxed never succeeds
	DISPATCH_INTERNAL_CRASH(du._du->du_filter,
			"Unregister of an unregistered muxed unote");
}

#pragma mark timers

static void
_dispatch_event_merge_timer(dispatch_clock_t clock)
{
	dispatch_timer_heap_t dth = _dispatch_timers_heap;
	uint32_t tidx = DISPATCH_TIMER_INDEX(clock, 0);

	_dispatch_wasi_timeout[clock].dwt_armed = false;

	_dispatch_timers_heap_dirty(dth, tidx);
	dth[tidx].dth_needs_program = true;
	dth[tidx].dth_armed = false;
}

void
_dispatch_event_loop_timer_arm(dispatch_timer_heap_t dth DISPATCH_UNUSED,
		uint32_t tidx, dispatch_timer_delay_s range,
		dispatch_clock_now_cache_t nows)
{
	dispatch_clock_t clock = DISPATCH_TIMER_CLOCK(tidx);

	_dispatch_wasi_timeout[clock].dwt_deadline = range.delay +
			_dispatch_time_now_cached(clock, nows);
	_dispatch_wasi_timeout[clock].dwt_armed = true;
}

void
_dispatch_event_loop_timer_delete(dispatch_timer_heap_t dth DISPATCH_UNUSED,
		uint32_t tidx)
{
	_dispatch_wasi_timeout[DISPATCH_TIMER_CLOCK(tidx)].dwt_armed = false;
}

// Returns the nearest armed deadline in the _dispatch_uptime() domain, or 0.
// A deadline on another clock is projected with that clock's current value.
// WASI has no clock-change notification, so a wall-clock change takes
// effect at the next wake.
uint64_t
_dispatch_wasi_next_timer_ns(void)
{
	uint64_t uptime = 0;
	uint64_t next = 0;
	for (size_t i = 0; i < countof(_dispatch_wasi_timeout); i++) {
		if (!_dispatch_wasi_timeout[i].dwt_armed) continue;
		uint64_t deadline = _dispatch_wasi_timeout[i].dwt_deadline;
		if (i != DISPATCH_CLOCK_UPTIME) {
			if (!uptime) uptime = _dispatch_uptime();
			uint64_t now = _dispatch_time_now((dispatch_clock_t)i);
			deadline = uptime + (deadline > now ? deadline - now : 0);
		}
		if (!next || deadline < next) next = deadline;
	}
	return next;
}

DISPATCH_ALWAYS_INLINE
static inline bool
_dispatch_wasi_merge_due_timers(void)
{
	bool fired = false;
	for (size_t i = 0; i < countof(_dispatch_wasi_timeout); i++) {
		if (_dispatch_wasi_timeout[i].dwt_armed &&
				_dispatch_wasi_timeout[i].dwt_deadline <=
						_dispatch_time_now((dispatch_clock_t)i)) {
			_dispatch_event_merge_timer((dispatch_clock_t)i);
			fired = true;
		}
	}
	return fired;
}

#pragma mark sleeping

// Without poll_oneoff clock support (as in WasmKit 0.1.x) every sleep fails.
// After the first failure, log once and return at once. Each caller loops to
// its own deadline.
static bool _dispatch_wasi_cannot_sleep;

void
_dispatch_wasi_sleep_until(uint64_t uptime_ns)
{
	uint64_t now = _dispatch_uptime();
	if (uptime_ns <= now || _dispatch_wasi_cannot_sleep) return;
	uint64_t delta = uptime_ns - now;
	struct timespec ts = {
		.tv_sec = (time_t)(delta / NSEC_PER_SEC),
		.tv_nsec = (long)(delta % NSEC_PER_SEC),
	};
	// Sleep on the clock that _dispatch_uptime() reads. wasi-libc nanosleep()
	// uses CLOCK_REALTIME. clock_nanosleep() returns the error, not errno.
	int rc = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
	if (unlikely(rc != 0)) {
		_dispatch_wasi_cannot_sleep = true;
		_dispatch_log("clock_nanosleep() failed (%d), waits poll the clock",
				rc);
	}
}

#pragma mark cooperative drain

bool
_dispatch_wasi_drain_step(void)
{
	if (_dispatch_wasi_draining) {
		// Drains do not nest, because the queue state machinery is not
		// reentrant. The outer drain takes the work.
		return false;
	}
	_dispatch_wasi_draining = true;

	// Hide a borrowed wlh, such as the _dispatch_fake_wlh of
	// dispatch_async_and_wait, from the drains. Never release it.
	void *borrowed_wlh = _dispatch_thread_getspecific(dispatch_wlh_key);
	_dispatch_thread_setspecific(dispatch_wlh_key, NULL);

	bool did_work = true;
	if (_dispatch_wasi_merge_due_timers() ||
			_dispatch_timers_heap[0].dth_dirty_bits) {
		// A due timer counts as work. Firing it queues the timer handler.
		_dispatch_event_loop_drain_timers(_dispatch_timers_heap,
				DISPATCH_TIMER_COUNT);
	} else if (_dispatch_wasi_mgr_pending) {
		_dispatch_wasi_mgr_pending = false;
		_dispatch_wasi_mgr_queue_drain();
	} else if (_dispatch_wasi_main_pending &&
			!(_dispatch_wasi_main_ran_last &&
			_dispatch_wasi_root_work_pending())) {
		// Main and the roots take turns, so main work that re-posts itself
		// cannot starve the roots.
		_dispatch_wasi_main_pending = false;
		_dispatch_wasi_main_ran_last = true;
		_dispatch_wasi_main_queue_drain();
	} else {
		did_work = false;
		// Round-robin: while other roots wait, the next scan starts below the
		// root that ran.
		for (size_t offset = 0;
				offset < countof(_dispatch_wasi_root_pending); offset++) {
			size_t i = (_dispatch_wasi_next_root +
					countof(_dispatch_wasi_root_pending) - offset) %
					countof(_dispatch_wasi_root_pending);
			if (!_dispatch_wasi_root_pending[i]) continue;
			bool roots_were_waiting = false;
			for (size_t j = 0;
					j < countof(_dispatch_wasi_root_pending); j++) {
				roots_were_waiting |= j != i &&
						_dispatch_wasi_root_pending[j];
			}
			_dispatch_wasi_root_pending[i] = false;
			_dispatch_wasi_main_ran_last = false;
			_dispatch_wasi_root_queue_drain(&_dispatch_root_queues[i]);
			bool keep_rotating = roots_were_waiting ||
					_dispatch_wasi_root_pending[i];
			_dispatch_wasi_next_root = keep_rotating && i ? i - 1 :
					countof(_dispatch_wasi_root_pending) - 1;
			did_work = true;
			break;
		}
	}

	_dispatch_thread_setspecific(dispatch_wlh_key, borrowed_wlh);
	_dispatch_wasi_draining = false;
	return did_work;
}

void
_dispatch_wasi_drain(void)
{
	while (_dispatch_wasi_drain_step()) {
	}
}

bool
_dispatch_wasi_in_drain(void)
{
	return _dispatch_wasi_draining;
}

void
_dispatch_wasi_root_queue_poke(dispatch_queue_global_t dq)
{
	size_t idx = (size_t)(dq - _dispatch_root_queues);
	if (unlikely(idx >= DISPATCH_ROOT_QUEUE_COUNT)) {
		DISPATCH_INTERNAL_CRASH(dq, "Poke of a non-global root queue");
	}
	_dispatch_wasi_root_pending[idx] = true;
}

void
_dispatch_wasi_main_queue_poke(void)
{
	_dispatch_wasi_main_pending = true;
}

#pragma mark dispatch_loop

void
_dispatch_event_loop_atfork_child(void)
{
	// wasi-libc has no fork(), so no child process exists to reset.
}

void
_dispatch_event_loop_poke(dispatch_wlh_t wlh,
		uint64_t dq_state DISPATCH_UNUSED, uint32_t flags DISPATCH_UNUSED)
{
	if (wlh == DISPATCH_WLH_MANAGER) {
		_dispatch_wasi_mgr_pending = true;
		return;
	}
	// Without DISPATCH_USE_KEVENT_WORKLOOP, every poke caller passes
	// DISPATCH_WLH_MANAGER.
	DISPATCH_INTERNAL_CRASH((uintptr_t)wlh,
			"Unexpected non-manager event loop poke");
}

DISPATCH_NOINLINE
void
_dispatch_event_loop_drain(uint32_t flags DISPATCH_UNUSED)
{
	// Only the manager thread loop calls this, and it never runs on WASI.
	// _dispatch_wasi_mgr_queue_drain drains the manager queue instead.
	DISPATCH_INTERNAL_CRASH(0, "Manager event loop cannot run");
}

void
_dispatch_event_loop_cancel_waiter(dispatch_sync_context_t dsc)
{
	(void)dsc;
}

void
_dispatch_event_loop_wake_owner(dispatch_sync_context_t dsc,
		dispatch_wlh_t wlh, uint64_t old_state, uint64_t new_state)
{
	(void)dsc; (void)wlh; (void)old_state; (void)new_state;
}

void
_dispatch_event_loop_wait_for_ownership(dispatch_sync_context_t dsc)
{
	if (dsc->dsc_release_storage) {
		_dispatch_queue_release_storage(dsc->dc_data);
	}
}

void
_dispatch_event_loop_end_ownership(dispatch_wlh_t wlh, uint64_t old_state,
		uint64_t new_state, uint32_t flags)
{
	(void)wlh; (void)old_state; (void)new_state; (void)flags;
}

#if DISPATCH_WLH_DEBUG
void
_dispatch_event_loop_assert_not_owned(dispatch_wlh_t wlh)
{
	(void)wlh;
}
#endif

void
_dispatch_event_loop_leave_immediate(uint64_t dq_state)
{
	(void)dq_state;
}

#endif // DISPATCH_EVENT_BACKEND_WASI

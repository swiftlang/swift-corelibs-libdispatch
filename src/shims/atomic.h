/*
 * Copyright (c) 2008-2016 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

/*
 * IMPORTANT: This header file describes INTERNAL interfaces to libdispatch
 * which are subject to change in future releases of Mac OS X. Any applications
 * relying on these interfaces WILL break.
 */

#ifndef __DISPATCH_SHIMS_ATOMIC__
#define __DISPATCH_SHIMS_ATOMIC__

#ifndef __has_extension
#define __has_extension(x) 0
#endif
#ifndef __has_include
#define __has_include(x) 0
#endif

#if !(defined(__cplusplus) && __cplusplus >= 202302L && \
		defined(__GNUC__) && !defined(__clang__)) && \
		(!__has_extension(c_atomic) || !__has_include(<stdatomic.h>))
#error libdispatch requires C11 with <stdatomic.h>
#endif

// FreeBSD only defines _Bool in C mode. In C++ mode _Bool is not being defined.
#if defined(__cplusplus)
#define _Bool bool
#endif
#if defined(__ANDROID__) && __NDK_MAJOR__ >= 23
#include <bits/stdatomic.h>
#elif defined(__cplusplus) && __cplusplus >= 202302L && defined(__clang__)
/*
 * C++23 makes libc++'s <stdatomic.h> expose std::atomic-based functions.
 * These do not accept Clang's C _Atomic objects, which this shim uses.
 */
#define _dispatch_memory_order_relaxed __ATOMIC_RELAXED
#define _dispatch_memory_order_consume __ATOMIC_CONSUME
#define _dispatch_memory_order_acquire __ATOMIC_ACQUIRE
#define _dispatch_memory_order_release __ATOMIC_RELEASE
#define _dispatch_memory_order_acq_rel __ATOMIC_ACQ_REL
#define _dispatch_memory_order_seq_cst __ATOMIC_SEQ_CST
#define _dispatch_atomic_thread_fence __c11_atomic_thread_fence
#define _dispatch_atomic_load_explicit __c11_atomic_load
#define _dispatch_atomic_store_explicit __c11_atomic_store
#define _dispatch_atomic_exchange_explicit __c11_atomic_exchange
#define _dispatch_atomic_compare_exchange_strong_explicit \
		__c11_atomic_compare_exchange_strong
#define _dispatch_atomic_compare_exchange_weak_explicit \
		__c11_atomic_compare_exchange_weak
#define _dispatch_atomic_fetch_add_explicit __c11_atomic_fetch_add
#define _dispatch_atomic_fetch_sub_explicit __c11_atomic_fetch_sub
#define _dispatch_atomic_fetch_and_explicit __c11_atomic_fetch_and
#define _dispatch_atomic_fetch_or_explicit __c11_atomic_fetch_or
#define _dispatch_atomic_fetch_xor_explicit __c11_atomic_fetch_xor
#define os_atomic(type) type _Atomic
#define _os_atomic_c11_atomic(p) \
		((__typeof__(*(p)) _Atomic *)(p))
#elif defined(__cplusplus) && __cplusplus >= 202302L && defined(__GNUC__)
#define _dispatch_memory_order_relaxed __ATOMIC_RELAXED
#define _dispatch_memory_order_consume __ATOMIC_CONSUME
#define _dispatch_memory_order_acquire __ATOMIC_ACQUIRE
#define _dispatch_memory_order_release __ATOMIC_RELEASE
#define _dispatch_memory_order_acq_rel __ATOMIC_ACQ_REL
#define _dispatch_memory_order_seq_cst __ATOMIC_SEQ_CST
#define _dispatch_atomic_thread_fence __atomic_thread_fence
#define _dispatch_atomic_load_explicit __atomic_load_n
#define _dispatch_atomic_store_explicit __atomic_store_n
#define _dispatch_atomic_exchange_explicit __atomic_exchange_n
#define _dispatch_atomic_compare_exchange_strong_explicit(p, e, v, s, f) \
		__atomic_compare_exchange_n((p), (e), (v), false, (s), (f))
#define _dispatch_atomic_compare_exchange_weak_explicit(p, e, v, s, f) \
		__atomic_compare_exchange_n((p), (e), (v), true, (s), (f))
#define _dispatch_atomic_fetch_add_explicit __atomic_fetch_add
#define _dispatch_atomic_fetch_sub_explicit __atomic_fetch_sub
#define _dispatch_atomic_fetch_and_explicit __atomic_fetch_and
#define _dispatch_atomic_fetch_or_explicit __atomic_fetch_or
#define _dispatch_atomic_fetch_xor_explicit __atomic_fetch_xor
#define os_atomic(type) type
#define _os_atomic_c11_atomic(p) (p)
#else
#include <stdatomic.h>
#define _dispatch_memory_order_relaxed memory_order_relaxed
#define _dispatch_memory_order_consume memory_order_consume
#define _dispatch_memory_order_acquire memory_order_acquire
#define _dispatch_memory_order_release memory_order_release
#define _dispatch_memory_order_acq_rel memory_order_acq_rel
#define _dispatch_memory_order_seq_cst memory_order_seq_cst
#define _dispatch_atomic_thread_fence atomic_thread_fence
#define _dispatch_atomic_load_explicit atomic_load_explicit
#define _dispatch_atomic_store_explicit atomic_store_explicit
#define _dispatch_atomic_exchange_explicit atomic_exchange_explicit
#define _dispatch_atomic_compare_exchange_strong_explicit \
		atomic_compare_exchange_strong_explicit
#define _dispatch_atomic_compare_exchange_weak_explicit \
		atomic_compare_exchange_weak_explicit
#define _dispatch_atomic_fetch_add_explicit atomic_fetch_add_explicit
#define _dispatch_atomic_fetch_sub_explicit atomic_fetch_sub_explicit
#define _dispatch_atomic_fetch_and_explicit atomic_fetch_and_explicit
#define _dispatch_atomic_fetch_or_explicit atomic_fetch_or_explicit
#define _dispatch_atomic_fetch_xor_explicit atomic_fetch_xor_explicit
#define os_atomic(type) type _Atomic
#define _os_atomic_c11_atomic(p) \
		((__typeof__(*(p)) _Atomic *)(p))
#endif

#define _dispatch_memory_order_ordered    _dispatch_memory_order_seq_cst
#define _dispatch_memory_order_dependency _dispatch_memory_order_acquire

// This removes the _Atomic and volatile qualifiers on the type of *p
#define _os_atomic_basetypeof(p) \
		__typeof__(_dispatch_atomic_load_explicit(_os_atomic_c11_atomic(p), \
		_dispatch_memory_order_relaxed))

#define os_atomic_load(p, m) \
		_dispatch_atomic_load_explicit(_os_atomic_c11_atomic(p), \
		_dispatch_memory_order_##m)
#define os_atomic_store(p, v, m) \
		_dispatch_atomic_store_explicit(_os_atomic_c11_atomic(p), v, \
		_dispatch_memory_order_##m)
#define os_atomic_xchg(p, v, m) \
		_dispatch_atomic_exchange_explicit(_os_atomic_c11_atomic(p), v, \
		_dispatch_memory_order_##m)
#define os_atomic_cmpxchg(p, e, v, m) \
		({ _os_atomic_basetypeof(p) _r = (e); \
		_dispatch_atomic_compare_exchange_strong_explicit( \
		_os_atomic_c11_atomic(p), &_r, v, _dispatch_memory_order_##m, \
		_dispatch_memory_order_relaxed); })
#define os_atomic_cmpxchgv(p, e, v, g, m) \
		({ _os_atomic_basetypeof(p) _r = (e); _Bool _b = \
		_dispatch_atomic_compare_exchange_strong_explicit( \
		_os_atomic_c11_atomic(p), &_r, v, _dispatch_memory_order_##m, \
		_dispatch_memory_order_relaxed); *(g) = _r; _b; })
#define os_atomic_cmpxchgvw(p, e, v, g, m) \
		({ _os_atomic_basetypeof(p) _r = (e); _Bool _b = \
		_dispatch_atomic_compare_exchange_weak_explicit( \
		_os_atomic_c11_atomic(p), &_r, v, _dispatch_memory_order_##m, \
		_dispatch_memory_order_relaxed); *(g) = _r;  _b; })

#define _os_atomic_c11_op(p, v, m, o, op) \
		({ _os_atomic_basetypeof(p) _v = (v), _r = \
		_dispatch_atomic_fetch_##o##_explicit(_os_atomic_c11_atomic(p), _v, \
		_dispatch_memory_order_##m); (__typeof__(_r))(_r op _v); })
#define _os_atomic_c11_op_orig(p, v, m, o, op) \
		_dispatch_atomic_fetch_##o##_explicit(_os_atomic_c11_atomic(p), v, \
		_dispatch_memory_order_##m)
#define os_atomic_add(p, v, m) \
		_os_atomic_c11_op((p), (v), m, add, +)
#define os_atomic_add_orig(p, v, m) \
		_os_atomic_c11_op_orig((p), (v), m, add, +)
#define os_atomic_sub(p, v, m) \
		_os_atomic_c11_op((p), (v), m, sub, -)
#define os_atomic_sub_orig(p, v, m) \
		_os_atomic_c11_op_orig((p), (v), m, sub, -)
#define os_atomic_and(p, v, m) \
		_os_atomic_c11_op((p), (v), m, and, &)
#define os_atomic_and_orig(p, v, m) \
		_os_atomic_c11_op_orig((p), (v), m, and, &)
#define os_atomic_or(p, v, m) \
		_os_atomic_c11_op((p), (v), m, or, |)
#define os_atomic_or_orig(p, v, m) \
		_os_atomic_c11_op_orig((p), (v), m, or, |)
#define os_atomic_xor(p, v, m) \
		_os_atomic_c11_op((p), (v), m, xor, ^)
#define os_atomic_xor_orig(p, v, m) \
		_os_atomic_c11_op_orig((p), (v), m, xor, ^)

#define os_atomic_force_dependency_on(p, e) (p)
#define os_atomic_load_with_dependency_on(p, e) \
		os_atomic_load(os_atomic_force_dependency_on(p, e), relaxed)
#define os_atomic_load_with_dependency_on2o(p, f, e) \
		os_atomic_load_with_dependency_on(&(p)->f, e)

#define os_atomic_thread_fence(m) \
		_dispatch_atomic_thread_fence(_dispatch_memory_order_##m)

#define os_atomic_load2o(p, f, m) \
		os_atomic_load(&(p)->f, m)
#define os_atomic_store2o(p, f, v, m) \
		os_atomic_store(&(p)->f, (v), m)
#define os_atomic_xchg2o(p, f, v, m) \
		os_atomic_xchg(&(p)->f, (v), m)
#define os_atomic_cmpxchg2o(p, f, e, v, m) \
		os_atomic_cmpxchg(&(p)->f, (e), (v), m)
#define os_atomic_cmpxchgv2o(p, f, e, v, g, m) \
		os_atomic_cmpxchgv(&(p)->f, (e), (v), (g), m)
#define os_atomic_cmpxchgvw2o(p, f, e, v, g, m) \
		os_atomic_cmpxchgvw(&(p)->f, (e), (v), (g), m)
#define os_atomic_add2o(p, f, v, m) \
		os_atomic_add(&(p)->f, (v), m)
#define os_atomic_add_orig2o(p, f, v, m) \
		os_atomic_add_orig(&(p)->f, (v), m)
#define os_atomic_sub2o(p, f, v, m) \
		os_atomic_sub(&(p)->f, (v), m)
#define os_atomic_sub_orig2o(p, f, v, m) \
		os_atomic_sub_orig(&(p)->f, (v), m)
#define os_atomic_and2o(p, f, v, m) \
		os_atomic_and(&(p)->f, (v), m)
#define os_atomic_and_orig2o(p, f, v, m) \
		os_atomic_and_orig(&(p)->f, (v), m)
#define os_atomic_or2o(p, f, v, m) \
		os_atomic_or(&(p)->f, (v), m)
#define os_atomic_or_orig2o(p, f, v, m) \
		os_atomic_or_orig(&(p)->f, (v), m)
#define os_atomic_xor2o(p, f, v, m) \
		os_atomic_xor(&(p)->f, (v), m)
#define os_atomic_xor_orig2o(p, f, v, m) \
		os_atomic_xor_orig(&(p)->f, (v), m)

#define os_atomic_inc(p, m) \
		os_atomic_add((p), 1, m)
#define os_atomic_inc_orig(p, m) \
		os_atomic_add_orig((p), 1, m)
#define os_atomic_inc2o(p, f, m) \
		os_atomic_add2o(p, f, 1, m)
#define os_atomic_inc_orig2o(p, f, m) \
		os_atomic_add_orig2o(p, f, 1, m)
#define os_atomic_dec(p, m) \
		os_atomic_sub((p), 1, m)
#define os_atomic_dec_orig(p, m) \
		os_atomic_sub_orig((p), 1, m)
#define os_atomic_dec2o(p, f, m) \
		os_atomic_sub2o(p, f, 1, m)
#define os_atomic_dec_orig2o(p, f, m) \
		os_atomic_sub_orig2o(p, f, 1, m)

#define os_atomic_rmw_loop(p, ov, nv, m, ...)  ({ \
		bool _result = false; \
		__typeof__(p) _p = (p); \
		ov = os_atomic_load(_p, relaxed); \
		do { \
			__VA_ARGS__; \
			_result = os_atomic_cmpxchgvw(_p, ov, nv, &ov, m); \
		} while (unlikely(!_result)); \
		_result; \
	})
#define os_atomic_rmw_loop2o(p, f, ov, nv, m, ...) \
		os_atomic_rmw_loop(&(p)->f, ov, nv, m, __VA_ARGS__)
#define os_atomic_rmw_loop_give_up_with_fence(m, expr) \
		({ os_atomic_thread_fence(m); expr; __builtin_unreachable(); })
#define os_atomic_rmw_loop_give_up(expr) \
		os_atomic_rmw_loop_give_up_with_fence(relaxed, expr)

#endif // __DISPATCH_SHIMS_ATOMIC__

// Copyright 2026 Kakehashi Project
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#define KH_EXPORT __attribute__((visibility("default")))
#define KH_DISPATCH_TIME_FOREVER UINT64_MAX
#define KH_QOS_CLASS_DEFAULT 0x15u

typedef void *dispatch_queue_t;
typedef void *dispatch_semaphore_t;
typedef void *dispatch_workloop_t;
typedef void *os_workgroup_t;
typedef void *os_workgroup_interval_t;
typedef void *os_workgroup_parallel_t;
typedef void *os_workgroup_attr_t;
typedef void *os_workgroup_mpt_attr_t;
typedef void *os_workgroup_interval_data_t;
typedef void *os_workgroup_join_token_t;
typedef uint32_t mach_port_t;
typedef uint32_t os_workgroup_index;
typedef void (*os_workgroup_working_arena_destructor_t)(void *);
typedef unsigned int qos_class_t;

extern char _dispatch_main_q;
extern intptr_t dispatch_semaphore_signal(dispatch_semaphore_t dsema);
extern intptr_t dispatch_semaphore_wait(dispatch_semaphore_t dsema,
		uint64_t timeout);
extern void dispatch_assert_queue(dispatch_queue_t queue);

struct kh_objc_class {
	uint64_t isa;
	uint64_t superclass;
	uint64_t name;
	uint64_t instance_size;
	uint32_t flags;
	uint32_t reserved;
	uint64_t methods;
	uint64_t ivars;
	uint64_t cache;
};

extern void *objc_getClass(const char *name);
extern void *kh_objc_class_register(const uint8_t *name, void *superclass,
		void *class_storage, void *metaclass_storage, size_t instance_size,
		uint32_t kind);

KH_EXPORT int dispatch_allow_send_signals(int preserve_signum);
KH_EXPORT void dispatch_workloop_set_os_workgroup(dispatch_workloop_t workloop,
		os_workgroup_t workgroup);
KH_EXPORT void dispatch_workloop_set_scheduler_priority(
		dispatch_workloop_t workloop, int priority);
KH_EXPORT void os_workgroup_cancel(os_workgroup_t wg);
KH_EXPORT int os_workgroup_copy_port(os_workgroup_t wg,
		mach_port_t *mach_port_out);
KH_EXPORT os_workgroup_t os_workgroup_create_with_port(const char *name,
		mach_port_t mach_port);
KH_EXPORT os_workgroup_t os_workgroup_create_with_workgroup(const char *name,
		os_workgroup_t wg);
KH_EXPORT void *os_workgroup_get_working_arena(os_workgroup_t wg,
		os_workgroup_index *index_out);
KH_EXPORT int os_workgroup_interval_finish(os_workgroup_interval_t wg,
		os_workgroup_interval_data_t data);
KH_EXPORT int os_workgroup_interval_start(os_workgroup_interval_t wg,
		uint64_t start, uint64_t deadline, os_workgroup_interval_data_t data);
KH_EXPORT int os_workgroup_interval_update(os_workgroup_interval_t wg,
		uint64_t deadline, os_workgroup_interval_data_t data);
KH_EXPORT int os_workgroup_join(os_workgroup_t wg,
		os_workgroup_join_token_t token_out);
KH_EXPORT void os_workgroup_leave(os_workgroup_t wg,
		os_workgroup_join_token_t token);
KH_EXPORT int os_workgroup_max_parallel_threads(os_workgroup_t wg,
		os_workgroup_mpt_attr_t attr);
KH_EXPORT os_workgroup_parallel_t os_workgroup_parallel_create(
		const char *name, os_workgroup_attr_t attr);
KH_EXPORT int os_workgroup_set_working_arena(os_workgroup_t wg, void *arena,
		uint32_t max_workers,
		os_workgroup_working_arena_destructor_t destructor);
KH_EXPORT bool os_workgroup_testcancel(os_workgroup_t wg);

#define KH_DISPATCH_SUBCLASS_ROWS(X) \
	X(OS_dispatch_channel) \
	X(OS_dispatch_data) \
	X(OS_dispatch_disk) \
	X(OS_dispatch_group) \
	X(OS_dispatch_io) \
	X(OS_dispatch_mach) \
	X(OS_dispatch_mach_msg) \
	X(OS_dispatch_object) \
	X(OS_dispatch_operation) \
	X(OS_dispatch_queue) \
	X(OS_dispatch_queue_attr) \
	X(OS_dispatch_queue_concurrent) \
	X(OS_dispatch_queue_global) \
	X(OS_dispatch_queue_main) \
	X(OS_dispatch_queue_serial) \
	X(OS_dispatch_queue_serial_executor) \
	X(OS_dispatch_semaphore) \
	X(OS_dispatch_source) \
	X(OS_dispatch_workloop) \
	X(OS_os_eventlink) \
	X(OS_os_workgroup) \
	X(OS_os_workgroup_interval) \
	X(OS_os_workgroup_parallel) \
	X(OS_voucher)

#define KH_DEFINE_DISPATCH_CLASS(name) \
	KH_EXPORT struct kh_objc_class kh_##name##_class \
			__asm("_OBJC_CLASS_$_" #name) = { 0 }; \
	KH_EXPORT struct kh_objc_class kh_##name##_metaclass \
			__asm("_OBJC_METACLASS_$_" #name) = { 0 };

KH_EXPORT struct kh_objc_class kh_OS_object_class
		__asm("_OBJC_CLASS_$_OS_object") = { 0 };
KH_EXPORT struct kh_objc_class kh_OS_object_metaclass
		__asm("_OBJC_METACLASS_$_OS_object") = { 0 };
KH_DISPATCH_SUBCLASS_ROWS(KH_DEFINE_DISPATCH_CLASS)

static _Atomic uint32_t kh_dispatch_classes_registered;

static void
kh_register_dispatch_classes(void)
{
	uint32_t expected = 0;
	if (!atomic_compare_exchange_strong_explicit(
			&kh_dispatch_classes_registered, &expected, 1,
			memory_order_acq_rel, memory_order_acquire)) {
		return;
	}
	void *nsobject = objc_getClass("NSObject");
	void *os_object = kh_objc_class_register((const uint8_t *)"OS_object",
			nsobject, &kh_OS_object_class, &kh_OS_object_metaclass, 0, 1);
#define KH_REGISTER_DISPATCH_CLASS(name) \
	(void)kh_objc_class_register((const uint8_t *)#name, os_object, \
			&kh_##name##_class, &kh_##name##_metaclass, 0, 1);
	KH_DISPATCH_SUBCLASS_ROWS(KH_REGISTER_DISPATCH_CLASS)
#undef KH_REGISTER_DISPATCH_CLASS
}

KH_EXPORT void kh_product_init(void);
KH_EXPORT void
kh_product_init(void)
{
	kh_register_dispatch_classes();
}

KH_EXPORT void *kh_swift_os_dispatch_queue_metadata(void)
		__asm("_$sSo17OS_dispatch_queueCMa");
KH_EXPORT void *
kh_swift_os_dispatch_queue_metadata(void)
{
	kh_register_dispatch_classes();
	return &kh_OS_dispatch_queue_class;
}

KH_EXPORT int
dispatch_allow_send_signals(int preserve_signum)
{
	(void)preserve_signum;
	errno = ENOTSUP;
	return -1;
}

KH_EXPORT void
dispatch_workloop_set_os_workgroup(dispatch_workloop_t workloop,
		os_workgroup_t workgroup)
{
	(void)workloop;
	(void)workgroup;
}

KH_EXPORT void
dispatch_workloop_set_scheduler_priority(dispatch_workloop_t workloop,
		int priority)
{
	(void)workloop;
	(void)priority;
}

KH_EXPORT void
os_workgroup_cancel(os_workgroup_t wg)
{
	(void)wg;
}

KH_EXPORT int
os_workgroup_copy_port(os_workgroup_t wg, mach_port_t *mach_port_out)
{
	(void)wg;
	if (mach_port_out) {
		*mach_port_out = 0;
	}
	errno = ENOTSUP;
	return ENOTSUP;
}

KH_EXPORT os_workgroup_t
os_workgroup_create_with_port(const char *name, mach_port_t mach_port)
{
	(void)name;
	(void)mach_port;
	errno = ENOTSUP;
	return NULL;
}

KH_EXPORT os_workgroup_t
os_workgroup_create_with_workgroup(const char *name, os_workgroup_t wg)
{
	(void)name;
	(void)wg;
	errno = ENOTSUP;
	return NULL;
}

KH_EXPORT void *
os_workgroup_get_working_arena(os_workgroup_t wg,
		os_workgroup_index *index_out)
{
	(void)wg;
	if (index_out) {
		*index_out = 0;
	}
	return NULL;
}

KH_EXPORT int
os_workgroup_interval_finish(os_workgroup_interval_t wg,
		os_workgroup_interval_data_t data)
{
	(void)wg;
	(void)data;
	return ENOTSUP;
}

KH_EXPORT int
os_workgroup_interval_start(os_workgroup_interval_t wg, uint64_t start,
		uint64_t deadline, os_workgroup_interval_data_t data)
{
	(void)wg;
	(void)start;
	(void)deadline;
	(void)data;
	return ENOTSUP;
}

KH_EXPORT int
os_workgroup_interval_update(os_workgroup_interval_t wg, uint64_t deadline,
		os_workgroup_interval_data_t data)
{
	(void)wg;
	(void)deadline;
	(void)data;
	return ENOTSUP;
}

KH_EXPORT int
os_workgroup_join(os_workgroup_t wg, os_workgroup_join_token_t token_out)
{
	(void)wg;
	(void)token_out;
	return ENOTSUP;
}

KH_EXPORT void
os_workgroup_leave(os_workgroup_t wg, os_workgroup_join_token_t token)
{
	(void)wg;
	(void)token;
}

KH_EXPORT int
os_workgroup_max_parallel_threads(os_workgroup_t wg,
		os_workgroup_mpt_attr_t attr)
{
	(void)wg;
	(void)attr;
	return 1;
}

KH_EXPORT os_workgroup_parallel_t
os_workgroup_parallel_create(const char *name, os_workgroup_attr_t attr)
{
	(void)name;
	(void)attr;
	errno = ENOTSUP;
	return NULL;
}

KH_EXPORT int
os_workgroup_set_working_arena(os_workgroup_t wg, void *arena,
		uint32_t max_workers,
		os_workgroup_working_arena_destructor_t destructor)
{
	(void)wg;
	(void)arena;
	(void)max_workers;
	(void)destructor;
	return ENOTSUP;
}

KH_EXPORT bool
os_workgroup_testcancel(os_workgroup_t wg)
{
	(void)wg;
	return false;
}

KH_EXPORT intptr_t kh_swift_dispatch_semaphore_signal(dispatch_semaphore_t dsema)
		__asm("_$sSo21OS_dispatch_semaphoreC8DispatchE6signalSiyF");
KH_EXPORT intptr_t
kh_swift_dispatch_semaphore_signal(dispatch_semaphore_t dsema)
{
	return dispatch_semaphore_signal(dsema);
}

KH_EXPORT void kh_swift_dispatch_semaphore_wait(dispatch_semaphore_t dsema)
		__asm("_$sSo21OS_dispatch_semaphoreC8DispatchE4waityyF");
KH_EXPORT void
kh_swift_dispatch_semaphore_wait(dispatch_semaphore_t dsema)
{
	(void)dispatch_semaphore_wait(dsema, KH_DISPATCH_TIME_FOREVER);
}

KH_EXPORT dispatch_queue_t dispatch_get_main_queue(void);
KH_EXPORT dispatch_queue_t
dispatch_get_main_queue(void)
{
	return &_dispatch_main_q;
}

KH_EXPORT void kh_dispatch_assert_queue_v2(dispatch_queue_t queue)
		__asm("_dispatch_assert_queue$V2");
KH_EXPORT void
kh_dispatch_assert_queue_v2(dispatch_queue_t queue)
{
	dispatch_assert_queue(queue);
}

KH_EXPORT qos_class_t qos_class_self(void);
KH_EXPORT qos_class_t
qos_class_self(void)
{
	return KH_QOS_CLASS_DEFAULT;
}

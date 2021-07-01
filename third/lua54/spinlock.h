#ifndef  _SPINLOCK_H__
#define _SPINLOCK_H__

#if defined(_MSC_VER)
#include "atomic.h"
#define atomic_test_and_set_(ptr) _InterlockedExchange((volatile long*)(ptr), 1)
#define atomic_clear_(ptr)   (void)(*(ptr) = (0))
#define atomic_load_relaxed_(ptr)  *(ptr)
#define atomic_pause_() _mm_pause()

#else
#include <stdatomic.h>
#define atomic_test_and_set_(ptr) atomic_exchange_explicit(ptr, 1, memory_order_acquire)
#define atomic_clear_(ptr) atomic_store_explicit(ptr, 0, memory_order_release);
#define atomic_load_relaxed_(ptr) atomic_load_explicit(ptr, memory_order_relaxed)

#if defined(__x86_64__)
#define atomic_pause_() __builtin_ia32_pause()
#else
#define atomic_pause_() ((void)0)
#endif

#endif

#define SPIN_INIT(Q) spinlock_init(&(Q)->lock);
#define SPIN_LOCK(Q) spinlock_lock(&(Q)->lock);
#define SPIN_UNLOCK(Q) spinlock_unlock(&(Q)->lock);
#define SPIN_TRYLOCK(Q) spinlock_trylock(&(Q)->lock);

struct spinlock {
	atomic_int lock;
};

static inline void
spinlock_init(struct spinlock *lock) {
	atomic_init(&lock->lock, 0);
}

static inline void
spinlock_lock(struct spinlock *lock) {
	for (;;) {
	  if (!atomic_test_and_set_(&lock->lock))
		return;
	  while (atomic_load_relaxed_(&lock->lock))
		atomic_pause_();
	}
}

static inline int
spinlock_trylock(struct spinlock *lock) {
	return !atomic_load_relaxed_(&lock->lock) &&
		!atomic_test_and_set_(&lock->lock);
}

static inline void
spinlock_unlock(struct spinlock *lock) {
	atomic_clear_(&lock->lock);
}

static inline void
spinlock_destroy(struct spinlock *lock) {
	(void) lock;
}

#endif
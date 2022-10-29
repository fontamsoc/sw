// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef MUTEX_H
#define MUTEX_H

// A mutex is used to serialize access to a section of code that
// cannot be executed concurrently by more than one thread.
// A mutex object only allows one thread into a controlled section,
// forcing other threads which attempt to gain access to that section
// to wait until the first thread has exited from that section.

// Struct representing a mutex.
// Before use, the mutex should be zeroed.
typedef struct {
	// This field is used to prevent more than one threads
	// from incrementing the field ticket at the same time.
	volatile unsigned long lock;
	// The fields ticket and current are used to insure that
	// each thread having called mutex_lock() on the same mutex,
	// get a lock on the mutex in the order they called mutex_lock().
	// Each thread calling mutex_lock() on the mutex, saves the value
	// from the field ticket, increments it and enters a loop until
	// the field current becomes equal to the value previously
	// saved from the field ticket.
	// The field current is incremented everytime a thread
	// calls mutex_unlock().
	volatile unsigned long ticket;
	volatile unsigned long current;
} mutex;

// Define MUTEX_CHECK_ENOUGH_TICKET if there could be
// more than (unsigned long)-2 acquiring a mutex lock
// at the same time.

// This function acquires a mutex lock.
// It enters a spinlock until any other thread having acquired
// the same mutex lock, calls mutex_unlock() to release it.
// The order in which mutex locks are acquired is preserved.
static inline void mutex_lock (mutex* s) {
	// Only one thread can have access to the field mutex.ticket.
	// Loop until the lock is removed if there was already a lock.
	__asm__ __volatile__ (
		"rli %%sr, 0f; 0: ldst %0, %1; jnz %0, %%sr\n"
		: "+r"((unsigned long){1}) : "r"(&s->lock) : "memory");
	#ifdef MUTEX_CHECK_ENOUGH_TICKET
	// If there is not enough tickets, loop around until a ticket
	// is made available.
	// In fact, if many tickets are being requested, the value
	// of the field mutex.ticket may wrap around and catch up with
	// the value of the field mutex.current.
	// -1 is the maximum value of an unsigned long.
	while ((s->ticket - s->current) == -1)
		__asm__ __volatile__ ("" ::: "memory");
	#endif
	unsigned long ticket = s->ticket++;
	s->lock = 0;
	while (s->current != ticket)
		__asm__ __volatile__ ("" ::: "memory");
}

// This function releases a mutex lock.
static inline void mutex_unlock (mutex* s) {
	++s->current;
}

#endif /* MUTEX_H */

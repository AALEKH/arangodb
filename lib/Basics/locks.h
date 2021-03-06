////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_BASICS_LOCKS_H
#define LIB_BASICS_LOCKS_H 1

#include "Basics/Common.h"

#ifdef TRI_HAVE_POSIX_THREADS
#include "Basics/locks-posix.h"
#endif

#ifdef TRI_HAVE_WIN32_THREADS
#include "Basics/locks-win32.h"
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a new mutex
///
/// Mutual exclusion (often abbreviated to mutex) algorithms are used in
/// concurrent programming to avoid the simultaneous use of a common resource,
/// such as a global variable, by pieces of computer code called critical
/// sections. A critical section is a piece of code in which a process or thread
/// accesses a common resource. The critical section by itself is not a
/// mechanism or algorithm for mutual exclusion. A program, process, or thread
/// can have the critical section in it without any mechanism or algorithm which
/// implements mutual exclusion. For details see www.wikipedia.org.
////////////////////////////////////////////////////////////////////////////////

int TRI_InitMutex(TRI_mutex_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a mutex
////////////////////////////////////////////////////////////////////////////////

int TRI_DestroyMutex(TRI_mutex_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief locks mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_LockMutex(TRI_mutex_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockMutex(TRI_mutex_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a new read-write lock
///
/// A ReadWriteLock maintains a pair of associated locks, one for read-only
/// operations and one for writing. The read lock may be held simultaneously by
/// multiple reader threads, so long as there are no writers. The write lock is
/// exclusive.
///
/// A read-write lock allows for a greater level of concurrency in accessing
/// shared data than that permitted by a mutual exclusion lock. It exploits the
/// fact that while only a single thread at a time (a writer thread) can modify
/// the shared data, in many cases any number of threads can concurrently read
/// the data (hence reader threads). In theory, the increase in concurrency
/// permitted by the use of a read-write lock will lead to performance
/// improvements over the use of a mutual exclusion lock. In practice this
/// increase in concurrency will only be fully realized on a multi-processor,
/// and then only if the access patterns for the shared data are suitable.
////////////////////////////////////////////////////////////////////////////////

void TRI_InitReadWriteLock(TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes a read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyReadWriteLock(TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to read lock read-write lock
////////////////////////////////////////////////////////////////////////////////

bool TRI_TryReadLockReadWriteLock(TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadLockReadWriteLock(TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockReadWriteLock(TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to write lock read-write lock
////////////////////////////////////////////////////////////////////////////////

bool TRI_TryWriteLockReadWriteLock(TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteLockReadWriteLock(TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteUnlockReadWriteLock(TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a new condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitCondition(TRI_condition_t* cond);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCondition(TRI_condition_t* cond);

////////////////////////////////////////////////////////////////////////////////
/// @brief signals a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_SignalCondition(TRI_condition_t* cond);

////////////////////////////////////////////////////////////////////////////////
/// @brief broad casts a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_BroadcastCondition(TRI_condition_t* cond);

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a signal on a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_WaitCondition(TRI_condition_t* cond);

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a signal with a timeout in micro-seconds
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

bool TRI_TimedWaitCondition(TRI_condition_t* cond, uint64_t delay);

////////////////////////////////////////////////////////////////////////////////
/// @brief locks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_LockCondition(TRI_condition_t* cond);

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCondition(TRI_condition_t* cond);

#endif

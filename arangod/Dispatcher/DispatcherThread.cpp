////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "DispatcherThread.h"

#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/logging.h"
#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/DispatcherQueue.h"
#include "Dispatcher/Job.h"
#include "Scheduler/Scheduler.h"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                            class DispatcherThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                            thread local variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief a global, but thread-local place to hold the current dispatcher
/// thread. If we are not in a dispatcher thread this is set to nullptr.
////////////////////////////////////////////////////////////////////////////////

thread_local DispatcherThread* DispatcherThread::currentDispatcherThread = nullptr;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a dispatcher thread
////////////////////////////////////////////////////////////////////////////////

DispatcherThread::DispatcherThread (DispatcherQueue* queue)
  : Thread("dispat"+ 
           (queue->_id == Dispatcher::STANDARD_QUEUE 
            ? std::string("_std")
            : (queue->_id == Dispatcher::AQL_QUEUE 
               ? std::string("_aql") : ("_" + to_string(queue->_id))))),
    _queue(queue) {

  allowAsynchronousCancelation();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::run () {
  currentDispatcherThread = this;
  double worked = 0;
  double grace = 0.2;

  // iterate until we are shutting down
  while (! _queue->_stopping.load(memory_order_relaxed)) {
    double now = TRI_microtime();

    // drain the job queue
    {
      Job* job = nullptr;

      while (_queue->_readyJobs.pop(job)) {
        if (job != nullptr) {
          --(_queue->_numberJobs);

          worked = now;
          handleJob(job);
        }
      }

      // we need to check again if more work has arrived after we have
      // aquired the lock. The lockfree queue and _nrWaiting are accessed
      // using "memory_order_seq_cst", this guaranties that we do not
      // miss a signal.

      if (worked + grace < now) {
        ++_queue->_nrWaiting;

        CONDITION_LOCKER(guard, _queue->_waitLock);

        if (! _queue->_readyJobs.empty()) {
          --_queue->_nrWaiting;
          continue;
        }

        // wait at most 100ms
        uintptr_t n = (uintptr_t) this;
        _queue->_waitLock.wait((1 + ((n >> 3) % 9)) * 100 * 1000);

        --_queue->_nrWaiting;

        // there is a chance, that we created more threads than necessary because
        // we ignore race conditions for the statistic variables
        if (_queue->tooManyThreads()) {
          break;
        }
      }
      else if (worked < now) {
        uintptr_t n = (uintptr_t) this;
        usleep(1 + ((n >> 3) % 19));
      }
    }
  }

  LOG_TRACE("dispatcher thread has finished");

  // this will delete the thread
  _queue->removeStartedThread(this);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::addStatus(VPackBuilder* b) {
  Thread::addStatus(b);
  b->add("queue", VPackValue(_queue->_id));
  b->add("stopping", VPackValue(_queue->_stopping.load()));
  b->add("waitingJobs", VPackValue(_queue->_numberJobs.load()));
  b->add("numberRunning", VPackValue((int) _queue->_nrRunning.load()));
  b->add("numberWaiting", VPackValue((int) _queue->_nrWaiting.load()));
  b->add("numberBlocked", VPackValue((int) _queue->_nrBlocked.load()));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread is doing a blocking operation
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::block () {
  _queue->blockThread();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread has resumed work
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::unblock () {
  _queue->unblockThread();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief do the real work
////////////////////////////////////////////////////////////////////////////////

void DispatcherThread::handleJob (Job* job) {
  LOG_DEBUG("starting to run job: %s", job->getName().c_str());

  // start all the dirty work
  try {
    job->requestStatisticsAgentSetQueueEnd();
    job->work();
  }
  catch (Exception const& ex) {
    try {
      job->handleError(ex);
    }
    catch (Exception const& ex) {
      LOG_WARNING("caught error while handling error: %s", ex.what());
    }
    catch (std::exception const& ex) {
      LOG_WARNING("caught error while handling error: %s", ex.what());
    }
    catch (...) {
      LOG_WARNING("caught unknown error while handling error!");
    }
  }
  catch (std::bad_alloc const& ex) {
    try {
      Exception ex2(TRI_ERROR_OUT_OF_MEMORY, string("job failed with bad_alloc: ") + ex.what(), __FILE__, __LINE__);

      job->handleError(ex2);
      LOG_WARNING("caught exception in work(): %s", ex2.what());
    }
    catch (...) {
      LOG_WARNING("caught unknown error while handling error!");
    }
  }
  catch (std::exception const& ex) {
    try {
      Exception ex2(TRI_ERROR_INTERNAL, string("job failed with error: ") + ex.what(), __FILE__, __LINE__);

      job->handleError(ex2);
      LOG_WARNING("caught exception in work(): %s", ex2.what());
    }
    catch (...) {
      LOG_WARNING("caught unknown error while handling error!");
    }
  }
  catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
    if (_queue->_stopping.load(memory_order_relaxed)) {
      LOG_WARNING("caught cancelation exception during work");
      throw;
    }
#endif

    try {
      Exception ex(TRI_ERROR_INTERNAL, "job failed with unknown error", __FILE__, __LINE__);

      job->handleError(ex);
      LOG_WARNING("caught unknown exception in work()");
    }
    catch (...) {
      LOG_WARNING("caught unknown error while handling error!");
    }
  }

  // finish jobs
  try {
    job->cleanup(_queue);
  }
  catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
    if (_queue->_stopping.load(memory_order_relaxed)) {
      LOG_WARNING("caught cancelation exception during cleanup");
      throw;
    }
#endif

    LOG_WARNING("caught error while cleaning up!");
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

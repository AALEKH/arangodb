////////////////////////////////////////////////////////////////////////////////
/// @brief http server job
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_HTTP_SERVER_HTTP_SERVER_JOB_H
#define ARANGODB_HTTP_SERVER_HTTP_SERVER_JOB_H 1

#include "Dispatcher/Job.h"

#include "Basics/Exceptions.h"
#include "Basics/WorkMonitor.h"

// -----------------------------------------------------------------------------
// --SECTION--                                               class HttpServerJob
// -----------------------------------------------------------------------------

namespace triagens {
namespace rest {
class HttpHandler;
class HttpServer;

////////////////////////////////////////////////////////////////////////////////
/// @brief general server job
////////////////////////////////////////////////////////////////////////////////

class HttpServerJob : public Job {
  HttpServerJob(HttpServerJob const&) = delete;
  HttpServerJob& operator=(HttpServerJob const&) = delete;

  // ---------------------------------------------------------------------------
  // --SECTION--                                    constructors and destructors
  // ---------------------------------------------------------------------------

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructs a new server job
  //////////////////////////////////////////////////////////////////////////////

  HttpServerJob(HttpServer*, arangodb::WorkItem::uptr<HttpHandler>&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destructs a server job
  //////////////////////////////////////////////////////////////////////////////

  ~HttpServerJob();

  // ---------------------------------------------------------------------------
  // --SECTION--                                                  public methods
  // ---------------------------------------------------------------------------

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the underlying handler
  //////////////////////////////////////////////////////////////////////////////

  HttpHandler* handler() const { return _handler.get(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief shutdown the job
  //////////////////////////////////////////////////////////////////////////////

  void beginShutdown();

  // ---------------------------------------------------------------------------
  // --SECTION--                                                     Job methods
  // ---------------------------------------------------------------------------

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// {@inheritDoc}
  //////////////////////////////////////////////////////////////////////////////

  size_t queue() const override;

  //////////////////////////////////////////////////////////////////////////////
  /// {@inheritDoc}
  //////////////////////////////////////////////////////////////////////////////

  void work() override;

  //////////////////////////////////////////////////////////////////////////////
  /// {@inheritDoc}
  //////////////////////////////////////////////////////////////////////////////

  bool cancel() override;

  //////////////////////////////////////////////////////////////////////////////
  /// {@inheritDoc}
  //////////////////////////////////////////////////////////////////////////////

  void cleanup(DispatcherQueue*) override;

  //////////////////////////////////////////////////////////////////////////////
  /// {@inheritDoc}
  //////////////////////////////////////////////////////////////////////////////

  void handleError(basics::Exception const&) override;

  // ---------------------------------------------------------------------------
  // --SECTION--                                             protected variables
  // ---------------------------------------------------------------------------

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief general server
  //////////////////////////////////////////////////////////////////////////////

  HttpServer* _server;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handler
  //////////////////////////////////////////////////////////////////////////////

  arangodb::WorkItem::uptr<HttpHandler> _handler;
};
}
}

#endif

// ---------------------------------------------------------------------------
// --SECTION--                                                     END-OF-FILE
// ---------------------------------------------------------------------------

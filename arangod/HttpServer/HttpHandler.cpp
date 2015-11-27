////////////////////////////////////////////////////////////////////////////////
/// @brief abstract class for http handlers
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpHandler.h"

#include "Basics/StringUtils.h"
#include "Basics/logging.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/HttpServerJob.h"
#include "Rest/HttpRequest.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                 class HttpHandler
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new handler
////////////////////////////////////////////////////////////////////////////////

HttpHandler::HttpHandler(HttpRequest *request)
    : _taskId(0), _request(request), _response(nullptr), _server(nullptr) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a handler
////////////////////////////////////////////////////////////////////////////////

HttpHandler::~HttpHandler() {
  delete _request;
  delete _response;
}

// -----------------------------------------------------------------------------
// --SECTION--                                            virtual public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the queue name
////////////////////////////////////////////////////////////////////////////////

size_t HttpHandler::queue() const {
  bool found;
  const char *queue = _request->header("x-arango-queue", found);

  if (found) {
    uint32_t n = StringUtils::uint32(queue);

    if (n == 0) {
      return Dispatcher::STANDARD_QUEUE;
    }

    return n + (Dispatcher::SYSTEM_QUEUE_SIZE - 1);
  }

  return Dispatcher::STANDARD_QUEUE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares for execution
////////////////////////////////////////////////////////////////////////////////

void HttpHandler::prepareExecute() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to cancel an execution
////////////////////////////////////////////////////////////////////////////////

void HttpHandler::finalizeExecute() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to cancel an execution
////////////////////////////////////////////////////////////////////////////////

bool HttpHandler::cancel() { return false; }

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a response
////////////////////////////////////////////////////////////////////////////////

void HttpHandler::addResponse(HttpHandler *) {
  // nothing by default
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////////
/// @brief returns the id of the underlying task
//////////////////////////////////////////////////////////////////////////////

uint64_t HttpHandler::taskId() {
  return _taskId;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief returns the event loop of the underlying task
//////////////////////////////////////////////////////////////////////////////

EventLoop HttpHandler::eventLoop() {
  return _loop;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief sets the id of the underlying task or 0 to dettach
//////////////////////////////////////////////////////////////////////////////

void HttpHandler::setTaskId(uint64_t id, EventLoop loop) {
  _taskId = id;
  _loop = loop;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief execution cycle including error handling and prepare
//////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t HttpHandler::executeFull() {
  HttpHandler::status_t status(HttpHandler::HANDLER_FAILED);

  RequestStatisticsAgentSetRequestStart(this); // FMH TODO: move into monitor

  try {
    prepareExecute();

    try {
      status = execute();
    } catch (Exception const &ex) {
      RequestStatisticsAgentSetExecuteError(this);
      handleError(ex);
    } catch (std::exception const &ex) {
      RequestStatisticsAgentSetExecuteError(this);
      Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
      handleError(err);
    } catch (...) {
      RequestStatisticsAgentSetExecuteError(this);
      Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
      handleError(err);
    }

    finalizeExecute();

    RequestStatisticsAgentSetRequestEnd(this);

    return status;
  } catch (Exception const &ex) {
    RequestStatisticsAgentSetExecuteError(this);
    LOG_ERROR("caught exception: %s", DIAGNOSTIC_INFORMATION(ex));
  } catch (std::exception const &ex) {
    RequestStatisticsAgentSetExecuteError(this);
    LOG_ERROR("caught exception: %s", ex.what());
  } catch (...) {
    RequestStatisticsAgentSetExecuteError(this);
    LOG_ERROR("caught exception");
  }

  RequestStatisticsAgentSetRequestEnd(this);

  return status_t(HttpHandler::HANDLER_FAILED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register the server object
////////////////////////////////////////////////////////////////////////////////

void HttpHandler::setServer(HttpHandlerFactory *server) { _server = server; }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the request
////////////////////////////////////////////////////////////////////////////////

HttpRequest const *HttpHandler::getRequest() const { return _request; }

////////////////////////////////////////////////////////////////////////////////
/// @brief steal the request
////////////////////////////////////////////////////////////////////////////////

HttpRequest *HttpHandler::stealRequest() {
  HttpRequest *tmp = _request;
  _request = nullptr;
  return tmp;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the response
////////////////////////////////////////////////////////////////////////////////

HttpResponse *HttpHandler::getResponse() const { return _response; }

////////////////////////////////////////////////////////////////////////////////
/// @brief steal the response
////////////////////////////////////////////////////////////////////////////////

HttpResponse *HttpHandler::stealResponse() {
  HttpResponse *tmp = _response;
  _response = nullptr;
  return tmp;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new HTTP response
////////////////////////////////////////////////////////////////////////////////

void HttpHandler::createResponse(HttpResponse::HttpResponseCode code) {

  // avoid having multiple responses. this would be a memleak
  if (_response != nullptr) {
    delete _response;
    _response = nullptr;
  }

  int32_t apiCompatibility;

  if (_request != nullptr) {
    apiCompatibility = _request->compatibility();
  } else {
    apiCompatibility = HttpRequest::MinCompatibility;
  }

  // create a "standard" (standalone) Http response
  _response = new HttpResponse(code, apiCompatibility);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

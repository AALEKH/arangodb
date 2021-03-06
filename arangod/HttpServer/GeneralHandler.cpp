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

#include "GeneralHandler.h"

#include "Basics/StringUtils.h"
#include "Basics/Logger.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/GeneralServerJob.h"
#include "Rest/GeneralRequest.h"

using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
std::atomic_uint_fast64_t NEXT_HANDLER_ID(
    static_cast<uint64_t>(TRI_microtime() * 100000.0));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new handler
////////////////////////////////////////////////////////////////////////////////

GeneralHandler::GeneralHandler(GeneralRequest* request)
    : _handlerId(NEXT_HANDLER_ID.fetch_add(1, std::memory_order_seq_cst)),
      _taskId(0),
      _request(request),
      _response(nullptr),
      _server(nullptr) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a handler
////////////////////////////////////////////////////////////////////////////////

GeneralHandler::~GeneralHandler() {
  delete _request;
  delete _response;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the queue name
////////////////////////////////////////////////////////////////////////////////

size_t GeneralHandler::queue() const {
  bool found;
  char const* queue = _request->header("x-arango-queue", found);

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

void GeneralHandler::prepareExecute() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief finalize the execution
////////////////////////////////////////////////////////////////////////////////

void GeneralHandler::finalizeExecute() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to cancel an execution
////////////////////////////////////////////////////////////////////////////////

bool GeneralHandler::cancel() { return false; }

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a response
////////////////////////////////////////////////////////////////////////////////

void GeneralHandler::addResponse(GeneralHandler*) {
  // nothing by default
}

//////////////////////////////////////////////////////////////////////////////
/// @brief returns the id of the underlying task
//////////////////////////////////////////////////////////////////////////////

uint64_t GeneralHandler::taskId() const { return _taskId; }

//////////////////////////////////////////////////////////////////////////////
/// @brief returns the event loop of the underlying task
//////////////////////////////////////////////////////////////////////////////

EventLoop GeneralHandler::eventLoop() const { return _loop; }

//////////////////////////////////////////////////////////////////////////////
/// @brief sets the id of the underlying task or 0 to dettach
//////////////////////////////////////////////////////////////////////////////

void GeneralHandler::setTaskId(uint64_t id, EventLoop loop) {
  _taskId = id;
  _loop = loop;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief execution cycle including error handling and prepare
//////////////////////////////////////////////////////////////////////////////

GeneralHandler::status_t GeneralHandler::executeFull() {
  GeneralHandler::status_t status(GeneralHandler::HANDLER_FAILED);

  requestStatisticsAgentSetRequestStart();

  try {
    prepareExecute();

    try {
      status = execute();
    } catch (Exception const& ex) {
      requestStatisticsAgentSetExecuteError();
      handleError(ex);
    } catch (std::bad_alloc const& ex) {
      requestStatisticsAgentSetExecuteError();
      Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what(), __FILE__, __LINE__);
      handleError(err);
    } catch (std::exception const& ex) {
      requestStatisticsAgentSetExecuteError();
      Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
      handleError(err);
    } catch (...) {
      requestStatisticsAgentSetExecuteError();
      Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
      handleError(err);
    }

    finalizeExecute();

    if (status._status != HANDLER_ASYNC && _response == nullptr) {
      Exception err(TRI_ERROR_INTERNAL, "no response received from handler",
                    __FILE__, __LINE__);

      handleError(err);
    }
  } catch (Exception const& ex) {
    status = HANDLER_FAILED;
    requestStatisticsAgentSetExecuteError();
    LOG(ERR) << "caught exception: " << DIAGNOSTIC_INFORMATION(ex);
  } catch (std::exception const& ex) {
    status = HANDLER_FAILED;
    requestStatisticsAgentSetExecuteError();
    LOG(ERR) << "caught exception: " << ex.what();
  } catch (...) {
    status = HANDLER_FAILED;
    requestStatisticsAgentSetExecuteError();
    LOG(ERR) << "caught exception";
  }

  if (status._status != HANDLER_ASYNC && _response == nullptr) {
    _response = new GeneralResponse(GeneralResponse::SERVER_ERROR,
                                 GeneralRequest::MinCompatibility);
  }

  requestStatisticsAgentSetRequestEnd();

  return status;
}

//// @TODO: Check for a way to remove this method

GeneralHandler::status_t GeneralHandler::executeFullVstream() {
  GeneralHandler::status_t status(GeneralHandler::HANDLER_FAILED);

  requestStatisticsAgentSetRequestStart();

  try {
    prepareExecute();

    try {
      status = execute();
    } catch (Exception const& ex) {
      requestStatisticsAgentSetExecuteError();
      handleError(ex);
    } catch (std::bad_alloc const& ex) {
      requestStatisticsAgentSetExecuteError();
      Exception err(TRI_ERROR_OUT_OF_MEMORY, ex.what(), __FILE__, __LINE__);
      handleError(err);
    } catch (std::exception const& ex) {
      requestStatisticsAgentSetExecuteError();
      Exception err(TRI_ERROR_INTERNAL, ex.what(), __FILE__, __LINE__);
      handleError(err);
    } catch (...) {
      requestStatisticsAgentSetExecuteError();
      Exception err(TRI_ERROR_INTERNAL, __FILE__, __LINE__);
      handleError(err);
    }

    finalizeExecute();

    if (status._status != HANDLER_ASYNC && _response == nullptr) {
      Exception err(TRI_ERROR_INTERNAL, "no response received from handler",
                    __FILE__, __LINE__);

      handleError(err);
    }
  } catch (Exception const& ex) {
    status = HANDLER_FAILED;
    requestStatisticsAgentSetExecuteError();
    LOG(TRACE) << "caught exception: " << DIAGNOSTIC_INFORMATION(ex);
  } catch (std::exception const& ex) {
    status = HANDLER_FAILED;
    requestStatisticsAgentSetExecuteError();
    LOG(TRACE) << "caught exception: " << ex.what();
  } catch (...) {
    status = HANDLER_FAILED;
    requestStatisticsAgentSetExecuteError();
    LOG(TRACE) << "caught exception";
  }

  if (status._status != HANDLER_ASYNC && _response == nullptr) {
    _response = new GeneralResponse(GeneralResponse::VSTREAM_SERVER_ERROR,
                                 GeneralRequest::MinCompatibility);
  }

  requestStatisticsAgentSetRequestEnd();

  return status;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief register the server object
////////////////////////////////////////////////////////////////////////////////

void GeneralHandler::setServer(GeneralHandlerFactory* server) { _server = server; }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the request
////////////////////////////////////////////////////////////////////////////////

GeneralRequest const* GeneralHandler::getRequest() const { return _request; }

////////////////////////////////////////////////////////////////////////////////
/// @brief steal the request
////////////////////////////////////////////////////////////////////////////////

GeneralRequest* GeneralHandler::stealRequest() {
  GeneralRequest* tmp = _request;
  _request = nullptr;
  return tmp;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the response
////////////////////////////////////////////////////////////////////////////////

GeneralResponse* GeneralHandler::getResponse() const { return _response; }

////////////////////////////////////////////////////////////////////////////////
/// @brief steal the response
////////////////////////////////////////////////////////////////////////////////

GeneralResponse* GeneralHandler::stealResponse() {
  GeneralResponse* tmp = _response;
  _response = nullptr;
  return tmp;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new HTTP response
////////////////////////////////////////////////////////////////////////////////

void GeneralHandler::createResponse(GeneralResponse::HttpResponseCode code) {
  // avoid having multiple responses. this would be a memleak
  if (_response != nullptr) {
    delete _response;
    _response = nullptr;
  }

  int32_t apiCompatibility;

  if (_request != nullptr) {
    apiCompatibility = _request->compatibility();
  } else {
    apiCompatibility = GeneralRequest::MinCompatibility;
  }

  // create a "standard" (standalone) Http response
  _response = new GeneralResponse(code, apiCompatibility);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new VSTREAM response
/// @TODO: Check for a way to remove this method
////////////////////////////////////////////////////////////////////////////////

void GeneralHandler::createResponse(GeneralResponse::VstreamResponseCode code) {
  // avoid having multiple responses. this would be a memleak
  if (_response != nullptr) {
    delete _response;
    _response = nullptr;
  }

  int32_t apiCompatibility;

  if (_request != nullptr) {
    apiCompatibility = _request->compatibility();
  } else {
    apiCompatibility = GeneralRequest::MinCompatibility;
  }

  // create a "standard" (standalone) Http response
  _response = new GeneralResponse(code, apiCompatibility);
}

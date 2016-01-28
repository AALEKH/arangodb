////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
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

#include "WorkMonitorHandler.h"

#include "HttpServer/HttpHandler.h"
#include "Rest/GeneralRequest.h"

using namespace arangodb;
using namespace arangodb::basics;

using arangodb::rest::GeneralRequest;
using arangodb::rest::HttpHandler;


WorkMonitorHandler::WorkMonitorHandler(GeneralRequest* request)
    : RestBaseHandler(request) {}


bool WorkMonitorHandler::isDirect() const { return true; }


#include <iostream>

HttpHandler::status_t WorkMonitorHandler::execute() {
  WorkMonitor::requestWorkOverview(_taskId);
  return status_t(HANDLER_ASYNC);
}

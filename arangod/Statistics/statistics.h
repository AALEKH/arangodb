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

#ifndef ARANGOD_STATISTICS_STATISTICS_H
#define ARANGOD_STATISTICS_STATISTICS_H 1

#include "Basics/Common.h"
#include "Rest/GeneralRequest.h"
#include "Statistics/figures.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief request statistics
////////////////////////////////////////////////////////////////////////////////

struct TRI_request_statistics_t {
  TRI_request_statistics_t()
      : _readStart(0.0),
        _readEnd(0.0),
        _queueStart(0.0),
        _queueEnd(0.0),
        _requestStart(0.0),
        _requestEnd(0.0),
        _writeStart(0.0),
        _writeEnd(0.0),
        _receivedBytes(0.0),
        _sentBytes(0.0),
        _requestType(arangodb::rest::GeneralRequest::HTTP_REQUEST_ILLEGAL),
        _async(false),
        _tooLarge(false),
        _executeError(false),
        _ignore(false) {}

  void reset() {
    _readStart = 0.0;
    _readEnd = 0.0;
    _queueStart = 0.0;
    _queueEnd = 0.0;
    _requestStart = 0.0;
    _requestEnd = 0.0;
    _writeStart = 0.0;
    _writeEnd = 0.0;
    _receivedBytes = 0.0;
    _sentBytes = 0.0;
    _requestType = arangodb::rest::GeneralRequest::HTTP_REQUEST_ILLEGAL;
    _async = false;
    _tooLarge = false;
    _executeError = false;
    _ignore = false;
  }

  double _readStart;
  double _readEnd;
  double _queueStart;
  double _queueEnd;
  double _requestStart;
  double _requestEnd;
  double _writeStart;
  double _writeEnd;

  double _receivedBytes;
  double _sentBytes;

  arangodb::rest::GeneralRequest::RequestType _requestType;

  bool _async;
  bool _tooLarge;
  bool _executeError;
  bool _ignore;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief connection statistics
////////////////////////////////////////////////////////////////////////////////

struct TRI_connection_statistics_t {
  TRI_connection_statistics_t()
      : _connStart(0.0), _connEnd(0.0), _http(false), _error(false) {}

  void reset() {
    _connStart = 0.0;
    _connEnd = 0.0;
    _http = false;
    _error = false;
  }

  double _connStart;
  double _connEnd;

  bool _http;
  bool _error;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief global server statistics
////////////////////////////////////////////////////////////////////////////////

struct TRI_server_statistics_t {
  TRI_server_statistics_t() : _startTime(0.0), _uptime(0.0) {}

  double _startTime;
  double _uptime;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a new statistics block
////////////////////////////////////////////////////////////////////////////////

TRI_request_statistics_t* TRI_AcquireRequestStatistics(void);

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseRequestStatistics(TRI_request_statistics_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the current statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_FillRequestStatistics(
    arangodb::basics::StatisticsDistribution& totalTime,
    arangodb::basics::StatisticsDistribution& requestTime,
    arangodb::basics::StatisticsDistribution& queueTime,
    arangodb::basics::StatisticsDistribution& ioTime,
    arangodb::basics::StatisticsDistribution& bytesSent,
    arangodb::basics::StatisticsDistribution& bytesReceived);

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a new statistics block
////////////////////////////////////////////////////////////////////////////////

TRI_connection_statistics_t* TRI_AcquireConnectionStatistics(void);

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a statistics block
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseConnectionStatistics(TRI_connection_statistics_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the current statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_FillConnectionStatistics(
    arangodb::basics::StatisticsCounter& httpConnections,
    arangodb::basics::StatisticsCounter& totalRequests,
    std::vector<arangodb::basics::StatisticsCounter>& methodRequests,
    arangodb::basics::StatisticsCounter& asyncRequests,
    arangodb::basics::StatisticsDistribution& connectionTime);

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the server statistics
////////////////////////////////////////////////////////////////////////////////

TRI_server_statistics_t TRI_GetServerStatistics();

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics enabled flags
////////////////////////////////////////////////////////////////////////////////

extern bool TRI_ENABLE_STATISTICS;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of http connections
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsCounter TRI_HttpConnectionsStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief total number of requests
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsCounter TRI_TotalRequestsStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of requests by HTTP method
////////////////////////////////////////////////////////////////////////////////

extern std::vector<arangodb::basics::StatisticsCounter>
    TRI_MethodRequestsStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of async requests
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsCounter TRI_AsyncRequestsStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief connection time distribution vector
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsVector
    TRI_ConnectionTimeDistributionVectorStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief total time distribution
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsDistribution*
    TRI_ConnectionTimeDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief request time distribution vector
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsVector
    TRI_RequestTimeDistributionVectorStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief total time distribution
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsDistribution*
    TRI_TotalTimeDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief request time distribution
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsDistribution*
    TRI_RequestTimeDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief queue time distribution
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsDistribution*
    TRI_QueueTimeDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief i/o distribution
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsDistribution*
    TRI_IoTimeDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief bytes sent distribution vector
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsVector
    TRI_BytesSentDistributionVectorStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief bytes sent distribution
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsDistribution*
    TRI_BytesSentDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief bytes received distribution vector
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsVector
    TRI_BytesReceivedDistributionVectorStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief bytes received distribution
////////////////////////////////////////////////////////////////////////////////

extern arangodb::basics::StatisticsDistribution*
    TRI_BytesReceivedDistributionStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief global server statistics
////////////////////////////////////////////////////////////////////////////////

extern TRI_server_statistics_t TRI_ServerStatistics;

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the current wallclock time
////////////////////////////////////////////////////////////////////////////////

double TRI_StatisticsTime(void);

////////////////////////////////////////////////////////////////////////////////
/// @brief module init function
////////////////////////////////////////////////////////////////////////////////

void TRI_InitializeStatistics(void);

////////////////////////////////////////////////////////////////////////////////
/// @brief shut down statistics
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownStatistics(void);

#endif

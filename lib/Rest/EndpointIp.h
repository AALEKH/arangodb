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
/// @author Dr. Oreste Costa-Panaia
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_REST_ENDPOINT_IP_H
#define LIB_REST_ENDPOINT_IP_H 1

#include "Basics/Common.h"

#include "Rest/Endpoint.h"

namespace arangodb {
namespace rest {

class EndpointIp : public Endpoint {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates an endpoint
  //////////////////////////////////////////////////////////////////////////////

 protected:
  EndpointIp(const EndpointType, const DomainType, const EncryptionType,
             std::string const&, int, bool, std::string const&, uint16_t const);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroys an endpoint
  //////////////////////////////////////////////////////////////////////////////

 public:
  ~EndpointIp();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief default port number if none specified
  //////////////////////////////////////////////////////////////////////////////

  static uint16_t const _defaultPort; // Http

  static uint16_t const _defaultPortVstream; // Vstream

  //////////////////////////////////////////////////////////////////////////////
  /// @brief default host if none specified
  //////////////////////////////////////////////////////////////////////////////

  static std::string const _defaultHost;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief connect the socket
  //////////////////////////////////////////////////////////////////////////////

  TRI_socket_t connectSocket(const struct addrinfo*, double, double);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief connect the endpoint
  //////////////////////////////////////////////////////////////////////////////

  TRI_socket_t connect(double, double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief disconnect the endpoint
  //////////////////////////////////////////////////////////////////////////////

  virtual void disconnect();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief init an incoming connection
  //////////////////////////////////////////////////////////////////////////////

  virtual bool initIncoming(TRI_socket_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get port
  //////////////////////////////////////////////////////////////////////////////

  int getPort() const { return _port; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get host
  //////////////////////////////////////////////////////////////////////////////

  std::string getHost() const { return _host; }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief host name / address (IPv4 or IPv6)
  //////////////////////////////////////////////////////////////////////////////

  std::string const _host;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief port number
  //////////////////////////////////////////////////////////////////////////////

  uint16_t _port;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not to reuse the address
  //////////////////////////////////////////////////////////////////////////////

  bool _reuseAddress;
};
}
}

#endif

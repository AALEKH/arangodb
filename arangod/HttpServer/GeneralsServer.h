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
/// @author Aalekh Nigam
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_HTTP_SERVER_GENERALS_SERVER_H
#define ARANGOD_HTTP_SERVER_GENERALS_SERVER_H 1

#include "HttpServer/GeneralServer.h"

#include <openssl/ssl.h>


namespace arangodb {
namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief general(http/velocy) server
////////////////////////////////////////////////////////////////////////////////

class GeneralsServer : public GeneralServer {
  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructs a new general(https/velocys) server
  //////////////////////////////////////////////////////////////////////////////

  GeneralsServer(Scheduler*, Dispatcher*, GeneralHandlerFactory*, AsyncJobManager*,
              double keepAliveTimeout, SSL_CTX*);


  ~GeneralsServer();

  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the verification mode
  //////////////////////////////////////////////////////////////////////////////

  void setVerificationMode(int mode);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the verification callback
  //////////////////////////////////////////////////////////////////////////////

  void setVerificationCallback(int (*func)(int, X509_STORE_CTX*));

  
 public:
  // char const* protocol() const override { return "https"; }
  virtual char const* protocol() const = 0;


  Endpoint::EncryptionType encryptionType() const override {
    return Endpoint::ENCRYPTION_SSL;
  }


  virtual ArangoTask* createCommTask(TRI_socket_t, const ConnectionInfo&) = 0;

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief ssl context
  //////////////////////////////////////////////////////////////////////////////

  SSL_CTX* _ctx;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief verification mode
  //////////////////////////////////////////////////////////////////////////////

  int _verificationMode;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief verification callback
  //////////////////////////////////////////////////////////////////////////////

  int (*_verificationCallback)(int, X509_STORE_CTX*);
};
}
}

#endif
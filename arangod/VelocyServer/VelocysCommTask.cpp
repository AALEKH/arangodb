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
/// @author Aalekh Nigam
////////////////////////////////////////////////////////////////////////////////

#include "VelocysCommTask.h"

#include <openssl/err.h>
#include "Basics/Logger.h"
#include "Basics/socket-utils.h"
#include "Basics/ssl-helper.h"
#include "Basics/StringBuffer.h"
#include "VelocyServer/VelocysServer.h"
#include "Scheduler/Scheduler.h"

using namespace arangodb::rest;


////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task with a given socket
////////////////////////////////////////////////////////////////////////////////

VelocysCommTask::VelocysCommTask(VelocysServer* server, TRI_socket_t socket,
                             ConnectionInfo const& info,
                             double keepAliveTimeout, SSL_CTX* ctx,
                             int verificationMode,
                             int (*verificationCallback)(int, X509_STORE_CTX*))
    : Task("HttpsCommTask"),
      VelocyCommTask(server, socket, info, keepAliveTimeout),
      _accepted(false),
      _readBlockedOnWrite(false),
      _writeBlockedOnRead(false),
      _tmpReadBuffer(nullptr),
      _ssl(nullptr),
      _ctx(ctx),
      _verificationMode(verificationMode),
      _verificationCallback(verificationCallback) {
  _tmpReadBuffer = new char[READ_BLOCK_SIZE];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a task
////////////////////////////////////////////////////////////////////////////////

VelocysCommTask::~VelocysCommTask() {
  shutdownSsl(true);

  delete[] _tmpReadBuffer;
}

bool VelocysCommTask::setup(Scheduler* scheduler, EventLoop loop) {
  // setup base class
  bool ok = VelocyCommTask::setup(scheduler, loop);

  if (!ok) {
    return false;
  }

  // build a new connection
  TRI_ASSERT(_ssl == nullptr);

  ERR_clear_error();
  _ssl = SSL_new(_ctx);

  _connectionInfo.sslContext = _ssl;

  if (_ssl == nullptr) {
    LOG(DEBUG) << "cannot build new SSL connection: " << arangodb::basics::lastSSLError().c_str();

    shutdownSsl(false);
    return false;  // terminate ourselves, ssl is nullptr
  }

  // enforce verification
  ERR_clear_error();
  SSL_set_verify(_ssl, _verificationMode, _verificationCallback);

  // with the file descriptor
  ERR_clear_error();
  SSL_set_fd(_ssl, (int)TRI_get_fd_or_handle_of_socket(_commSocket));

  // accept might need writes
  _scheduler->startSocketEvents(_writeWatcher);

  return true;
}

bool VelocysCommTask::handleEvent(EventToken token, EventType revents) {
  // try to accept the SSL connection
  if (!_accepted) {
    bool result = false;  // be pessimistic

    if ((token == _readWatcher && (revents & EVENT_SOCKET_READ)) ||
        (token == _writeWatcher && (revents & EVENT_SOCKET_WRITE))) {
      // must do the SSL handshake first
      result = trySSLAccept();
    }

    if (!result) {
      // status is somehow invalid. we got here even though no accept was ever
      // successful
      _clientClosed = true;
      // this will remove the corresponding chunkedTask from the global list
      // if we would leave it in there, then the server may crash on shutdown
      _server->handleCommunicationFailure(this);

      _scheduler->destroyTask(this);
    }

    return result;
  }

  // if we blocked on write, read can be called when the socket is writeable
  if (_readBlockedOnWrite && token == _writeWatcher &&
      (revents & EVENT_SOCKET_WRITE)) {
    _readBlockedOnWrite = false;
    revents &= ~EVENT_SOCKET_WRITE;
    revents |= EVENT_SOCKET_READ;
    token = _readWatcher;
  }

  // handle normal socket operation
  bool result = VelocyCommTask::handleEvent(token, revents);

  // warning: if _clientClosed is true here, the task (this) is already deleted!

  // we might need to start listing for writes (even we only want to READ)
  if (result && !_clientClosed) {
    if (_readBlockedOnWrite || _writeBlockedOnRead) {
      _scheduler->startSocketEvents(_writeWatcher);
    }
  }

  return result;
}

bool VelocysCommTask::fillVelocyStream() {
  if (nullptr == _ssl) {
    _clientClosed = true;
    return false;
  }

  // is the handshake already done?
  if (!_accepted) {
    return false;
  }

  return trySSLRead();
}

bool VelocysCommTask::handleWrite() {
  if (nullptr == _ssl) {
    _clientClosed = true;
    return false;
  }

  // is the handshake already done?
  if (!_accepted) {
    return false;
  }

  return trySSLWrite();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief accepts SSL connection
////////////////////////////////////////////////////////////////////////////////

bool VelocysCommTask::trySSLAccept() {
  if (nullptr == _ssl) {
    _clientClosed = true;
    return false;
  }

  ERR_clear_error();
  int res = SSL_accept(_ssl);

  // accept successful
  if (res == 1) {
    LOG(DEBUG) << "established SSL connection";
    _accepted = true;

    // accept done, remove write events
    _scheduler->stopSocketEvents(_writeWatcher);

    return true;
  }

  // shutdown of connection
  if (res == 0) {
    LOG(DEBUG) << "SSL_accept failed: " << arangodb::basics::lastSSLError().c_str();

    shutdownSsl(false);
    return false;
  }

  // maybe we need more data
  int err = SSL_get_error(_ssl, res);

  if (err == SSL_ERROR_WANT_READ) {
    return true;
  } else if (err == SSL_ERROR_WANT_WRITE) {
    return true;
  }

  LOG(TRACE) << "error in SSL handshake: " << arangodb::basics::lastSSLError().c_str();

  shutdownSsl(false);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads from SSL connection
////////////////////////////////////////////////////////////////////////////////

bool VelocysCommTask::trySSLRead() {
  _readBlockedOnWrite = false;

again:
  ERR_clear_error();
  int nr = SSL_read(_ssl, _tmpReadBuffer, READ_BLOCK_SIZE);

  if (nr <= 0) {
    int res = SSL_get_error(_ssl, nr);

    switch (res) {
      case SSL_ERROR_NONE:
        return true;

      case SSL_ERROR_SSL:
        LOG(DEBUG) << "received SSL error (bytes read " << (int)TRI_get_fd_or_handle_of_socket(_commSocket) <<", socket " << nr <<"): " << arangodb::basics::lastSSLError().c_str();

        shutdownSsl(false);
        return false;

      case SSL_ERROR_ZERO_RETURN:
        shutdownSsl(true);
        _clientClosed = true;
        return false;

      case SSL_ERROR_WANT_READ:
        // we must retry with the EXACT same parameters later
        return true;

      case SSL_ERROR_WANT_WRITE:
        _readBlockedOnWrite = true;
        return true;

      case SSL_ERROR_WANT_CONNECT:
        LOG(DEBUG) << "received SSL_ERROR_WANT_CONNECT";
        break;

      case SSL_ERROR_WANT_ACCEPT:
        LOG(DEBUG) << "received SSL_ERROR_WANT_ACCEPT";
        break;

      case SSL_ERROR_SYSCALL:
        if (res != 0) {
          LOG(DEBUG) << "SSL_read returned syscall error with: " << arangodb::basics::lastSSLError().c_str();
        } else if (nr == 0) {
          LOG(DEBUG) << "SSL_read returned syscall error because an EOF was received";
        } else {
          LOG(DEBUG) << "SSL_read return syscall error: " << (int)errno<< ": " << strerror(errno);
        }

        shutdownSsl(false);
        return false;

      default:
        LOG(DEBUG) << "received error with "<< res <<" and " << nr<<" : " << arangodb::basics::lastSSLError().c_str();

        shutdownSsl(false);
        return false;
    }
  } else {
    _readBuffer->appendText(_tmpReadBuffer, nr);

    // we might have more data to read
    // if we do not iterate again, the reading process would stop
    goto again;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes from SSL connection
////////////////////////////////////////////////////////////////////////////////

bool VelocysCommTask::trySSLWrite() {
  _writeBlockedOnRead = false;

  size_t len = 0;

  if (nullptr != _writeBuffer) {
    TRI_ASSERT(_writeBuffer->length() >= _writeLength);

    // size_t is unsigned, should never get < 0
    len = _writeBuffer->length() - _writeLength;
  }

  // write buffer to SSL connection
  int nr = 0;

  if (0 < len) {
    ERR_clear_error();

    nr = SSL_write(_ssl, _writeBuffer->begin() + _writeLength, (int)len);

    if (nr <= 0) {
      int res = SSL_get_error(_ssl, nr);

      switch (res) {
        case SSL_ERROR_NONE:
          return true;

        case SSL_ERROR_ZERO_RETURN:
          shutdownSsl(true);
          _clientClosed = true;
          return false;

        case SSL_ERROR_WANT_CONNECT:
          LOG(DEBUG) << "received SSL_ERROR_WANT_CONNECT";
          break;

        case SSL_ERROR_WANT_ACCEPT:
          LOG(DEBUG) << "received SSL_ERROR_WANT_ACCEPT";
          break;

        case SSL_ERROR_WANT_WRITE:
          // we must retry with the EXACT same parameters later
          return true;

        case SSL_ERROR_WANT_READ:
          _writeBlockedOnRead = true;
          return true;

        case SSL_ERROR_SYSCALL:
          if (res != 0) {
            LOG(DEBUG) << "SSL_write returned syscall error with: " << arangodb::basics::lastSSLError().c_str();
          } else if (nr == 0) {
            LOG(DEBUG) << "SSL_write returned syscall error because an EOF was received";
          } else {
            LOG(DEBUG) << "SSL_write return syscall error: "<< errno <<": " << strerror(errno);
          }

          shutdownSsl(false);
          return false;

        default:
          LOG(DEBUG) << "received error with " << res <<" and "<< nr<<": " << arangodb::basics::lastSSLError().c_str();

          shutdownSsl(false);
          return false;
      }
    } else {
      len -= nr;
    }
  }

  if (len == 0) {
    delete _writeBuffer;
    _writeBuffer = nullptr;

    completedWriteBuffer();
  } else if (nr > 0) {
    // nr might have been negative here
    _writeLength += nr;
  }

  // return immediately, everything is closed down
  if (_clientClosed) {
    return false;
  }

  // we might have a new write buffer
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the SSL connection
////////////////////////////////////////////////////////////////////////////////

void VelocysCommTask::shutdownSsl(bool initShutdown) {
  static int const SHUTDOWN_ITERATIONS = 10;

  if (nullptr != _ssl) {
    if (initShutdown) {
      bool ok = false;

      for (int i = 0; i < SHUTDOWN_ITERATIONS; ++i) {
        ERR_clear_error();
        int res = SSL_shutdown(_ssl);

        if (res == 1) {
          ok = true;
          break;
        }

        if (res == -1) {
          int err = SSL_get_error(_ssl, res);

          if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
            LOG(DEBUG) << "received shutdown error with "<< res <<", "<< err <<": " << arangodb::basics::lastSSLError().c_str();
            break;
          }
        }
      }

      if (!ok) {
        LOG(DEBUG) << "cannot complete SSL shutdown in socket " << (int)TRI_get_fd_or_handle_of_socket(_commSocket);
      }
    } else {
      ERR_clear_error();
      SSL_clear(_ssl);
    }

    ERR_clear_error();
    SSL_free(_ssl);  // this will free bio as well

    _ssl = nullptr;
  }

  if (TRI_isvalidsocket(_commSocket)) {
    TRI_CLOSE_SOCKET(_commSocket);
    TRI_invalidatesocket(&_commSocket);
  }
}
#ifndef _protoo_WebSocketTransport_h_
#define _protoo_WebSocketTransport_h_

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <string>
#include "json.hpp"
#include "log.h"
#include "request_id.h"

using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;
using std::string;
namespace websocket = boost::beast::websocket;

namespace protoo {

class WebSocketTransport
 : public std::enable_shared_from_this<WebSocketTransport>
 {
  public:
  class TransportListener {
    public:
    ~TransportListener() {
      log("~TransportListener");
    }
    virtual void onTransportError(string error) = 0;
    virtual void onTransportConnected() = 0;
    virtual void onTransportClose() = 0;
    virtual void onNotification(json notification) = 0;
    virtual void handleRequest(json request) = 0;
  };

  private:
  string host;
  string port;
  string path;

  std::shared_ptr<TransportListener> listener;
  websocket::stream<tcp::socket> ws;
  tcp::resolver resolver;
  boost::beast::multi_buffer buffer; // used to read incoming websocket messages

  std::map<int, std::function<void(json)>> handlers;

  std::queue<json> writes;
  bool isWriting = false;

  public:
  WebSocketTransport(
    boost::asio::io_context &ioc,
    std::shared_ptr<TransportListener> listener,
    string host,
    string port,
    string path
  ):
      host(host),
      port(port),
      path(path),
      listener(listener),
      ws(ioc),
      resolver(ioc) {
  }

  void connect() {
    resolver.async_resolve(
      host,
      port,
      std::bind(
        &WebSocketTransport::onResolve,
        shared_from_this(),
        std::placeholders::_1,
        std::placeholders::_2
    ));
  }

  void send(json payload) {
    writes.push(payload);
    if (!isWriting) {
      doWrite();
    }
  }

  void request(string method, json data, std::function<void(json)> onResponse) {
      int requestId = make_request_id();
      json req = {
        {"request", true},
        {"id", requestId},
        {"method", method},
        {"data", data}
      };

      handlers.emplace(requestId, onResponse);
      send(req);
    }

  void close() {
    ws.close(websocket::close_code::normal);
  }

  private:
  void doWrite() {
    isWriting = true;
    auto payload = writes.front();
    writes.pop();
    auto output = payload.dump();
    // log("Sending message " + output);
    ws.async_write(boost::asio::buffer(output), std::bind(
      &WebSocketTransport::onWriteDone,
      shared_from_this(),
      std::placeholders::_1,
      std::placeholders::_2
    ));
  }
  void onWriteDone(boost::system::error_code ec,
      std::size_t bytes_transferred) {
    isWriting = false;
    // If there are more elements in the write queue, continue with the next one.
    if (!writes.empty()) {
      doWrite();
    }
  }

  void onResolve(
      boost::system::error_code ec,
      tcp::resolver::results_type results) {
    if(ec) {
      auto error = "Could not resolve host address";
      logError(error);
      listener->onTransportError(error);
    } else {
      boost::asio::async_connect(
        ws.next_layer(),
        results.begin(),
        results.end(),
        std::bind(
            &WebSocketTransport::onConnect,
            shared_from_this(),
            std::placeholders::_1));
    }
  }

  void onConnect(boost::system::error_code ec) {
    if(ec) {
      auto error = "Could not connect to the host";
      logError(error);
      listener->onTransportError(error);
    } else {
      ws.async_handshake_ex(
        host,
        path,
        [](websocket::request_type& m) {
          m.insert(boost::beast::http::field::sec_websocket_protocol, "protoo");
        },
        std::bind(
            &WebSocketTransport::onHandshake,
            shared_from_this(),
            std::placeholders::_1));
    }
  }

  void onHandshake(boost::system::error_code ec) {
    if(ec) {
      auto error = "Could not perform WebSocket handshake";
      logError(error);
      listener->onTransportError(error);
    } else {
      listener->onTransportConnected();
      readMessage();
    }
  }

  void readMessage() {
    // Clear the buffer
    buffer.consume(buffer.size());

    ws.async_read(
      buffer,
      std::bind(
        &WebSocketTransport::onReadMessage,
        shared_from_this(),
        std::placeholders::_1,
        std::placeholders::_2
      )
    );
  }

  void onReadMessage(
    boost::system::error_code ec,
    std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if(ec == websocket::error::closed) {
      listener->onTransportClose();
      return;
    }

    if (ec) {
        logError("ERROR: could not receive message! " + ec.message());
        return;
    }

    std::stringstream ss;
    ss << boost::beast::buffers(buffer.data());

    // Clear the buffer
    buffer.consume(buffer.size());

    auto str = ss.str();

    // log("Message: " + str);

    // Read the next message
    readMessage();

    json parsed;
    try {
      parsed = json::parse(str);
    } catch (json::type_error& e) {
      logError("Could not parse JSON: " + str);
      throw e;
    }
    handleMessage(parsed);
  }

  void handleMessage(json parsed) {
    bool isRequest = parsed.count("request") > 0 && parsed.at("request").get<bool>();
    bool isResponse = !isRequest && parsed.count("response") > 0 && parsed.at("response").get<bool>();
    if (isRequest) {
      listener->handleRequest(parsed);
    } else if (isResponse) {
      int responseId = parsed.at("id").get<int>();
      auto search = handlers.find(responseId);
      if (search != handlers.end()) {
        std::function<void(json)> handler = handlers.find(responseId)->second;
        handlers.erase(responseId);
        handler(parsed);
      } else {
        logError("No handler found for response " + std::to_string(responseId));
      }
    } else {
      listener->onNotification(parsed);
    }
  }
};
}
#endif

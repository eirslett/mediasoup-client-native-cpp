#ifndef _VoiceChannel_h_
#define _VoiceChannel_h_

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>

#include "webrtc/rtc_base/ssladapter.h"

#include "Handler.h"
#include "WebSocketTransport.h"
#include "json.hpp"
#include "log.h"
#include "VoiceChannelData.h"
#include "request_id.h"

#include <string>

using json = nlohmann::json;
namespace ssl = boost::asio::ssl;

class VoiceChannel
  : public protoo::WebSocketTransport::TransportListener,
    public std::enable_shared_from_this<VoiceChannel>,
    public Handler::HandlerListener
    {

  string peerName;
  boost::asio::io_context &ioc;
  std::shared_ptr<protoo::WebSocketTransport> transport = nullptr;
  rtc::scoped_refptr<Handler> handler = nullptr;
  int sendTransportId;
  int receiveTransportId;

  public:
  VoiceChannel(
    boost::asio::io_context &ioc,
    string const host,
    string const port,
    string const roomId,
    string const peerName
  ): peerName(peerName), ioc(ioc) {
    log("Init SSL");
    // for WebRTC
    rtc::InitializeSSL();
    // for Boost
    ssl::context ctx{ssl::context::sslv23_client};
    log("SSL ok");
    auto path = "/?peerName=" + peerName + "&roomId=" + roomId;

    // cannot use shared_from_this() because we're in the constructor
    auto shared = std::shared_ptr<VoiceChannel>(this);
    transport = std::make_shared<protoo::WebSocketTransport>(
      ioc,
      ctx,
      shared,
      host,
      port,
      path
    );

    // auto pcFactory = webrtc::CreatePeerConnectionFactory();

    this->handler = new rtc::RefCountedObject<Handler>(shared_from_this(), transport);
  }

  void joinRoom() {
    log("Join room");
    this->transport->connect();
  }

  void onTransportError(string error) override {
    logError("Transport error: " + error);
  }
  void onTransportConnected() override {
    log("Connected!");
    this->transport->request("mediasoup-request", {
      {"method", "queryRoom"},
      {"target", "room"}
    }, std::bind(&VoiceChannel::initWebRTC, shared_from_this(), std::placeholders::_1));

  }
  void onTransportClose() override {
    log("Transport closed!");
  }
  void onNotification(json const notification) override {
    log("On notification " + notification.dump());
  }
  void handleRequest(json const request) override {
    auto requestId = request.at("id").get<int>();

    string method;
    if (request.count("method") > 0) {
      method = request.at("method").get<string>();
    }

    string dataMethod;
    if (request.count("data") > 0 && request.at("data").count("method") > 0) {
      dataMethod = request.at("data").at("method").get<string>();
    }

    if (dataMethod == "newPeer") {
      handlePeer(request.at("data"));
      respondOK(requestId);
    } else if (dataMethod == "newConsumer") {
      addConsumer(request.at("data"));
      respondOK(requestId);
    } else if (dataMethod == "peerClosed") {
      log("Peer left: " + request.at("data").at("name").get<string>());
      respondOK(requestId);
    } else if (dataMethod == "consumerPreferredProfileSet") {
      log("Consumer set preferred profile on server - ignore");
      respondOK(requestId);
    } else if (method == "active-speaker") {
      string activeSpeaker;
      if (request.at("data").count("peerName") > 0 && request.at("data").at("peerName").is_string()) {
        activeSpeaker = request.at("data").at("peerName").get<string>();
      }
      log("Active speaker: " + activeSpeaker);
      respondOK(requestId);
    } else {
      log("Could not understand the request: " + request.dump());
      this->transport->send({
        {"response", true},
        {"id", requestId},
        {"codeCode", 400},
        {"errorReason", "Could not understand the request"}
      });
    }
  }
  void respondOK(int requestId) {
    transport->send({
      {"response", true},
      {"id",       requestId},
      {"ok",       true}
    });
  }

  void initWebRTC(json const roomSettings) {
    log("Init WebRTC here! Got room settings: " + roomSettings.at("data").dump());
    handler->roomSettings = roomSettings.at("data");
    handler->initWebRTC();
  }

  void onRtpCapabilities(json const nativeCapabilities) override {

    this->transport->request("mediasoup-request", {
      {"appData", {
        {"device", {
          {"flag", "mediasoup-client-cpp"},
          {"name", "mediasoup-client-cpp"},
          {"version", "1.0"}
        }},
        {"displayName", peerName}
      }},
      {"method", "join"},
      {"target", "room"},
      {"peerName", peerName},
      {"rtpCapabilities", nativeCapabilities}
    }, std::bind(&VoiceChannel::onRoomJoin, shared_from_this(), std::placeholders::_1));
  };

  void onRoomJoin(json const response) {
    // ok, let's assume that we joined the room
    bool isOk = response.count("ok") > 0 && response.at("ok").get<bool>();
    if (!isOk) {
      logError("Could not join room: " + response.dump());
      return;
    }

    handler->roomSettings = response.at("data");

    auto peers = response.at("data").at("peers");

    log("Room join OK!");

    this->receiveTransportId = randomNumber();
    this->transport->request("mediasoup-request", {
      {"appData", {
        {"media", "RECV"}
      }},
      {"id", receiveTransportId},
      {"direction", "recv"},
      {"method",  "createTransport"},
      {"options", {
        {"tcp", false}
      }},
      {"target",  "peer"},
    }, std::bind(&VoiceChannel::onReceiveTransportCreated, shared_from_this(), peers, std::placeholders::_1));
  }

  void onReceiveTransportCreated(json peers, json response) {
    log("----> Receive transport!");
    auto remoteTransportSdp = response.at("data");
    handler->remoteReceiveTransportSdp = remoteTransportSdp;

    for(auto const& peer: peers) {
      log("Handle peer " + peer.dump());
      handlePeer(peer);
    }

    handler->addProducers();

    /*
    this->sendTransportId = randomNumber();
    handler->sendTransportId = this->sendTransportId;
    this->transport->request("mediasoup-request", {
      {"appData", {
                    {"media", "SEND_MIC"}
                  }},
      {"id", sendTransportId},
      {"direction", "send"},
      {"method",  "createTransport"},
      {"options", {
                    {"tcp", false}
                  }},
      {"target",  "peer"},
    }, std::bind(&VoiceChannel::onSendTransportCreated, shared_from_this(), std::placeholders::_1));
     */
  }

  void onSendTransportCreated(json response) {
    log("----> Send transport!");
    log("Send transport created: " + response.dump());
    auto remoteTransportSdp = response.at("data");
    handler->remoteSendTransportSdp = remoteTransportSdp;
  }

  void handlePeer(json const peer) {
    auto consumers = peer.at("consumers");
    for(auto const& consumer: consumers) {
      addConsumer(consumer);
    }
  }

  void addConsumer(json const consumer) {
    handler->addConsumer(consumer);
  }
};

VoiceChannel* makeVoiceChannelAndJoin () {
  boost::asio::io_context ioc;
  auto vc = std::make_shared<VoiceChannel>(
    ioc,
    "localhost", "3480",
    // "ENGM_TWR",
    "g5loxak4",
    "N922MB");

  // vc->initWebRTC(nullptr);
  vc->joinRoom();
  ioc.run();
  return vc.get();
}

#endif

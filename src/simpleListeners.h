#ifndef _SimpleSetSessionDescriptionListener_h_
#define _SimpleSetSessionDescriptionListener_h_

#include <webrtc/api/jsep.h>
#include <functional>
#include <future>
#include "log.h"

class SimpleCreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
private:
    std::string handlerName;
    std::function<void(webrtc::SessionDescriptionInterface* desc)> onSuccess;
public:
    explicit SimpleCreateSessionDescriptionObserver(
      std::string handlerName,
      std::function<void(webrtc::SessionDescriptionInterface*)> onSuccess
    ):
      handlerName(handlerName),
      onSuccess(onSuccess) {}

    // CreateSessionDescriptionObserver implementation.
    void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
      onSuccess(desc);
    };

    void OnFailure(const std::string& error) override {
      logError("Failed to create SDP [" + handlerName + "]: " + error);
    };
};

class SimpleSetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
private:
    std::string handlerName;
    std::function<void()> onSuccess;

public:
    explicit SimpleSetSessionDescriptionObserver(
      std::string handlerName,
      std::function<void()> onSuccess
    ):
      handlerName(handlerName),
      onSuccess(onSuccess) {}
    void OnSuccess() override {
      log("Set SDP success [" + handlerName + "]!");
      onSuccess();
    };
    void OnFailure(const std::string& error) override {
      logError("Set SDP failed [" + handlerName + "]: " + error);
    };
};


webrtc::SessionDescriptionInterface* getOfferSync(webrtc::PeerConnectionInterface* connection, string handlerName) {
  webrtc::SessionDescriptionInterface* p;
  std::promise<void> waiting;
  connection->CreateOffer(new rtc::RefCountedObject<SimpleCreateSessionDescriptionObserver>(handlerName, [&](webrtc::SessionDescriptionInterface* desc) {
      p = desc;
  }), nullptr);
  waiting.get_future().get();
  return p;
}


#endif

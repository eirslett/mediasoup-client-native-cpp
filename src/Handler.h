#ifndef _ReceiveHandler_h_
#define _ReceiveHandler_h_

#include <webrtc/api/audio_codecs/builtin_audio_encoder_factory.h>
#include <webrtc/api/audio_codecs/builtin_audio_decoder_factory.h>
#include <webrtc/api/audio_codecs/audio_encoder_factory.h>
#include <webrtc/api/peerconnectioninterface.h>
#include <webrtc/api/mediastreaminterface.h>
#include <webrtc/api/test/fakeconstraints.h>

#include <functional>
#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include "WebSocketTransport.h"
#include "VoiceChannelData.h"
#include "simpleListeners.h"
#include "sdpUtils.h"
#include "RemotePlanBSdp.h"
#include "WorkQueue.h"
#include "log.h"

#include "webrtc/media/engine/webrtcvideocapturerfactory.h"
#include "webrtc/modules/video_capture/video_capture_factory.h"

using std::string;

std::unique_ptr<cricket::VideoCapturer> OpenVideoCaptureDevice() {
  std::vector<std::string> device_names;
  {
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!info) {
      return nullptr;
    }
    int num_devices = info->NumberOfDevices();
    for (int i = 0; i < num_devices; ++i) {
      const uint32_t kSize = 256;
      char name[kSize] = {0};
      char id[kSize] = {0};
      if (info->GetDeviceName(i, name, kSize, id, kSize) != -1) {
        device_names.push_back(name);
      }
    }
  }

  cricket::WebRtcVideoDeviceCapturerFactory factory;
  std::unique_ptr<cricket::VideoCapturer> capturer;

  // This little loop is for debugging
  for (const auto& name : device_names) {
    log("// Found video device: " + name);
  }

  for (const auto& name : device_names) {
    capturer = factory.Create(cricket::Device(name, 0));
    if (capturer) {
      break;
    }
  }
  return capturer;
}

class Handler
    : public rtc::RefCountInterface
    // public webrtc::AudioTrackSinkInterface
  {

  class SimplePeerObserver : public webrtc::PeerConnectionObserver, public rtc::RefCountInterface {
    private:
    string name;
    Handler* handler;
    public:
    SimplePeerObserver(string name, Handler* handler): name(name), handler(handler) {}

    // PeerConnectionObserver implementation.
    void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {
      log(printName() + " OnSignalingChange");
    };
    void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {
      log(printName() + " OnAddStream, " + std::to_string(stream->GetAudioTracks().size()) + " audio tracks, " + std::to_string(stream->GetVideoTracks().size()) + " video tracks");
      for(auto const& audioTrack: stream->GetAudioTracks()) {
        log(printName() + " Got an audio track: " + audioTrack->id());
        // handler->addProducer("audio", audioTrack->id());
        /*
        handler->addConsumer({
                               {"kind", "audio"},
                               {"id", audioTrack->id()}
                             });
        */
      }
      for(auto const& videoTrack: stream->GetVideoTracks()) {
        log(printName() + " Got a video track: " + videoTrack->id());
        // handler->addProducer("video", videoTrack->id());
        /*
        handler->addConsumer({
                               {"kind", "video"},
                               {"id", videoTrack->id()}
                             });
        */
      }
    };
    void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {
      log(printName() + " OnRemoveStream");
    };
    void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {
      log(printName() + " OnDataChannel");
    }
    void OnRenegotiationNeeded() override {
      log(printName() + " OnRenegotiationNeeded");
    }
    void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
      log(printName() + " OnIceConnectionChange");
    };
    void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
      log(printName() + " OnIceGatheringChange");
    };
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
      log(printName() + " OnIceCandidate");
    };
    void OnIceConnectionReceivingChange(bool receiving) override {
      // log(printName() + " OnIceConnectionReceivingChange: " + std::to_string(receiving));
    }

    string printName() {
      return "[" + name + "]";
    }
  };

  public:
  class HandlerListener {
  public:
    virtual void onRtpCapabilities(json nativeRtpCapabilities) = 0;
  };

  RoomSettings roomSettings;
  std::map<int, ConsumerInfo> consumers;
  json remoteReceiveTransportSdp;
  json remoteSendTransportSdp;
  int sendTransportId;
  int receiveTransportId;
  int sendTransportVersion = 0;
  int receiveTransportVersion = 0;

  WorkQueue workQueue;

  protected:
  std::shared_ptr<HandlerListener> listener;
  std::shared_ptr<protoo::WebSocketTransport> transport;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory = nullptr;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> receivePeerConnection;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> sendPeerConnection;
  rtc::scoped_refptr<SimplePeerObserver> receiveConnectionListener;
  rtc::scoped_refptr<SimplePeerObserver> sendConnectionListener;

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track = nullptr;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track = nullptr;

  private:
  /*
  rtc::Thread* worker_thread_ = nullptr;
  rtc::Thread* signaling_thread_ = nullptr;
  */

  public:
  std::string initialSendOfferSdp;
  std::string initialReceiveOfferSdp;

    Handler(
      // rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory,
      std::shared_ptr<HandlerListener> listener,
      std::shared_ptr<protoo::WebSocketTransport> transport
    ):
      // peerConnectionFactory(peerConnectionFactory),
      listener(listener),
      transport(transport)
    {
      receiveTransportId = randomNumber();

    /*
    auto owned_worker_thread_ = rtc::Thread::Create();
    owned_worker_thread_->Start();
    worker_thread_ = owned_worker_thread_.get();

    auto owned_signaling_thread_ = rtc::Thread::Create();
    owned_signaling_thread_->Start();
    signaling_thread_ = owned_signaling_thread_.get();

    peerConnectionFactory = webrtc::CreatePeerConnectionFactory(
      worker_thread_,
      signaling_thread_,
      nullptr,
      nullptr,
      nullptr,
      nullptr
            // webrtc::CreateBuiltinAudioEncoderFactory(),
            // webrtc::CreateBuiltinAudioDecoderFactory()
    );
     */
    peerConnectionFactory = webrtc::CreatePeerConnectionFactory();
  }
  ~Handler() {
    logError("Destroying the Handler! Much sadness :-(");
  }

  void initWebRTC() {
    log("makeOffer");
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    /*
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = "stun:stun.l.google.com:19302";
    config.servers.push_back(server);
    */

    /* This was just a test in mediasoup-client (JS client)
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = "turn:worker2.versatica.com:3478?transport=udp";
    server.username = "testuser1";
    server.password = "testpasswd1";
    config.servers.push_back(server);
     */

    webrtc::FakeConstraints constraints;

    constraints.SetMandatoryReceiveAudio(true);
    constraints.SetMandatoryReceiveVideo(true);

    config.type = webrtc::PeerConnectionInterface::IceTransportsType::kAll;
    config.bundle_policy = webrtc::PeerConnectionInterface::BundlePolicy::kBundlePolicyMaxBundle;
    config.rtcp_mux_policy = webrtc::PeerConnectionInterface::RtcpMuxPolicy::kRtcpMuxPolicyRequire;

    sendConnectionListener = new rtc::RefCountedObject<SimplePeerObserver>("send", this);
    receiveConnectionListener = new rtc::RefCountedObject<SimplePeerObserver>("receive", this);

    receivePeerConnection = peerConnectionFactory->CreatePeerConnection(
        config,
        &constraints,
        nullptr,
        nullptr,
        sendConnectionListener
    );

    sendPeerConnection = peerConnectionFactory->CreatePeerConnection(
      config,
      &constraints,
      nullptr,
      nullptr,
      receiveConnectionListener
    );

    audio_track = peerConnectionFactory->CreateAudioTrack(
      "audio_label", peerConnectionFactory->CreateAudioSource(nullptr));


    auto videoSource = peerConnectionFactory->CreateVideoSource(OpenVideoCaptureDevice(), nullptr);
    video_track = peerConnectionFactory->CreateVideoTrack("video_label", videoSource);

    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
      peerConnectionFactory->CreateLocalMediaStream("stream_label");

    stream->AddTrack(audio_track);
    stream->AddTrack(video_track);
    if (!sendPeerConnection->AddStream(stream)) {
      logError("Adding stream to PeerConnection failed");
    }

    auto sdpListener = new rtc::RefCountedObject<SimpleCreateSessionDescriptionObserver>("send-createOffer", [&](auto* desc) {
        std::string serialized;
        desc->ToString(&serialized);
        initialSendOfferSdp = serialized;
        sendPeerConnection->SetLocalDescription(new rtc::RefCountedObject<SimpleSetSessionDescriptionObserver>("send-setLocal", [&](){
            log("Success: set up send peer connection");
        }), desc);
        log("Room settings: " + roomSettings.dump());
        auto capabilities = getEffectiveClientRtpCapabilities(serialized, roomSettings);
        log("Our capabilities: " + capabilities.dump());
        // listener->onRtpCapabilities(serialized);
        listener->onRtpCapabilities(capabilities);
    });

    log("Creating offer...");
    sendPeerConnection->CreateOffer(sdpListener, nullptr);

    /*
    receivePeerConnection->CreateOffer(new rtc::RefCountedObject<SimpleCreateSessionDescriptionObserver>("receive-createOffer", [&](auto* desc) {
        std::string serialized;
        desc->ToString(&serialized);
        initialReceiveOfferSdp = serialized;
        receivePeerConnection->SetLocalDescription(new rtc::RefCountedObject<SimpleSetSessionDescriptionObserver>("receive-setLocal", [&](){
            log("Success: set up receive peer connection");
        }), desc);
    }), nullptr);
    */
    log("My work here is done!");
  }

  void addProducers () {
    // addProducer("video", video_track->id());
    addProducer("audio", audio_track->id());
  }

  void ensureSendTransportCreated(std::function<void(json)> callback) {
    if (remoteSendTransportSdp != nullptr) {
      callback(remoteSendTransportSdp);
    } else {
      sendTransportId = randomNumber();
      transport->request("mediasoup-request", {
        {"appData", {
                      {"media", "SEND"}
                    }},
        {"id", sendTransportId},
        {"version", sendTransportVersion++},
        {"direction", "send"},
        {"method",  "createTransport"},
        {"options", {
                      {"tcp", false}
                    }},
        {"target",  "peer"},
      }, [=](json response) {
        // log("createTransport response: " + response.dump());
        remoteSendTransportSdp = response.at("data");
        callback(remoteSendTransportSdp);
      });
    }
  }

  void addProducer(std::string kind, std::string id) {
    log("Ensure send transport created!");
    ensureSendTransportCreated([=](json desc) {

      log("Now we are sure that it was created");
      //sendPeerConnection->CreateOffer(new rtc::RefCountedObject<SimpleCreateSessionDescriptionObserver>("send-createOffer", [=](auto* desc) {
        std::string serialized;
        //desc->ToString(&serialized);
        auto sessionDesc = sendPeerConnection->local_description();
        sessionDesc->ToString(&serialized);
        // sendPeerConnection->SetLocalDescription(new rtc::RefCountedObject<SimpleSetSessionDescriptionObserver>("send-setLocal", [=](){
            log("Created new offer and set it");
            json request = {
              {"method", "newProducerSdp"},
              {"kind", kind},
              {"trackId", id},
              {"initialOfferSdp", serialized},
              {"remoteTransportSdp", remoteSendTransportSdp},
              {"transportId", sendTransportId}
            };
            transport->request("mediasoup-request", request, std::bind(&Handler::gotProducerData, this, std::placeholders::_1, kind));
        // }), sessionDesc);
      //}), nullptr);
    });
  }

  void gotProducerData(json data, std::string kind) {
    auto remoteSdp = data.at("data").at("sdp").get<string>();
    json rtpParameters = data.at("data").at("rtpParameters");

    webrtc::SdpParseError error;

    auto remoteAnswer =
      webrtc::CreateSessionDescription(webrtc::SessionDescriptionInterface::kAnswer,
                                       remoteSdp, &error);

    sendPeerConnection->SetRemoteDescription(new rtc::RefCountedObject<SimpleSetSessionDescriptionObserver>("send-setRemote", [=](){

        std::string source = "mic";
        if (kind == "video") {
          source = "webcam";
        }
        json req = {
          {"method", "createProducer"},
          {"appData", {
            {"source", source}
          }},
          {"kind", kind},
          {"paused", false},
          {"target", "peer"},
          {"rtpParameters", rtpParameters},
          {"transportId", sendTransportId}
        };

        transport->request("mediasoup-request", req, [&](json response) {
            log("Oh yere, created producer on WS");
            log("Is the video track enabled? " + std::to_string(video_track->enabled()));
            if (video_track->state() == webrtc::MediaStreamTrackInterface::TrackState::kLive) {
              log("Video track is live :-D");
            } else if (video_track->state() == webrtc::MediaStreamTrackInterface::TrackState::kEnded) {
              log("NOOO audio has ended :'(");
            } else {
              log("WTF audio unknown state...");
            }

            log("Is the audio track enabled? " + std::to_string(audio_track->enabled()));
            if (audio_track->state() == webrtc::MediaStreamTrackInterface::TrackState::kLive) {
              log("audio track is live :-D");
            } else if (audio_track->state() == webrtc::MediaStreamTrackInterface::TrackState::kEnded) {
              log("NOOO audio has ended :'(");
            } else {
              log("WTF audio unknown state...");
            }
        });
    }), remoteAnswer);
  }

  // bool alreadyAddedUseForTesting = false;
  void addConsumer(ConsumerInfo consumer) {
    log(">=>=>=>=> Adding CONSUMER <=<=<=<=<=<=!");

    workQueue.run([=](std::function<void()> cb) {
      /* just used as a guard to prevent more consumers, during testing
      if (alreadyAddedUseForTesting) {
        return;
      }
      alreadyAddedUseForTesting = true;
      */

      // return; // TODO actually add!
      auto consumerId = consumer.at("id").get<int>();
      // log("consumerId=" + std::to_string(consumerId));
      auto kind = consumer.at("kind").get<string>();
      // log("kind=" + kind);

  /*
      if (kind != "audio") {
        logError("Kind " + kind + " not supported, skipping.");
        return;
      }
  */

      auto id = consumer.at("id").get<int>();
      // log("id=" + std::to_string(id));
      auto trackId = "consumerZZZZZZ-" + kind + "-" + std::to_string(id);
      auto encoding = consumer.at("rtpParameters").at("encodings").at(0);
      // log("trackId=" + trackId);
      auto ssrc = encoding.at("ssrc").get<int>();
      // log("ssrc=" + std::to_string(ssrc));
      auto cname = consumer.at("rtpParameters").at("rtcp").at("cname").get<string>();
      // log("cname=" + cname);
      json consumerInfo = {
        {"kind", kind},
        {"trackId", trackId},
        {"ssrc", ssrc},
        {"cname", cname}
      };

      if (encoding.count("rtx") > 0 && encoding.at("rtx").count("ssrc") > 0) {
        consumerInfo.emplace("rtxSsrc", encoding.at("rtx").at("ssrc"));
      }

      consumers.emplace(consumerId, consumerInfo);

      log("ADDING CONSUMER RIGHT OVER HERE");

      json request = {
        {"method", "newConsumerSdp"},
        {"initialOfferSdp", initialSendOfferSdp},
        // {"initialOfferSdp", initialReceiveOfferSdp},
        {"remoteTransportSdp", remoteReceiveTransportSdp},
        {"transportId", receiveTransportId},
        {"version", receiveTransportVersion++},
        {"consumers", consumers}
      };
      transport->request("mediasoup-request", request, std::bind(&Handler::addConsumerWithSdp, this, std::placeholders::_1));
      cb();
    });
  }

  void addConsumerWithSdp(json const sdpResponse) {
    auto sdp = sdpResponse.at("data").get<string>();

    webrtc::SdpParseError error;

    log("DO ADD CONSUMER");

    auto remoteOffer =
      webrtc::CreateSessionDescription(webrtc::SessionDescriptionInterface::kOffer,
                                       sdp, &error);

    if (error.description != "") {
      logError("SDP error: " + error.description);
    } else {
      log("1) Setting remote description");
      receivePeerConnection->SetRemoteDescription(new rtc::RefCountedObject<SimpleSetSessionDescriptionObserver>("receive-setRemote", [&](){
        log("2) Remote description set");
        receivePeerConnection->CreateAnswer(new rtc::RefCountedObject<SimpleCreateSessionDescriptionObserver>("receive-createAnswer",
          [&](webrtc::SessionDescriptionInterface* answer){
            log("3) Answer created");
            receivePeerConnection->SetLocalDescription(
              new rtc::RefCountedObject<SimpleSetSessionDescriptionObserver>("receive-setLocal", [&](){
                log("4) Success: consumer added, SDP descriptions set etc.");
              }),
            answer);
          }), nullptr);
      }), remoteOffer);
    }
  }
};

#endif

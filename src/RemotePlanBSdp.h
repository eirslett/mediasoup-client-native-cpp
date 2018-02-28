#ifndef _RemotePlanBSdp_h_
#define _RemotePlanBSdp_h_

#include "json.hpp"
#include "request_id.h"
#include "log.h"

using json = nlohmann::json;
using std::string;

class RemoteSdp {
  protected:
  json rtpParametersByKind;
  json transportLocalParameters = nullptr;
  json transportRemoteParameters = nullptr;
  int globalId;
  int globalVersion = 0;

  public:
  RemoteSdp (json rtpParametersByKind)
    : rtpParametersByKind(rtpParametersByKind)
    {
      globalId = randomNumber();
  }

  void setTransportLocalParameters (json transportLocalParameters) {
    this->transportLocalParameters = transportLocalParameters;
  }

  void setTransportRemoteParameters (json transportRemoteParameters) {
    this->transportRemoteParameters = transportRemoteParameters;
  }
};

class SendRemoteSdp : RemoteSdp {
  public:
  SendRemoteSdp (json rtpParametersByKind): RemoteSdp(rtpParametersByKind) {
  }
  json createAnswerSdp (json localSdpObj) {
    if (transportLocalParameters == nullptr) {
      logError("No transport local parameters");
    }
    if (transportRemoteParameters == nullptr) {
      logError("No transport remote parameters");
    }

    auto remoteIceParameters = transportRemoteParameters.at("iceParameters");
    auto remoteIceCandidates = transportRemoteParameters.at("iceCandidates");
    auto remoteDtlsParameters = transportRemoteParameters.at("dtlsParameters");

    string midsStr = "";
    if (localSdpObj.count("media") > 0) {
      bool first = true;
      for (auto media : localSdpObj.at("media")) {
        if (first) {
          first = false;
        } else {
          midsStr += " ";
        }
        midsStr += media.at("mid").get<string>();
      }
    }

    // Increase our SDP version.
    globalVersion++;

    json iceLite = remoteIceParameters.at("iceLite").get<bool>() ? "ice-lite" : nullptr;

    auto fingerprints = remoteDtlsParameters.at("fingerprints").get<std::vector<json>>();
    auto lastFingerprint = fingerprints.back();

    json sdpObj = {
      {"version", 0},
      {"origin", {
        {"address", "0.0.0.0"},
        {"ipVer", 4},
        {"netType", "IN"},
        {"sessionId", globalId},
        {"sessionVersion", globalVersion},
        {"username", "mediasoup-client"}
      }},
      {"name", "-"},
      {"timing", {
        {"start", 0},
        {"stop", 0}
      }},
      {"iceLite", iceLite},
      {"msidSemantic", {
        {"semantic", "WMS"},
        {"token", "*"}
      }},
      {"groups", {
        {{"type", {"bundle"}}, {"mids", midsStr}}
      }},
      {"media", json::array()},
      {"fingerprint", {
        {"type", lastFingerprint.at("algorithm")},
        {"hash", lastFingerprint.at("value")}
      }}
    };

    if (localSdpObj.count("media") > 0) {
      for (auto &localMediaObj : localSdpObj.at("media")) {
        auto kind = localMediaObj.at("type").get<string>();
        auto codecs = rtpParametersByKind.at(kind).at("codecs");
        auto headerExtensions = rtpParametersByKind.at(kind).at("headerExtensions");

        json remoteMediaObj = {
          {"type", kind},
          {"port", 7},
          {"protocol", "RTP/SAVPF"},
          {"connection", {
            {"ip", "127.0.0.0"},
            {"version", 4}
          }},
          {"mid", localMediaObj.at("mid")},
          {"iceUfrag", remoteIceParameters.at("usernameFragment")},
          {"icePwd", remoteIceParameters.at("password")},
          {"candidates", json::array()},
          {"endOfCandidates", "end-of-candidates"},
          {"iceOptions", "renomination"},
          {"rtp", json::array()},
          {"rtcpFb", json::array()},
          {"fmtp", json::array()}
        };

        for (auto &candidate : remoteIceCandidates) {
          json candidateObj = {
            {"component", 1},
            {"foundation", candidate.at("foundation")},
            {"ip", candidate.at("ip")},
            {"port", candidate.at("port")},
            {"priority", candidate.at("priority")},
            {"transport", candidate.at("protocol")},
            {"type", candidate.at("type")}
          };
          if (candidate.count("tcpType") > 0) {
            candidateObj.emplace("tcpType", candidate.at("tcpType"));
          }
          remoteMediaObj.at("candidates").push_back(candidateObj);
        }

        if (remoteDtlsParameters.count("role") > 0) {
          string role = remoteDtlsParameters.at("role");
          if (role == "client") {
            remoteMediaObj.emplace("setup", "active");
          } else if (role == "server") {
            remoteMediaObj.emplace("setup", "passive");
          }
        }

        if (remoteDtlsParameters.count("direction") > 0) {
          string direction = remoteDtlsParameters.at("direction");
          if (direction == "sendrecv" || direction == "recvonly") {
            remoteMediaObj.emplace("direction", "recvonly");
          } else if (direction == "recvonly" || direction == "inactive") {
            remoteMediaObj.emplace("setup", "inactive");
          }
        }

        // If video, be ready for simulcast.
        if (kind == "video") {
          remoteMediaObj.emplace("xGoogleFlag", "conference");
        }

        for (auto &codec : codecs) {
          json rtp = {
            {"payload", codec.at("payloadType")},
            {"codec", codec.at("name")},
            {"rate", codec.at("clockRate")}
          };

          if (codec.at("channels") > 1) {
            rtp.emplace("encoding", codec.at("channels"));
          }

          remoteMediaObj.at("rtp").push_back(rtp);

          if (codec.count("parameters") > 0) {
            json paramFmtp = {
              {"payload", codec.at("payloadType")},
              {"config", ""}
            };
          }
        }
      }
    }

    return sdpObj;
  }
};
#endif

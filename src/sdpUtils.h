#ifndef _sdpUtils_h_
#define _sdpUtils_h_

#include <boost/algorithm/string.hpp>

#include <sdptransform/sdptransform.hpp>
#include <string>
#include <json.hpp>

using std::string;

namespace ortc {
  json getRtpCapabilities(json extendedRtpCapabilities) {
    auto codecs = json::array();
    auto headerExtensions = json::array();

    for (auto capCodec : extendedRtpCapabilities.at("codecs")) {
      json codec = {
        {"name", capCodec.at("name")},
        {"mimeType", capCodec.at("mimeType")},
        {"kind", capCodec.at("kind")},
        {"clockRate", capCodec.at("clockRate")},
        {"preferredPayloadType", capCodec.at("recvPayloadType")},
        {"rtcpFeedback", capCodec.at("rtcpFeedback")},
        {"parameters", capCodec.at("parameters")}
      };

      if (capCodec.count("channels") > 0) {
        codec.emplace("channels", capCodec.at("channels"));
      }

      codecs.push_back(codec);

      // Add RTX codec.
      if (capCodec.count("recvRtxPayloadType")) {
        log("capCodec: " + capCodec.dump());
        json rtxCapCodec = {
          {"name", "rtx"},
          {"mimeType", capCodec.at("kind").get<string>() + "/rtx"},
          {"kind", capCodec.at("kind")},
          {"clockRate", capCodec.at("clockRate")},
          {"preferredPayloadType", capCodec.at("recvRtxPayloadType")},
          {"parameters", {
            {"apt", capCodec.at("recvPayloadType")}
          }}
        };

        log("FOUND RTX CODEC, ADD IT");
        codecs.push_back(rtxCapCodec);
      }

      // TODO: In the future, we need to add FEC, CN, etc, codecs.
    }

    for(auto capExt : extendedRtpCapabilities.at("headerExtensions")) {
      json ext = {
        {"kind", capExt.at("kind")},
        {"uri", capExt.at("uri")},
        {"preferredId", capExt.at("recvId")}
      };
      headerExtensions.push_back(ext);
    }

    return {
      {"codecs", codecs},
      {"headerExtensions", headerExtensions},
      {"fecMechanisms", extendedRtpCapabilities.at("fecMechanisms")}
    };
  }

  bool matchCapCodecs (json aCodec, json bCodec) {
    auto aMimeType = boost::algorithm::to_lower_copy(aCodec.at("mimeType").get<string>());
    auto bMimeType = boost::algorithm::to_lower_copy(bCodec.at("mimeType").get<string>());
    if (aMimeType !=  bMimeType) {
      return false;
    }
    if (aCodec.at("clockRate") != bCodec.at("clockRate")) {
      return false;
    }
    if (aCodec.count("channels") > 0 && aCodec.at("channels") != bCodec.at("channels")) {
      return false;
    }

    // TODO: Match H264 parameters.

    return true;
  }

  bool matchCapHeaderExtensions(json aExt, json bExt) {
    if (aExt.count("kind") > 0 && bExt.count("kind") > 0 && aExt.at("kind") != bExt.at("kind")) {
      return false;
    }
    if (aExt.at("uri") != bExt.at("uri")) {
      return false;
    }
    return true;
  }

  json reduceRtcpFeedback(json codecA, json codecB) {
    auto reducedRtcpFeedback = json::array();
    if (codecA.count("rtcpFeedback")) {
      for (auto aFb : codecA.at("rtcpFeedback")) {
        if (codecB.count("rtcpFeedback")) {
          auto codecBRtcpFeedback = codecB.at("rtcpFeedback").get<std::vector<json>>();
          auto search = std::find_if(codecBRtcpFeedback.begin(), codecBRtcpFeedback.end(), [&](auto bFb) {
              return bFb.at("type") == aFb.at("type") && bFb.at("parameter") == aFb.at("parameter");
          });
          if (search != codecBRtcpFeedback.end()) {
            reducedRtcpFeedback.push_back(*search);
          }
        }
      }
    }
    return reducedRtcpFeedback;
  }

  json getExtendedRtpCapabilities(json localCaps, json remoteCaps) {
    auto codecs = json::array();
    auto headerExtensions = json::array();
    auto fecMechanisms = json::array();

    if (remoteCaps.count("codecs") > 0) {
      // Match media codecs and keep the order preferred by remoteCaps.
      for (auto &remoteCodec : remoteCaps.at("codecs")) {
        // TODO: Ignore pseudo-codecs and feature codecs.
        if (remoteCodec.at("name") == "rtx") {
          log("Found RTX codec, skip it!");
          continue;
        }

        if (localCaps.count("codecs") > 0) {
          auto localCapsCodecs = localCaps.at("codecs").get<std::vector<json>>();
          auto searchMatchingLocalCodec = std::find_if(localCapsCodecs.begin(), localCapsCodecs.end(), [&](auto localCodec) {
              return matchCapCodecs(localCodec, remoteCodec);
          });
          if (searchMatchingLocalCodec != localCapsCodecs.end()) {
            json matchingLocalCodec = *searchMatchingLocalCodec;
            json extendedCodec = {
              {"name", remoteCodec.at("name")},
              {"mimeType", remoteCodec.at("mimeType")},
              {"kind", remoteCodec.at("kind")},
              {"clockRate", remoteCodec.at("clockRate")},
              {"sendPayloadType", remoteCodec.at("preferredPayloadType")},
              {"sendRtxPayloadType", 102 /*nullptr */}, // TODO this should be nullptr, and the code should figure out 102 by itself!
              {"recvPayloadType", remoteCodec.at("preferredPayloadType")},
              {"recvRtxPayloadType", 102 /*nullptr */}, // TODO this should be nullptr, and the code should figure out 102 by itself!
              {"rtcpFeedback", reduceRtcpFeedback(matchingLocalCodec, remoteCodec)},
              {"parameters", remoteCodec.at("parameters")}
            };
            if (remoteCodec.count("channels") > 0) {
              extendedCodec.emplace("channels", remoteCodec.at("channels"));
            }
            codecs.push_back(extendedCodec);
          }
        }
      }
    }

    // Match RTX codecs.
    for (auto extendedCodec : codecs) {
      log("Match RTX codecs... " + codecs.dump());
      log("LocalCaps " + localCaps.dump());
      log("RemoteCaps " + remoteCaps.dump());

      auto localCapsCodecs = localCaps.at("codecs").get<std::vector<json>>();
      auto searchMatchingLocalRtxCodec = std::find_if(localCapsCodecs.begin(), localCapsCodecs.end(), [&](auto localCodec) {
        return localCodec.at("name") == "rtx" && localCodec.at("parameters").count("apt") > 0 && localCodec.at("parameters").at("apt") == extendedCodec.at("sendPayloadType");
      });

      auto remoteCapsCodecs = remoteCaps.at("codecs").get<std::vector<json>>();
      auto searchMatchingRemoteRtxCodec = std::find_if(remoteCapsCodecs.begin(), remoteCapsCodecs.end(), [&](auto remoteCodec) {
        return remoteCodec.at("name") == "rtx" && extendedCodec.at("parameters").is_object() && extendedCodec.at("parameters").count("apt") > 0 && extendedCodec.at("parameters").at("apt") == extendedCodec.at("recvPayloadType");
      });

      if (searchMatchingLocalRtxCodec != localCapsCodecs.end() && searchMatchingRemoteRtxCodec != remoteCapsCodecs.end()) {
        log("FOUND MATCH SO WE ARE HERE WITH TYPES");
        extendedCodec.emplace("sendRtxPayloadType", (*searchMatchingLocalRtxCodec).at("preferredPayloadType"));
        extendedCodec.emplace("recvRtxPayloadType", (*searchMatchingRemoteRtxCodec).at("preferredPayloadType"));
      }
    }

    // Match header extensions.
    for (auto remoteExt : remoteCaps.at("headerExtensions")) {
      if (localCaps.count("headerExtensions") > 0) {
        auto localCapsHeaderExtensions = localCaps.at("headerExtensions").get<std::vector<json>>();
        auto matchingLocalExt = std::find_if(localCapsHeaderExtensions.begin(), localCapsHeaderExtensions.end(), [&](auto localExt) {
          return matchCapHeaderExtensions(localExt, remoteExt);
        });

        if (matchingLocalExt != localCapsHeaderExtensions.end()) {
          json extendedExt = {
            {"kind", remoteExt.at("kind")},
            {"uri", remoteExt.at("uri")},
            {"sendId", (*matchingLocalExt).at("preferredId")},
            {"recvId", remoteExt.at("preferredId")}
          };
          headerExtensions.push_back(extendedExt);
        }
      }
    }

    return {
      {"codecs", codecs},
      {"headerExtensions", headerExtensions},
      {"fecMechanisms", fecMechanisms}
    };
  }
}

namespace commonUtils {
  json extractRtpCapabilities(json sdpObj) {
    // auto dumped = sdpObj.dump();
    // log("SDP OJB" + dumped);
    // Map of RtpCodecParameters indexed by payload type.
    std::map<int, json> codecsMap;

    // Array of RtpHeaderExtensions.
    std::vector<json> headerExtensions;

    // Whether a m=audio/video section has been already found.
    bool gotAudio = false;
    bool gotVideo = false;

    for (auto& m : sdpObj.at("media")) {
      auto kind = m.at("type").get<string>();
      if (kind == "audio") {
        if (gotAudio) {
          continue;
        }
        gotAudio = true;

      } else if (kind == "video") {
        if (gotVideo) {
          continue;
        }
        gotVideo = true;
      } else {
        continue;
      }

      // Get codecs.
      for (auto &rtp : m.at("rtp")) {
        auto codecName = rtp.at("codec").get<string>();
        json channels = 1;
        if (rtp.count("encoding") > 0) {
          channels = rtp.at("encoding");
        }
        json codec = {
          {"name", codecName},
          {"mimeType", kind + "/" + codecName},
          {"kind", kind},
          {"clockRate", rtp.at("rate")},
          {"preferredPayloadType", rtp.at("payload")},
          {"channels", channels},
          {"rtcpFeedback", json::array()},
          {"parameters", json::array()}
        };

        if (kind != "audio") {
          codec.erase("channels");
        }

        codecsMap.emplace(rtp.at("payload").get<int>(), codec);
      }

      // Get codec parameters.
      std::vector<json> fmtps;
      if (m.count("fmtp") > 0) {
        fmtps = m.at("fmtp").get<std::vector<json>>();
      }
      for (auto &fmtp : fmtps) {
        auto parameters = sdptransform::parseFmtpConfig(fmtp.at("config").get<string>());
        auto search = codecsMap.find(fmtp.at("payload").get<int>());
        if (search == codecsMap.end()) {
          continue;
        } else {
          json codec = search->second;
          codec.emplace("parameters", parameters);
        }
      }

      // Get RTCP feedback for each codec.
      std::vector<json> rtcpFbs;
      if (m.count("rtcpFb") > 0) {
        rtcpFbs = m.at("rtcpFb").get<std::vector<json>>();
      }
      for(auto &fb : rtcpFbs) {
        // log("dump codecsmap " + json(codecsMap).dump());
        int payload = 0;
        if (fb.at("payload").is_string()) {
          payload = std::stoi(fb.at("payload").get<string>());
        } else {
          payload = fb.at("payload").get<int>();
        }
        auto search = codecsMap.find(payload);
        if (search == codecsMap.end()) {
          continue;
        } else {
          json codec = search->second;

          json feedback = {
            {"type", fb.at("type")}
          };

          if (fb.count("subtype") > 0) {
            feedback.emplace("parameter", fb.at("subtype"));
          }

          codec.at("rtcpFeedback").push_back(feedback);
        }
      }

      // Get RTP header extensions.
      if (m.count("ext") > 0) {
        for (auto &ext : m.at("ext")) {
          json headerExtension = {
            {"kind", kind},
            {"uri", ext.at("uri")},
            {"preferredId", ext.at("value")}
          };
          headerExtensions.push_back(headerExtension);
        }
      }
    }

    std::vector<json> codecs;

    for (auto &codecEntry : codecsMap)
    {
      codecs.push_back(codecEntry.second);
    }

    json rtpCapabilities = {
      {"codecs", codecs},
      {"headerExtensions", headerExtensions},
      {"fecMechanisms", json::array()}
    };

    return rtpCapabilities;
  }
}

json getExtendedRtpCapabilities(json sdpObj, json roomCapabilities) {
  auto clientCapabilities = commonUtils::extractRtpCapabilities(sdpObj);
  return ortc::getExtendedRtpCapabilities(
    clientCapabilities,
    roomCapabilities.at("rtpCapabilities")
  );
}

json getEffectiveClientRtpCapabilities(string sdp, json roomCapabilities) {
  json sdpObj = sdptransform::parse(sdp);
  log("sdpObj: " + sdpObj.dump());
  auto extendedRtpCapabilities = getExtendedRtpCapabilities(sdpObj, roomCapabilities);
  return ortc::getRtpCapabilities(extendedRtpCapabilities);
}


#endif //_sdpUtils_h_

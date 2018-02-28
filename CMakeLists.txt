project(mediasoup-client-native-cpp)
cmake_minimum_required (VERSION 3.3)

find_package(libwebrtc REQUIRED)
find_package(openssl REQUIRED)
find_package(boost REQUIRED)
set(CMAKE_CXX_STANDARD 14)

include_directories(src)

add_executable(example src/example.cpp src/log.h src/request_id.h src/json.hpp src/VoiceChannel.h src/Handler.h src/WebSocketTransport.h src/VoiceChannelData.h src/simpleListeners.h src/sdpUtils.h src/RemotePlanBSdp.h src/WorkQueue.h)

target_link_libraries(example ${WEBRTC_LIBRARIES} ${OPENSSL_LIBRARIES} sdptransform)

set_target_properties(example PROPERTIES LINK_FLAGS "-lboost_system -lboost_random")

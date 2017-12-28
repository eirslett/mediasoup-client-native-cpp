project(mediasoup-client-native-cpp)
cmake_minimum_required (VERSION 3.3)

find_package(libwebrtc REQUIRED)
find_package(boost REQUIRED)
set(CMAKE_CXX_STANDARD 14)

include_directories(src)

add_executable(example src/example.cpp src/json.hpp src/log.h src/requestId.h src/WebSocketTransport.h)

target_link_libraries(example ${WEBRTC_LIBRARIES})

set_target_properties(example PROPERTIES LINK_FLAGS "-lboost_system -lboost_random")

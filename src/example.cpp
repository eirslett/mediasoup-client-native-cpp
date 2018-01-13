#include "VoiceChannel.h"
#include "log.h"
#include <webrtc/rtc_base/thread.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/connect.hpp>
#include <future>
#include <chrono>
#include <thread>

#include "WebSocketTransport.h"
#include "Handler.h"
#include "request_id.h"

int main() {
  log("Example App");
  boost::asio::io_context ioc;
  auto username = "Kim-" + std::to_string(randomNumber());
  auto vc = std::make_shared<VoiceChannel>(
    ioc,
    "localhost", "3443",
    "testroom",
    username);
  vc->joinRoom();

  while (true) {
    ioc.poll();
    rtc::Thread::Current()->ProcessMessages(0);
  }
}

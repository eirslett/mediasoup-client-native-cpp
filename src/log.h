#ifndef API_LOG_H_
#define API_LOG_H_

#include <iostream>
#include <thread>

void log(std::string message) {
  std::cout << "[" << std::this_thread::get_id() << "] " << message << std::endl;
}

void logError(std::string message) {
  std::cerr << "\033[1;31m[" << std::this_thread::get_id() << "]\033[0m " << message << std::endl;
}

#endif

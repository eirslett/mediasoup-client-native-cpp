#ifndef _request_id_h_
#define _request_id_h_

#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

// Our random number generator
boost::random::random_device rng;
boost::random::uniform_int_distribution<> request_id_dist(1000000, 9999999);

int make_request_id () {
  // log("Generating request id...");
  int request_id = request_id_dist(rng);
  // log("Request id is generated! " + std::to_string(request_id));
  return request_id;
}

int randomNumber () {
  return make_request_id();
}
#endif

#ifndef _requestId_h_
#define _requestId_h_

#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

// Our random number generator
boost::random::random_device rng;
boost::random::uniform_int_distribution<> requestIdDist(1000000, 9999999);

namespace mediasoup {

  int makeRequestId() {
    return requestIdDist(rng);
  }

}

#endif

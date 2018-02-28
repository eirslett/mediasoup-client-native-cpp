#ifndef _WorkQueue_h_
#define _WorkQueue_h_

#include <functional>
#include <queue>

#include "log.h"

class WorkQueue {
  std::queue<std::function<void(std::function<void(void)>)>> elements;

  public:
  void run (std::function<void(std::function<void(void)>)> func) {
    log(">=>=>=>=> Adding task to the work queue!");
    elements.push(func);
    taskLoop();
  }

  void taskLoop () {
    if (!elements.empty()) {
      auto workFunction = elements.front();
      elements.pop();
      log(">=>=>=>=> Now running a task in the queue!");
      workFunction([=]() {
        log(">=>=>=>=> Task done!!");
        taskLoop();
      });
    }
  }
};

#endif //_WorkQueue_h_

#include "Poller.h"
#include "PollPoller.h"
#include "EpollPoller.h"

Poller* Poller::newDefaultPoller(EventLoop* loop) {
  if (::getenv("USE_POLL")) {
    return new PollPoller(loop);
  }
  else {
    return new EpollPoller(loop);
  }
}
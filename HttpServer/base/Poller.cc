#include "Poller.h"
#include "Channel.h"
#include "EventLoop.h"


bool Poller::hasChannel(Channel * channel) {
  assertInLoopThread();
  auto it = channels_.find(channel->fd());
  return it != channels_.end() && it->second == channel;
}

void Poller::assertInLoopThread() const {
  ownerLoop_->assertInLoopThread();
}
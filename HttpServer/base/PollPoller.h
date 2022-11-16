#ifndef REACTOR_BASE_POLLPOLLER_H
#define REACTOR_BASE_POLLPOLLER_H

#include "Poller.h"

#include <vector>

struct pollfd;

class PollPoller : public Poller {
 public:
  typedef std::vector<Channel*> ChannelList;
  
  PollPoller(EventLoop* loop);
  ~PollPoller() override;

  Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
  void updateChannel(Channel* channel) override;
  void removeChannel(Channel* channel) override;

 private:
  void fillActiveChannels(int numEvents, ChannelList* activeChannels) const override;
  std::vector<struct pollfd> pollfds_;
};

#endif  // REACTOR_BASE_POLLPOLLER_H
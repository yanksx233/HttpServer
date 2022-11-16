#ifndef REACTOR_BASE_EPOLLPOLLER_H
#define REACTOR_BASE_EPOLLPOLLER_H

#include "Poller.h"

#include <sys/epoll.h>

class EpollPoller : public Poller {
 public:
  EpollPoller(EventLoop *loop);
  ~EpollPoller() override;
  
  Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
  void updateChannel(Channel* channel) override;
  void removeChannel(Channel* channel) override;

 private:
  void fillActiveChannels(int numEvents, ChannelList* activeChannels) const override;
  void updateEpoll(int epollOperation, Channel* channel);
  static const char* operationToString(int epollOperation);

  static const int kInitEventListSize = 16;

  int epollfd_;
  std::vector<struct epoll_event> events_;
};


#endif  // REACTOR_BASE_EPOLLPOLLER_H
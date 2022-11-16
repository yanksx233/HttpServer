#ifndef REACTOR_BASE_POLLER_H
#define REACTOR_BASE_POLLER_H

#include "noncopyable.h"

#include <map>
#include <vector>
#include <sys/time.h>

class Timestamp;
class Channel;
class EventLoop;

class Poller : noncopyable {
 public:
  typedef std::vector<Channel*> ChannelList;

  Poller(EventLoop* loop) : ownerLoop_(loop) {};
  virtual ~Poller() = default;

  static Poller* newDefaultPoller(EventLoop* loop); 

  virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;

  virtual void updateChannel(Channel* channel) = 0;
  virtual void removeChannel(Channel* channel) = 0;
  bool hasChannel(Channel * channel);

  void assertInLoopThread() const;

 protected:
  virtual void fillActiveChannels(int numEvents, ChannelList* activeChannels) const = 0;
  std::map<int, Channel*> channels_;

 private:
  EventLoop* ownerLoop_;
};

#endif  // REACTOR_BASE_POLLER_H
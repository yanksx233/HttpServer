#ifndef REACTOR_BASE_CHANNEL_H
#define REACTOR_BASE_CHANNEL_H

#include "noncopyable.h"

#include <functional>
#include <memory>

class Timestamp;
class EventLoop;

class Channel : noncopyable {
 public:
  typedef std::function<void()> EventCallback;

  Channel(EventLoop *loop, int fd);
  ~Channel();

  void handleEvent(Timestamp receiveTime);

  int fd() const { return fd_; }
  int events() const { return events_; }

  void set_revents(int revents) { revents_ = revents; }
  bool isNoneEvent() const { return events_ == kNoneEvent; }
  bool isReading() const { return events_ & kReadEvent; }
  bool isWriting() const { return events_ & kWriteEvent; }

  /// Tie this channel to the owner object managed by shared_ptr,
  /// prevent the owner object being destroyed in handleEvent.
  void tie(const std::shared_ptr<void>& obj) { tie_ = obj; tied_ = true; }

  // used by Poller
  int index() const { return index_; }
  void set_index(int idx) { index_ = idx; }

  void setReadCallback(std::function<void(Timestamp)> cb) { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

  void enableReading() { events_ |= kReadEvent; update(); }
  void disableReading() { events_ &= ~kReadEvent; update(); }
  void enableWriting() { events_ |= kWriteEvent; update(); }
  void disableWriting() { events_ &= ~kWriteEvent; update(); }
  void disableAll() { events_ = kNoneEvent; update(); }

  EventLoop* ownerLoop() { return loop_; };
  void remove();

  std::string eventsToString() const;
  std::string reventsToString() const;
  static std::string eventsToString(int fd, int events);

 private:
  void update();
  void handleEventWithGuard(Timestamp receiveTime);

  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop* loop_;
  const int fd_;
  int events_;
  int revents_;  // it's the received event types of poll
  int index_;  // used by Poller
  bool addedToLoop_;
  bool eventHandling_;
  std::weak_ptr<void> tie_;
  bool tied_;

  std::function<void(Timestamp)> readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};

#endif  // REACTOR_BASE_CHANNEL_H
#include "Channel.h"
#include "EventLoop.h"
#include "Logging.h"
#include "Timestamp.h"

#include <poll.h>
#include <sstream>
#include <assert.h>


const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop *loop, int fd)
  : loop_(loop),
    fd_(fd),
    events_(0),
    revents_(0),
    index_(-1),
    addedToLoop_(false),
    eventHandling_(false),
    tied_(false) {}

Channel::~Channel() {
  assert(!addedToLoop_);
  assert(!eventHandling_);
  if (loop_->isInLoopThread()) {  // TODO: ??
    assert(!loop_->hasChannel(this));
  }
}

void Channel::handleEvent(Timestamp receiveTime) {
  if (tied_) {
    std::shared_ptr<void> guard = tie_.lock();
    if (guard) {
      handleEventWithGuard(receiveTime);
    }
  }
  else {
    handleEventWithGuard(receiveTime);
  }
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
  eventHandling_ = true;
  if (revents_ & POLLNVAL) {
    LOG_WARN << "Channel::handleEvent() POLLNVAL";
  }

  if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
    if (closeCallback_) closeCallback_();
  }

  if (revents_ & (POLLERR | POLLNVAL)) {
    if (errorCallback_) errorCallback_();
  }

  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) {
    if (readCallback_) readCallback_(receiveTime);  // readCallback_应该处理read()返回0的情况，比如直接调用closeCallback_
  }

  if (revents_ & POLLOUT) {
    if (writeCallback_) writeCallback_();  // 先处理读再处理写，由readCallback_处理对方关闭读的情况，writeCallback_里面也应该处理上一步readCallback_中read()返回0的情况
  }
  eventHandling_ = false;
}

void Channel::update() {
  addedToLoop_ = true;
  loop_->updateChannel(this);
}

void Channel::remove() {
  assert(isNoneEvent());
  loop_->removeChannel(this);
  addedToLoop_ = false;
}

std::string Channel::eventsToString() const {
  return eventsToString(fd_, events_);
}

std::string Channel::reventsToString() const {
  return eventsToString(fd_, revents_);
}

std::string Channel::eventsToString(int fd, int ev)
{
  std::ostringstream oss;
  oss << fd << ": ";
  if (ev & POLLIN)
    oss << "IN ";
  if (ev & POLLPRI)
    oss << "PRI ";
  if (ev & POLLOUT)
    oss << "OUT ";
  if (ev & POLLHUP)
    oss << "HUP ";
  if (ev & POLLRDHUP)
    oss << "RDHUP ";
  if (ev & POLLERR)
    oss << "ERR ";
  if (ev & POLLNVAL)
    oss << "NVAL ";

  return oss.str();
}

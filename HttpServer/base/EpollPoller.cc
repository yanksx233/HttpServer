#include "EpollPoller.h"
#include "Logging.h"
#include "Channel.h"
#include "Timestamp.h"

#include <unistd.h>
#include <poll.h>
#include <assert.h>

// On Linux, the constants of poll(2) and epoll(4)
// are expected to be the same.
static_assert(EPOLLIN == POLLIN,        "epoll uses same flag values as poll");
static_assert(EPOLLPRI == POLLPRI,      "epoll uses same flag values as poll");
static_assert(EPOLLOUT == POLLOUT,      "epoll uses same flag values as poll");
static_assert(EPOLLRDHUP == POLLRDHUP,  "epoll uses same flag values as poll");
static_assert(EPOLLERR == POLLERR,      "epoll uses same flag values as poll");
static_assert(EPOLLHUP == POLLHUP,      "epoll uses same flag values as poll");

const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;

EpollPoller::EpollPoller(EventLoop *loop)
  : Poller(loop),
    epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
    events_(kInitEventListSize) {
  if (epollfd_ < 0) {
    LOG_SYSFATAL << "EPollPoller::EPollPoller";
  }
}

EpollPoller::~EpollPoller() {
  ::close(epollfd_);
}

Timestamp EpollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
  LOG_TRACE << "fd total count: " << channels_.size();
  int numEvents = ::epoll_wait(epollfd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
  
  int savedErrno = errno;
  Timestamp now = Timestamp::now();
  if (numEvents > 0) {
    LOG_TRACE << numEvents << " events happened";
    fillActiveChannels(numEvents, activeChannels);
    if (static_cast<size_t>(numEvents) == events_.size()) {
      events_.resize(events_.size()*2);
    }
  }
  else if (numEvents == 0) {
    LOG_TRACE << "nothing happended";
  }
  else {
    if (savedErrno != EINTR) {
      errno = savedErrno;
      LOG_SYSERR << "EpollPoller::poll()";
    }
  }
  return now;
}

void EpollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const {
  assert(static_cast<size_t>(numEvents) <= events_.size());
  for (int i = 0; i < numEvents; ++i) {
    Channel* channel = static_cast<Channel*>(events_[i].data.ptr);

    auto it = channels_.find(channel->fd());
    assert(it != channels_.end());
    assert(it->second == channel);
    
    channel->set_revents(events_[i].events);
    activeChannels->push_back(channel);
  }
}

void EpollPoller::updateChannel(Channel* channel) {
  assertInLoopThread();
  const int index = channel->index();
  if (index == kNew || index == kDeleted) {
    // a new one, add with EPOLL_CTL_ADD
    int fd = channel->fd();
    if (index == kNew) {
      assert(channels_.find(fd) == channels_.end());
      channels_[fd] = channel;
    }
    else {
      assert(channels_.find(fd) != channels_.end());
      assert(channels_[fd] == channel);
    }
    channel->set_index(kAdded);
    updateEpoll(EPOLL_CTL_ADD, channel);
  }
  else {
    // update existed one with EPOLL_CTL_MOD/DEL
    int fd = channel->fd();
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(index == kAdded);
    if (channel->isNoneEvent()) {
      updateEpoll(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);
    }
    else {
      updateEpoll(EPOLL_CTL_MOD, channel);
    }
  }
}

void EpollPoller::removeChannel(Channel* channel) {
  assertInLoopThread();
  int fd = channel->fd();
  assert(channels_.find(fd) != channels_.end());
  assert(channels_[fd] == channel);
  assert(channel->isNoneEvent());
  int index = channel->index();
  assert(index == kAdded || index == kDeleted);

  assert(channels_.erase(fd) == 1);
  if (index == kAdded) {
    updateEpoll(EPOLL_CTL_DEL, channel);
  }
  channel->set_index(kNew);
}

void EpollPoller::updateEpoll(int epollOperation, Channel* channel) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = channel->events();
  event.data.ptr = channel;

  int fd = channel->fd();
  LOG_TRACE << "epoll_ctl_op = " << operationToString(epollOperation)
            << ", events = { " << channel->eventsToString() << " }";
  
  if (::epoll_ctl(epollfd_, epollOperation, fd, &event) < 0) {
    if (epollOperation == EPOLL_CTL_DEL) {
      LOG_SYSERR << "epoll_ctl op = " << operationToString(epollOperation) << ", fd = " << fd;
    }
    else {
      LOG_SYSFATAL << "epoll_ctl op = " << operationToString(epollOperation) << ", fd = " << fd;
    }
  }
}

const char* EpollPoller::operationToString(int epollOperation) {
  switch (epollOperation) {
    case EPOLL_CTL_ADD:
      return "ADD";
    case EPOLL_CTL_MOD:
      return "MOD";
    case EPOLL_CTL_DEL:
      return "DEL";
    default:
      assert(false && "Unreachable");
      return "Unknown Operation";
  }
}

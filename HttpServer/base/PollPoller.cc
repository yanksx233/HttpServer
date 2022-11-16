#include "PollPoller.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logging.h"
#include "Timestamp.h"

#include <poll.h>
#include <assert.h>
#include <algorithm>


PollPoller::PollPoller(EventLoop* loop)
  : Poller(loop) {}

PollPoller::~PollPoller() = default;

Timestamp PollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
  int numEvents = ::poll(&pollfds_[0], pollfds_.size(), timeoutMs);

  int savedErrno = errno;
  Timestamp now = Timestamp::now();
  if (numEvents > 0) {
    LOG_TRACE << numEvents << " events happended";
    fillActiveChannels(numEvents, activeChannels);
  }
  else if (numEvents == 0) {
    LOG_TRACE << "nothing happended";
  }
  else {
    if (savedErrno != EINTR){
      errno = savedErrno;
      LOG_SYSERR << "PollPoller::poll()";
    }
  }
  return now;
}

void PollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const {
  for (auto pfd = pollfds_.begin();
      pfd != pollfds_.end() && numEvents > 0;
      ++pfd) {
    if (pfd->revents > 0) {
      --numEvents;
      Channel* channel = channels_.at(pfd->fd);
      assert(channel->fd() == pfd->fd);
      channel->set_revents(pfd->revents);
      activeChannels->push_back(channel);
    }
  }
}

void PollPoller::updateChannel(Channel* channel) {
  assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd() << ", events = " << channel->events();
  if (channel->index() < 0) {
    // a new one, add to pollfds_
    assert(channels_.find(channel->fd()) == channels_.end());
    struct pollfd pfd;
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    pollfds_.push_back(pfd);
    int idx = static_cast<int>(pollfds_.size()) - 1;
    channel->set_index(idx);
    channels_[pfd.fd] = channel;
  }
  else {
    // update existing one
    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(channels_.size()));
    struct pollfd& pfd = pollfds_[idx];
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    if (channel->isNoneEvent()) {
      pfd.fd = -channel->fd()-1;
    }
  }
}

void PollPoller::removeChannel(Channel* channel) {
  assertInLoopThread();
  LOG_TRACE << "fd == " << channel->fd();

  assert(channels_.find(channel->fd()) != channels_.end());
  assert(channels_[channel->fd()] == channel);
  assert(channel->isNoneEvent());
  int idx = channel->index();
  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
  assert(pollfds_[idx].fd == -channel->fd()-1 && pollfds_[idx].events == channel->events());
  channels_.erase(channel->fd());

  if (idx == static_cast<int>(pollfds_.size()-1)) {
    pollfds_.pop_back();
  }
  else {
    int fdAtEnd = pollfds_.back().fd;
    iter_swap(pollfds_.begin()+idx, pollfds_.end()-1);
    if (fdAtEnd < 0) {
        fdAtEnd = -fdAtEnd - 1;
    }
    channels_[fdAtEnd]->set_index(idx);
    pollfds_.pop_back();
  }
}

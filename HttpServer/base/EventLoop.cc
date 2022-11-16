#include "EventLoop.h"
#include "Logging.h"
#include "Poller.h"
#include "Channel.h"
#include "Timestamp.h"

#include <stdio.h>
#include <assert.h>
#include <algorithm>
#include <sys/eventfd.h>


__thread EventLoop* t_loopInThisThread = nullptr;
const int kPollTimeMs = 10000;

int createEventfd() {
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);   // read 会让@para1减1，write 会加1，0读或max写会阻塞或返回EAGAIN
  if (evtfd < 0) {
    LOG_SYSFATAL << "Failed in eventfd";
  }
  return evtfd;
}

EventLoop::EventLoop()
  : looping_(false),
    quit_(false),
    eventHandling_(false),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    callingPendingFunctors_(false),
    wakeupFd_(createEventfd()),
    pwakeupChannel_(new Channel(this, wakeupFd_)),
    timerQueue_(new TimerQueue(this))
{
  LOG_TRACE << "Event Loop created " << this << " in thread " << threadId_;
  if (t_loopInThisThread) {
    LOG_FATAL << "error: Another EventLoop " << t_loopInThisThread << " exist in this thread " << threadId_;
  }
  else {
    t_loopInThisThread = this;
  }

  pwakeupChannel_->setReadCallback(std::bind(&EventLoop::handleWakeupRead, this)); // FIXME: here implicitly ignore timeArgs
  pwakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
  LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_
            << " destructs in thread " << CurrentThread::tid();
  assert(!looping_);
  pwakeupChannel_->disableAll();
  pwakeupChannel_->remove();
  ::close(wakeupFd_);
  t_loopInThisThread = nullptr;
}

EventLoop* EventLoop::getEventLoopOfCurrentThread() {
  return t_loopInThisThread;
}

void EventLoop::loop() {
  assert(!looping_);
  assertInLoopThread();
  looping_ = true;
  quit_ = false;

  while (!quit_) {
    activeChannels_.clear();
    Timestamp pollReturnTime =  poller_->poll(kPollTimeMs, &activeChannels_);
    if (Logger::LogLevel() <= Logger::TRACE) {
      printActiveChannels();
    }

    eventHandling_ = true;
    for (auto& channel : activeChannels_) {
      channel->handleEvent(pollReturnTime);
    }
    eventHandling_ = false;
    doPendingFunctors();
  }
  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

void EventLoop::quit() {
  quit_ = true;
  if (!isInLoopThread()) {
    wakeup();
  }
}

void EventLoop::runInLoop(const Functor& cb) {
  if (isInLoopThread()) {
    cb();
  }
  else {
    queueInLoop(cb);
  }
}

void EventLoop::queueInLoop(Functor cb) {
  {
    MutexLockGuard lock(mutex_);
    pendingFunctors_.push_back(std::move(cb));
  }

  if (!isInLoopThread() || callingPendingFunctors_) {
    wakeup();  // 第二个条件可以让eventloop直接在下一次epoll/poll wait中返回，但可能push进去的cb已经swap，以致一次无效唤醒，不过问题不大
  }
}

void EventLoop::wakeup() {
  uint64_t one = 1;
  ssize_t n = write(wakeupFd_, &one, sizeof(one));
  if (n != sizeof(one)) {
    LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
    MutexLockGuard lock(mutex_);
    functors.swap(pendingFunctors_);
  }

  LOG_TRACE << "number of Functors is " << functors.size();
  for (const Functor& functor : functors) {
    functor();
  }
  callingPendingFunctors_ = false;
}

void EventLoop::handleWakeupRead() {
  uint64_t one;
  ssize_t n = ::read(wakeupFd_, &one, sizeof(one));
  if (n != sizeof(one)) {
    LOG_ERROR << "EventLoop::handleWakeupRead() reads " << n << " bytes instead of 8";
  }
}


TimerId EventLoop::runAt(TimerCallback cb, Timestamp when) {
  return timerQueue_->addTimer(std::move(cb), when, 0);
}

TimerId EventLoop::runAfter(TimerCallback cb, double seconds) {
  return runAt(std::move(cb), addTime(Timestamp::now(), seconds));
}

TimerId EventLoop::runEvery(TimerCallback cb, double interval) {
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(std::move(cb), time, interval);
}

void EventLoop::cancel(TimerId timerId) {
  return timerQueue_->cancel(timerId);
}

void EventLoop::updateChannel(Channel* channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_) {
    assert(std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  return poller_->hasChannel(channel);
}

void EventLoop::abortNotInLoopThread() {
  LOG_FATAL << "EventLoop " << this << " was created in tid " 
            << threadId_ << ", current thread id = " << CurrentThread::tid();
}

void EventLoop::printActiveChannels() const
{
  for (const Channel* channel : activeChannels_)
  {
    LOG_TRACE << "{" << channel->reventsToString() << "} ";
  }
}
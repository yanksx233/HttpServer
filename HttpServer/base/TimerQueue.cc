#include "TimerQueue.h"
#include "Logging.h"
#include "EventLoop.h"

#include <unistd.h>
#include <sys/timerfd.h>


std::atomic<int64_t> Timer::s_numCreated_(0);

void Timer::restart(Timestamp now) {
  if (repeat_) {
    expiration_ = addTime(now, interval_);
  }
  else {
    expiration_ = Timestamp();
  }
}


int createTimerfd() {
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0) {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp expiration) {
  int64_t microseconds = expiration.microSecondsSinceEpoch()
                         - Timestamp::now().microSecondsSinceEpoch();
  if (microseconds < 100) {
    microseconds = 100;
  }

  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(
      microseconds / Timestamp::kMircoSecondsPerSecond);
  ts.tv_nsec = static_cast<time_t>(
      (microseconds % Timestamp::kMircoSecondsPerSecond) * 1000);
  return ts;
}

void resetTimerfd(int timerfd, Timestamp expiration) {
  struct itimerspec newVal;
  memset(&newVal, 0, sizeof(newVal));
  newVal.it_value = howMuchTimeFromNow(expiration);

  int ret = ::timerfd_settime(timerfd, 0, &newVal, nullptr);
  if (ret) {
    LOG_SYSERR << "timerfd_settime()";
  }
}

void readTimerfd(int timerfd, Timestamp now) {
  uint64_t howmany;
  ssize_t n = ::read(timerfd, &howmany, sizeof(howmany));  // 将到期了多少次存入8-bit buf中
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toLocalTime();
  if (n != sizeof(howmany)) {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
  }
}


TimerQueue::TimerQueue(EventLoop* loop)
  : loop_(loop),
    timerfd_(createTimerfd()),
    timerfdChannel_(loop, timerfd_),
    timers_(),
    callingExpiredTimers_(false)
{
  timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
  timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue() {
  timerfdChannel_.disableAll();
  timerfdChannel_.remove();
  ::close(timerfd_);
  for (const Entry& it : timers_) {
    delete it.second;
  }
}

TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval) {
  Timer* timer = new Timer(std::move(cb), when, interval);
  loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
  return TimerId(timer, timer->sequence());
}

void TimerQueue::addTimerInLoop(Timer* timer) {
  loop_->assertInLoopThread();
  bool earliestChanged = insert(timer);
  if (earliestChanged) {
    resetTimerfd(timerfd_, timer->expiration());
  }
}

void TimerQueue::cancel(TimerId timerId) {
  loop_->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::cancelInLoop(TimerId timerId) {
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());

  auto it = activeTimers_.find(timerId.timer_);
  if (it != activeTimers_.end()) {
    size_t n = timers_.erase(Entry(timerId.timer_->expiration(), timerId.timer_));
    assert(n == 1); (void)n;
    delete timerId.timer_;
    activeTimers_.erase(it);
  }
  else if (callingExpiredTimers_) {  // 当前定时到期处理中，无法取消，但可以取消循环模式
    cancellingTimers_.insert(timerId.timer_);
  }
  assert(timers_.size() == activeTimers_.size());
}


bool TimerQueue::insert(Timer* timer) {
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());

  bool earliestChanged = false;
  Timestamp when = timer->expiration();
  auto it = timers_.begin();
  if (it == timers_.end() || when < it->first) {
    earliestChanged = true;
  }

  {
    auto result = timers_.insert(Entry(when, timer));
    assert(result.second); (void)result;
  }
  {
    auto result = activeTimers_.insert(timer);
    assert(result.second); (void)result;
  }
  
  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;
}

void TimerQueue::handleRead() {
  loop_->assertInLoopThread();
  Timestamp now(Timestamp::now());
  readTimerfd(timerfd_, now);

  std::vector<Entry> expired = getExpired(now);

  callingExpiredTimers_ = true;
  for (const auto& it : expired) {
    it.second->run();
  }
  callingExpiredTimers_ = false;
  reset(expired, now);
  cancellingTimers_.clear();
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now) {
  assert(timers_.size() == activeTimers_.size());

  std::vector<Entry> expired;
  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));  // pair的 < 先比较first，若first相当则比较second
  TimerList::iterator end = timers_.lower_bound(sentry);  // 第一个不小于sentry的位置，set会自动按 < 排序
  assert(end == timers_.end() || now < end->first);
  std::copy(timers_.begin(), end, std::back_inserter(expired));  // 相当于范围 push_back 
  timers_.erase(timers_.begin(), end);

  for (const auto& it : expired) {
    size_t n = activeTimers_.erase(it.second);
    assert(n == 1); (void)n;
  }

  assert(timers_.size() == activeTimers_.size());
  return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now) {
  Timestamp nextExpire;

  for (const Entry& it : expired) {
    if (it.second->repeat() && cancellingTimers_.find(it.second) == cancellingTimers_.end()) {
      it.second->restart(now);
      insert(it.second);
    }
    else {
      delete it.second;
    }
  }

  if (!timers_.empty()) {
    nextExpire = timers_.begin()->first;
  }

  if (nextExpire.valid()) {
    resetTimerfd(timerfd_, nextExpire);
  }
}
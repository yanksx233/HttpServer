#ifndef REACTOR_BASE_TIMERQUEUE_H
#define REACTOR_BASE_TIMERQUEUE_H


#include "noncopyable.h"
#include "Callbacks.h"
#include "Timestamp.h"
#include "Channel.h"

#include <set>
#include <atomic>
#include <vector>


class EventLoop;


class Timer : noncopyable {
 public:
  Timer(TimerCallback cb, Timestamp when, double interval)
    : callback_(std::move(cb)),
      expiration_(when),
      interval_(interval),
      repeat_(interval > 0.0),
      sequence_(static_cast<int64_t>(s_numCreated_.fetch_add(1))) {}

  void run() const { callback_(); }
  Timestamp expiration() const { return expiration_; }
  bool repeat() const { return repeat_; }
  int64_t sequence() const { return sequence_; }

  void restart(Timestamp now);

 private:
  const TimerCallback callback_;
  Timestamp expiration_;
  const double interval_;
  const bool repeat_;
  const int64_t sequence_;

  static std::atomic<int64_t> s_numCreated_;
};


class TimerId /* copyable */ {
 public:
  TimerId()
    : timer_(nullptr),
      sequence_(0) {}

  TimerId(Timer* timer, int64_t seq)
    : timer_(timer),
      sequence_(seq) {}

  friend class TimerQueue;

 private:
  Timer* timer_;
  int64_t sequence_;
};


class TimerQueue : noncopyable {
 public:
  explicit TimerQueue(EventLoop* loop);
  ~TimerQueue();

  TimerId addTimer(TimerCallback cb, Timestamp when, double interval);
  void cancel(TimerId timerId);

 private:
  typedef std::pair<Timestamp, Timer*> Entry;
  typedef std::set<Entry> TimerList;

  void handleRead();
  void addTimerInLoop(Timer* timer);
  void cancelInLoop(TimerId timerId);
  bool insert(Timer* timer);
  std::vector<Entry> getExpired(Timestamp now);
  void reset(const std::vector<Entry>& expired, Timestamp now);

  EventLoop* loop_;
  const int timerfd_;
  Channel timerfdChannel_;
  TimerList timers_;
  bool callingExpiredTimers_;
  std::set<Timer*> activeTimers_;
  std::set<Timer*> cancellingTimers_;
};


#endif  // REACTOR_BASE_TIMERQUEUE_H
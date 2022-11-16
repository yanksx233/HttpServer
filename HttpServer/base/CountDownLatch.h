#ifndef REACTOR_BASE_COUNTDOWNLATCH_H
#define REACTOR_BASE_COUNTDOWNLATCH_H

#include "noncopyable.h"
#include "Mutex.h"
#include "Condition.h"

class CountDownLatch : noncopyable {
 public:
  explicit CountDownLatch(int count)
    : mutex_(),
      cond_(mutex_),
      count_(count) {}

  void wait() {
    MutexLockGuard lock(mutex_);
    while (count_ > 0) {
      cond_.wait();
    }
  }

  void countDown() {
    MutexLockGuard lock(mutex_);
    count_--;
    if (count_ == 0) {
      cond_.notifyAll();
    }
  }

  int getCount() const {
    MutexLockGuard lock(mutex_);
    return count_;
  }

 private:
  mutable MutexLock mutex_;
  Condition cond_;
  int count_;
};

#endif  // REACTOR_BASE_COUNTDOWNLATCH_H
#ifndef REACTOR_BASE_MUTEX_H
#define REACTOR_BASE_MUTEX_H

#include "noncopyable.h"
#include "CurrentThread.h"

#include <pthread.h>
#include <assert.h>

class MutexLock : noncopyable {
 public:
  MutexLock() : holder_(0) { pthread_mutex_init(&mutex_, nullptr); }
  ~MutexLock() { assert(holder_ == 0); pthread_mutex_destroy(&mutex_); }
  
  pthread_mutex_t* getPhreadMutex() { return &mutex_; }

  void lock() {
    pthread_mutex_lock(&mutex_);
    holder_ = CurrentThread::tid();
  }

  void unlock() {
    holder_ = 0;
    pthread_mutex_unlock(&mutex_);
  }
  
 private:
  pid_t holder_;
  pthread_mutex_t mutex_;
};


class MutexLockGuard : noncopyable {
 public:
  explicit MutexLockGuard(MutexLock& mutex) : mutex_(mutex) { mutex_.lock(); }
  ~MutexLockGuard() { mutex_.unlock(); }

 private:
  MutexLock& mutex_;
};


#define MutexLockGuard(x) static_assert(false, "missing MutexLockGuard variable name")
#endif  // REACTOR_BASE_MUTEX_H
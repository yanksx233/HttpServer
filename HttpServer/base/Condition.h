#ifndef REACTOR_BASE_CONDITION_H
#define REACTOR_BASE_CONDITION_H

#include "noncopyable.h"
#include "Mutex.h"

#include <pthread.h>
#include <time.h>
#include <errno.h>


class Condition : noncopyable {
 public:
  explicit Condition(MutexLock& mutex) : mutex_(mutex) 
  { pthread_cond_init(&pcond_, nullptr); }
  
  ~Condition() { pthread_cond_destroy(&pcond_); }
  
  void wait() { pthread_cond_wait(&pcond_, mutex_.getPhreadMutex()); }
  // return ture if timeout
  bool waitForSeconds(int seconds) {
    struct timespec abstime;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &abstime);
    abstime.tv_sec += static_cast<time_t>(seconds);
    return ETIMEDOUT == pthread_cond_timedwait(&pcond_, mutex_.getPhreadMutex(), &abstime);
  }

  void notify() { pthread_cond_signal(&pcond_); }
  void notifyAll() {pthread_cond_broadcast(&pcond_); }

 private:
  MutexLock& mutex_;
  pthread_cond_t pcond_;
};


#endif  // REACTOR_BASE_CONDITION_H
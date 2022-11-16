#ifndef REACTOR_BASE_EVENTLOOPTHREAD_H
#define REACTOR_BASE_EVENTLOOPTHREAD_H

#include "noncopyable.h"

#include "Mutex.h"
#include "Condition.h"
#include "Thread.h"

#include <functional>

class EventLoop;

class EventLoopThread : noncopyable {
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;
  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                  const std::string name = std::string());
  ~EventLoopThread();

  EventLoop* startLoop();

 private:
  void threadFunc();

  MutexLock mutex_;
  Condition cond_;  // guarded by mutex_
  EventLoop* ploop_;  // guarded by mutex_
  Thread thread_;
  ThreadInitCallback callback_;
};



#endif  // REACTOR_BASE_EVENTLOOPTHREAD_H
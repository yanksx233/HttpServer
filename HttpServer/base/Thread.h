#ifndef REACTOR_BASE_THREAD_H
#define REACTOR_BASE_THREAD_H

#include "noncopyable.h"
#include "CountDownLatch.h"

#include <atomic>
#include <functional>

class Thread : noncopyable {
 public:
  typedef std::function<void()> ThreadFunc;

  explicit Thread(ThreadFunc, const std::string& name = std::string());
  ~Thread();

  void start();
  int join();

  bool started() const { return started_; }
  pid_t tid() const { return tid_; }
  const std::string& name() const { return name_; }
  
  static int getNumCreated() { return numCreated.load(); }

 private:
  ThreadFunc func_;
  bool started_;
  bool joined_;
  pid_t tid_;
  pthread_t pthreadId_;
  std::string name_;
  CountDownLatch latch_;

  static std::atomic<int32_t> numCreated;
};




#endif  // REACTOR_BASE_THREAD_H
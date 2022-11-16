#ifndef REACTOR_BASE_EVENTLOOPTHREADPOOL_H
#define REACTOR_BASE_EVENTLOOPTHREADPOOL_H

#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>


class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable {
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThreadPool(EventLoop* baseLoop, const std::string& name);
  ~EventLoopThreadPool() = default;

  void setThreadNum(int numThreads) { numThreads_ = numThreads; }

  void start(const ThreadInitCallback& cb);

  EventLoop* getNextLoop();
  EventLoop* getLoopFromHash(size_t hash);

  const std::string& name() const { return name_; }
  bool started() const { return started_; }
  
 private:
  EventLoop* baseLoop_;
  std::string name_;
  int numThreads_;
  bool started_;
  int next_;

  // typedef  EventLoopThreadPtr;
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  std::vector<EventLoop*> loops_;
};







#endif
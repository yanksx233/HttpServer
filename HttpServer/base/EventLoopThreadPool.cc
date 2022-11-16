#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "EventLoopThread.h"

#include <assert.h>


EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& name)
  : baseLoop_(baseLoop),
    name_(name),
    numThreads_(0),
    started_(false),
    next_(0) {}

void EventLoopThreadPool::start(const ThreadInitCallback& cb) {
  assert(!started_);
  baseLoop_->assertInLoopThread();
  started_ = true;

  for (int i = 0; i < numThreads_; ++i) {
    char buf[name_.size()+32];
    snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
    
    threads_.push_back(std::make_unique<EventLoopThread>(cb, std::string(buf)));
    loops_.push_back(threads_.back()->startLoop());
  }

  assert(threads_.size() == static_cast<size_t>(numThreads_));
  assert(loops_.size() == static_cast<size_t>(numThreads_));

  if (numThreads_ == 0 && cb) {
    cb(baseLoop_);
  }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
  baseLoop_->assertInLoopThread();
  assert(started_);

  EventLoop* loop = baseLoop_;
  if (!loops_.empty()) {
    loop = loops_[next_];
    ++next_;
    if (next_ >= numThreads_) {
      next_ = 0;
    }
  }
  return loop;
}

EventLoop* EventLoopThreadPool::getLoopFromHash(size_t hash) {
  baseLoop_->assertInLoopThread();
  assert(started_);

  EventLoop* loop = baseLoop_;
  if (!loops_.empty()) {
    loop = loops_[hash % loops_.size()];
  }
  return loop;
}
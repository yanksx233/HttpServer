#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const std::string name)
  : mutex_(),
    cond_(mutex_),
    ploop_(nullptr),
    thread_(std::bind(&EventLoopThread::threadFunc, this), name),
    callback_(cb) {}

EventLoopThread::~EventLoopThread() {
  if (ploop_ != nullptr) {
    ploop_->quit();
    thread_.join();
  }
}

EventLoop* EventLoopThread::startLoop() {
  assert(!thread_.started());
  thread_.start();

  EventLoop* loop = nullptr;
  {
    MutexLockGuard lock(mutex_);
    while (ploop_ == nullptr) {
      cond_.wait();
    }
    loop = ploop_;
  }
  return loop;
}

void EventLoopThread::threadFunc() {
  EventLoop loop;
  if (callback_) {
    callback_(&loop);
  }

  {
    MutexLockGuard lock(mutex_);
    ploop_ = &loop;
    cond_.notify();
  }
  loop.loop();

  MutexLockGuard lock(mutex_);
  ploop_ = nullptr;
}
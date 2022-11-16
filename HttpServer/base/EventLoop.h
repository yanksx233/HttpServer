#ifndef REACTOR_BASE_EVENTLOOP_H
#define REACTOR_BASE_EVENTLOOP_H

#include "noncopyable.h"
#include "Mutex.h"
#include "CurrentThread.h"
#include "TimerQueue.h"

#include <unistd.h>
#include <vector>
#include <memory>
#include <functional>


class Channel;
class Poller;

class EventLoop : noncopyable {
 public:
  typedef std::function<void()> Functor;

  EventLoop();
  ~EventLoop();

  EventLoop* getEventLoopOfCurrentThread();
  void loop();
  void quit();
  
  void runInLoop(const Functor& cb);
  void queueInLoop(Functor cb);
  void wakeup();  // wakeup form poll/epoll wait

  TimerId runAt(TimerCallback cb, Timestamp when);
  TimerId runAfter(TimerCallback cb, double seconds);
  TimerId runEvery(TimerCallback cb, double interval);
  void cancel(TimerId timerId);

  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);
  bool hasChannel(Channel* channel);

  void assertInLoopThread() {
    if (!isInLoopThread()) {
      abortNotInLoopThread();
    }
  }

  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

 private:
  void abortNotInLoopThread();
  void printActiveChannels() const;
  void handleWakeupRead();
  void doPendingFunctors();

  bool looping_;
  bool quit_;
  bool eventHandling_;
  const pid_t threadId_;
  std::unique_ptr<Poller> poller_;
  std::vector<Channel*> activeChannels_;

  mutable MutexLock mutex_;
  std::vector<Functor> pendingFunctors_;  // guarded by mutex_
  bool callingPendingFunctors_;
  int wakeupFd_;
  std::unique_ptr<Channel> pwakeupChannel_;
  
  std::unique_ptr<TimerQueue> timerQueue_;
};


#endif  // REACTOR_BASE_EVENTLOOP_H
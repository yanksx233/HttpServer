#include "Thread.h"
#include "CurrentThread.h"
#include "Logging.h"

#include <unistd.h>
#include <sys/syscall.h>
#include <assert.h>
#include <sys/prctl.h>
#include <pthread.h>


namespace CurrentThread {
  __thread int t_cachedTid;
  __thread char t_tidString[32];
  __thread int t_tidStringLength;
  __thread const char* t_threadName;

  void cacheTid() {
    if (t_cachedTid == 0) {
      t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
      t_tidStringLength = snprintf(t_tidString, sizeof(t_tidString), "%05d", t_cachedTid);
    }
  }

}  // namespace CurrentThread


struct ThreadData {
  Thread::ThreadFunc func_;
  std::string name_;
  pid_t* p_tid_;
  CountDownLatch* p_latch_;

  ThreadData(Thread::ThreadFunc func,
             std::string name,
             pid_t* tid,
             CountDownLatch* latch)
    : func_(std::move(func)),
      name_(name),
      p_tid_(tid),
      p_latch_(latch) {}

  void runInThread() {
    *p_tid_ = CurrentThread::tid();
    p_tid_ = nullptr;
    p_latch_->countDown();
    p_latch_ = nullptr;

    CurrentThread::t_threadName = name_.empty() ? "Thread" : name_.c_str();
    ::prctl(PR_SET_NAME, CurrentThread::t_threadName);
    func_();
    CurrentThread::t_threadName = "Finished";
  }
};

void* startThread(void* obj) {
  ThreadData* data = static_cast<ThreadData*>(obj);
  data->runInThread();
  delete data;
  return nullptr;
}

std::atomic<int32_t> Thread::numCreated(0);  // FIXME: 是否需要在析构函数里--？

Thread::Thread(ThreadFunc func, const std::string& name)
  : func_(std::move(func)),
    started_(false),
    joined_(false),
    tid_(0),
    pthreadId_(0),
    name_(name),
    latch_(1)
{
  if (name_.empty()) {
    int num = ++numCreated;
    char buf[32];
    snprintf(buf, sizeof(buf), "Thread%d", num);
    name_ = buf;
  }
}

Thread::~Thread() {
  if (started_ && !joined_) {
    pthread_detach(pthreadId_);
  }
}

void Thread::start() {
  assert(!started_);
  started_ = true;
  ThreadData* data = new ThreadData(func_, name_, &tid_, &latch_);
  if (pthread_create(&pthreadId_, nullptr, startThread, data)) {
    started_ = false;
    delete data;
    LOG_SYSFATAL << "Failed in pthread_create";
  }
  else {
    latch_.wait();
    assert(tid_ > 0);
  }
}

int Thread::join() {
  assert(started_);
  assert(!joined_);
  joined_ = true;
  return pthread_join(pthreadId_, nullptr);
}
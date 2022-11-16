#ifndef REACTOR_BASE_CURRENTTHREAD_H
#define REACTOR_BASE_CURRENTTHREAD_H

namespace CurrentThread
{
  extern __thread int t_cachedTid;
  extern __thread char t_tidString[32];
  extern __thread int t_tidStringLength;
  extern __thread const char* t_threadName;
  void cacheTid();

  inline int tid() {
    if (__builtin_expect(t_cachedTid == 0, 0)) {  // @para1 == @para2 的概率更大
      cacheTid();
    }
    return t_cachedTid;
  }

  inline const char* tidString() {
    return t_tidString;
  }

  inline int tidStringLength() {
    return t_tidStringLength;
  }

  inline const char* threadName() {
    return t_threadName;
  }
  
} // namespace CurrentThread


#endif  // REACTOR_BASE_CURRENTTHREAD_H
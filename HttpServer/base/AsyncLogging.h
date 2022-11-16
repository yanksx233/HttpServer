#ifndef REACTOR_BASE_ASYNCLOGGING_H
#define REACTOR_BASE_ASYNCLOGGING_H

#include "noncopyable.h"
#include "Thread.h"
#include "LogStream.h"
#include "Mutex.h"

#include <memory>
#include <atomic>
#include <string>
#include <vector>


class AppendFile : noncopyable {
 public:
  AppendFile(const std::string& filename);
  ~AppendFile();

  void append(const char* logline, int len);
  void flush() { ::fflush(fp_); }

  int writtenBytes() const { return writtenBytes_; }

 private:
  FILE* fp_;
  size_t writtenBytes_;

  char buf_[64*1024];
};


class LogFile : noncopyable {
 public:
  LogFile(const std::string& basename,
          int rollSize,
          bool threadSafe = true,
          int checkEvery = 1024);
  ~LogFile() = default;

  void rollFile();
  void rollFile(struct tm* tm_time);

  void append(const char* logline, int len);
  void flush();

 private:
  static std::string getFileName(const std::string& basename, struct tm* tm_time);
  void appendUnlocked(const char* logline, int len);

  const std::string basename_;
  int rollSize_;    // 文件过大时换新文件，注意是bytes而不是行数
  int checkEvery_;  // 进入新的一天时换新文件

  int count_;
  int lastRollDay_;
  std::unique_ptr<MutexLock> mutex_;
  std::unique_ptr<AppendFile> file_;
};


class AsyncLogging : noncopyable {
 public:
  AsyncLogging(const std::string& basename, int rollSize, int flushInterval = 3);
  ~AsyncLogging();

  void start();
  void stop();

  void append(const char* logline, int len);

  bool running() const { return running_; }

 private:
  void threadFunc();

  typedef FixedBuffer<kLargeBuffer> Buffer;

  const int flushInterval_;
  std::atomic<bool> running_;
  std::string basename_;
  int rollSize_;
  Thread thread_;

  MutexLock mutex_;
  Condition cond_;  // guarded by mutex_
  std::unique_ptr<Buffer> currentBuffer_;  // guarded by mutex_
  std::unique_ptr<Buffer> nextBuffer_;  // guarded by mutex_
  std::vector<std::unique_ptr<Buffer>> buffers_;  // guarded by mutex_
};


#endif  // REACTOR_BASE_ASYNCLOGGING_H
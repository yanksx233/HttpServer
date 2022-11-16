#ifndef REACTOR_BASE_LOGGING_H
#define REACTOR_BASE_LOGGING_H

#include "noncopyable.h"
#include "LogStream.h"

#include <string>
#include <functional>


class Logger : noncopyable {
 public:
  enum LogLevel
  {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    NUM_LOG_LEVELS,
  };

  typedef std::function<void(const char* msg, int len)> OutputFunc;
  typedef std::function<void()> FlushFunc;

  Logger(const char* filename, int line, LogLevel level);
  Logger(const char* filename, int line, bool toAbort);
  ~Logger();
  
  LogStream& stream() { return impl_.stream_; }
  
  static LogLevel logLevel();
  static void setLogLevel(LogLevel level);
  static void setOutput(OutputFunc);
  static void setFlush(FlushFunc);

  static void useAsyncLog(const std::string& basename, int rollSize, int flushInterval = 3);

 private:

class Impl {
 public:
  Impl(LogLevel level, int old_errno, const char* filename, int line);
  void formatTime();
  void filenameLine();

  LogStream stream_;
  LogLevel level_;
  std::string basename_;
  int line_;
};

  Impl impl_;
};

extern Logger::LogLevel g_logLevel;
inline Logger::LogLevel Logger::logLevel() {
  return g_logLevel;
}

#define LOG_TRACE if (Logger::logLevel() <= Logger::TRACE) \
  Logger(__FILE__, __LINE__, Logger::TRACE).stream()
#define LOG_DEBUG if (Logger::logLevel() <= Logger::DEBUG) \
  Logger(__FILE__, __LINE__, Logger::DEBUG).stream()
#define LOG_INFO if (Logger::logLevel() <= Logger::INFO) \
  Logger(__FILE__, __LINE__, Logger::INFO).stream()
#define LOG_WARN Logger(__FILE__, __LINE__, Logger::WARN).stream()
#define LOG_ERROR Logger(__FILE__, __LINE__, Logger::ERROR).stream()
#define LOG_FATAL Logger(__FILE__, __LINE__, Logger::FATAL).stream()
#define LOG_SYSERR Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL Logger(__FILE__, __LINE__, true).stream()

const char* strerror_tl(int savedErrno);

#endif  // REACTOR_BASE_LOGGING_H
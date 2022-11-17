#include "Logging.h"
#include "CurrentThread.h"
#include "AsyncLogging.h"

#include <sys/time.h>
#include <assert.h>

__thread char t_errnobuf[512];
__thread char t_time[64];
__thread time_t t_lastSecond;


const char* strerror_tl(int savedErrno) {
  return ::strerror_r(savedErrno, t_errnobuf, sizeof(t_errnobuf));
}

const char* LogLevelName[Logger::NUM_LOG_LEVELS] = {
  "TRACE",
  "DEBUG",
  "INFO",
  "WARN",
  "ERROR",
  "FATAL",
};

Logger::Impl::Impl(LogLevel level, int old_errno, const char* filename, int line)
  : stream_(),
    level_(level),
    basename_(filename),
    line_(line)
{
  formatTime();
  filenameLine();
  CurrentThread::tid();
  stream_ << CurrentThread::tidString() << " ";
  stream_ << LogLevelName[level] << " - ";
  if (old_errno != 0) {
    stream_ << strerror_tl(old_errno)
      << "(errno = " << old_errno << " ) - ";
  }
}

void Logger::Impl::formatTime() {
  timeval now;
  ::gettimeofday(&now, nullptr);

  if (now.tv_sec != t_lastSecond) {
    time_t second = now.tv_sec;
    t_lastSecond = second;
    struct tm* p_tm = ::localtime(&second); 
    int n = snprintf(t_time, sizeof(t_time), "%4d-%02d-%02d %02d:%02d:%02d",  // 年-月-日 时:分:秒
                     p_tm->tm_year+1900, p_tm->tm_mon+1, p_tm->tm_mday, 
                     p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec);
    assert(n == 19); (void)n;
  }
  snprintf(t_time+19, sizeof(t_time)-19, ",%06ld", now.tv_usec);
  stream_ << t_time << " - ";
}

void Logger::Impl::filenameLine() {
  stream_ << basename(basename_.c_str()) << ':' << line_ << " - ";
}


void defaultOutput(const char* msg, int len) {
  fwrite(msg, 1, len, stdout);
}

void defaultFlush()
{
  fflush(stdout);
}

Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;
Logger::LogLevel g_logLevel = Logger::INFO;  // default INFO

void Logger::setOutput(OutputFunc out) {
    g_output = out;
}

void Logger::setFlush(FlushFunc flush) {
    g_flush = flush;
}

void Logger::setLogLevel(LogLevel level) {
  g_logLevel = level;
}

Logger::Logger(const char* filename, int line, LogLevel level)
  : impl_(level, 0, filename, line) {}

Logger::Logger(const char* filename, int line, bool toAbort)
  : impl_(toAbort?FATAL:ERROR, errno, filename, line) {}

Logger::~Logger() {
  stream() << '\n';
  const LogStream::Buffer& buf(stream().buffer());
  g_output(buf.data(), buf.length());
  if (impl_.level_ == FATAL) {
    g_flush();
    abort();
  }
}

void Logger::useAsyncLog(const std::string& basename, int rollSize, int flushInterval) {
  static AsyncLogging asyncLog(basename, rollSize, flushInterval);  // singleton
  if (!asyncLog.running()) {
    setOutput(std::bind(&AsyncLogging::append, &asyncLog, std::placeholders::_1, std::placeholders::_2));
    asyncLog.start();
  }
}
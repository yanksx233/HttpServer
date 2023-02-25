#include "AsyncLogging.h"
#include "Timestamp.h"
#include "Logging.h"

#include <stdio.h>
#include <sys/stat.h>


AppendFile::AppendFile(const std::string& filename)
  : writtenBytes_(0)
{
  std::string pathname = filename.substr(0, filename.find_last_of('/'));
  struct stat st;
  if (stat(pathname.c_str(), &st) < 0) {
    assert(mkdir(pathname.c_str(), 0777) == 0);  // 不能递归创建
  }
  else {
    assert(S_ISDIR(st.st_mode));
  }

  fp_ = ::fopen(filename.c_str(), "ae");  // append | close on exec
  assert(fp_);
  ::setbuffer(fp_, buf_, sizeof(buf_));
}

AppendFile::~AppendFile() {
  ::fclose(fp_);
}

void AppendFile::append(const char* logline, int len) {
  size_t written = 0;
  while (written != static_cast<size_t>(len)) {
    size_t remain = len - written;
    size_t n = ::fwrite_unlocked(logline+written, 1, remain, fp_);
    if (n != remain) {
      int err = ferror(fp_);
      if (err) {
        fprintf(stderr, "AppendFile::append() failed: %s\n", strerror_tl(err));
        break;
      }
    }
    written += n;  // 有无 0 < n < remain 然后 break 导致 written 计数偏少的情况？
  }
  writtenBytes_ += written;
}


LogFile::LogFile(const std::string& basename,
                 int rollSize,
                 bool threadSafe,
                 int checkEvery)
  : basename_(basename),
    rollSize_(rollSize),
    checkEvery_(checkEvery),
    count_(0),
    lastRollDay_(0),
    mutex_(threadSafe ? new MutexLock : nullptr)
{
  // assert(basename_.find('/') == std::string::npos);
  rollFile();
}

void LogFile::rollFile() {
  time_t now = time(nullptr);
  struct tm tm_time;
  localtime_r(&now, &tm_time);
  rollFile(&tm_time);
}

void LogFile::rollFile(struct tm* tm_time) {
  lastRollDay_ = tm_time->tm_mday;
  std::string filename = getFileName(basename_, tm_time);
  file_.reset(new AppendFile(filename));
}

std::string LogFile::getFileName(const std::string& basename, struct tm* tm_time) {
  std::string filename(basename);

  char buf[64];
  strftime(buf, sizeof(buf), ".%Y%m%d-%H%M%S.log", tm_time);

  filename += buf;  // 还可以添加 hostname pid等
  return filename;
}

void LogFile::append(const char* logline, int len) {
  if (mutex_) {
    MutexLockGuard lock(*mutex_);
    appendUnlocked(logline, len);
  }
  else {
    appendUnlocked(logline, len);
  }
}

void LogFile::appendUnlocked(const char* logline, int len) {
  file_->append(logline, len);
  
  if (file_->writtenBytes() > rollSize_) {
    rollFile();
  }
  else {
    ++count_;
    if (count_ >= checkEvery_) {
      count_ = 0;
      time_t now = time(nullptr);
      struct tm tm_time;
      localtime_r(&now, &tm_time);
      if (tm_time.tm_mday != lastRollDay_) {
        rollFile(&tm_time);
      }
    }
  }
}

void LogFile::flush() {
  if (mutex_) {
    MutexLockGuard lock(*mutex_);
    file_->flush();
  }
  else {
    file_->flush();
  }
}


AsyncLogging::AsyncLogging(const std::string& basename, int rollSize, int flushInterval)
  : flushInterval_(flushInterval),
    running_(false),
    basename_(basename),
    rollSize_(rollSize),
    thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
    mutex_(),
    cond_(mutex_),
    currentBuffer_(new Buffer),
    nextBuffer_(new Buffer),
    buffers_()
{
  buffers_.reserve(16);
}

AsyncLogging::~AsyncLogging() {
  if (running_) {
    stop();
  }
}

void AsyncLogging::start() {
  running_ = true;
  thread_.start();
}

void AsyncLogging::stop() {
  running_ = false;
  cond_.notify();
  thread_.join();
}

void AsyncLogging::append(const char* logline, int len) {  // 前端生成日志
  MutexLockGuard lock(mutex_);
  if (currentBuffer_->avail() > len) {
    currentBuffer_->append(logline, len);
  }
  else {
    buffers_.push_back(std::move(currentBuffer_));
    if (nextBuffer_) {
      currentBuffer_ = std::move(nextBuffer_);
    }
    else {
      currentBuffer_.reset(new Buffer);
    }
    currentBuffer_->append(logline, len);
    cond_.notify();
  }
}

void AsyncLogging::threadFunc() {  // 后端线程写日志到文件
  assert(running_ == true);
  LogFile file(basename_, rollSize_, false);
  std::unique_ptr<Buffer> newBuffer1(new Buffer);
  std::unique_ptr<Buffer> newBuffer2(new Buffer);
  std::vector<std::unique_ptr<Buffer>> buffersToWrite;
  buffersToWrite.reserve(16);

  while (running_) {
    assert(newBuffer1 && newBuffer1->length() == 0);
    assert(newBuffer2 && newBuffer2->length() == 0);
    assert(buffersToWrite.empty());

    {
      MutexLockGuard lock(mutex_);
      if (buffers_.empty()) {
        cond_.waitForSeconds(flushInterval_);
      }
      buffers_.push_back(std::move(currentBuffer_));
      currentBuffer_ = std::move((newBuffer1));
      buffersToWrite.swap(buffers_);
      if (!nextBuffer_) {
        nextBuffer_ = std::move(newBuffer2);
      }
    }

    assert(!buffersToWrite.empty());
    if (buffersToWrite.size() > 25) {
      char buf[128];
      snprintf(buf, sizeof(buf), "Dropped log messages at %s, %zd larger buffers\n",
               Timestamp::now().toLocalTime().c_str(), buffersToWrite.size()-2);
      fputs(buf, stderr);
      file.append(buf, static_cast<int>(strlen(buf)));
      buffersToWrite.erase(buffersToWrite.begin()+2, buffersToWrite.end());
    }

    for (const auto& buffer : buffersToWrite) {
      file.append(buffer->data(), buffer->length());
    }

    if (!newBuffer1) {
      assert(!buffersToWrite.empty());
      newBuffer1 = std::move(buffersToWrite.back());
      buffersToWrite.pop_back();
      newBuffer1->clear();
    }
    if (!newBuffer2) {
      assert(!buffersToWrite.empty());
      newBuffer2 = std::move(buffersToWrite.back());
      buffersToWrite.pop_back();
      newBuffer2->clear();
    }
    assert(buffersToWrite.empty());
    file.flush();
  }
  file.flush();
}
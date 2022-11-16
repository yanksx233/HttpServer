#ifndef REACTOR_BASE_LOGSTREAM_H
#define REACTOR_BASE_LOGSTREAM_H

#include "noncopyable.h"

#include <string.h>
#include <string>

const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000*1000;

template <int SIZE>
class FixedBuffer : noncopyable {
 public:
  FixedBuffer() : cur_(data_) {}
  ~FixedBuffer() {}

  void clear() { cur_ = data_; }

  void append(const char* buf, size_t len) {
    if (avail() > static_cast<int>(len)) {
      memcpy(cur_, buf, len);
      cur_ += len;
    }
  }
  
  const char* data() const { return data_; }
  char* current() { return cur_; }

  void add(size_t len) { cur_ += len; }

  int avail() const { return static_cast<int>(end() - cur_); }
  int length() const { return static_cast<int>(cur_ - data_); }

 private:
  const char* end() const { return data_ + sizeof(data_); }

  char data_[SIZE];
  char* cur_;
};


class LogStream : noncopyable {
 public:
  typedef FixedBuffer<kSmallBuffer> Buffer;
  
  const Buffer& buffer() const { return buffer_; }

  LogStream& operator<<(bool v) {
    buffer_.append(v ? "1" : "0", 1);
    return *this;
  }

  LogStream& operator<<(short);
  LogStream& operator<<(unsigned short);
  LogStream& operator<<(int);
  LogStream& operator<<(unsigned int);
  LogStream& operator<<(long);
  LogStream& operator<<(unsigned long);
  LogStream& operator<<(long long);
  LogStream& operator<<(unsigned long long);

  LogStream& operator<<(const void*);

  LogStream& operator<<(float v) {
    return operator<<(static_cast<double>(v));
  }
  LogStream& operator<<(double);

  LogStream& operator<<(char v) {
    buffer_.append(&v, 1);
    return *this;
  }

  LogStream& operator<<(const char* str) {
    if (str) {
      buffer_.append(str, strlen(str));
    }
    else {
      buffer_.append("(null)", 6);
    }
    return *this;
  }

  LogStream& operator<<(const unsigned char* str) {
    return operator<<(reinterpret_cast<const char*>(str));
  }
  
  LogStream& operator<<(const std::string& v) {
    buffer_.append(v.c_str(), v.size());
    return *this;
  }

 private:
  template<typename T>
  void formatInteger(T);

  Buffer buffer_;
  static const int kMaxNumericSize = 48;
};


#endif  // REACTOR_BASE_LOGSTREAM_H
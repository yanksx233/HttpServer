#ifndef REACTOR_BASE_BUFFER_H
#define REACTOR_BASE_BUFFER_H

#include <vector>
#include <assert.h>
#include <string>
#include <string.h>
#include <algorithm>


class Buffer /* copyable */ {
 public:
  static const size_t kCheapPrepend = 8;
  static const size_t kInitialSize = 1024;

  explicit Buffer(size_t initialSize = kInitialSize)
    : buffer_(kCheapPrepend + initialSize),
      readIndex_(kCheapPrepend),
      writeIndex_(kCheapPrepend) {}

  void swap(Buffer& rhs) {
    buffer_.swap(rhs.buffer_);
    std::swap(readIndex_, rhs.readIndex_);
    std::swap(writeIndex_, rhs.writeIndex_);
  }

  size_t readableBytes() const { return writeIndex_ - readIndex_; }
  size_t writableBytes() const { return buffer_.size() - writeIndex_; }
  size_t prependableBytes() const { return readIndex_; }
 
  const char* beginRead() const { return begin() + readIndex_; }
  const char* beginWrite() const { return begin() + writeIndex_; }
  char* beginWrite() { return begin() + writeIndex_; }
  
  void retrieve(size_t len) {
    assert(len <= readableBytes());
    if (len < readableBytes()) {
      readIndex_ += len;
    }
    else {
      retrieveAll();
    }
  }

  void retrieveUntil(const char* end) {
    assert(beginRead() <= end);
    assert(end <= beginWrite());
    retrieve(static_cast<size_t>(end - beginRead()));
  }

  void retrieveAll() {
    readIndex_ = kCheapPrepend;
    writeIndex_ = kCheapPrepend;
  }

  std::string retrieveAsString(size_t len) {
    assert(len <= readableBytes());
    std::string str(beginRead(), len);
    retrieve(len);
    return str;
  }

  std::string retrieveAllAsString() {
    return retrieveAsString(readableBytes());
  }

  void append(const char* data, size_t len) {
    ensureWritableBytes(len);
    std::copy(data, data+len, beginWrite());
    hasWritten(len);
  }

  void append(const std::string& str) {
    append(str.c_str(), str.size());
  }

  void ensureWritableBytes(size_t len) {
    if (writableBytes() < len) {
      makeSpace(len);
    }
    assert(writableBytes() >= len);
  }

  void hasWritten(size_t len) {
    assert(len <= writableBytes());
    writeIndex_ += len;
  }

  ssize_t readFd(int fd, int* savedErrno);
  
  // find \r\n
  const char* findCrlf() const {
    const char* crlf = std::search(beginRead(), beginWrite(), kCRLF, kCRLF+2);
    return crlf == beginWrite() ? nullptr : crlf;
  }

  // find \n
  const char* findEol() const {
    return static_cast<const char*>(memchr(beginRead(), '\n', readableBytes()));
  }

 private:
  char* begin() { return buffer_.data(); }
  const char* begin() const { return buffer_.data(); }

  void makeSpace(size_t len) {
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
      buffer_.resize(writeIndex_ + len);
    }
    else {  // move readable data to front
      // assert(kCheapPrepend < readIndex_);  // 从ensureWritableBytes进入这个分支时，该assert必定正确
      size_t readable = readableBytes();
      std::copy(begin()+readIndex_, begin()+writeIndex_, begin()+kCheapPrepend);
      readIndex_ = kCheapPrepend;
      writeIndex_ = readIndex_ + readable;
      assert(readableBytes() == readable);
    }
  }


  std::vector<char> buffer_;
  size_t readIndex_;
  size_t writeIndex_;

  static const char* kCRLF;
};

#endif  // REACTOR_BASE_BUFFER_H
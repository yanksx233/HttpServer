#include "Buffer.h"

#include <sys/uio.h>


const char* Buffer::kCRLF = "\r\n";

ssize_t Buffer::readFd(int fd, int* savedErrno) {
  char extrabuf[65536];
  struct iovec vec[2];

  const size_t writable = writableBytes();

  vec[0].iov_base = beginWrite();
  vec[0].iov_len = writable;
  vec[1].iov_base = extrabuf;
  vec[1].iov_len = sizeof(extrabuf);

  const ssize_t n = readv(fd, vec, 2);
  if (n < 0) {
    *savedErrno = errno;
  }
  else if (static_cast<size_t>(n) <= writable) {
    hasWritten(n);
  }
  else {
    hasWritten(writable);
    append(extrabuf, n-writable);
  }
  return n;
}


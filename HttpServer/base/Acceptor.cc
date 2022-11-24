#include "Acceptor.h"
#include "Logging.h"
#include "EventLoop.h"

#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


InetAddress::InetAddress(uint16_t port, bool loopbackOnly) {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  in_addr_t ip = loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY;
  addr_.sin_addr.s_addr = htonl(ip);
  addr_.sin_port = htons(port);
}

InetAddress::InetAddress(std::string ip, uint16_t port) {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
  addr_.sin_port = htons(port);
}

std::string InetAddress::toIp() const {
  char buf[64];
  inet_ntop(addr_.sin_family, &addr_.sin_addr, buf, static_cast<socklen_t>(sizeof(buf)));
  return buf;
}

uint16_t InetAddress::toPort() const {
  return ntohs(addr_.sin_port);
}

std::string InetAddress::toIpPort() const {
  std::string ip = toIp();
  char port[16];
  snprintf(port, sizeof(port), ":%d", toPort());
  return ip + port;
}


Socket::~Socket() {
  if (::close(sockfd_) < 0) {
    LOG_SYSERR << "Socket::~Socket()";
  }
}

void Socket::setReuseAddr(bool on) {
  int optval = on ? 1 : 0;
  int ret = setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
                       &optval, static_cast<socklen_t>(sizeof(optval)));
  
  if (ret < 0) {
    LOG_SYSERR << "Socket::setReuseAddr()";
  }
}

void Socket::setReusePort(bool on) {
  int optval = on ? 1 : 0;
  int ret = setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT,
                       &optval, static_cast<socklen_t>(sizeof(optval)));
  if (ret < 0) {
    LOG_SYSERR << "Socket::setReusePort()";
  }
}

void Socket::setKeepAlive(bool on) {
  int optval = on ? 1 : 0;
  int ret = setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
                       &optval, static_cast<socklen_t>(sizeof(optval)));
  if (ret < 0) {
    LOG_SYSERR << "Socket::setKeepAlive()";
  }
}

void Socket::bindAddr(const InetAddress& addr) {
  int ret = ::bind(sockfd_, addr.sockaddr(), sizeof(struct sockaddr));
  if (ret < 0) {
    LOG_SYSFATAL << "Socket::bindAddr()";
  }
}

int Socket::accept(InetAddress* p_peerAddr) {
  sockaddr_in addr;
  socklen_t addrLen = sizeof(addr);
  int connfd = ::accept4(sockfd_,
                        static_cast<sockaddr*>(static_cast<void*>(&addr)),
                        &addrLen, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (connfd >= 0) {
    p_peerAddr->setSockaddr(addr);
  }
  else {
    LOG_SYSERR << "Socket::accept()";
  }
  return connfd;
}

void Socket::listen() {
  if (::listen(sockfd_, SOMAXCONN) < 0) {
    LOG_SYSFATAL << "Socket::listen()";
  }
}

void Socket::shutdown() {
  if (::shutdown(sockfd_, SHUT_RDWR) < 0) {
    LOG_SYSERR << "Socket::shutdown()";
  }
}

void Socket::shutdownWrite() {
  if (::shutdown(sockfd_, SHUT_WR) < 0) {
    LOG_SYSERR << "Socket::shutdownWrite()";
  }
}

int createNonblockingSocket(sa_family_t family)
{
  int sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
  if (sockfd < 0)
  {
    LOG_SYSFATAL << "createNonblockingSocket()";
  }
  return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reusePort)
  : loop_(loop),
    acceptSocket_(createNonblockingSocket(listenAddr.family())),
    acceptChannel_(loop_, acceptSocket_.fd()),
    listening_(false),
    idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
  assert(idleFd_ >= 0);
  acceptSocket_.setReuseAddr(true);
  acceptSocket_.setReusePort(reusePort);
  acceptSocket_.bindAddr(listenAddr);
  acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));  // FIXME: here implicitly ignore timeArgs
}

Acceptor::~Acceptor() {
  acceptChannel_.disableAll();
  acceptChannel_.remove();
}

void Acceptor::listen() {
  loop_->assertInLoopThread();
  listening_ = true;
  acceptSocket_.listen();
  acceptChannel_.enableReading();
}

void Acceptor::handleRead() {  // TODO: 一次读完所有等待接受的连接，不然连接较多时每次epoll只读一个，且一直被唤醒，效率低。
  loop_->assertInLoopThread();

  InetAddress peerAddr;
  int connfd = acceptSocket_.accept(&peerAddr);
  if (connfd >= 0) {
    if (newConnectionCallback_) {
      newConnectionCallback_(connfd, peerAddr);
    }
    else {
      if (::close(connfd) < 0) {
        LOG_SYSERR << "Socket::handleRead(), close connected fd error";
      }
    }
  }
  else {
    if (errno == EMFILE) {
      ::close(idleFd_);  // 关闭后可能被其他线程抢占fd
      idleFd_ = ::accept(acceptSocket_.fd(), nullptr, nullptr);
      // assert(idleFd_ >= 0);
      ::close(idleFd_);
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
      // assert(idleFd_ >= 0);
    }
    else {
      LOG_SYSERR << "Socket::handleRead(), accept error";
    }
  }
}
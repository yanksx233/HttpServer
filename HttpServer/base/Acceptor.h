#ifndef REACTOR_BASE_ACCEPTOR_H
#define REACTOR_BASE_ACCEPTOR_H

#include "noncopyable.h"
#include "Channel.h"

#include <functional>
#include <netinet/in.h>
#include <string>

class EventLoop;


class InetAddress /* copyable */ {
 public:
  // Used for listening
  explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false); 
  InetAddress(std::string ip, uint16_t port);  // ip should be "1.2.3.4"
  explicit InetAddress(const struct sockaddr_in& addr) : addr_(addr) {}
  
  void setSockaddr(const struct sockaddr_in& addr) { addr_ = addr; }

  sa_family_t family() const { return addr_.sin_family; }
  const struct sockaddr* sockaddr() const
  { return static_cast<const struct sockaddr*>(static_cast<const void*>(&addr_)); }

  std::string toIp() const;
  uint16_t toPort() const;
  std::string toIpPort() const;

 private:
  struct sockaddr_in addr_;
};


class Socket : noncopyable {
 public:
  explicit Socket(int sockfd) : sockfd_(sockfd) {}
  ~Socket();
  
  void setReuseAddr(bool on);
  void setReusePort(bool on);
  void setKeepAlive(bool on);
  void bindAddr(const InetAddress& addr);

  int accept(InetAddress* p_peerAddr);
  void listen();
  void shutdown();
  void shutdownWrite();

  int fd() const { return sockfd_; }
  
 private:
  const int sockfd_;
};


class Acceptor : noncopyable {
 public:
  typedef std::function<void(int sockfd, const InetAddress&)> NewConnectionCallback;

  Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reusePort);
  ~Acceptor();

  void setNewConnectionCallback(const NewConnectionCallback& cb) { newConnectionCallback_ = cb; }
  void listen();
  bool listening() const { return listening_; }

  void handleRead();

 private:
  EventLoop* loop_;
  Socket acceptSocket_;
  Channel acceptChannel_;
  NewConnectionCallback newConnectionCallback_;
  bool listening_;
  int idleFd_;
};



#endif  // REACTOR_BASE_ACCEPTOR_H

#ifndef REACTOR_BASE_TCPSERVER_H
#define REACTOR_BASE_TCPSERVER_H

#include "noncopyable.h"
#include "Acceptor.h"
#include "Callbacks.h"

#include <map>
#include <atomic>
#include <string>

class EventLoop;
class EventLoopThreadPool;


class TcpServer : noncopyable {
 public:  
  TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& name);
  ~TcpServer();

  void setThreadNum(int numThreads);
  void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }

  void start();

  void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
  void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
  void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

 private:
  void newConnection(int sockfd, const InetAddress& peerAddr);  // for Acceptor
  void removeConnection(const TcpConnectionPtr& conn);
  void removeConnectionInLoop(const TcpConnectionPtr& conn);

  EventLoop* loop_;
  std::unique_ptr<Acceptor> acceptor_;
  const std::string ipPort_;
  std::string name_;
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  std::atomic<bool> started_;
  std::map<std::string, TcpConnectionPtr> connections_;

  std::unique_ptr<EventLoopThreadPool> threadPool_;
  ThreadInitCallback threadInitCallback_;
  int nextConnId_;
};

#endif  // REACTOR_BASE_TCPSERVER_H
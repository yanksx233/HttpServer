#ifndef REACTOR_BASE_TCPCONNECTION_H
#define REACTOR_BASE_TCPCONNECTION_H

#include "noncopyable.h"
#include "Acceptor.h"
#include "Buffer.h"
#include "Callbacks.h"

#include <boost/any.hpp>
#include <atomic>


class EventLoop;
class Channel;


class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
 public:
  TcpConnection(EventLoop* loop,
                const std::string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
  ~TcpConnection();

  // called when TcpServer accepts a new connection
  void connectEstablished();   // should be called only once
  // called when TcpServer has removed me from its map
  void connectDestroyed();  // should be called only once

  EventLoop* getLoop() const { return loop_; }
  const std::string& name() const { return name_; }
  const InetAddress& localAddr() const { return localAddr_; }
  const InetAddress& peerAddr() const { return peerAddr_; }
  bool connected() const { return state_ == kConnected; }

  void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
  void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
  void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
  void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }

  Buffer* inputBuffer() { return &inputBuffer_; }
  Buffer* outputBuffer() { return &outputBuffer_; }

  void setContext(const boost::any& context) { context_ = context; }
  const boost::any& getContext() const { return context_; }

  void send(const void* message, size_t len);
  void send(const std::string& message);
  void send(Buffer* message);
  void sendInLoop(const std::string& message);

  void shutdown();
  void shutdownInLoop();

 private:
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

  void setState(StateE state) { state_ = state; }
  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();

  const char* stateToString() const;

  EventLoop* loop_;
  std::string name_;
  std::atomic<StateE> state_;
  std::unique_ptr<Socket> socket_;
  std::unique_ptr<Channel> channel_;
  InetAddress localAddr_;
  InetAddress peerAddr_;

  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  CloseCallback closeCallback_;

  Buffer inputBuffer_;
  Buffer outputBuffer_;
  boost::any context_;

};


#endif  // REACTOR_BASE_TCPCONNECTION_H
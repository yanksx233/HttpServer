#include "TcpServer.h"
#include "Logging.h"
#include "TcpConnection.h"
#include "Buffer.h"
#include "EventLoop.h"
#include "EventLoopThread.h"
#include "EventLoopThreadPool.h"
#include "Timestamp.h"


void defaultConnectionCallback(const TcpConnectionPtr& conn) {
  LOG_TRACE << conn->localAddr().toIpPort() << " -> "
            << conn->peerAddr().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
}

void defaultMessageCallback(const TcpConnectionPtr&, Buffer* buf, Timestamp) {
  buf->retrieveAll();
}

struct sockaddr_in getLocalAddr(int sockfd) {
  struct sockaddr_in localAddr;
  memset(&localAddr, 0, sizeof(localAddr));
  socklen_t addrLen = sizeof(localAddr);
  if (getsockname(sockfd, static_cast<sockaddr*>(static_cast<void*>(&localAddr)), &addrLen) < 0) {
    LOG_SYSERR << "getLocalAddr()";
  }
  return localAddr;
}


TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& name)
  : loop_(loop),
    acceptor_(new Acceptor(loop_, listenAddr, true)),
    ipPort_(listenAddr.toIpPort()),
    name_(name),
    connectionCallback_(defaultConnectionCallback),
    messageCallback_(defaultMessageCallback),
    started_(false),
    threadPool_(new EventLoopThreadPool(loop_, name_)),
    nextConnId_(1)
{
  acceptor_->setNewConnectionCallback(
    std::bind(&TcpServer::newConnection, this, _1, _2)
  );
}

TcpServer::~TcpServer() {
  loop_->assertInLoopThread();
  LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] dtor";

  for (auto& item : connections_) {
    TcpConnectionPtr conn(item.second);
    item.second.reset();
    conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    // 由io线程执行conn的析构，TcpServer的线程池析构时会唤醒io线程并等待io线程结束，io线程被唤醒后会执行pendingFuncs，因此可以确保connectDestroyed被执行
  }
}

void TcpServer::setThreadNum(int numThreads) {
  assert(!started_);
  assert(numThreads >= 0);
  threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() {
  bool noStart = false;
  if (started_.compare_exchange_strong(noStart, true)) {
    threadPool_->start(threadInitCallback_);
    assert(!acceptor_->listening());
    loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
  }
  assert(started_ == true);
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
  loop_->assertInLoopThread();
  EventLoop* ioLoop = threadPool_->getNextLoop();

  char buf[64];
  snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;  // FIXME: 没有--，不是现有连接数，是运行以来总连接数
  std::string connName = name_ + buf;

  InetAddress localAddr(getLocalAddr(sockfd));
  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));
  connections_[connName] = conn;
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, _1));  // TODO: bind conn to _1 似乎conn就不能析构了
  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
  loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
  loop_->assertInLoopThread();
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "], connection: " << conn->name();
  assert(connections_.erase(conn->name()) == 1);

  EventLoop* ioLoop = conn->getLoop();
  ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
#include "TcpConnection.h"
#include "EventLoop.h"
#include "Logging.h"
#include "Timestamp.h"

#include <errno.h>


TcpConnection::TcpConnection(EventLoop* loop,
                             const std::string& name,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr) 
  : loop_(loop),
    name_(name),
    state_(kConnecting),
    socket_(new Socket(sockfd)),
    channel_(new Channel(loop, sockfd)),
    localAddr_(localAddr),
    peerAddr_(peerAddr) 
{
  channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, _1));
  channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
  channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
  channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

  LOG_DEBUG << "TcpConnection::ctor[" <<  name_ << "] at " << this
            << " fd = " << sockfd;
  socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
  LOG_DEBUG << "TcpConnection::dtor[" <<  name_ << "] at " << this
            << ", fd = " << channel_->fd()
            << ", state = " << stateToString();
  assert(state_ == kDisconnected);
}

void TcpConnection::connectEstablished() {
  loop_->assertInLoopThread();
  assert(state_ == kConnecting);
  setState(kConnected);
  channel_->tie(shared_from_this());
  channel_->enableReading();

  connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed() {
  loop_->assertInLoopThread();
  if (state_ == kConnected) {  // handleClose是被动关闭连接，调用handleClose后不会再执行这里，可用于server主动关闭连接，由server自行管理其map
    setState(kDisconnected);
    channel_->disableAll();
    connectionCallback_(shared_from_this());
  }
  channel_->remove();
}

void TcpConnection::send(const void* message, size_t len) {
  send(std::string(static_cast<const char*>(message), len));
}

void TcpConnection::send(const std::string& message) {
  if (state_ == kConnected) {
    loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, message));
  }
}

void TcpConnection::send(Buffer* message) {
  if (state_ == kConnected) {
    loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, message->retrieveAllAsString()));
  }
}

void TcpConnection::sendInLoop(const std::string& message) {
  loop_->assertInLoopThread();
  if (state_ == kDisconnected) {
    LOG_WARN << "fd " << channel_->fd() << " disconnected, give up writing";
    return;
  }

  ssize_t n = 0;
  ssize_t remain = message.size();
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {  // 没有待处理的写事件时直接write
    n = ::write(channel_->fd(), message.c_str(), message.size());
    if (n >= 0) {
      remain -= n;
      if (remain == 0 && writeCompleteCallback_) {
        loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));  // queueInLoop会在下一次处理pendingFuncs时执行，runInLoop可能会直接执行
      }
    }
    else {
      if (errno != EWOULDBLOCK) {
        LOG_SYSERR << "TcpConnection::sendInLoop()";
      }
    }
  }

  if (remain > 0) {
    outputBuffer_.append(message.data()+n, remain);
    if (!channel_->isWriting()) {
      channel_->enableWriting();
    }
  }
}

void TcpConnection::shutdown() {
  StateE connected = kConnected;
  if (state_.compare_exchange_strong(connected, kDisconnecting)) {
    assert(state_ == kDisconnecting);
    loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
  }
}

void TcpConnection::shutdownInLoop() {
  loop_->assertInLoopThread();
  if (!channel_->isWriting()) {
    // socket_->shutdownWrite();
    socket_->shutdown();
  }
}

void TcpConnection::handleRead(Timestamp receiveTime) {
  loop_->assertInLoopThread();

  int savedErrno = 0;
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0) {
    messageCallback_(shared_from_this(), inputBuffer(), receiveTime);
  }
  else if (n == 0) {
    handleClose();  // TODO: 这是否会造成多次close回调？在channel中激活read回调的时候不应该激活close回调
  }
  else {
    errno = savedErrno;
    LOG_SYSERR << "TcpConnection::handleRead()";
  }
}

void TcpConnection::handleWrite() {
  loop_->assertInLoopThread();

  if (channel_->isWriting()) {  // 这里也可用 kConnected | kDisconnecting判断
    ssize_t n = ::write(channel_->fd(), outputBuffer_.beginRead(), outputBuffer_.readableBytes());
    if (n > 0) {
      outputBuffer_.retrieve(n);
      if (outputBuffer_.readableBytes() == 0) {
        channel_->disableWriting();
        if (writeCompleteCallback_) {
          loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
        }
        if (state_ == kDisconnecting) {
          shutdownInLoop();
        }
      }
    }
    else {
      LOG_SYSERR << "TcpConnection::handleWrite()";
    }
  }
  else {  // 写之前对方就关闭了连接，服务端调用TcpConnection::handleClose()关闭了channel
    LOG_TRACE << "Connection fd = " << channel_->fd() << " is down, no more writing";
  }
}

void TcpConnection::handleClose() {
  loop_->assertInLoopThread();
  LOG_TRACE << "fd = " << channel_->fd() << " state = " << stateToString();
  assert(state_ == kConnected || state_ == kDisconnecting);
  setState(kDisconnected);
  channel_->disableAll();

  TcpConnectionPtr guardThis(shared_from_this());
  connectionCallback_(guardThis);
  closeCallback_(guardThis);  // 移除 server map 中的 this, 再执行 connectDestroyed
}

void TcpConnection::handleError() {
  int err;
  socklen_t errlen = sizeof(err);
  if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &err, &errlen) < 0) {
    err = errno;
  }
  LOG_ERROR << "TcpConnection::handleError [" << name_
            << "]: SO_ERROR = " << err << " " << strerror_tl(err);
}

const char* TcpConnection::stateToString() const {
  switch (state_) {
    case kDisconnected:
      return "kDisconnected";
    case kConnecting:
      return "kConnecting";
    case kConnected:
      return "kConnected";
    case kDisconnecting:
      return "kDisconnecting";
    default:
      return "Unknown state";
  }
}
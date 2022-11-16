#ifndef REACTOR_BASE_CALLBACKS_H
#define REACTOR_BASE_CALLBACKS_H

#include <memory>
#include <functional>

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

class Timestamp;
class Buffer;
class EventLoop;
class TcpConnection;

typedef std::function<void()> TimerCallback;
typedef std::function<void(EventLoop*)> ThreadInitCallback;
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
typedef std::function<void(const TcpConnectionPtr&)> ConnectionCallback;
typedef std::function<void(const TcpConnectionPtr&, Buffer*, Timestamp)> MessageCallback;
typedef std::function<void(const TcpConnectionPtr&)> WriteCompleteCallback;
typedef std::function<void(const TcpConnectionPtr&)> CloseCallback;






#endif // REACTOR_BASE_CALLBACKS_H
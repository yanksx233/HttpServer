#include "HttpConnection.h"
#include "base/EventLoop.h"
#include "base/TcpServer.h"
#include "base/Acceptor.h"
#include "base/Timestamp.h"
#include "base/TcpConnection.h"
#include "base/Logging.h"


int main() {
  Logger::setLogLevel(Logger::TRACE);
  Logger::useAsyncLog("./log/http", 1024*1024);

  EventLoop loop;
  InetAddress listenAddr(12345);
  TcpServer server(&loop, listenAddr, "HttpServer");

  server.setConnectionCallback(onConnection);
  server.setMessageCallback(onMessage);

  server.setThreadNum(1);
  server.start();
  loop.loop();
}
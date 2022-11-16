#include "base/EventLoop.h"
#include "base/Logging.h"
#include "base/Channel.h"
#include "base/EventLoopThread.h"
#include "base/Acceptor.h"
#include "base/Buffer.h"
#include "base/TcpServer.h"
#include "base/TcpConnection.h"
#include "base/Timestamp.h"
#include "base/AsyncLogging.h"

#include <string.h>
#include <sys/timerfd.h>
#include <sys/time.h>
#include <iostream>
#include <sys/stat.h>
#include <boost/any.hpp>


using namespace std;

EventLoop* g_loop;

void timeout(Timestamp tm) {
    printf("%s timeout!\n", tm.toLocalTime().c_str());
    g_loop->quit();
}

void testEventLoop() {  // true test
    EventLoop loop;
    g_loop = &loop;

    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    
    Channel channel(&loop, timerfd);
    channel.setReadCallback(timeout);
    channel.enableReading();

    struct itimerspec howlong;
    memset(&howlong, 0, sizeof(howlong));
    howlong.it_value.tv_sec = 5;
    ::timerfd_settime(timerfd, 0, &howlong, nullptr);

    loop.loop();

    channel.disableAll();
    channel.remove();
    ::close(timerfd);
}

int testEventLoopThread() {  // error case, no run
    EventLoopThread loopThread;
    EventLoop* ploop = loopThread.startLoop();
    g_loop = ploop;

    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    Channel channel(ploop, timerfd);

    // channel.setReadCallback(timeout);
    ploop->runInLoop(std::bind(&Channel::setReadCallback, &channel, timeout));  // timeout后EventLoopThread中的EventLoop会马上析构，于是wakeupFd被关闭
    // channel.enableReading();
    ploop->runInLoop(std::bind(&Channel::enableReading, &channel));

    struct itimerspec howlong;
    memset(&howlong, 0, sizeof(howlong));
    howlong.it_value.tv_sec = 5;
    ::timerfd_settime(timerfd, 0, &howlong, nullptr);

    sleep(howlong.it_value.tv_sec);  // 只sleep(tv_sec) ploop 刚被唤醒，于是调用cb即timeout，然后刚好调用下面插入的pendingFunc解绑channel，实现了安全退出
    
    // channel.disableAll();
    ploop->runInLoop(std::bind(&Channel::disableAll, &channel));  // 1. sleep(tv_sec+)后ploop指向析构后的位置  2. 写被关闭的wakeupFd
    // channel.remove();
    ploop->runInLoop(std::bind(&Channel::remove, &channel));   // 3. pendingFunc是在looping中回调的，ploop指向的位置都被析构了，looping已经结束，故实际不会执行对应的cb

    usleep(1000);  // 这里能保证上面的回调被执行，且timerfd还没关闭
    ::close(timerfd);  // 要保证 close 前DEL poller中对应fd的回调已经执行，否则epoll_ctl操作已关闭的fd会失败
    usleep(1000);  // 这里能保证上面的回调被执行，若timerfd被关闭，epoll_ctl DEL会失败，但仍可安全退出
    return 0;
}

void newConnection1(int sockfd, const InetAddress& peerAddr) {
    char buf[256];
    snprintf(buf, sizeof(buf), "Local time: %s", Timestamp::now().toLocalTime().c_str());
    int n = write(sockfd, buf, strlen(buf));
    printf("writed %d bytes\n", n);
    close(sockfd);
}

void newConnection2(int sockfd, const InetAddress& peerAddr) {
    char buf[256];
    snprintf(buf, sizeof(buf), "Your ip:port is %s", peerAddr.toIpPort().c_str());
    write(sockfd, buf, strlen(buf));
    close(sockfd);
}

void testAcceptor() {
    printf("main(): pid = %d\n", getpid());
    EventLoop loop;

    InetAddress listenAddr1(1234);
    Acceptor acceptor1(&loop, listenAddr1, true);
    acceptor1.setNewConnectionCallback(newConnection1);
    acceptor1.listen();

    InetAddress listenAddr2(1235);
    Acceptor acceptor2(&loop, listenAddr2, true);
    acceptor2.setNewConnectionCallback(newConnection2);
    acceptor2.listen();

    loop.loop();
}

void testBuffer() {
    Buffer buf(8);
    const char* data = "12345678910";
    buf.append(data, strlen(data));
    buf.retrieve(1);
    cout << buf.readableBytes() << ", " << buf.writableBytes() << ", " << buf.prependableBytes() << endl;
    buf.append(data, 8);
}

std::string msg1 = std::string(1000*1000, 'a');
std::string msg2 = std::string(1000, 'b');

struct Context{
    int a;
    int b;
};

void connectionCallback(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        // sleep(5);
        // conn->send(msg1);
        // conn->send(msg2);
        // conn->shutdown();
        Context a = {0, 1};
        conn->setContext(a);
        printf("new connection comming\n");
    }
    else {
        printf("connection [%s] is down\n", conn->name().c_str());
        Context info = boost::any_cast<const struct Context>(conn->getContext());
        cout << info.a << ", " << info.b << endl;
    }
}

void messageCallback(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
    Context info = boost::any_cast<const struct Context>(conn->getContext());
    info.a += 1;
    info.b += 1;
    conn->setContext(info);
    
    conn->send(buf);  // echo server
    conn->send(receiveTime.toLocalTime());  // time server
    conn->send("\n");
}

void testTcpServer() {
    EventLoop loop;
    InetAddress listenAddr(12345);
    TcpServer server(&loop, listenAddr, "test_server");
    server.setConnectionCallback(connectionCallback);
    server.setMessageCallback(messageCallback);

    // server.setThreadNum(0);
    server.setThreadNum(1);
    server.start();
    loop.loop();
}

void testAsyncLogging() {
    /// usage 1
    // AsyncLogging alog("./test_log/async", 5000);
    // Logger::setOutput(std::bind(&AsyncLogging::append, &alog, _1, _2));
    // alog.start();

    /// usage 2, test singleton
    Logger::useAsyncLog("./test_log/async", 5000);
    Logger::useAsyncLog("./test_log/async", 5000);
    Logger::useAsyncLog("./test_log/async1", 5000);
    Logger::useAsyncLog("./test_log/async2", 5000);

    int64_t count = 0;
    for (int i = 0; i < 10000; ++i) {
        for (int j = 0; j < 50; ++j) {
            LOG_INFO << "Hello 0123456789" << " abcdefghijklmnopqrstuvwxyz " << ++count;
        }
        usleep(500);  // 一秒内输出所有log导致新log文件跟原来有一样的文件名
    }
}

int main() {
    Logger::setLogLevel(Logger::TRACE);
    testTcpServer();
    return 0;
}
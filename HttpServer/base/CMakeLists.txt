set(base_SOURCE
    Acceptor.cc
    AsyncLogging.cc
    Buffer.cc
    Channel.cc
    DefaultPoll.cc
    EpollPoller.cc
    EventLoop.cc
    EventLoopThread.cc
    EventLoopThreadPool.cc
    Logging.cc
    LogStream.cc
    Poller.cc
    PollPoller.cc
    TcpConnection.cc
    TcpServer.cc
    Thread.cc
    TimerQueue.cc
    Timestamp.cc)

add_library(base ${base_SOURCE})

target_link_libraries(base pthread)
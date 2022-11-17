# C++ HTTP Server

## 简介

自行实现的基于 muduo 的 HTTP 服务器，支持 GET、POST（未实现数据库） 请求。

采用 Reactor 模式

主线程负责 listenfd： 1）接受新连接并记录，将新连接交给线程池， 2）连接关闭时移除记录。

线程池负责 connfd： 读、处理事件、写。

每个线程采用 epoll 轮询 IO 事件，且用 eventfd 和 timerfd 处理唤醒和定时。



## 技术

- epoll电平触发，非阻塞IO
- one event loop per thread + thread pool
- 基于 timerfd + std::set（红黑树）的定时器，可以关闭不活跃的 HTTP 连接
- eventfd 实现异步线程唤醒
- 双缓冲异步日志
- 支持 HTTP keep-alive选项



## 运行

```shell
cd HttpServer && bash ./build.sh && cd .. 
./build/Debug/HttpServer
```


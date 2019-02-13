#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#include <memory> // unique_ptr

#define TIMEOUTMS -1 // epoll_wait超时时间，-1表示不设超时

namespace swings {

// 前置声明，不需要包含HttpRequest.h和Epoll.h
class HttpRequest;
class Epoll;

class HttpServer {
public:
    HttpServer(int port);
    ~HttpServer();
    void run(); // 启动HTTP服务器
    
private:
    void __acceptConnection(); // 接受新连接
    void __closeConnection(int idx); // 关闭连接
    void __doRequest(int idx); // 处理HTTP请求报文，这个函数由线程池调用

private:
    using ListenRequestPtr = unique_ptr<HttpRequest>;
    using EpollPtr = unique_ptr<Epoll>;

    int port_; // 监听端口
    int listenFd_; // 监听套接字
    ListenRequestPtr listenRequest_; // 监听套接字的HttpRequest实例
    EpollPtr epoll_; // epoll实例
    // ThreadPool threadPool_; // TODO 线程池
}; // class HttpServer

} // namespace swings

#endif

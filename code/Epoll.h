#ifndef __EPOLL_H__
#define __EPOLL_H__

#include <functional> // function
#include <vector> // vector
#include <memory> // shared_ptr

#include <sys/epoll.h> // epoll_event

#define MAXEVENTS 1024

namespace swings {

class HttpRequest;
class ThreadPool;

class Epoll {
public:
    using NewConnectionCallback = std::function<void()>;
    using CloseConnectionCallback = std::function<void(HttpRequest*)>;
    using HandleRequestCallback = std::function<void(HttpRequest*)>;
    using HandleResponseCallback = std::function<void(HttpRequest*)>;

    Epoll();
    ~Epoll();
    int add(int fd, HttpRequest* request, int events); // 注册新描述符
    int mod(int fd, HttpRequest* request, int events); // 修改描述符状态
    int del(int fd, HttpRequest* request, int events); // 从epoll中删除描述符
    int wait(int timeoutMs); // 等待事件发生, 返回活跃描述符数量
    void handleEvent(int listenFd, std::shared_ptr<ThreadPool>& threadPool, int eventsNum); // 调用事件处理函数
    void setOnConnection(const NewConnectionCallback& cb) { onConnection_ = cb; } // 设置新连接回调函数
    void setOnCloseConnection(const CloseConnectionCallback& cb) { onCloseConnection_ = cb; } // 设置关闭连接回调函数
    void setOnRequest(const HandleRequestCallback& cb) { onRequest_ = cb; } // 设置处理请求回调函数
    void setOnResponse(const HandleResponseCallback& cb) { onResponse_ = cb; } // 设置响应请求回调函数

private: 
    using EventList = std::vector<struct epoll_event>;
    
    int epollFd_;
    EventList events_;
    NewConnectionCallback onConnection_;
    CloseConnectionCallback onCloseConnection_;
    HandleRequestCallback onRequest_;
    HandleResponseCallback onResponse_;
}; // class Epoll

} // namespace swings

#endif

#ifndef __EPOLL_H__
#define __EPOLL_H__

#define MAXEVENTS 1024

namespace swings {
class Epoll {
public:
    Epoll();
    ~Epoll();
    int add(int fd, HttpRequest* request, int events); // 注册新描述符
    int mod(int fd, HttpRequest* request, int events); // 修改描述符状态
    int del(int fd, HttpRequest* request, int events); // 从epoll中删除描述符
    int wait(int timeoutMs); // 等待事件发生, 返回活跃描述符数量
    void handleEvent(int listenFd, int eventsNum); // 调用事件处理函数
    void setOnConnection(NewConnectionCallback& cb) { onConnection_ = cb; }
private: 
    using EventList = std::vector<struct epoll_event>;
    using NewConnectionCallback = std::function<void()>;
    
    int epollFd_;
    EventList events_;
    NewConnectionCallback onConnection_;
}; // class Epoll
} // namespace swings

#endif

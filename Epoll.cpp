#include "Epoll.h"

using namespace swings;

Epoll::Epoll() 
    : epollFd_(::epoll_create1(EPOLL_CLOEXEC))
      events_(MAXEVENTS)
{
    assert(epollFd_ >= 0);
}

Epoll::~Epoll()
{
    ::close(epollFd_);
}

int Epoll::add(int fd, HttpRequest* request, int events)
{
    struct epoll_event event;
    event.data.ptr = (void*)request;
    event.events = events;
    int ret = ::epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event);

    return ret;
}

int Epoll::mod(int fd, HttpRequest* request, int events)
{
    struct epoll_event event;
    event.data.ptr = (void*)request;
    event.events = events;
    int ret = ::epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event);

    return ret;
}

int Epoll::del(int fd, HttpRequest* request, int events)
{
    struct epoll_event event;
    event.data.ptr = (void*)request;
    event.events = events;
    int ret = ::epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &event);

    return ret;
}

int Epoll::wait(int timeoutMs)
{
    int eventsNum = ::epoll_wait(epollFd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    assert(eventsNum >= 0);
    
    return eventsNum;
}

void handleEvent(int listenFd, int eventsNum)
{
    if(eventsNum == 0) {
        cout << "EPOLL Timeout..." << endl; // FIXME 使用日志库
        return;
    }

    for(int i = 0; i < eventsNum; ++i) {
        HttpRequest* request = (HttpRequest*)(events[i].data.ptr);
        int fd = request -> fd();

        if(fd == listenFd) {
            onConnection_(); // 新连接回调函数
        } else {
            // 排除错误事件
            if((events[i].events & EPOLLERR) ||
               (events[i].events & EPOLLHUP) ||
               (!events[i].events & EPOLLIN)) {
                ::close(fd); // FIXME 释放fd对应的HttpRequest资源
                continue;
            }
            request -> doRequest(); // TODO 把任务交给线程池去做
        }
    }
    return;
}


#include "Epoll.h"
#include "HttpRequest.h"

#include <iostream>
#include <cassert>
#include <cstring> // perror

#include <unistd.h> // close

using namespace swings;

Epoll::Epoll() 
    : epollFd_(::epoll_create1(EPOLL_CLOEXEC)),
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
    event.data.ptr = (void*)request; // XXX 使用cast系列函数
    event.events = events;
    int ret = ::epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event);
    std::cout << "[Epoll::add] fd = " << fd << std::endl;
    return ret;
}

int Epoll::mod(int fd, HttpRequest* request, int events)
{
    struct epoll_event event;
    event.data.ptr = (void*)request; // XXX 使用cast系列函数
    event.events = events;
    int ret = ::epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event);
    std::cout << "[Epoll::mod] fd = " << fd << std::endl;
    return ret;
}

int Epoll::del(int fd, HttpRequest* request, int events)
{
    struct epoll_event event;
    event.data.ptr = (void*)request; // XXX 使用cast系列函数
    event.events = events;
    int ret = ::epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &event);
    std::cout << "[Epoll::del] fd = " << fd << std::endl;
    return ret;
}

int Epoll::wait(int timeoutMs)
{
    int eventsNum = ::epoll_wait(epollFd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    if(eventsNum == 0) {
        std::cout << "[Epoll::wait] nothing happen, epoll timeout" << std::endl;
    } else if(eventsNum < 0) {
        std::perror("[Epoll::wait] epoll");
    }
    
    return eventsNum;
}

void Epoll::handleEvent(int listenFd, int eventsNum)
{
    assert(eventsNum > 0);
    for(int i = 0; i < eventsNum; ++i) {
        HttpRequest* request = (HttpRequest*)(events_[i].data.ptr); // XXX 使用cast系列函数
        int fd = request -> fd();

        if(fd == listenFd) {
            // 新连接回调函数
            onConnection_();
        } else {
            // 排除错误事件
            if((events_[i].events & EPOLLERR) ||
               (events_[i].events & EPOLLHUP) ||
               (!events_[i].events & EPOLLIN)) {
                // 出错则关闭连接
                onCloseConnection_(request);
                std::cout << "[Epoll::handleEvent] error happen in socket(fd=" 
                          << fd << "), close it" << std::endl;
                continue;
            }
            onRequest_(request); // TODO 把任务交给线程池去做
        }
    }
    return;
}


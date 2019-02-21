#include "Epoll.h"
#include "HttpRequest.h"
#include "ThreadPool.h"

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
    return ret;
}

int Epoll::mod(int fd, HttpRequest* request, int events)
{
    struct epoll_event event;
    event.data.ptr = (void*)request; // XXX 使用cast系列函数
    event.events = events;
    int ret = ::epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event);
    return ret;
}

int Epoll::del(int fd, HttpRequest* request, int events)
{
    struct epoll_event event;
    event.data.ptr = (void*)request; // XXX 使用cast系列函数
    event.events = events;
    int ret = ::epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &event);
    return ret;
}

int Epoll::wait(int timeoutMs)
{
    int eventsNum = ::epoll_wait(epollFd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    if(eventsNum == 0) {
        // printf("[Epoll::wait] nothing happen, epoll timeout\n");
    } else if(eventsNum < 0) {
        printf("[Epoll::wait] epoll : %s\n", strerror(errno));
    }
    
    return eventsNum;
}

void Epoll::handleEvent(int listenFd, std::shared_ptr<ThreadPool>& threadPool, int eventsNum)
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
                request -> setNoWorking();
                // 出错则关闭连接
                onCloseConnection_(request);
            } else if(events_[i].events & EPOLLIN) {
                request -> setWorking();
                threadPool -> pushJob(std::bind(onRequest_, request));
            } else if(events_[i].events & EPOLLOUT) {
                request -> setWorking();
                threadPool -> pushJob(std::bind(onResponse_, request));
            } else {
                printf("[Epoll::handleEvent] unexpected event\n");
            }
        }
    }
    return;
}

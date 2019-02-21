#include "HttpRequest.h"
#include "Timer.h"

#include <iostream>
#include <functional>

#include <sys/epoll.h>

using namespace swings;

void func(HttpRequest* req) {
    // std::cout << "timeout id=" << req -> fd() << std::endl;
    return;
}

int main()
{
    HttpRequest req1(2), req2(4), req3(9), req4(7);
    TimerManager timerManager;
    timerManager.addTimer(&req1, 2000, std::bind(&func, &req1));
    timerManager.addTimer(&req2, 4000, std::bind(&func, &req2));
    timerManager.addTimer(&req3, 9000, std::bind(&func, &req3));
    // std::cout << "[main] add timer finish" << std::endl;
    int epollFd = ::epoll_create1(EPOLL_CLOEXEC);
    // std::cout << "[main] create epoll finish" << std::endl;

    int flag = 0;

    while(1) {
        int time = timerManager.getNextExpireTime();
        // std::cout << "wait " << time << " ms ..." << std::endl;
        if(time == -1)
            break;
        epoll_event events[1];
        epoll_wait(epollFd, events, 1, time);

        timerManager.handleExpireTimers();
        if(flag == 0) {
            // 测试删除定时器
            timerManager.delTimer(&req2);
            // 测试连续两次添加定时器
            timerManager.addTimer(&req4, 5000, std::bind(&func, &req4));
            timerManager.addTimer(&req4, 6000, std::bind(&func, &req4));
            flag = 1;
        }
    }
}


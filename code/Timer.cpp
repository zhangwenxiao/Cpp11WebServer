#include "Timer.h"
#include "HttpRequest.h"

#include <cassert>

using namespace swings;

void TimerManager::addTimer(HttpRequest* request, 
                     const int& timeout, 
                     const TimeoutCallBack& cb)
{
    std::unique_lock<std::mutex> lock(lock_);
    assert(request != nullptr);

    updateTime();
    Timer* timer = new Timer(now_ + MS(timeout), cb);
    timerQueue_.push(timer);

    // std::cout << "[TimerManager::addTimer] now_ = " << Clock::to_time_t(now_)
    //           << " , new timer = " << Clock::to_time_t(now_ + MS(timeout)) << std::endl;

    // 对同一个request连续调用两次addTimer，需要把前一个定时器删除
    if(request -> getTimer() != nullptr)
        delTimer(request);

    request -> setTimer(timer);
}

void TimerManager::delTimer(HttpRequest* request)
{
    std::unique_lock<std::mutex> lock(lock_);
    assert(request != nullptr);

    Timer* timer = request -> getTimer();
    if(timer == nullptr)
        return;

    // 如果这里写成delete timeNode，会使priority_queue里的对应指针变成垂悬指针
    // 正确的方法是惰性删除
    timer -> del();
    // 防止request -> getTimer()访问到垂悬指针
    request -> setTimer(nullptr);
}

void TimerManager::handleExpireTimers()
{
    std::unique_lock<std::mutex> lock(lock_);
    updateTime();
    while(!timerQueue_.empty()) {
        Timer* timer = timerQueue_.top();
        assert(timer != nullptr);
        // 定时器被删除
        if(timer -> isDeleted()) {
            // std::cout << "[TimerManager::handleExpireTimers] timer = " << Clock::to_time_t(timer -> getExpireTime())
            //           << " is deleted" << std::endl;
            timerQueue_.pop();
            delete timer;
            continue;
        }
        // 优先队列头部的定时器也没有超时，return
        if(std::chrono::duration_cast<MS>(timer -> getExpireTime() - now_).count() > 0) {
            // std::cout << "[TimerManager::handleExpireTimers] no timeout : " 
            //           << Clock::to_time_t(timer -> getExpireTime()) << " - " 
            //           << Clock::to_time_t(now_) << std::endl;
            std::cout << "[TimerManager::handleExpireTimers] there is no timeout timer" << std::endl;
            return;
        }
        std::cout << "[TimerManager::handleExpireTimers] timeout" << std::endl;
        // 超时
        timer -> runCallBack();
        timerQueue_.pop();
        delete timer;
    }
}

int TimerManager::getNextExpireTime()
{
    std::unique_lock<std::mutex> lock(lock_);
    updateTime();
    int res = -1;
    while(!timerQueue_.empty()) {
        Timer* timer = timerQueue_.top();
        if(timer -> isDeleted()) {
            // std::cout << "[TimerManager::getNextExpireTime] timer = " << Clock::to_time_t(timer -> getExpireTime())
            //           << " is deleted" << std::endl;
            timerQueue_.pop();
            delete timer;
            continue;
        }
        res = std::chrono::duration_cast<MS>(timer -> getExpireTime() - now_).count();
        res = (res < 0) ? 0 : res;
        break;
    }
    return res;
}

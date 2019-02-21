#ifndef __TIMER_H__
#define __TIMER_H__

#include <functional>
#include <chrono>
#include <queue>
#include <vector>
#include <iostream>
#include <cassert>
#include <mutex>

namespace swings {

using TimeoutCallBack = std::function<void()>;
using Clock = std::chrono::high_resolution_clock;
using MS = std::chrono::milliseconds;
using Timestamp = Clock::time_point;

class HttpRequest;

class Timer {
public:
    Timer(const Timestamp& when, const TimeoutCallBack& cb)
        : expireTime_(when),
          callBack_(cb),
          delete_(false) {}
    ~Timer() {}
    void del() { delete_ = true; }
    bool isDeleted() { return delete_; }
    Timestamp getExpireTime() const { return expireTime_; }
    void runCallBack() { callBack_(); }

private:
    Timestamp expireTime_;
    TimeoutCallBack callBack_;
    bool delete_;
}; // class Timer

// 比较函数，用于priority_queue，时间值最小的在队头
struct cmp {
    bool operator()(Timer* a, Timer* b)
    {
        assert(a != nullptr && b != nullptr);
        return (a -> getExpireTime()) > (b -> getExpireTime());
    }
};

class TimerManager {
public:
    TimerManager() 
        : now_(Clock::now()) {}
    ~TimerManager() {}
    void updateTime() { now_ = Clock::now(); }

    void addTimer(HttpRequest* request, const int& timeout, const TimeoutCallBack& cb); // timeout单位ms
    void delTimer(HttpRequest* request);
    void handleExpireTimers();
    int getNextExpireTime(); // 返回超时时间(优先队列中最早超时时间和当前时间差)

private:
    using TimerQueue = std::priority_queue<Timer*, std::vector<Timer*>, cmp>;

    TimerQueue timerQueue_;
    Timestamp now_;
    std::mutex lock_;
}; // class TimerManager

} // namespace swings

#endif

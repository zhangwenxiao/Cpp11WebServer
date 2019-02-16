#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <vector>
#include <queue>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>

namespace swings {

class ThreadPool {
public:
    using JobFunction = std::function<void()>;

    ThreadPool(int numWorkers);
    ~ThreadPool();
    void pushJob(const JobFunction& job);

private:
    std::vector<std::thread> threads_;
    std::mutex lock_;
    std::condition_variable cond_;
    std::queue<JobFunction> jobs_;
    bool stop_;
};

}

#endif

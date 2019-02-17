#include "ThreadPool.h"

#include <iostream>

using namespace swings;

ThreadPool::ThreadPool(int numWorkers)
    : stop_(false)
{
    numWorkers = numWorkers <= 0 ? 1 : numWorkers;
    for(int i = 0; i < numWorkers; ++i)
        threads_.emplace_back([this]() {
            while(!stop_) {
                JobFunction func;
                {
                    std::unique_lock<std::mutex> lock(this -> lock_);    
                    while(!(this -> stop_) && this -> jobs_.empty())
                        cond_.wait(lock);
                    func = jobs_.front();
                    jobs_.pop();
                }
                std::cout << "[ThreadPool::ThreadPool] threadid=" << std::this_thread::get_id() 
                          << " get a job" << std::endl;
                func();
            }
        });
}

ThreadPool::~ThreadPool()
{
    std::cout << "[ThreadPool::~ThreadPool] threadpool is remove" << std::endl;
    stop_ = true;
    cond_.notify_all();
    for(auto& thread: threads_)
        thread.join();
}

void ThreadPool::pushJob(const JobFunction& job)
{
    {
        std::unique_lock<std::mutex> lock(lock_);
        std::cout << "[ThreadPool::pushJob] new job was push to threadpool" << std::endl;
        jobs_.push(job);
    }
    cond_.notify_one();
}

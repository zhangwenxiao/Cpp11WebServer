#include "ThreadPool.h"

#include <iostream>
#include <cassert>

using namespace swings;

ThreadPool::ThreadPool(int numWorkers)
    : stop_(false)
{
    numWorkers = numWorkers <= 0 ? 1 : numWorkers;
    for(int i = 0; i < numWorkers; ++i)
        threads_.emplace_back([this]() {
            while(1) {
                JobFunction func;
                {
                    std::unique_lock<std::mutex> lock(lock_);    
                    while(!stop_ && jobs_.empty())
                        cond_.wait(lock);
                    if(jobs_.empty() && stop_) {
                        // printf("[ThreadPool::ThreadPool] threadid = %lu return\n", pthread_self());
                        return;
                    }
                    // if(!jobs_.empty()) {
                    func = jobs_.front();
                    jobs_.pop();
                    // }
                }
                if(func) {
                    // printf("[ThreadPool::ThreadPool] threadid = %lu get a job\n", pthread_self()/*std::this_thread::get_id()*/);
                    func();
                    // printf("[ThreadPool::ThreadPool] threadid = %lu job finish\n", pthread_self()/*std::this_thread::get_id()*/);
                } 
            }
        });
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(lock_);
        stop_ = true;
    } 
    cond_.notify_all();
    for(auto& thread: threads_)
        thread.join();
    // printf("[ThreadPool::~ThreadPool] threadpool is remove\n");
}

void ThreadPool::pushJob(const JobFunction& job)
{
    {
        std::unique_lock<std::mutex> lock(lock_);
        jobs_.push(job);
    }
    // printf("[ThreadPool::pushJob] push new job\n");
    cond_.notify_one();
}

#include "ThreadPool.h"

#include <chrono>

using namespace swings;

void func(int n) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    printf("%d\n", n);
}

int main() 
{
    ThreadPool tp(4);
    for(int i = 0; i < 10; ++i) {
        tp.pushJob(std::bind(&func, i));
    }
    return 0;
}
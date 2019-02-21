SRC := ../code/HttpRequest.cpp ../code/Timer.cpp ../code/Buffer.cpp
HEADER := ../code/HttpRequest.h ../code/Timer.h ../code/Buffer.h

test_timer: test_timer.cpp ${SRC} ${HEADER}
	g++ -std=c++11 -I ../code -o test_timer test_timer.cpp ${SRC} -lpthread

test_thread_pool: test_thread_pool.cpp ../code/ThreadPool.cpp ../code/ThreadPool.h
	g++ -std=c++11 -I ../code -o test_thread_pool test_thread_pool.cpp ../code/ThreadPool.cpp -lpthread

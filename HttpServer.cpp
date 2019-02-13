#include "HttpServer.h"
#include <cassert>

using namespace swings;

HttpServer::HttpServer(int port) 
    : port_(port),
      listenFd_(createListenFd(port_))
{
    assert(listenFd_ >= 0);
}

HttpServer::~HttpServer()
{
}

void HttpServer::run()
{
    // 注册监听套接字的可读事件，ET模式
    HttpRequest listenRequest(listenFd_);
    epoll_.add(listenFd_, &listenRequest, (EPOLLIN | EPOLLET));

    // 注册新连接回调函数
    epoll_.setOnConnection(std::bind(&HttpServer::acceptConnection, this));

    // 注册关闭连接回调函数
    epoll_.setOnCloseConnection(std::bind(&HttpServer::closeConnection, this));

    // 事件循环
    while(1) { // FIXME 服务器应该能够停止
        int eventsNum = epoll_.wait(TIMEOUTMS);
        epoll_.handleEvent(listenFd_, eventsNum);
    }
}

void HttpServer::acceptConnection()
{
    // 接受新连接 
    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(struct sockaddr_in));
    socklen_t clientAddrLen = 0;
    int acceptFd = ::accept(listenFd_, (struct sockaddr*)&clientAddr, &clientAddrLen);

    // accept系统调用出错
    if(acceptFd == -1) {
        perror("accept");
        return;
    }

    // 设置为非阻塞
    if(setNonBlocking(acceptFd) == -1) {
        ::close(acceptFd); // 此时acceptFd还没有HttpRequst资源，不需要释放
        return;
    }

    // 用shared_ptr管理连接套接字
    HttpRequestPtr conn(new HttpRequest(acceptFd, requests_.size()));
    requests_.push_back(conn);
    
    // 注册连接套接字到epoll
    // 可读，边缘触发，保证任一时刻只被一个线程处理
    epoll_.add(acceptFd, conn.get(), (EPOLLIN | EPOLLET | EPOLLONESHOT)); 
}

void HttpServer::closeConnection(int idx)
{
    // TODO 是否需要epoll_.del
    assert(idx >= 0 && idx < requests_.size());

    // request在request_的末尾，直接pop_back
    if(idx == requests_.size() - 1)
        requests_.pop_back();
    // request不在request_的末尾，则与末尾交换后pop_back
    else {
        // 把要删除的request和requests_的末尾交换，再pop_back
        std::swap(requests_[idx], requests_[requests_.size() - 1]);
        requests_.pop_back();
        // 原来的末尾元素被交换到前面，需要修改其下标
        requests_[idx] -> setIdx(idx);
    }
}

// FIXME 这个函数在线程池运行，需要考虑线程安全问题
void HttpServer::doRequest(int idx)
{
    {
        std::lock_guard<std::mutex> lock(lock_);
        assert(idx >= 0 && idx < requests_.size());
        HttpRequestPtr request = requests_[idx]; // 多线程访问requests_需要加锁
    }
    int fd = request -> fd();
    int nRead, readErrno;

    while(1) {
        nRead = request -> read(&readErrno);

        // 已读到文件尾或无可读数据，断开连接
        if(nRead == 0) {
            {
                std::lock_guard<std::mutex> lock(lock_);
                closeConnection(idx); // 断开连接
            }
            return; // 释放线程使用权
        }

        // 非EAGAIN错误，断开连接
        if(nRead < 0 && (readErrno != EAGAIN)) {
            {
                std::lock_guard<std::mutex> lock(lock_);
                closeConnection(idx); // 断开连接
            }
            return; // 释放线程使用权
        }

        // EAGAIN错误则修改描述符状态并释放线程使用权
        if(nRead < 0 && readErrno == EAGAIN)
            break;

        // 解析报文，出错则断开连接
        if(!request -> parseRequest()) {
            {
                std::lock_guard<std::mutex> lock(lock_);
                closeConnection(idx); // 断开连接
            }
            return; // 释放线程使用权
        }

        // 解析完成
        if(request -> parseFinish()) {
            // FIXME 判断长连接的方法应封装到HttpRequest内
            HttpResponse response(200, request -> getPath(), request -> getHeader("Connection") == "Keep-Alive");
            response -> sendResponse();
            
            if(request -> getHeader("Connection") == "Keep-Alive") { // 长连接
                // 重置报文解析相关状态
                request -> resetParse();
            } else { // 短连接
                {
                    std::lock_guard<std::mutex> lock(lock_);
                    closeConnection(idx); // 断开连接
                }
                return; // 释放线程使用权
            }
        }
    }

    // 需要修改已注册描述符（因为使用了EPOLLONESHOT）
    {
        std::lock_guard<std::mutex> lock(lock_);
        epoll_.mod(fd, request.get(), (EPOLLIN | EPOLLET | EPOLLONESHOT));
    }
}


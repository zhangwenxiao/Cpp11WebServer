#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Utils.h"
#include "Epoll.h"
#include "ThreadPool.h"
#include "Timer.h"

#include <iostream>
#include <functional> // bind
#include <cassert> // assert
#include <cstring> // bzero
 
#include <unistd.h> // close, read
#include <sys/socket.h> // accept
#include <arpa/inet.h> // sockaddr_in

using namespace swings;

HttpServer::HttpServer(int port) 
    : port_(port),
      listenFd_(utils::createListenFd(port_)),
      listenRequest_(new HttpRequest(listenFd_)),
      epoll_(new Epoll()),
      threadPool_(new ThreadPool(NUM_WORKERS)),
      timerManager_(new TimerManager())
{
    assert(listenFd_ >= 0);
    // printf("[HttpServer::HttpServer] create listen socket, fd = %d, port = %d\n", listenFd_, port_);
}

HttpServer::~HttpServer()
{
    // printf("[HttpServer::~HttpServer]\n");
}

void HttpServer::run()
{
    // 注册监听套接字到epoll（可读事件，ET模式）
    epoll_ -> add(listenFd_, listenRequest_.get(), (EPOLLIN | EPOLLET));

    // 注册新连接回调函数
    epoll_ -> setOnConnection(std::bind(&HttpServer::__acceptConnection, this));

    // 注册关闭连接回调函数
    epoll_ -> setOnCloseConnection(std::bind(&HttpServer::__closeConnection, this, std::placeholders::_1));

    // 注册请求处理回调函数
    epoll_ -> setOnRequest(std::bind(&HttpServer::__doRequest, this, std::placeholders::_1));

    // 注册响应处理回调函数
    epoll_ -> setOnResponse(std::bind(&HttpServer::__doResponse, this, std::placeholders::_1));

    // printf("[HttpServer::run] server is running ...\n");
    // 服务器循环
    // XXX 服务器应该能够停止
    while(1) {
        // int time;
        // {
        //     std::unique_lock<std::mutex> lock(timerLock_);
        //     time = timerManager_ -> getNextExpireTime(); 
        // }
        // std::cout << "[HttpServer::run] next expire time = " << time << std::endl;
        
        // 等待事件发生
        int eventsNum = epoll_ -> wait(TIMEOUTMS);
        // int eventsNum = epoll_ -> wait(time);

        if(eventsNum > 0) {
            // printf("[HttpServer::run] epoll return %d\n", eventsNum);
            // 分发事件处理函数
            epoll_ -> handleEvent(listenFd_, threadPool_, eventsNum);
        }   

        // {
        //     std::unique_lock<std::mutex> lock(timerLock_);
        //     // 处理超时事件
        //     timerManager_ -> handleExpireTimers();
        // }
    }
}

// ET
void HttpServer::__acceptConnection()
{
    while(1) {
        // int acceptFd = ::accept(listenFd_, nullptr, nullptr);
        int acceptFd = ::accept4(listenFd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);

        // accept系统调用出错
        if(acceptFd == -1) {
            if(errno == EAGAIN) {
                // printf("[HttpServer::__acceptConnection] there is not new connection\n");
                break;
            }
            printf("[HttpServer::__acceptConnection] accept : %s\n", strerror(errno));
            break;
        }

        // printf("[HttpServer::__acceptConnection] new connection is comming, fd = %d\n", acceptFd);

        // // 设置为非阻塞
        // if(utils::setNonBlocking(acceptFd) == -1) {
        //     // 出错则关闭
        //     // 此时acceptFd还没有HttpRequst资源，不需要释放
        //     // printf("[HttpServer::__acceptConnection] fd = %d set non blocking fail, close it\n", acceptFd);
        //     ::close(acceptFd); 
        //     break;
        // }

        // 为新的连接套接字分配HttpRequest资源
        HttpRequest* request = new HttpRequest(acceptFd);
        // 注册连接套接字到epoll（可读，边缘触发，保证任一时刻只被一个线程处理）
        epoll_ -> add(acceptFd, request, (EPOLLIN | EPOLLONESHOT));

        // {
        //     std::unique_lock<std::mutex> lock(timerLock_);
        //     timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
        // }
    }
}

// // LT
// void HttpServer::__acceptConnection()
// {
//     int acceptFd = ::accept(listenFd_, nullptr, nullptr);

//     // accept系统调用出错
//     if(acceptFd == -1) {
//         if(errno == EAGAIN) {
//             // printf("[HttpServer::__acceptConnection] there is not new connection\n");
//             return;
//         }
//         printf("[HttpServer::__acceptConnection] accept : %s\n", strerror(errno));
//         return;
//     }

//     // printf("[HttpServer::__acceptConnection] new connection is comming, fd = %d\n", acceptFd);

//     // 设置为非阻塞
//     if(utils::setNonBlocking(acceptFd) == -1) {
//         // 出错则关闭
//         // 此时acceptFd还没有HttpRequst资源，不需要释放
//         // printf("[HttpServer::__acceptConnection] fd = %d set non blocking fail, close it\n", acceptFd);
//         ::close(acceptFd); 
//         return;
//     }

//     // 为新的连接套接字分配HttpRequest资源
//     HttpRequest* request = new HttpRequest(acceptFd);
//     // 注册连接套接字到epoll（可读，边缘触发，保证任一时刻只被一个线程处理）
//     epoll_ -> add(acceptFd, request, (EPOLLIN | EPOLLONESHOT));

//     // {
//     //     std::unique_lock<std::mutex> lock(timerLock_);
//     //     timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
//     // }
// }

void HttpServer::__closeConnection(HttpRequest* request)
{
    int fd = request -> fd();
    if(request -> isWorking()) {
        // printf("[HttpServer::__closeConnection] fd = %d is working\n", fd);
        return;
    }
    // printf("[HttpServer::__closeConnection] fd = %d, close it\n", fd);
    // FIXME 这里上锁会死锁，HttpServer::run中handleExpireTimers前上了一次锁
    // {
    //     std::unique_lock<std::mutex> lock(timerLock_);
    // timerManager_ -> delTimer(request);
    // }
    // FIXME 使用了EPOLLONESHOT是否还需要epoll_.del
    epoll_ -> del(fd, request, 0);
    // 关闭套接字
    // ::close(fd);
    // 释放该套接字占用的HttpRequest资源
    delete request;
    request = nullptr;
}

// LT模式
void HttpServer::__doRequest(HttpRequest* request)
{
    assert(request != nullptr);
    int fd = request -> fd();
    // std::cout << "[HttpServer::__doRequest] something happen in socket(fd=" << fd << ")" << std::endl;
    // std::cout << "[HttpServer::__doRequest] delete timer of socket(fd=" << fd << ")" << std::endl;
    // {
    //     std::unique_lock<std::mutex> lock(timerLock_);
    //     timerManager_ -> delTimer(request);
    // }

    int readErrno;
    int nRead = request -> read(&readErrno);

    // read返回0表示客户端断开连接
    if(nRead == 0) {
        // printf("[HttpServer::__doRequest] fd = %d, client is close, close connection\n", fd);
        request -> setNoWorking();
        __closeConnection(request);
        return; 
    }

    // 非EAGAIN错误，断开连接
    if(nRead < 0 && (readErrno != EAGAIN)) {
        // printf("[HttpServer::__doRequest] fd = %d, read error, close connection\n", fd);
        request -> setNoWorking();
        __closeConnection(request);
        return; 
    }

    // EAGAIN错误则释放线程使用权，并监听下次可读事件epoll_ -> mod(...)
    if(nRead < 0 && readErrno == EAGAIN) {
        // printf("[HttpServer::__doRequest] fd = %d, read EAGAIN happen\n", fd);
        epoll_ -> mod(fd, request, (EPOLLIN | EPOLLONESHOT));
        // {
        //     std::unique_lock<std::mutex> lock(timerLock_);
        //     timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
        // }
        request -> setNoWorking();
        return;
    }

    // 解析报文，出错则断开连接
    if(!request -> parseRequest()) {
        // printf("[HttpServer::__doRequest] fd = %d, parsing error, send 400\n", fd);
        // 发送400报文
        HttpResponse response(400, "", false);
        request -> appendOutBuffer(response.makeResponse());

        // XXX 立刻关闭连接了，所以就算没写完也只能写一次？
        int writeErrno;
        request -> write(&writeErrno);
        // printf("[HttpServer::__doRequest] fd = %d, send 400 finish, close it\n", fd);
        request -> setNoWorking();
        __closeConnection(request); 
        return; 
    }

    // 解析完成
    if(request -> parseFinish()) {
        // printf("[HttpServer::__doRequest] fd = %d, parsing finish, URL=%s, Method=%s\n", 
        //        fd, (request -> getPath()).data(), (request -> getMethod()).data());
        HttpResponse response(200, request -> getPath(), request -> keepAlive());
        request -> appendOutBuffer(response.makeResponse());
        epoll_ -> mod(fd, request, (EPOLLIN | EPOLLOUT | EPOLLONESHOT));
    }
}

// LT模式
void HttpServer::__doResponse(HttpRequest* request)
{
    assert(request != nullptr);
    int fd = request -> fd();

    int toWrite = request -> writableBytes();

    if(toWrite == 0) {
        // printf("[HttpServer::__doResponse] fd = %d, nothing to write\n", fd);
        epoll_ -> mod(fd, request, (EPOLLIN | EPOLLONESHOT));
        // {
        //     std::unique_lock<std::mutex> lock(timerLock_);
        //     timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
        // }
        request -> setNoWorking();
        return;
    }

    int writeErrno;
    int ret = request -> write(&writeErrno);

    if(ret < 0 && writeErrno == EAGAIN) {
        // printf("[HttpServer::__doResponse] fd = %d, write EAGAIN\n", fd);
        epoll_ -> mod(fd, request, (EPOLLIN | EPOLLOUT | EPOLLONESHOT));
        request -> setNoWorking(); // XXX 这里需要setNoWorking吗
        return;
    }

    // 非EAGAIN错误，断开连接
    if(ret < 0 && (writeErrno != EAGAIN)) {
        // printf("[HttpServer::__doResponse] fd = %d, write error, close it\n", fd);
        request -> setNoWorking();
        __closeConnection(request);
        return; 
    }

    if(ret == toWrite) {
        // printf("[HttpServer::__doResponse] fd = %d, write finish\n", fd);
        if(request -> keepAlive()) {
            // printf("[HttpServer::__doResponse] fd = %d, keep alive\n", fd);
            request -> resetParse();
            epoll_ -> mod(fd, request, (EPOLLIN | EPOLLONESHOT));
            // {
            //     std::unique_lock<std::mutex> lock(timerLock_);
            //     timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
            // }
            request -> setNoWorking();
        }
        else {
            // printf("[HttpServer::__doResponse] fd = %d, don't keep alive, close it\n", fd);
            request -> setNoWorking();
            __closeConnection(request);
        }
        return;
    }

    // printf("[HttpServer::__doResponse] fd = %d, write doesn't finish\n", fd);
    epoll_ -> mod(fd, request, (EPOLLIN | EPOLLOUT | EPOLLONESHOT));
    request -> setNoWorking();

    return;
}

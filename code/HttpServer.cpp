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
    std::cout << "[HttpServer::HttpServer] create listen socket, fd = " 
              << listenFd_ << ", port = " << port_ << std::endl;
}

HttpServer::~HttpServer()
{
    std::cout << "[HttpServer::~HttpServer]" << std::endl;
}

void HttpServer::run()
{
    // 注册监听套接字到epoll（可读事件，ET模式）
    epoll_ -> add(listenFd_, listenRequest_.get(), (EPOLLIN/* | EPOLLET*/));

    // 注册新连接回调函数
    epoll_ -> setOnConnection(std::bind(&HttpServer::__acceptConnection, this));

    // 注册关闭连接回调函数
    epoll_ -> setOnCloseConnection(std::bind(&HttpServer::__closeConnection, this, std::placeholders::_1));

    // 注册请求处理回调函数
    epoll_ -> setOnRequest(std::bind(&HttpServer::__doRequest, this, std::placeholders::_1));

    // 注册响应处理回调函数
    epoll_ -> setOnResponse(std::bind(&HttpServer::__doResponse, this, std::placeholders::_1));

    std::cout << "[HttpServer::run] server is running ..." << std::endl;
    // 服务器循环
    // XXX 服务器应该能够停止
    while(1) {
        int time;
        {
            std::unique_lock<std::mutex> lock(timerLock_);
            time = timerManager_ -> getNextExpireTime(); 
        }
        std::cout << "[HttpServer::run] next expire time = " << time << std::endl;
        
        // 等待事件发生
        // int eventsNum = epoll_ -> wait(TIMEOUTMS);
        int eventsNum = epoll_ -> wait(time);

        if(eventsNum > 0) {
            std::cout << "[HttpServer::run] epoll return, " << eventsNum << " events happen" << std::endl;
            // 分发事件处理函数
            epoll_ -> handleEvent(listenFd_, threadPool_, eventsNum);
        }   

        {
            std::unique_lock<std::mutex> lock(timerLock_);
            // 处理超时事件
            timerManager_ -> handleExpireTimers();
        }
    }
}

// LT
void HttpServer::__acceptConnection()
{
    int acceptFd = ::accept(listenFd_, nullptr, nullptr);

    // accept系统调用出错
    if(acceptFd == -1) {
        if(errno == EAGAIN) {
            std::cout << "[HttpServer::__acceptConnection] there is not more new connection" << std::endl;
            return;
        }
        ::perror("[HttpServer::__acceptConnection] accept");
        return;
    }

    std::cout << "[HttpServer::__acceptConnection] new connection is comming" << std::endl;

    // 设置为非阻塞
    if(utils::setNonBlocking(acceptFd) == -1) {
        // 出错则关闭
        // 此时acceptFd还没有HttpRequst资源，不需要释放
        ::close(acceptFd); 
        return;
    }

    // 为新的连接套接字分配HttpRequest资源
    HttpRequest* request = new HttpRequest(acceptFd);
    // 注册连接套接字到epoll（可读，边缘触发，保证任一时刻只被一个线程处理）
    epoll_ -> add(acceptFd, request, (EPOLLIN /*| EPOLLET*/ | EPOLLONESHOT));

    {
        std::unique_lock<std::mutex> lock(timerLock_);
        timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
    }
}

void HttpServer::__closeConnection(HttpRequest* request)
{
    int fd = request -> fd();
    if(request -> isWorking()) {
        std::cout << "[HttpServer::__closeConnection] fd = " << fd << " is working, return" << std::endl;
        return;
    }
    std::cout << "[HttpServer::__closeConnection] delete timer fd=" << fd << std::endl;
    // FIXME 这里上锁会死锁，HttpServer::run中handleExpireTimers前上了一次锁
    // {
    //     std::unique_lock<std::mutex> lock(timerLock_);
        timerManager_ -> delTimer(request);
    // }
    // FIXME 使用了EPOLLONESHOT是否还需要epoll_.del
    epoll_ -> del(fd, request, 0);
    // 关闭套接字
    ::close(fd);
    // 释放该套接字占用的HttpRequest资源
    delete request;

    std::cout << "[HttpServer::__closeConnection] close connection(fd=" << fd << ")" << std::endl;
}

// LT模式
void HttpServer::__doRequest(HttpRequest* request)
{
    int fd = request -> fd();
    std::cout << "[HttpServer::__doRequest] something happen in socket(fd=" << fd << ")" << std::endl;
    std::cout << "[HttpServer::__doRequest] delete timer of socket(fd=" << fd << ")" << std::endl;
    {
        std::unique_lock<std::mutex> lock(timerLock_);
        timerManager_ -> delTimer(request);
    }

    int readErrno;
    int nRead = request -> read(&readErrno);

    // read返回0表示客户端断开连接
    if(nRead == 0) {
        std::cout << "[HttpServer::__doRequest] client(fd=" << fd << ") is close" << std::endl;
        request -> setNoWorking();
        __closeConnection(request);
        return; 
    }

    // 非EAGAIN错误，断开连接
    if(nRead < 0 && (readErrno != EAGAIN)) {
        std::cout << "[HttpServer::__doRequest] error in socket(fd=" << fd << "), close it" << std::endl;
        request -> setNoWorking();
        __closeConnection(request);
        return; 
    }

    // EAGAIN错误则释放线程使用权，并监听下次可读事件epoll_ -> mod(...)
    if(nRead < 0 && readErrno == EAGAIN) {
        std::cout << "[HttpServer::__doRequest] EAGAIN happen in socket(fd=" << fd << ")" << std::endl;
        epoll_ -> mod(fd, request, (EPOLLIN | EPOLLONESHOT));
        std::cout << "[HttpServer::__doRequest] modify socket(fd=" << fd << ") to epoll(EPOLLIN)" << std::endl;
        std::cout << "[HttpServer::__doRequest] add timer to fd = " << fd << std::endl;
        {
            std::unique_lock<std::mutex> lock(timerLock_);
            timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
        }
        request -> setNoWorking();
        return;
    }

    // 解析报文，出错则断开连接
    if(!request -> parseRequest()) {
        std::cout << "[HttpServer::__doRequest] parsing error happen in socket(fd=" 
                    << fd << "), send 400 to it" << std::endl;
        // 发送400报文
        HttpResponse response(400, "", false);
        request -> appendOutBuffer(response.makeResponse());

        // XXX 立刻关闭连接了，所以就算没写完也只能写一次？
        int writeErrno;
        request -> write(&writeErrno);
        request -> setNoWorking();
        __closeConnection(request); 
        return; 
    }

    // 解析完成
    if(request -> parseFinish()) {
        std::cout << "[HttpServer::__doRequest] parsing is finish in socket(fd=" 
                    << fd << "), send response to it" << std::endl;
        HttpResponse response(200, request -> getPath(), request -> keepAlive());
        std::cout << "[HttpServer::__doRequest] make response finish" << std::endl;
        request -> appendOutBuffer(response.makeResponse());
        std::cout << "[HttpServer::__doRequest] modify socket(fd=" << fd << ") to epoll(EPOLLIN | EPOLLOUT)" << std::endl;
        epoll_ -> mod(fd, request, (EPOLLIN | EPOLLOUT | EPOLLONESHOT));
    }
}

// LT模式
void HttpServer::__doResponse(HttpRequest* request)
{
    int fd = request -> fd();

    int toWrite = request -> writableBytes();

    if(toWrite == 0) {
        std::cout << "[HttpServer::__doResponse] nothing to write in socket(fd=" << fd << ")" << std::endl;
        epoll_ -> mod(fd, request, (EPOLLIN | EPOLLONESHOT));
        std::cout << "[HttpServer::__doResponse] add timer to fd = " << fd << std::endl;
        {
            std::unique_lock<std::mutex> lock(timerLock_);
            timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
        }
        request -> setNoWorking();
        return;
    }

    int writeErrno;
    int ret = request -> write(&writeErrno);

    if(ret < 0 && writeErrno == EAGAIN) {
        std::cout << "[HttpServer::__doResponse] EAGAIN happen in socket(fd=" << fd << ")" << std::endl;
        epoll_ -> mod(fd, request, (EPOLLIN | EPOLLOUT | EPOLLONESHOT));
        request -> setNoWorking();
        return;
    }

    // 非EAGAIN错误，断开连接
    if(ret < 0 && (writeErrno != EAGAIN)) {
        std::cout << "[HttpServer::__doResponse] error in socket(fd=" << fd << "), close it" << std::endl;
        request -> setNoWorking();
        __closeConnection(request);
        return; 
    }

    if(ret == toWrite) {
        std::cout << "[HttpServer::__doResponse] write finish" << std::endl;
        if(request -> keepAlive()) {
            request -> resetParse();
            epoll_ -> mod(fd, request, (EPOLLIN | EPOLLONESHOT));
            std::cout << "[HttpServer::__doResponse] keep alive, modify socket(fd=" 
                      << fd << ") to epoll(EPOLLIN)" << std::endl;
            std::cout << "[HttpServer::__doResponse] add timer to fd = " << fd << std::endl;
            {
                std::unique_lock<std::mutex> lock(timerLock_);
                timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
            }
            request -> setNoWorking();
        }
        else {
            std::cout << "[HttpServer::__doResponse] don't keep alive, close it" << std::endl; 
            request -> setNoWorking();
            __closeConnection(request);
        }
        return;
    }

    std::cout << "[HttpServer::__doResponse] write doesn't finish" << std::endl;
    epoll_ -> mod(fd, request, (EPOLLIN | EPOLLOUT | EPOLLONESHOT));
    request -> setNoWorking();

    return;
}

// // ET模式
// // FIXME 这个函数在线程池运行，需要考虑线程安全问题
// void HttpServer::__doRequest(HttpRequest* request)
// {
//     int fd = request -> fd();
//     int nRead, readErrno;

//     std::cout << "[HttpServer::__doRequest] something happen in socket(fd=" << fd << ")" << std::endl;

//     while(1) {
//         nRead = request -> read(&readErrno);

//         // read返回0表示客户端断开连接
//         if(nRead == 0) {
//             std::cout << "[HttpServer::__doRequest] client(fd=" << fd << ") is close" << std::endl;
//             __closeConnection(request);
//             return; 
//         }

//         // 非EAGAIN错误，断开连接
//         if(nRead < 0 && (readErrno != EAGAIN)) {
//             std::cout << "[HttpServer::__doRequest] error in socket(fd=" << fd << "), close it" << std::endl;
//             __closeConnection(request);
//             return; 
//         }

//         // EAGAIN错误则释放线程使用权，并监听下次可读事件epoll_ -> mod(...)
//         if(nRead < 0 && readErrno == EAGAIN) {
//             std::cout << "[HttpServer::__doRequest] EAGAIN happen in socket(fd=" << fd << ")" << std::endl;
//             break;
//         }

//         // 解析报文，出错则断开连接
//         if(!request -> parseRequest()) {
//             std::cout << "[HttpServer::__doRequest] parsing error happen in socket(fd=" 
//                       << fd << "), send 400 to it" << std::endl;
//             // 发送400报文
//             HttpResponse response(400, "", false);
//             request -> appendOutBuffer(response.makeResponse());

//             // XXX 立刻关闭连接了，所以就算没写完也只能写一次？
//             int writeErrno;
//             request -> write(&writeErrno);

//             __closeConnection(request); 
//             return; 
//         }

//         // 解析完成
//         if(request -> parseFinish()) {
//             std::cout << "[HttpServer::__doRequest] parsing is finish in socket(fd=" 
//                       << fd << "), send response to it" << std::endl;
//             HttpResponse response(200, request -> getPath(), request -> keepAlive());
//             std::cout << "[HttpServer::__doRequest] make response finish" << std::endl;
//             request -> appendOutBuffer(response.makeResponse());
//             __doResponse(request);
            
//             if(request -> keepAlive()) { // 长连接
//                 std::cout << "[HttpServer::__doRequest] socket(fd=" 
//                       << fd << ") is keep alive, reset its parsing status" << std::endl;
//                 // 重置报文解析相关状态
//                 request -> resetParse();
//             } else { // 短连接
//                 std::cout << "[HttpServer::__doRequest] socket(fd=" 
//                       << fd << ") isn't keep alive, close it" << std::endl;
//                 __closeConnection(request); 
//                 return; 
//             }
//         }
//     }

//     std::cout << "[HttpServer::__doRequest] modify socket(fd=" << fd << ") to epoll" << std::endl;
//     // FIXME 多线程访问epoll_，应该加锁?
//     // 需要修改已注册描述符（因为使用了EPOLLONESHOT）
//     epoll_ -> mod(fd, request, (EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT));
// }

// // ET模式
// void HttpServer::__doResponse(HttpRequest* request)
// {
//     int fd = request -> fd();

//     int toWrite = request -> writableBytes();

//     int nWritten = 0;
//     while(1) {
//         if(toWrite == 0) {
//             std::cout << "[HttpServer::__doResponse] nothing to write in socket(fd=" << fd << ")" << std::endl;
//             epoll_ -> mod(fd, request, (EPOLLIN | EPOLLET | EPOLLONESHOT));
//             return;
//         }

//         int writeErrno;
//         int ret = request -> write(&writeErrno);

//         if(ret < 0 && writeErrno == EAGAIN) {
//             std::cout << "[HttpServer::__doResponse] EAGAIN happen in socket(fd=" << fd << ")" << std::endl;
//             break;
//         }

//         // 非EAGAIN错误，断开连接
//         if(ret < 0 && (writeErrno != EAGAIN)) {
//             std::cout << "[HttpServer::__doResponse] error in socket(fd=" << fd << "), close it" << std::endl;
//             __closeConnection(request);
//             return; 
//         }

//         nWritten += ret;
//         if(nWritten >= toWrite) // XXX 正常情况nWritten不会大于toWrite
//             return;
//     }

//     epoll_ -> mod(fd, request, (EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT));
//     return;
// }

// // ET
// void HttpServer::__acceptConnection()
// {
//     // ET模式下需要一直accept直到EAGAIN
//     while(1) {
//         int acceptFd = ::accept(listenFd_, nullptr, nullptr);

//         // accept系统调用出错
//         if(acceptFd == -1) {
//             if(errno == EAGAIN) {
//                 std::cout << "[HttpServer::__acceptConnection] there is not more new connection" << std::endl;
//                 break;
//             }
//             ::perror("[HttpServer::__acceptConnection] accept");
//             break;
//         }

//         std::cout << "[HttpServer::__acceptConnection] new connection is comming" << std::endl;

//         // 设置为非阻塞
//         if(utils::setNonBlocking(acceptFd) == -1) {
//             // 出错则关闭
//             // 此时acceptFd还没有HttpRequst资源，不需要释放
//             ::close(acceptFd); 
//             return;
//         }

//         // 为新的连接套接字分配HttpRequest资源
//         HttpRequest* request = new HttpRequest(acceptFd);
//         // 注册连接套接字到epoll（可读，边缘触发，保证任一时刻只被一个线程处理）
//         epoll_ -> add(acceptFd, request, (EPOLLIN /*| EPOLLET*/ | EPOLLONESHOT));
//     } 
// }

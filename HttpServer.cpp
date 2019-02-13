#include "HttpServer.h"

#include <iostream>
#include <cassert> // assert
#include <cstring> // bzero
 
#include <unistd.h> // close, read
#include <sys/socket.h> // accept

using namespace swings;

HttpServer::HttpServer(int port) 
    : port_(port),
      listenFd_(createListenFd(port_)),
      listenRequest_(new HttpRequest(listenFd_)),
      epoll_(new Epoll())
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
    epoll_ -> add(listenFd_, listenRequest_.get(), (EPOLLIN | EPOLLET));

    // 注册新连接回调函数
    epoll_ -> setOnConnection(std::bind(&HttpServer::__acceptConnection, this));

    // 注册关闭连接回调函数
    epoll_ -> setOnCloseConnection(std::bind(&HttpServer::__closeConnection, this));

    // 注册请求处理回调函数
    epoll_ -> setOnRequest(std::bind(&HttpServer::__doRequest, this));

    std::cout << "[HttpServer::run] server is running ..." << std::endl;
    // 服务器循环
    // XXX 服务器应该能够停止
    while(1) { 
        // 等待事件发生
        int eventsNum = epoll_ -> wait(TIMEOUTMS);

        // 分发事件处理函数
        epoll_ -> handleEvent(listenFd_, eventsNum);
    }
}

void HttpServer::__acceptConnection()
{
    std::cout << "[HttpServer::__acceptConnection] new connection is comming" << std::endl;
    // 接受新连接 
    struct sockaddr_in clientAddr;
    ::bzero((char*)&clientAddr, sizeof(clientAddr));
    socklen_t clientAddrLen = 0;
    // XXX 若不关心客户端标识，可以::accept(listenFd_, nullptr, nullptr)
    int acceptFd = ::accept(listenFd_, (struct sockaddr*)&clientAddr, &clientAddrLen);

    // accept系统调用出错
    if(acceptFd == -1) {
        ::perror("[HttpServer::__acceptConnection] accept");
        return;
    }

    // 设置为非阻塞
    if(setNonBlocking(acceptFd) == -1) {
        // 出错则关闭
        // 此时acceptFd还没有HttpRequst资源，不需要释放
        ::close(acceptFd); 
        return;
    }

    // 为新的连接套接字分配HttpRequest资源
    HttpRequest* request = new HttpRequest(acceptFd);
    // 注册连接套接字到epoll（可读，边缘触发，保证任一时刻只被一个线程处理）
    epoll_.add(acceptFd, request, (EPOLLIN | EPOLLET | EPOLLONESHOT)); 
}

void HttpServer::__closeConnection(HttpRequest* request)
{
    // FIXME 使用了EPOLLONESHOT是否还需要epoll_.del
    int fd = request -> fd();
    // 关闭套接字
    ::close(fd);
    // 释放该套接字占用的HttpRequest资源
    delete request;

    std::cout << "[HttpServer::__closeConnection] close connection(fd=" << fd << ")" << std::endl;
}

// FIXME 这个函数在线程池运行，需要考虑线程安全问题
void HttpServer::__doRequest(HttpRequest* request)
{
    int fd = request -> fd();
    int nRead, readErrno;

    std::cout << "[HttpServer::__doRequest] something happen in socket(fd=" << fd << ")" << std::endl;

    while(1) {
        nRead = request -> read(&readErrno);

        // read返回0表示客户端断开连接
        if(nRead == 0) {
            std::cout << "[HttpServer::__doRequest] client(fd=" << fd << ") is close" << std::endl;
            __closeConnection(request);
            return; 
        }

        // 非EAGAIN错误，断开连接
        if(nRead < 0 && (readErrno != EAGAIN)) {
            std::cout << "[HttpServer::__doRequest] error in socket(fd=" << fd << "), close it" << std::endl;
            __closeConnection(request);
            return; 
        }

        // EAGAIN错误则释放线程使用权，并监听下次可读事件epoll_ -> mod(...)
        if(nRead < 0 && readErrno == EAGAIN) {
            std::cout << "[HttpServer::__doRequest] EAGAIN happen in socket(fd=" << fd << ")" << std::endl;
            break;
        }

        // 解析报文，出错则断开连接
        if(!request -> parseRequest()) {
            std::cout << "[HttpServer::__doRequest] parsing error happen in socket(fd=" 
                      << fd << "), send 400 to it" << std::endl;
            // 发送400报文
            HttpResponse response(400, "", false);
            response.sendResponse();

            __closeConnection(request); 
            return; 
        }

        // 解析完成
        if(request -> parseFinish()) {
            std::cout << "[HttpServer::__doRequest] parsing is finish in socket(fd=" 
                      << fd << "), send response to it" << std::endl;
            HttpResponse response(200, request -> getPath(), request -> keepAlive());
            response -> sendResponse();
            
            if(request -> keepAlive()) { // 长连接
                std::cout << "[HttpServer::__doRequest] socket(fd=" 
                      << fd << ") is keep alive, reset its parsing status" << std::endl;
                // 重置报文解析相关状态
                request -> resetParse();
            } else { // 短连接
                std::cout << "[HttpServer::__doRequest] socket(fd=" 
                      << fd << ") isn't keep alive, close it" << std::endl;
                __closeConnection(request); 
                return; 
            }
        }
    }

    std::cout << "[HttpServer::__doRequest] modify socket(fd=" << fd << ") to epoll" << std::endl;
    // FIXME 多线程访问epoll_，应该加锁?
    // 需要修改已注册描述符（因为使用了EPOLLONESHOT）
    epoll_ -> mod(fd, request, (EPOLLIN | EPOLLET | EPOLLONESHOT));
}

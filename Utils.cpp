#include "Utils.h"

#include <iostream>
#include <cstring> // bzero

#include <stdio.h> // perror
#include <unistd.h> // fcntl, close
#include <fcntl.h> // fcntl
#include <sys/socket.h> // socket, setsockopt, bind, listen
#include <arpa/inet.h> // htonl, htons

int createListenFd(int port)
{
    // 处理非法端口
    port = ((port <= 1024) || (port >= 65535)) ? 6666 : port;
    std::cout << "[Utils::createListenFd] port = " << port << std::endl;

    // 创建套接字（IPv4，TCP，非阻塞）
    int listenFd = 0;
    if((listenFd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
        ::perror("[Utils::createListenFd] socket");
        return -1;
    }

    // 避免"Address already in use"
    int optval = 1;
    if(::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)) == -1) {
        ::perror("[Utils::createListenFd] setsockopt");
        return -1;
    }

    // 绑定IP和端口
    struct sockaddr_in serverAddr;
    ::bzero((char*)&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = ::htonl(INADDY_ANY);
    serverAddr.sin_port = ::htons((unsigned short)port);
    if(::bind(listenFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        ::perror("[Utils::createListenFd] bind");
        return -1;
    }

    // 开始监听，最大监听队列为LISTENQ
    if(::listen(listenFd, LISTENQ) == -1) {
        ::perror("[Utils::createListenFd] listen");
        return -1;
    }

    // 关闭无效监听描述符
    if(listenFd == -1) {
        std::cout << "[Utils::createListenFd] create invalid listen socket, close it and return -1" << std::endl;
        ::close(listenFd);
        return -1;
    }

    std::cout << "[Utils::createListenFd] create listen socket success, fd = " << listenFd << std::endl;

    return listenFd;
}

int setNonBlocking(int fd)
{
    // 获取套接字选项
    int flag = ::fcntl(fd, F_GETFL, 0);
    if(flag == -1) {
        ::perror("[Utils::setNonBlocking] fcntl");
        std::cout << "[Utils::setNonBlocking] set socket(fd=" 
                  << fd << ") to nonblocking fail and return -1" << std::endl;
        return -1;
    }

    // 设置非阻塞
    flag |= O_NONBLOCK;
    if(::fcntl(fd, F_SETFL, flag) == -1) {
        ::perror("[Utils::setNonBlocking] fcntl");
        std::cout << "[Utils::setNonBlocking] set socket(fd=" 
                  << fd << ") to nonblocking fail and return -1" << std::endl;
        return -1;
    }

    std::cout << "[Utils::setNonBlocking] set socket(fd=" 
                  << fd << ") to nonblocking success and return 0" << std::endl;

    return 0;
}


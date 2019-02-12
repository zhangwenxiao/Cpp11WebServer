#include "HttpRequest.h"
#include <cassert>

using namespace swings;

HttpRequest::HttpRequest(int fd, int idx, Epoll epoll)
    : fd_(fd),
      idx_(idx),
      epoll_(epoll)
{
    assert(fd_ >= 0);
}

HttpRequest::~HttpRequest()
{
}

void HttpRequest::doRequest()
{
    int nRead, readErrno;
    while(1) {
        // 从套接字读数据到缓冲区
        nRead = buff_.readFd(fd_, &readErrno);

        // 已读到文件尾或无可读数据，断开连接
        if(nRead == 0) {
            break; // TODO 断开连接，这里不好处理，考虑把这个函数放在HttpServer中
        }

        // 非EAGAIN错误，断开连接
        if(nRead < 0 && (readErrno != EAGAIN)) {
            break; // TODO 断开连接
        }

        // EAGAIN错误则再次注册套接字到epoll
        if(nRead < 0 && readErrno == EAGAIN) {
            break;
        }

        // TODO 处理HTTP报文
    }

    // 重新注册可读事件到epoll（因为使用了EPOLLONESHOT）
    epoll_ -> add(fd_, this, (EPOLLIN | EPOLLET | EPOLLONESHOT));
}


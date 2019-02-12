#ifndef __HTTP_REQUEST_H__
#define __HTTP_REQUEST_H__

namespace swings {
class HttpRequest {
public:
    HttpRequest(int fd, int idx, Epoll* epoll);
    ~HttpRequest();
    void doRequest(); // 处理HTTP请求报文
    int fd() { return fd_; } // 返回文件描述符
    int idx() { return idx_; } // 返回下标
    void setIdx(int idx) { idx_ = idx; } // 设置新下标

private:
    int fd_; // 文件描述符
    int idx_; // 当前HttpRequest在HttpServer::requests_中的下标
    Buffer buff_; // 缓冲区
    Epoll* epoll_; // epoll
}; // class HttpRequest
} // namespace swings

#endif

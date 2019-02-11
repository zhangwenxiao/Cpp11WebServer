#ifndef __HTTP_REQUEST_H__
#define __HTTP_REQUEST_H__

#define MAX_BUF 8124

namespace swings {
class HttpRequest {
public:
    HttpRequest(int fd);
    ~HttpRequest();
    doRequest(); // TODO 处理HTTP请求报文
    int fd(); // 返回文件描述符
private:
    int fd_; // 文件描述符
    char buff_[MAX_BUF]; // FIXME 实现Buffer类
}; // class HttpRequest
} // namespace swings

#endif

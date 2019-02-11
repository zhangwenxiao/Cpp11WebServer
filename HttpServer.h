#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#define TIMEOUTMS -1 // epoll_wait超时时间，-1表示不设超时

namespace swings {
class HttpServer {
public:
    HttpServer(int port);
    ~HttpServer();
    void run(); // 启动HTTP服务器
    void acceptConnection(); // 接受新连接
private:
    using HttpRequestPtr = shared_ptr<HttpRequest>;

    int port_; // 监听端口
    int listenFd_; // 监听套接字
    Epoll epoll_; // epoll实例
    ThreadPool threadPool_; // TODO 线程池
    vector<HttpRequestPtr> requests_; // http请求
}; // class HttpServer
} // namespace swings

#endif


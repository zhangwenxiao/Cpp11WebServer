#ifndef __HTTP_REQUEST_H__
#define __HTTP_REQUEST_H__

namespace swings {
class HttpRequest {
public:
    enum HttpRequestParseState { // 报文解析状态
        ExpectRequestLine,
        ExpectHeaders,
        ExpectBody,
        GotAll
    };

    enum Method { // HTTP方法
        Invalid, Get, Post, Head, Put, Delete
    };

    enum Version { // HTTP版本
        Unknown, HTTP10, HTTP11
    };

    HttpRequest(int fd, int idx, Epoll* epoll);
    ~HttpRequest();

    int fd() { return fd_; } // 返回文件描述符
    int idx() { return idx_; } // 返回下标
    void setIdx(int idx) { idx_ = idx; } // 设置新下标

    bool parseRequest(); // 解析Http报文
    bool parseFinish() { return state_ == GotAll; } // 是否解析完一个报文
    string getPath() const { return path_; }
    string getQuery() const { return query_; }
    string getHeader(const string& field) const;
    string getMethod() const;

private:
    // 解析请求行
    bool __parseRequestLine(const char* begin, const char* end);
    // 设置HTTP方法
    bool __setMethod(const char* begin, const char* end);
    // 设置URL路径
    void __setPath(const char* begin, const char* end)
    { path_.assign(begin, end); }
    // 设置URL参数
    void __setQuery(const char* begin, const char* end)
    { query_.assign(begin, end); }
    // 设置HTTP版本
    void __setVersion(Version version) 
    { version_ = version; }
    // 增加报文头
    void __addHeader(const char* start, const char* colon, const char* end);

    // 网络通信相关
    int fd_; // 文件描述符
    int idx_; // 当前HttpRequest在HttpServer::requests_中的下标
    Buffer buff_; // 缓冲区

    // 报文解析相关
    HttpRequestParseState state_; // 报文解析状态
    Method method_; // HTTP方法
    Version version_; // HTTP版本
    string path_; // URL路径
    string query_; // URL参数
    std::map<string, string> headers_; // 报文头部
}; // class HttpRequest
} // namespace swings

#endif

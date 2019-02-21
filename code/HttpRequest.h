#ifndef __HTTP_REQUEST_H__
#define __HTTP_REQUEST_H__

#include "Buffer.h"

#include <string>
#include <map>
#include <iostream>

#define STATIC_ROOT "../www"

namespace swings {

class Timer;

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

    HttpRequest(int fd);
    ~HttpRequest();

    int fd() { return fd_; } // 返回文件描述符
    int read(int* savedErrno); // 读数据
    int write(int* savedErrno); // 写数据

    void appendOutBuffer(const Buffer& buf) { outBuff_.append(buf); }
    int writableBytes() { return outBuff_.readableBytes(); }

    void setTimer(Timer* timer) { timer_ = timer; }
    Timer* getTimer() { return timer_; }

    void setWorking() { working_ = true; }
    void setNoWorking() { working_ = false; }
    bool isWorking() const { return working_; }

    bool parseRequest(); // 解析Http报文
    bool parseFinish() { return state_ == GotAll; } // 是否解析完一个报文
    void resetParse(); // 重置解析状态
    std::string getPath() const { return path_; }
    std::string getQuery() const { return query_; }
    std::string getHeader(const std::string& field) const;
    std::string getMethod() const;
    bool keepAlive() const; // 是否长连接

private:
    // 解析请求行
    bool __parseRequestLine(const char* begin, const char* end);
    // 设置HTTP方法
    bool __setMethod(const char* begin, const char* end);
    // 设置URL路径
    void __setPath(const char* begin, const char* end)
    { 
        std::string subPath;
        subPath.assign(begin, end);
        if(subPath == "/")
            subPath = "/index.html";
        path_ = STATIC_ROOT + subPath;
    }
    // 设置URL参数
    void __setQuery(const char* begin, const char* end)
    { query_.assign(begin, end); }
    // 设置HTTP版本
    void __setVersion(Version version) 
    { version_ = version; }
    // 增加报文头
    void __addHeader(const char* start, const char* colon, const char* end);

private:
    // 网络通信相关
    int fd_; // 文件描述符
    Buffer inBuff_; // 读缓冲区
    Buffer outBuff_; // 写缓冲区
    bool working_; // 若正在工作，则不能被超时事件断开连接

    // 定时器相关
    Timer* timer_;

    // 报文解析相关
    HttpRequestParseState state_; // 报文解析状态
    Method method_; // HTTP方法
    Version version_; // HTTP版本
    std::string path_; // URL路径
    std::string query_; // URL参数
    std::map<std::string, std::string> headers_; // 报文头部
}; // class HttpRequest
} // namespace swings

#endif

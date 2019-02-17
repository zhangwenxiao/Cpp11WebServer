#include "HttpRequest.h"

#include <iostream>
#include <cassert>

#include <unistd.h>

using namespace swings;

HttpRequest::HttpRequest(int fd)
    : fd_(fd),
      timer_(nullptr),
      state_(ExpectRequestLine),
      method_(Invalid),
      version_(Unknown)
{
    assert(fd_ >= 0);
    std::cout << "[HttpRequest::HttpRequest] fd = " << fd_ << std::endl;
}

HttpRequest::~HttpRequest()
{
    close(fd_);
    std::cout << "[HttpRequest::~HttpRequest] close socket(fd=" << fd_ << ")" << std::endl;
}

int HttpRequest::read(int* savedErrno)
{
    int ret = inBuff_.readFd(fd_, savedErrno);
    std::cout << "[HttpRequest::read] read from socket(fd=" << fd_ << "), return " << ret << std::endl;
    return ret;
}

int HttpRequest::write(int* savedErrno)
{
    int ret = outBuff_.writeFd(fd_, savedErrno);
    std::cout << "[HttpRequest::write] write to socket(fd=" << fd_ << "), return " << ret << std::endl;
    return ret;
}

bool HttpRequest::parseRequest()
{
    bool ok = true;
    bool hasMore = true;

    while(hasMore) {
        if(state_ == ExpectRequestLine) {
            // 处理请求行
            const char* crlf = inBuff_.findCRLF();
            if(crlf) {
                // std::cout << "[HttpRequest::parseRequest] find CRLF of request line" << std::endl;
                ok = __parseRequestLine(inBuff_.peek(), crlf);
                if(ok) {
                    // std::cout << "[HttpRequest::parseRequest] parsing request line finish" << std::endl;
                    inBuff_.retrieveUntil(crlf + 2);
                    state_ = ExpectHeaders;
                } else {
                    // std::cout << "[HttpRequest::parseRequest] parsing request line fail" << std::endl;
                    hasMore = false;
                }
            } else {
                // std::cout << "[HttpRequest::parseRequest] can't find CRLF of request line" << std::endl;
                hasMore = false;
            }
        } else if(state_ == ExpectHeaders) {
            // 处理报文头
            const char* crlf = inBuff_.findCRLF();
            if(crlf) {
                // std::cout << "[HttpRequest::parseRequest] find CRLF of request header" << std::endl;
                const char* colon = std::find(inBuff_.peek(), crlf, ':');
                if(colon != crlf) {
                    // std::cout << "[HttpRequest::parseRequest] find : of request header" << std::endl;
                    __addHeader(inBuff_.peek(), colon, crlf);
                } else {
                    // std::cout << "[HttpRequest::parseRequest]header parsing finish" << std::endl;
                    state_ = GotAll;
                    hasMore = false;
                }
                inBuff_.retrieveUntil(crlf + 2);
            } else {
                // std::cout << "[HttpRequest::parseRequest] can't find CRLF of request header" << std::endl;
                hasMore = false;
            } 
        } else if(state_ == ExpectBody) {
            // TODO 处理报文体 
        }
    }

    return ok;
}

bool HttpRequest::__parseRequestLine(const char* begin, const char* end)
{
    // std::cout << "[HttpRequest::__parseRequestLine] begin to parse request line" << std::endl;
    bool succeed = false;
    const char* start = begin;
    const char* space = std::find(start, end, ' ');
    if(space != end && __setMethod(start, space)) {
        start = space + 1;
        space = std::find(start, end, ' ');
        if(space != end) {
            const char* question = std::find(start, space, '?');
            if(question != space) {
                __setPath(start, question);
                __setQuery(question, space);
            } else {
                __setPath(start, space);
            }
            start = space + 1;
            succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
            if(succeed) {
                if(*(end - 1) == '1')
                    __setVersion(HTTP11);
                else if(*(end - 1) == '0')
                    __setVersion(HTTP10);
                else
                    succeed = false;
            } 
        }
    }

    return succeed;
}

bool HttpRequest::__setMethod(const char* start, const char* end)
{
    std::string m(start, end);
    std::cout << "[HttpRequest::__setMethod] method is " << m << std::endl;
    if(m == "GET")
        method_ = Get;
    else if(m == "POST")
        method_ = Post;
    else if(m == "HEAD")
        method_ = Head;
    else if(m == "PUT")
        method_ = Put;
    else if(m == "DELETE")
        method_ = Delete;
    else
        method_ = Invalid;

    return method_ != Invalid;
}

void HttpRequest::__addHeader(const char* start, const char* colon, const char* end)
{
    std::string field(start, colon);
    ++colon;
    while(colon < end && *colon == ' ')
        ++colon;
    std::string value(colon, end);
    while(!value.empty() && value[value.size() - 1] == ' ')
        value.resize(value.size() - 1);

    headers_[field] = value;
    // std::cout << "[HttpRequest::__addHeader] new header: " << field << " = " << value << std::endl;
}

std::string HttpRequest::getMethod() const
{
    std::string res;
    if(method_ == Get)
        res = "GET";
    else if(method_ == Post)
        res = "POST";
    else if(method_ == Head)
        res = "HEAD";
    else if(method_ == Put)
        res = "Put";
    else if(method_ == Delete)
        res = "DELETE";
    
    return res;
}

std::string HttpRequest::getHeader(const std::string& field) const
{
    std::string res;
    auto itr = headers_.find(field);
    if(itr != headers_.end())
        res = itr -> second;
    return res;
}

bool HttpRequest::keepAlive() const
{
    std::string connection = getHeader("Connection");
    bool res = connection == "Keep-Alive" || 
               (version_ == HTTP11 && connection != "close");

    return res;
}

void HttpRequest::resetParse()
{
    state_ = ExpectRequestLine; // 报文解析状态
    method_ = Invalid; // HTTP方法
    version_ = Unknown; // HTTP版本
    path_ = ""; // URL路径
    query_ = ""; // URL参数
    headers_.clear(); // 报文头部
}

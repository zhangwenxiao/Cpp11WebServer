#include "HttpRequest.h"
#include <cassert>

using namespace swings;

HttpRequest::HttpRequest(int fd, int idx, Epoll epoll)
    : fd_(fd),
      idx_(idx),
      state_(ExpectRequestLine),
      method_(Invalid),
      version_(Unknown)
{
    assert(fd_ >= 0);
}

HttpRequest::~HttpRequest()
{
    ::close(fd_);
}

HttpRequest::read(int* savedErrno)
{
    int ret = buff_.readFd(fd_, savedErrno);
    return ret;
}

bool HttpRequest::parseRequest()
{
    bool ok = true;
    bool hasMore = true;

    while(hasMore) {
        if(state_ == ExpectRequestLine) {
            // 处理请求行
            const char* crlf = buff_ -> findCRLF();
            if(crlf) {
                ok = __parseRequestLine(buff_ -> peek(), crlf);
                if(ok) {
                    buff_ -> retrieveUntil(crlf + 2);
                    state_ = ExpectHeaders;
                } else {
                    hasMore = false;
                }
            } else {
                hasMore = false;
            }
        } else if(state_ == ExpectHeaders) {
            // 处理报文头
            const char* crlf = buff_ -> findCRLF();
            if(crlf) {
                const char* colon = std::find(buff_ -> peek(), crlf, ':');
                if(colon != crlf)
                    __addHeader(buff_ -> peek(), colon, crlf);
                else {
                    state_ = GotAll;
                    hasMore = false;
                }
                buff_ -> retrieveUntil(crlf + 2);
            } else
                hasMore = false;
        } else if(state_ == ExpectBody) {
            // TODO 处理报文体 
        }
    }

    return ok;
}

bool HttpRequest::__parseRequestLine(const char* begin, const char* end)
{
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

bool __setMethod(const char* begin, const char* end)
{
    string m(start, end);
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

void __addHeader(const char* start, const char* colon, const char* end)
{
    string field(start, colon);
    ++colon;
    while(colon < end && *colon == ' ')
        ++colon;
    string value(colon, end);
    while(!value.empty() && value[value.size() - 1] == ' ')
        value.resize(value.size() - 1);

    headers_[field] = value;
}

string getMethod() const
{
    string res;
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

string getHeader(const string& field) const
{
    string res;
    auto itr = headers_.find(field);
    if(itr != headers_.end())
        result = itr -> second;
    return res;
}


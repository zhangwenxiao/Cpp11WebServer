#ifndef __HTTP_RESPONSE_H__
#define __HTTP_RESPONSE_H__

#include <map>
#include <string>

namespace swings {

// 状态码和对应的状态信息
static const std::map<int, std::string> statusCode2Message = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"}
};

// 文件后缀和对应的文件类型
static const std::map<std::string, std::string> suffix2Type = {
    {".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/nsword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},
    {".css", "text/css"},
};

class Buffer;

class HttpResponse {
public:
    HttpResponse(int statusCode, std::string path, bool keepAlive)
        : statusCode_(statusCode),
          path_(path),
          keepAlive_(keepAlive)
    {}

    ~HttpResponse() {}

    Buffer makeResponse();
    void doErrorResponse(Buffer& output, std::string message);
    void doStaticRequest(Buffer& output, long fileSize);

private:
    std::string __getFileType();

private:
    std::map<std::string, std::string> headers_; // 响应报文头部
    int statusCode_; // 响应状态码
    std::string path_; // 请求资源路径
    bool keepAlive_; // 长连接
}; // class HttpResponse
} // namespace swings

#endif

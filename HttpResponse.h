#ifndef __HTTP_RESPONSE_H__
#define __HTTP_RESPONSE_H__

namespace swings {
class HttpResponse {
public:
    // 状态码和对应的状态信息
    static const std::map<int, string> statusCode2Message = 
    {
        {200, "OK"},
        {400, "Bad Request"},
        {403, "Forbidden"},
        {404, "Not Found"}
    };

    // 文件后缀和对应的文件类型
    static const std::map<string, string> suffix2Type = 
    {
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

    HttpResponse(int statusCode, string path, bool keepAlive)
        : statusCode_(statusCode),
          path_(path),
          keepAlive_(keepAlive)
    {}

    ~HttpResponse() {}

    void sendResponse();

private:
    std::map<string, string> headers_; // 响应报文头部
    int statusCode_; // 响应状态码
    string path_; // 请求资源路径
    string contentType_; // 请求资源类型
    bool keepAlive_; // 长连接
}; // class HttpResponse
} // namespace swings

#endif

#include "HttpResponse.h"
#include "Buffer.h"

#include <string>
#include <iostream>
#include <cassert>
#include <cstring>

#include <fcntl.h> // open
#include <unistd.h> // close
#include <sys/stat.h> // stat
#include <sys/mman.h> // mmap, munmap

using namespace swings;

const std::map<int, std::string> HttpResponse::statusCode2Message = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"}
};

const std::map<std::string, std::string> HttpResponse::suffix2Type = {
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
    {".css", "text/css"}
};

Buffer HttpResponse::makeResponse()
{
    Buffer output;

    if(statusCode_ == 400) {
        doErrorResponse(output, "Swings can't parse the message");
        return output;
    }

    struct stat sbuf;
    // 文件找不到错误
    if(::stat(path_.data(), &sbuf) < 0) {
        statusCode_ = 404;
        doErrorResponse(output, "Swings can't find the file");
        return output;
    }
    // 权限错误
    if(!(S_ISREG(sbuf.st_mode) || !(S_IRUSR & sbuf.st_mode))) {
        statusCode_ = 403;
        doErrorResponse(output, "Swings can't read the file");
        return output;
    }

    // 处理静态文件请求
    doStaticRequest(output, sbuf.st_size);
    return output;
}

// TODO 还要填入哪些报文头部选项
void HttpResponse::doStaticRequest(Buffer& output, long fileSize)
{
    assert(fileSize >= 0);

    auto itr = statusCode2Message.find(statusCode_);
    if(itr == statusCode2Message.end()) {
        statusCode_ = 400;
        doErrorResponse(output, "Unknown status code");
        return;
    }

    // 响应行
    output.append("HTTP/1.1 " + std::to_string(statusCode_) + " " + itr -> second + "\r\n");
    // 报文头
    if(keepAlive_) {
        output.append("Connection: Keep-Alive\r\n");
        output.append("Keep-Alive: timeout=" + std::to_string(CONNECT_TIMEOUT) + "\r\n");
    } else {
        output.append("Connection: close\r\n");
    }
    output.append("Content-type: " + __getFileType() + "\r\n");
    output.append("Content-length: " + std::to_string(fileSize) + "\r\n");
    // TODO 添加头部Last-Modified: ?
    output.append("Server: Swings\r\n");
    output.append("\r\n");

    // 报文体
    int srcFd = ::open(path_.data(), O_RDONLY, 0);
    // 存储映射IO
    void* mmapRet = ::mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, srcFd, 0);
    ::close(srcFd);
    if(mmapRet == (void*) -1) {
        munmap(mmapRet, fileSize);
        output.retrieveAll();
        statusCode_ = 404;
        doErrorResponse(output, "Swings can't find the file");
        return;
    }
    char* srcAddr = static_cast<char*>(mmapRet);
    output.append(srcAddr, fileSize);

    munmap(srcAddr, fileSize);
}

std::string HttpResponse::__getFileType()
{
    int idx = path_.find_last_of('.');
    std::string suffix;
    // 找不到文件后缀，默认纯文本
    if(idx == std::string::npos) {
        return "text/plain";
    }
        
    suffix = path_.substr(idx);
    auto itr = suffix2Type.find(suffix);
    // 未知文件后缀，默认纯文本
    if(itr == suffix2Type.end()) {
        return "text/plain";
    }   
    return itr -> second;
}

// TODO 还要填入哪些报文头部选项
void HttpResponse::doErrorResponse(Buffer& output, std::string message) 
{
    std::string body;

    auto itr = statusCode2Message.find(statusCode_);
    if(itr == statusCode2Message.end()) {
        return;
    }

    body += "<html><title>Swings Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    body += std::to_string(statusCode_) + " : " + itr -> second + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>Swings web server</em></body></html>";

    // 响应行
    output.append("HTTP/1.1 " + std::to_string(statusCode_) + " " + itr -> second + "\r\n");
    // 报文头
    output.append("Server: Swings\r\n");
    output.append("Content-type: text/html\r\n");
    output.append("Connection: close\r\n");
    output.append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    // 报文体
    output.append(body);
}

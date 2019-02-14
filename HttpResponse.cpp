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

void HttpResponse::sendResponse(int fd)
{
    if(statusCode_ == 400) {
        doErrorResponse(fd, "Swings can't parse the message");
        return;
    }

    struct stat sbuf;
    // 文件找不到错误
    if(::stat(path_.data(), &sbuf) < 0) {
        statusCode_ = 404;
        doErrorResponse(fd, "Swings can't find the file");
        return;
    }
    // 权限错误
    if(!(S_ISREG(sbuf.st_mode) || !(S_IRUSR & sbuf.st_mode))) {
        statusCode_ = 403;
        doErrorResponse(fd, "Swings can't read the file");
        return;
    }

    // 处理静态文件请求
    doStaticRequest(fd, sbuf.st_size);
}

// TODO 还要填入哪些报文头部选项
void HttpResponse::doStaticRequest(int fd, long fileSize)
{
    assert(fileSize >= 0);
    // XXX 声明为栈上对象需要考虑生存期问题
    Buffer output;

    auto itr = statusCode2Message.find(statusCode_);
    if(itr == statusCode2Message.end()) {
        std::cout << "[HttpRequest::doStaticRequest] unknown status code: " 
                  << statusCode_ << std::endl;
        statusCode_ = 400;
        doErrorResponse(fd, "Unknown status code");
        return;
    }

    // 响应行
    output.append("HTTP/1.1 " + std::to_string(statusCode_) + " " + itr -> second);

    // 报文头
    if(keepAlive_) {
        output.append("Connection: Keep-Alive\r\n");
        // TODO 添加头部Keep-Alive: timeout=?
    }
    output.append("Content-type: " + __getFileType() + "\r\n");
    output.append("Content-length: " + std::to_string(fileSize) + "\r\n");
    // TODO 添加头部Last-Modified: ?
    output.append("Server: Swings\r\n");
    output.append("\r\n");

    // 报文体
    int srcFd = ::open(path_.data(), O_RDONLY, 0);
    // 存储映射IO
    char* srcAddr = (char*)::mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, srcFd, 0);
    ::close(srcFd);

    output.append(srcAddr, fileSize);
    // 发送响应报文到客户端
    // XXX 这里一定要一次发送完毕，不然output的生存期结束了
    int writeErrno;
    size_t toWrite = output.readableBytes();
    ssize_t ret = output.writeFd(fd, &writeErrno);
    std::cout << "[HttpRequest::doStaticRequest] " 
              << toWrite << " bytes to write, return " << ret << std::endl;

    munmap(srcAddr, fileSize);
}

std::string HttpResponse::__getFileType()
{
    int idx = path_.find_last_of('.');
    std::string suffix;
    // 找不到文件后缀，默认纯文本
    if(idx == std::string::npos) {
        std::cout << "[HttpResponse::__getFileType] can't find suffix, set file type to text/plain" << std::endl;
        return "text/plain";
    }
        
    suffix = path_.substr(idx);
    auto itr = suffix2Type.find(suffix);
    // 未知文件后缀，默认纯文本
    if(itr == suffix2Type.end()) {
        std::cout << "[HttpResponse::__getFileType] unknown suffix " << suffix 
                  << ", set file type to text/plain" << std::endl;
        return "text/plain";
    }
        
    return itr -> second;
}

// TODO 还要填入哪些报文头部选项
void HttpResponse::doErrorResponse(int fd, std::string message) 
{
    // XXX 声明为栈上对象需要考虑生存期问题
    Buffer output;
    std::string body;

    auto itr = statusCode2Message.find(statusCode_);
    if(itr == statusCode2Message.end()) {
        std::cout << "[HttpRequest::doErrorRequest] unknown status code: " 
                  << statusCode_ << std::endl;
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

    // 发送响应报文到客户端
    // XXX 这里一定要一次发送完毕，不然output的生存期结束了
    int writeErrno;
    size_t toWrite = output.readableBytes();
    ssize_t ret = output.writeFd(fd, &writeErrno);
    std::cout << "[HttpRequest::doErrorRequest] " 
              << toWrite << " bytes to write, return " << ret << std::endl;
}


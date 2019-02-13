#include "HttpResponse.h"

using namespace swings;

void HttpResponse::sendResponse(int fd)
{
    struct stat sbuf;
    // 文件找不到错误
    if(::stat(path_.data(), sbuf) < 0) {
        statusCode_ = 404;
        // 处理错误请求
        doErrorResponse(fd, 404, "Swings can't find the file");
        return;
    }
    // 权限错误
    if(!(S_ISREG(sbuf.st_mode) || !(S_IRUSR & sbuf.st_mode)) {
        statusCode_ = 403;
        // 处理错误请求
        doErrorResponse(fd, 403, "Swings can't read the file");
        return;
    }
    // 处理静态文件请求
    doStaticRequest(fd, subf.st_size);
}

// TODO 还要填入哪些报文头部选项
void HttpResponse::doStaticRequest(int fd, long fileSize)
{
    assert(fileSize >= 0);
    // XXX 声明为栈上对象需要考虑生存期问题
    Buffer output;
    char buf[32];

    // 响应行
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\n", 
             statusCode_, 
             statusCode2Message[statusCode_].data());
    output.append(buf, strlen(buf));

    // 报文头
    if(keepAlive_) {
        output.append("Connection: Keep-Alive\r\n");
        // TODO 添加头部Keep-Alive: timeout=?
    }
    string fileType = ;
    output.append("Content-type: " + __getFileType() + "\r\n");
    output.append("Content-length: " + to_string(fileSize) + "\r\n");
    // TODO 添加头部Last-Modified: ?
    output.append("Server: Swings\r\n");
    output.append("\r\n");

    // 报文体
    int srcFd = open(path_, O_RDONLY, 0);
    // 存储映射IO
    char* srcAddr = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, srcFd, 0);
    close(srcFd);

    output.append(srcAddr, fileSize);
    // 发送响应报文到客户端
    // TODO 这里一定要一次发送完毕，不然output的生存期结束了
    output.writeFd(fd);

    munmap(srcAddr, fileSize);
}

string HttpResponse::__getFileType()
{
    int idx = path_.find_last_of('.');
    string suffix;
    // 找不到文件后缀，默认纯文本
    if(idx == string::npos)
        return "text/plain";
    suffix = path_.substr(idx);
    auto itr = suffix2Type.find(suffix);
    // 未知文件后缀，默认纯文本
    if(itr == suffix2Type.end())
        return "text/plain";
    return itr -> second;
}

// TODO 还要填入哪些报文头部选项
void HttpResponse::doErrorResponse(int fd, string message) 
{
    // XXX 声明为栈上对象需要考虑生存期问题
    Buffer output;
    char buf[32];
    char body[8192];

    sprintf(body, "<html><title>Swings Error</title>");
    sprintf(body, "%s<body bgcolor=\"ffffff\">\n", body);
    sprintf(body, "%s%d : %s\n", body, 
             statusCode_, 
             statusCode2Message[statusCode_].data());
    sprintf(body, "%s<p>%s</p>", body, message.data());
    sprintf(body, "%s<hr><em>Swings web server</em>\n</body></html>", body);

    // 响应行
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\n", 
             statusCode_,
             statusCode2Message[statusCode_].data());
    output.append(buf, strlen(buf));
    // 报文头
    output.append("Server: Swings\r\n");
    output.append("Content-type: text/html\r\n");
    output.append("Connection: close\r\n");
    output.append("Content-length: %d\r\n\r\n", (int)strlen(body));
    // 报文体
    output.append(body, strlen(body));

    // 发送响应报文到客户端
    // TODO 这里一定要一次发送完毕，不然output的生存期结束了
    output.writeFd(fd);
}


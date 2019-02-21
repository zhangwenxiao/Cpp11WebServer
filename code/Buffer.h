#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <vector>
#include <string>
#include <algorithm> // copy
#include <iostream>

#include <cassert>

#define INIT_SIZE 1024

namespace swings {

class Buffer {
public:
    Buffer()
        : buffer_(INIT_SIZE),
          readerIndex_(0),
          writerIndex_(0)
    {
        assert(readableBytes() == 0);
        assert(writableBytes() == INIT_SIZE);
    }
    ~Buffer() {}

    // 默认拷贝构造函数和赋值函数可用

    size_t readableBytes() const // 可读字节数
    { return writerIndex_ - readerIndex_; }

    size_t writableBytes() const // 可写字节数
    { return buffer_.size() - writerIndex_; }

    size_t prependableBytes() const // readerIndex_前面的空闲缓冲区大小
    { return readerIndex_; }

    const char* peek() const // 第一个可读位置
    { return __begin() + readerIndex_; }

    void retrieve(size_t len) // 取出len个字节 
    {
        assert(len <= readableBytes());
        readerIndex_ += len;
    }

    void retrieveUntil(const char* end) // 取出数据直到end
    {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }

    void retrieveAll() // 取出buffer内全部数据
    {
        readerIndex_ = 0;
        writerIndex_ = 0;
    }

    std::string retrieveAsString() // 以string形式取出全部数据
    {
        std::string str(peek(), readableBytes());
        retrieveAll();
        return str;
    }

    void append(const std::string& str) // 插入数据
    { append(str.data(), str.length()); }

    void append(const char* data, size_t len) // 插入数据
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        hasWritten(len);
    }

    void append(const void* data, size_t len) // 插入数据
    { append(static_cast<const char*>(data), len); }

    void append(const Buffer& otherBuff) // 把其它缓冲区的数据添加到本缓冲区
    { append(otherBuff.peek(), otherBuff.readableBytes()); }

    void ensureWritableBytes(size_t len) // 确保缓冲区有足够空间
    {
        if(writableBytes() < len) {
            __makeSpace(len);
        }
        assert(writableBytes() >= len);
    }

    char* beginWrite() // 可写char指针
    { return __begin() + writerIndex_; }

    const char* beginWrite() const
    { return __begin() + writerIndex_; }

    void hasWritten(size_t len) // 写入数据后移动writerIndex_
    { writerIndex_ += len; }

    ssize_t readFd(int fd, int* savedErrno); // 从套接字读到缓冲区
    ssize_t writeFd(int fd, int* savedErrno); // 缓冲区写到套接字

    const char* findCRLF() const
    {
        const char CRLF[] = "\r\n";
        const char* crlf = std::search(peek(), beginWrite(), CRLF, CRLF+2);
        return crlf == beginWrite() ? nullptr : crlf;
    }

    const char* findCRLF(const char* start) const
    {
        assert(peek() <= start);
        assert(start <= beginWrite());
        const char CRLF[] = "\r\n";
        const char* crlf = std::search(start, beginWrite(), CRLF, CRLF + 2);
        return crlf == beginWrite() ? nullptr : crlf;
    }

private:
    char* __begin() // 返回缓冲区头指针
    { return &*buffer_.begin(); }

    const char* __begin() const // 返回缓冲区头指针
    { return &*buffer_.begin(); }

    void __makeSpace(size_t len) // 确保缓冲区有足够空间
    {
        if(writableBytes() + prependableBytes() < len) {
            buffer_.resize(writerIndex_ + len);
        }
        else {
            size_t readable = readableBytes();
            std::copy(__begin() + readerIndex_,
                      __begin() + writerIndex_,
                      __begin());
            readerIndex_ = 0;
            writerIndex_ = readerIndex_ + readable;
            assert(readable == readableBytes());
        }
    }

private:

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
}; // class Buffer

} // namespace swings

#endif

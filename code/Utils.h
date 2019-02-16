#ifndef __UTILS_H__ 
#define __UTILS_H__ 

#define LISTENQ 1024 // 监听队列长度,操作系统默认值为SOMAXCONN

namespace swings {

namespace utils {
    int createListenFd(int port); // 创建监听描述符
    int setNonBlocking(int fd); // 设置非阻塞模式
}

} // namespace swings

#endif

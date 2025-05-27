#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "Socket.h"
#include "InetAddress.h"
#include "Logger.h"

Socket::~Socket()
{
    close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localAddr)
{

    if (0 != bind(sockfd_, (sockaddr *)localAddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL("Socket::bindAddress sockfd:%d err errno:%s \n", sockfd_, strerror(errno));
    }
}
void Socket::listen()
{
    if (0 != ::listen(sockfd_, 1024))
    {
        LOG_FATAL("listen sockfd:%d err\n", sockfd_);
    }
}
int Socket::accept(InetAddress *peeraddr)
{
    sockaddr_in addr;
    bzero(&addr, sizeof addr);
    socklen_t len = sizeof(addr);//必须初始化addr大小
    int connfd = ::accept4(sockfd_, (sockaddr *)&addr, &len,SOCK_NONBLOCK);
    if (connfd >= 0)
    {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}
void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_RD) < 0)
    {
        LOG_ERROR("shutdownWrite err\n");
    }
}
/*1. setTcpNoDelay(bool on)
功能：设置TCP_NODELAY选项，用于禁用或启用Nagle算法。
Nagle算法通过缓冲小数据包来减少网络拥塞，但会增加延迟。
当on=true时禁用Nagle算法（无延迟），适合需要低延迟的场景（如实时应用）。
。
2. setReuseAddr(bool on)
功能：设置SO_REUSEADDR选项，允许重用本地地址。
当on=true时，允许立即重用处于TIME_WAIT状态的套接字的地址。
对于服务器快速重启很有用。

3. setReusePort(bool on)
功能：设置SO_REUSEPORT选项，允许完全重复的绑定。
允许多个套接字绑定到相同的IP地址和端口组合。
可用于实现负载均衡。

*/
void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}
void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}
void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}
void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}
/*
TCP Keepalive的作用：
检测连接是否存活：如果连接对端崩溃或网络断开，TCP保活机制可以检测到并关闭无效连接。
防止长时间空闲连接占用资源：通过定期发送保活探测包，确保连接仍然是可用的。
*/
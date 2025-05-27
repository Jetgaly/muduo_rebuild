#pragma once



#include"noncopyable.h"
class InetAddress;


class Socket:noncopyable
{
public:
    
    //传入listenSockfd进行地址绑定，封装
    explicit Socket(int sockfd):sockfd_(sockfd){};
    ~Socket();

    int fd() const{return sockfd_;}
    void bindAddress(const InetAddress& localAddr);
    void listen();
    int accept(InetAddress*peeraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);

    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);
private:
   const int sockfd_;
};

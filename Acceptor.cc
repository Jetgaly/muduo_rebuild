#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

// 创建监听socketfd
static int createNonblockingSocket()
{
    // SOCK_CLOEXEC 子进程不会继承父进程的这个sockfd
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0)
    {
        LOG_FATAL("listen socket createNonblockingSocket err\n");
    }

    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reusePort)
    : loop_(loop),
      acceptSocket_(createNonblockingSocket()),
      acceptChannel_(loop_, acceptSocket_.fd()),
      listenning_(false)
{

    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);

    acceptSocket_.bindAddress(listenAddr);

    // 绑定处理客户连接的回调
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();//设置event，加入Evenloop
}

// 处理客户连接的回调
void Acceptor::handleRead()
{
    sockaddr_in addr;
    InetAddress peerAddr(addr);
    int connfd = acceptSocket_.accept(&peerAddr);

    if (connfd >= 0)
    {
        if (newConnectionCallback)
        {
            newConnectionCallback(connfd, peerAddr);//调用这个返回连接给TcpServer
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s,%s,%d accept err:%d\n", __FILE__, __FUNCTION__, __LINE__,errno);
        if(errno == EMFILE){
            //系统资源不够 fd资源
            LOG_ERROR("sockfd reach limit num\n");
        }
    }
}
#include<strings.h>

#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"
static EventLoop *checkLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("mainLoop is null !!!\n");
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, Option opt)
    : loop_(checkLoopNotNull(loop)),
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr, opt == kReusePort)), // std::unique_ptr<Acceptor>
      threadPool_(new EventLoopThreadPool(loop, name_)),
      connectionCallback_(),
      messageCallback_(),
      nextConnId_(1),
      started_(0)
{
    // 绑定新用户连接回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

// 新用户连接回调
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    // 封装channel，通过轮询将连接分配给并唤醒subLoop

    // 轮询算法选择subloop
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;
    
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from [%s]\n",name_.c_str(),connName.c_str(),peerAddr.toIpPort().c_str());

    //获取连接的本地ip和端口，也就是服务器ip和端口
    sockaddr_in local;
    bzero(&local,sizeof local);
    socklen_t addrlen = sizeof local;
    if(getsockname(sockfd,(sockaddr*)&local,&addrlen)<0){
        LOG_ERROR("sockets::getLocalAddr err \n");
    }
    InetAddress localAddr(local);

    //创建TcpConnection
    //state_ = kConnecting
    TcpConnectionPtr conn(new TcpConnection(ioLoop,connName,sockfd,localAddr,peerAddr));

    connections_[connName] = conn;

    //下面的回调都是用户设置的
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    conn->setCloseCallback(std::bind(&TcpServer::removeConnection,this,std::placeholders::_1));
    //注册到subloop，state_ = kConnected
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished,conn));
}

TcpServer::~TcpServer(){
    for(auto &item : connections_){
        
        TcpConnectionPtr conn(item.second);//事先拷贝一个对象，用于后面的connnectDestroyed()
        item.second.reset();//将connections_中的智能指针reset

        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connnectDestroyed,conn));
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听
void TcpServer::start()
{
    if (started_++ == 0)
    { // 防止一个tcpserver被启动多次
        threadPool_->start(threadInitCallback_);
        // mainLoop启动监听
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}
void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(
        &TcpServer::removeConnectionInLoop,this,conn//默认拷贝构造conn TcpConnectionPtr
        //所以智能指针不会在connections_.erase(conn->name());的时候直接析构对象
    ));
}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",name_.c_str(),conn->name().c_str());

    size_t n = connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connnectDestroyed,conn));
}
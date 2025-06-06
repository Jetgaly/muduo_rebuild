# muduo_rebuild

## install

### Ubuntu 24.04.1 LTS

```bash
cd muduo_rebuild
sudo ./autobuild.sh
```

头文件和库文件的路径可见`autobuild.sh`的输出日志

## desc

###  V1.0 ：这个项目重写了muduo网络库的核心部分，实现了（epoll+同步非阻塞）Reactor模型，并且用C++11的新特性去除了原muduo库对boost库的依赖  

### else

#### class Channel

管理每一个用户连接(fd，回调函数)

这里的回调函数是从上层一层一层地传递下来绑定的

tcpserver -> tcpconnection -> channel



#### class EventLoop

主要整合了poller(epoll)和ChannelList(用户连接channel的指针vector)

`using ChannelList = std::vector<Channel*>;`

channel最开始创建实在 tcpconnection中



#### class EventLoopThread

打包thread和Eventloop(one Loop per Thread)



#### class EventLoopThreadPool

线程池，管理mainloop和subloop



#### class TcpConnection

用于管理连接

整合了Channel对象和接受数据缓冲区，发送数据缓冲区（Buffer）

Channel对象的回调函数就是经过TcpConnection给他绑定的

用户调用的send方法就是该对象的send方法



#### class Acceptor

该对象就是包装后的服务器的监听fd，运行在mainloop上



#### class TcpServer

就是整合了class EventLoopThreadPool，std::unordered_map<std::string, TcpConnectionPtr>(所有的连接)，用户设置的回调的服务器对象。

class Acceptor的实例化对象就是该对象的成员（在TcpServer调用构造函数的时候创建），因为TcpServer对象肯定是创建在mainloop的，在`server.start()`时将acceptor加入mainLoop的epoll监听中

```cc
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
```

**使用方法**

```cc
#include <muduo_rebuild/TcpServer.h>

class testServer
{
public:
    testServer(EventLoop *loop, const InetAddress &Addr, const std::string &nameArg, TcpServer::Option opt = TcpServer::kNOReusePort)
        : server_(loop, Addr, nameArg,opt), loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(std::bind(&testServer::onConnection,this,std::placeholders::_1));
        server_.setMessageCallback(std::bind(&testServer::onMessage,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));


        // 设置线程数量
        server_.setThreadNum(3);
    }

    void start(){
        server_.start();
    }
private:
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO("connected : %s\n", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("disconnected : %s\n", conn->peerAddress().toIpPort().c_str());
        }
    }
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, TimeStamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        std::cout<<"flagggg:"<<msg<<std::endl;
        conn->send(msg);
        conn->shutdown();
    }
    EventLoop *loop_;
    TcpServer server_;
};

int main(){
    EventLoop loop;
    InetAddress addr(19090);
    testServer server(&loop,addr,"testServer");

    server.start();
    loop.loop();
    return 0;
}
```

**逻辑流程**

`EventLoop loop`是用户手动创建在mainloop的事件循环，之后将服务器地址`InetAddress addr(19090)`和事件循环`EventLoop loop`作为参数传入给(拥有TcpServer对象成员的)`testServer`的构造函数，该构造函数初始化列表会调用`server_(loop, Addr, nameArg,opt)`也就是TcpServer的构造函数，原型如下

```cc
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
```

核心在于这个构造函数会创建一个`Acceptor`对象和一个线程池`EventLoopThreadPool`,并会绑定acceptor\_的新用户连接回调函数`TcpServer::newConnection()`，这个函数不是用户设置的，而是已经写好的，每当acceptor\_的fd上有可读时间发生时(也就是有新用户连接时)，acceptor_在`EventLoop`中调用对应的可读事件回调函数`handleRead()`,而在这个函数中会调用accept()来接受连接并且调用开始绑定的`TcpServer::newConnection()`函数，目的在于

- 将连接包装成一个 TcpConnection对象，并且储存起来
- 设置对应的回调函数
- 通过轮询分配给一个subloop

```cc
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
```

当用户调用`conn->send()`函数时，如果不能一次性发送完，就会将没发送完的数据写入`outputBuffer_`并注册可写事件，留给`EventLoop`发送

**这里发现一个比较有意思的点**

```cc
//关闭连接(只是关闭连接，并不销毁)
void TcpConnection::shutdown(){
    if(state_ == kConnected){
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop,this));
    }
}
void TcpConnection::shutdownInLoop(){
    //如果没发送完就根据TcpConnection::shutdown()的setState(kDisconnecting);
    //来让handleWrite中调用shutdownInLoop()
    if(!channel_->isWriting()){//说明当前outputBuffer的数据已经发送完了
        socket_->shutdownWrite();//触发Epollhub 间接触发handleClose
    }
}
```

如果用户调用`shutdown()`的时候，服务器会根据发送缓冲区的状态来关闭连接

- 如果发送缓冲区没有数据待发送(!channel_->isWriting())

​	这时候服务器会直接调用`socket_->shutdownWrite()`触发Epollhub 间接触发handleClose

- 如果发送缓冲区有数据待发送

​	如果没发送完就根据TcpConnection::shutdown()的setState(kDisconnecting);
​	来让handleWrite中调用shutdownInLoop()

也就是说服务器会先等待还在发送的数据发送完才关闭连接
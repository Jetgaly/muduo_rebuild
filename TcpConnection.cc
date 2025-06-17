#include <errno.h>
#include <memory>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <netinet/tcp.h>

#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

static EventLoop *checkLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("mainLoop is null !!!\n");
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &name, int sockfd, const InetAddress &localAddr, const InetAddress &peerAddr)
    : loop_(checkLoopNotNull(loop)),
      name_(name),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024) // 64M
{
    // tcpserver -> tcpconnection -> channel
    // 这里是 tcpconnection -> channel
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::ctor[%s] at fd=%d state=%d\n", name_.c_str(), socket_->fd(), state_.load());
}

void TcpConnection::handleRead(TimeStamp receiveTime)
{
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    if (n > 0)
    {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead\n");
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    int saveErrno = 0;
    if (channel_->isWriting())
    { // 其实这里可以不判断，因为能就到这个函数一定是EPOLLOUT事件发生
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();//发送完就关闭Epollout监听
                if (writeCompleteCallback_)
                {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
            }
            if (state_ == kDisconnecting)
            {
                shutdownInLoop();
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite\n");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down,no more writing\n", channel_->fd());
    }
}
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), state_.load());
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    if (connectionCallback_)
    {
        // 执行关闭连接的回调
        connectionCallback_(connPtr);
    }
    if (closeCallback_)
    {
        // 执行连接关闭之后的回调
        closeCallback_(connPtr);
    }
}
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s -SO_ERROR:%d\n", name_.c_str(), err);
}
/*
getsockopt(fd, SOL_SOCKET, SO_ERROR, ...)
该函数用于查询 socket 的 SO_ERROR 选项，获取 socket 上发生的错误码。
如果 socket 发生错误（如连接失败、读写错误等），SO_ERROR 会存储该错误码（如 ECONNREFUSED、ETIMEDOUT 等）。
如果 getsockopt 本身调用失败（返回 < 0），则用 errno 获取系统错误。
错误处理逻辑
如果 getsockopt 失败（如无效的 fd），则用 errno 作为错误码。
如果 getsockopt 成功，则 optval 存储 socket 的错误码（如 0 表示无错误）。
日志记录
最后通过 LOG_ERROR 打印错误信息，包括连接名称 name_ 和错误码 err。
*/


/*
用户直接调用
流程：
    调用TcpConnection::sendInLoop：
        如果是第一次发送，channel第一次开始写数据，而且缓冲区没有待发送的数据
        就直接发送，接着如果数据没全部发送完就写入outputBuffer，注册EPOLLOUT
        来调用TcpConnection::handleWrite来发送剩余的在outputBuffer中的数据，
        发送完就直接移除EPOLLOUT监听，调用writeCompleteCallback_
*/
void TcpConnection::send(const std::string &buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        { // 这里其实也可以省略，因为runInLloop已经处理了这一步的内容
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

void TcpConnection::sendInLoop(const void *message, size_t len)
{
    ssize_t nwrote = 0;
    ssize_t remainng = len;
    bool faultError = false;

    // 之前调用过shutdown了，不能在进行发送
    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing\n");
        return;
    }
    // channel第一次开始写数据，而且缓冲区没有待发送的数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = write(channel_->fd(),message, len);
        if (nwrote >= 0)
        {
            remainng = len - nwrote;
            if (remainng == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop err\n");
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }
    // 说明当前则会一次write没有把全部数据发送出去，剩余的数据需保存在缓冲区中
    // 注册EPOLLOUT事件，poller会通知channel调用对应的handleWrite回调方法
    if (!faultError && remainng > 0)
    {
        // 目前发送缓冲区剩余的待发送数据长度
        size_t oldLen = outputBuffer_.readableBytes();

        if (oldLen + remainng >= highWaterMark_ && oldLen < highWaterMark_ // 如果oldLen > highWaterMark_说明已经要调用highWaterMarkCallback_了
            && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remainng));
        }
        outputBuffer_.append((char *)message + nwrote, remainng);
        if (!channel_->isWriting())
        {
            // 注册写事件
            channel_->enableWriting();
        }
    }
}

// 建立连接
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();
    // 执行连接回调
    connectionCallback_(shared_from_this());
}

//销毁连接
//给TcpServer回调:用于修改连接状态为kDisconnected，并且把channel所有感兴趣的事件remove和把channel从poller删除
//TcpServer::removeConnectionInLoop
//TcpServer::~TcpServer
void TcpConnection::connnectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();//把channel所有感兴趣的事件remove
        connectionCallback_(shared_from_this());
    }
    channel_->remove();//把channel从poller删除
}

//关闭连接(只是关闭连接，销毁)
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
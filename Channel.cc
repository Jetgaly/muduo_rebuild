#include <sys/epoll.h>
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *p, int fd)
    : loop_(p),
      fd_(fd),
      events_(0),
      revents_(0),
      tied_(false),
      index_(-1) {}
Channel::~Channel() {
     
}

void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;//weakPtr指向对应的TcpConnection，防止TcpConnection被析构调了，无法调用回调函数，因为回调函数绑定的是TcpConnection的成员函数
    tied_ = true;
}

// 负责在poller里面更改fd的对应事件epoll_ctl
void Channel::update()
{
    loop_->updateChannel(this);
}

// 在Channel所属的EventLoop中把自己删除
void Channel::remove()
{
    loop_->removeChannel(this);
}

// 收到poller通知后，处理时间
void Channel::handleEvent(TimeStamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(TimeStamp recviveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n",revents_);
    
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(recviveTime);
        }
    }
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}
#include "EpollerPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
// channel 未添加到 poller 中
const int kNew = -1; // channel index_ = -1
// channel 已添加到 poller 中
const int kAdded = 1;
////channel 在 poller 中删除(并不是在map中删除，在poller监听中删除了epoll_ctl(del))
const int kDeleted = 2;

EpollerPoller::EpollerPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epollfd create err:%d\n", errno);
    }
}

EpollerPoller::~EpollerPoller()
{
    close(epollfd_);
}

TimeStamp EpollerPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    LOG_INFO("func=%s => fd total count:%lu \n",__FUNCTION__,channels_.size());

    int numEvents = epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);
    int saveErrno = errno;//保存epoll_wait返回的errno，防止下面的函数改变errno

    TimeStamp now(TimeStamp::now());

    if(numEvents>0){
        LOG_INFO("%d events happened \n",numEvents);
        fillActiveChannels(numEvents,activeChannels);

        if(numEvents == events_.size()){
            events_.resize(events_.size()*2);
        }

        /*
        如果当前有超过 events_.size() 个事件就绪，
        epoll_wait 只会返回 events_.size() 个事件，
        剩下的会留到下一次 epoll_wait 调用
        （因为 LT 模式会持续触发未处理的事件）。*/
    }
    else if(numEvents == 0){
        LOG_INFO("%s timeout!\n",__FUNCTION__);
    }else{
        //系统中断会设置errno=EINTR
        if(saveErrno != EINTR){//如果不是系统中断
            errno = saveErrno;
            LOG_ERROR("EpollerPoller::poll: saveErrno != EINTR errno:%d",errno);
        }
    }
    return now;
}

// channel update -> EventLoop updateChannel -> EpollerPoller::updateChannel
void EpollerPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("fd=%d events=%d index=%d \n", channel->fd(), channel->events(), channel->index());
    if (index == kNew || index == kDeleted)
    {
        int fd = channel->fd();
        if (index == kNew)
        {
            channels_[fd] = channel;
        }
        channel->setIndex(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else
    { // index == kAdded
        int fd = channel->fd();
        if (channel->isNoneEvent())
        { // 对任何事件都不感兴趣了
            update(EPOLL_CTL_DEL, channel);
            channel->setIndex(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}
void EpollerPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    int index = channel->index();
    channels_.erase(fd);//从map中删除
    if(index == kAdded){//epoll_ctl(del)
        update(EPOLL_CTL_DEL,channel);
    }
    channel->setIndex(kNew);
}
void EpollerPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    memset(&event, 0, sizeof event);
    event.events = channel->events();
    event.data.ptr = channel;
    int fd = channel->fd();
    
    if(epoll_ctl(epollfd_,operation,fd,&event)<0){
        if(operation == EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl del error:%d",errno);
        }else{
            LOG_FATAL("epoll_ctl add/mod error:%d",errno);
        }
    }
}
 void EpollerPoller::fillActiveChannels(int numEvents,ChannelList *activeChannels)const{
    for(int i =0;i<numEvents;++i){
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);//EventLoop就拿到了发生事件的channel
    }
 }
    
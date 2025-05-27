#pragma once
#include"Poller.h"

#include<vector>
#include<sys/epoll.h>
class EpollerPoller:public Poller
{
public:
    EpollerPoller(EventLoop* loop);
    ~EpollerPoller()override;//检查父类析构函数是否有关键字virtual

    //epoll_wait() 通过ChannelList *activeChannels传出活动的channel给EventLoop
    virtual TimeStamp poll(int timeoutMs, ChannelList *activeChannels)override;
    virtual void updateChannel(Channel *channel)override;
    virtual void removeChannel(Channel *channel)override;
    //如果派生类在虚函数声明时使用了override描述符，那么该函数必须重载其基类中的同名函数，否则代码将无法通过编译。
private:
    //EventList *events_ 初始容量
    static const int kInitEventListSize = 16;

    //填写活跃连接
    void fillActiveChannels(int numEvents,ChannelList *activeChannels)const;
    //epoll_ctl()
    void update(int operation,Channel*channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;

};
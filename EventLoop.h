#pragma once
#include<functional>
#include<vector>
#include<atomic>
#include<memory>
#include<mutex>

#include"noncopyable.h"//继承不能前置声明
#include"TimeStamp.h"//不是定义指针和引用
#include"CurrentThread.h"
class Channel;
class Poller;

class EventLoop:noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();//开始事件循环
    void quit();//退出事件循环

    TimeStamp pollReturnTime()const{return pollReturnTime_;}

    //在当前loop中执行cb
    void runInLoop(Functor cb);
    //把cb放入队列中，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);

    //唤醒loop所在的线程
    void wakeup();

    //EventLoop=>poller方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    bool isInLoopThread()const{return threadId_ == CurrentThread::tid();}
private:
    //wake up
    void handleRead();
    void doPendingFunctors();//执行回调

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;
    std::atomic_bool quit_;
   
    const pid_t threadId_;//记录loop所在线程的id
    TimeStamp pollReturnTime_;//poller返回发生事件的时间点
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;//当mainLoop获取一个新用户channel时，通过轮询算法选择一个subloop，通过该成员唤醒subloop
    std::unique_ptr<Channel> wakeupChannel_;
    
    ChannelList activeChannels_;
 

    std::atomic_bool callingPendingFunctors_;//标识当前loop是否有需要执行回调操作
    std::vector<Functor> pendingFunctors_;//存储回调操作
    std::mutex mutex_;//保护vector的线程安全 
    

};


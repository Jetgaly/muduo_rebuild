#include <unistd.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <errno.h>

#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"
// 防止一个线程创建多个EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;

// poller超时时间
const int kPollTimeMs = 10000;

// 创建wakeupFd，用来notify subReactor处理新来的channel
int creatEventFd()
{
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd err:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop() : looping_(false),
                         quit_(false),
                         callingPendingFunctors_(false),
                         threadId_(CurrentThread::tid()),
                         poller_(Poller::newDefaultPoller(this)),
                         wakeupFd_(creatEventFd()),
                         wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置wakefd的event和事件发生的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping \n", this);
    while (!quit_)
    {
        activeChannels_.clear();
        // 监听clientfd和wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        // 唤醒subReactor后subloop所需要做的mainReactor注册的回调函数
        // 因为刚唤醒的时候subloop只会监听wakeupChannel_
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping \n", this);
    looping_ = false;
}

// loop在自己线程中调用quit || 在其他线程中调用loop的quit(这时候要唤醒本线程的loop，因为有可能阻塞在poller_->poll)
void EventLoop::quit()
{
    quit_ = true;
    if (!isInLoopThread())
    {
        wakeup();
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8 bytes", n);
    }
}
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())
    {
        cb();
    }
    else
    { // 在非当前loop中执行cb，就需要唤醒loop所在的线程，执行cb
        queueInLoop(cb);
    }
}
// 把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    // 唤醒相应的loop线程
    if (!isInLoopThread() || callingPendingFunctors_)
    //callingPendingFunctors_ == true loop正在执行回调，
    //这时候写入新的functor，防止新的functor阻塞，
    //wakeup就是给wakefd发送消息，触发poller的epoll_wait();
    {
        wakeup();
    }
}
// 唤醒loop所在的线程
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_,&one,sizeof one);
    if(n != sizeof one){
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n",n);
    }
}

// EventLoop=>poller方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }
    for(const Functor&functor :functors){
        functor();
    }
    callingPendingFunctors_ = false;
}

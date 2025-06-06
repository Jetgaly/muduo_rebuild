#include "EventLoopThread.h"
#include "EventLoop.h"
EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb)
{
}
EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if(loop_!=nullptr){
        loop_->quit();
        thread_.join();
    }
}

EventLoop *EventLoopThread::startLoop()
{
    thread_.start();//保证tid已经被赋值了
    EventLoop *loop =nullptr;

    std::unique_lock<std::mutex> lock(mutex_);
    while (loop_ == nullptr)
    {
        cond_.wait(lock);//等待开启的新线程将loop对象返回
    }
    loop = loop_;
    return loop;
}

void EventLoopThread::threadFunc()
{
    EventLoop loop;
    if(callback_){
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }
    loop.loop();//EventLoop => poller.poll; 
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
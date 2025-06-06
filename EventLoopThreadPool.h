#pragma once
#include<functional>
#include<string>
#include<vector>
#include<memory>

#include"noncopyable.h"

class EventLoop;
class EventLoopThread;
class EventLoopThreadPool:noncopyable
{

public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;


    EventLoopThreadPool(EventLoop*baseLoop,const std::string &nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads){numThreads_ = numThreads;}
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    //通过轮询方式分配channel
    EventLoop* getNextLoop();
    
    std::vector<EventLoop*> getAllLoops();
    bool started()const{return started_;}
    const std::string name()const{return name_;}
private:
   EventLoop* baseloop_;
   std::string name_;
   bool started_;
   int numThreads_;
   int next_;
   std::vector<std::unique_ptr<EventLoopThread>> threads_;
   std::vector<EventLoop*> loops_;//EventLoopThread 栈上的loop对象，不用手动释放
};

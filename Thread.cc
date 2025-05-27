#include<semaphore.h>

#include "Thread.h"
#include "CurrentThread.h"

std::atomic_int Thread::numCreated_{0} ;


Thread::Thread(ThreadFunc func, const std::string &name) : started_(false),
                                                           joined_(false),
                                                           tid_(0),
                                                           func_(std::move(func)),
                                                           name_(name)
{
    setDefaultName();
}
Thread::~Thread()
{
    if (started_ && !joined_)
    {
        thread_->detach();
    }
}
void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32];
        snprintf(buf, sizeof buf, "thread-%d", num);
        name_ = buf;
    }
}
void Thread::start()
{
    started_ = true;
    sem_t sem;//等待tid赋值的信号量
    sem_init(&sem,false,0);
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        func_();
    }));
    //等待tid赋值
    sem_wait(&sem);


}
void Thread::join()
{
    joined_=true;
    thread_->join();
}
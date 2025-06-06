#include "Poller.h"
#include "EpollerPoller.h"
#include <stdlib.h>
Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr;//poll
    }
    else
    {
        return new EpollerPoller(loop);//epoll
    }
}
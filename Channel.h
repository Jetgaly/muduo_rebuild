#pragma once
#include "noncopyable.h"
#include <functional>
#include <memory>
class EventLoop; // 避免暴露
#include "TimeStamp.h"
/*
Channel 封装了sockFd 和 感兴趣的事件（EPOLLIN EPOLLOUT...）
还绑定了poller返回的发生的事件
*/
class Channel : noncopyable
{
public:
    // typedef std::function<void()> EventCallback;
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(TimeStamp)>;

    Channel(EventLoop *p, int fd);
    ~Channel();

    // fd得到poller通知以后，调用事件的处理函数
    void handleEvent(TimeStamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb)
    {
        readCallback_ = std::move(cb);
    } // function对象会有移动构造函数

    void setWriteCallback(EventCallback cb)
    {
        writeCallback_ = std::move(cb);
    }

    void setCloseCallback(EventCallback cb)
    {
        closeCallback_ = std::move(cb);
    }

    void setErrorCallback(EventCallback cb)
    {
        errorCallback_ = std::move(cb);
    }

    // 防止被手动remove的channel还在执行回调操作、
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; }
    int events() const { return events_; }
    // 提供给poller设置发生的事件
    void set_revents(int revt) { revents_ = revt; }
   

    // 设置fd感兴趣的事件状态
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    };
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    };

    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    };
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    };
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    };

    // 返回fd当前感兴趣事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void setIndex(int idx) { index_ = idx; }

    EventLoop *ownerLoop()
    {
        return loop_;
    }
    void remove();

private:
    void update();
    void handleEventWithGuard(TimeStamp recviveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;
    const int fd_; // poller 监听的对象
    int events_;   // 注册fd感兴趣的事件
    int revents_;  // poller返回的具体发生的事件
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

    // 事件发生回调函数
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

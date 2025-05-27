#pragma once
#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Logger.h"

class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    enum Option{
        kNOReusePort,
        kReusePort
    };

    TcpServer(EventLoop*loop,const InetAddress &listenAddr,const std::string &nameArg,Option opt = kNOReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback &cb){
        threadInitCallback_ = cb;
    }
    void setConnectionCallback(const ConnectionCallback &cb){
        connectionCallback_ = cb;
    }
    void setMessageCallback(const MessageCallback &cb){
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb){
        writeCompleteCallback_ = cb;
    }

    void setThreadNum(int numThreads);

    //开启服务器监听
    void start();

private:
    void newConnection(int sockfd,const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    EventLoop *loop_; // baseloop
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;//运行在mainloop 监听
    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成的callback

    ThreadInitCallback threadInitCallback_;
    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_;//保存所有连接

};

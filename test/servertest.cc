#include <muduo_rebuild/TcpServer.h>

class testServer
{
public:
    testServer(EventLoop *loop, const InetAddress &Addr, const std::string &nameArg, TcpServer::Option opt = TcpServer::kNOReusePort)
        : server_(loop, Addr, nameArg,opt), loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(std::bind(&testServer::onConnection,this,std::placeholders::_1));
        server_.setMessageCallback(std::bind(&testServer::onMessage,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));


        // 设置线程数量
        server_.setThreadNum(3);
    }

    void start(){
        server_.start();
    }
private:
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO("connected : %s\n", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("disconnected : %s\n", conn->peerAddress().toIpPort().c_str());
        }
    }
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, TimeStamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        std::cout<<"flagggg:"<<msg<<std::endl;
        conn->send(msg);
        conn->shutdown();
    }
    EventLoop *loop_;
    TcpServer server_;
};

int main(){
    EventLoop loop;
    InetAddress addr(19090);
    testServer server(&loop,addr,"testServer");

    server.start();
    loop.loop();
    return 0;
}
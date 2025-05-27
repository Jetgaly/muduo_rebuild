#include <arpa/inet.h>
#include "InetAddress.h"
#include "string.h"
// 参数的默认值声明只能出现一次
// ipv6不考虑 默认ipv4
InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}
std::string InetAddress::toIp() const
{
    char buf[64];
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    //将网络字节序的ip转化为string点分十进制
    return buf;
}
std::string InetAddress::toIpPort() const
{
    char buf[64];
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf+end,":%u",port);
    return buf;
}
uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}

// #include<iostream>
// int main(){
//     InetAddress addr(8080);
//     std::cout<<addr.toIpPort()<<std::endl;
//     return 0;
// }
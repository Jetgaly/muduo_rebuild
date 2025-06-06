#include <sys/uio.h>
#include <unistd.h>
#include "Buffer.h"

/*
从fd上读取数据 poller工作在LT模式
*/
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536] = {0};//64k

    struct iovec vec[2];

    const size_t writable = writerableBytes();
    vec[0].iov_base = begin()+writerIndex_;
    vec[0].iov_len = writerableBytes();

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable<sizeof extrabuf)?2:1;
    const ssize_t n = readv(fd,vec,iovcnt);//如果这次没读完数据，在下一轮还会触发可读事件
    if(n<0){
        *saveErrno = errno;
    }
    else if(n<=writable){
        writerIndex_+=n;
    }else{
        writerIndex_=buffer_.size();
        append(extrabuf,n - writable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd,int* saveErrno){
    ssize_t n = write(fd,peek(),readableBytes());
    if(n<0){
        *saveErrno = errno;
    }
    return n;
}
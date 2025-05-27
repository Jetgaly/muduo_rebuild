#pragma once
#include <vector>
#include <string>
#include <algorithm>

#include "noncopyable.h"

/*
buffer_
    prependable        readable bytes       writable bytes
   |------------------|--------------------|-------------------|
 begin           readerIndex_         writerIndex_            size

*/
class Buffer : noncopyable
{

public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {
    }
    ~Buffer() {};
    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }
    size_t writerableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }
    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    // 返回缓冲区中可读数据的起始地址
    const char *peek()
    {
        return begin() + readerIndex_;
    }

    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len;
        }
        else
        {
            retrieveAll();
        }
    }
    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len); // 对缓冲区进行复位操作
        return result;
    }

    void ensuredWriteableBytes(size_t len)
    {
        if (writerableBytes() < len)
        {
            makeSpace(len);
        }
    }

    //写入缓冲区 
    void append(const char *data, size_t len)
    {
        ensuredWriteableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_+=len;
    }

    char *beginWrite()
    {
        return begin() + writerIndex_;
    }
    const char *beginWrite() const
    {
        return begin() + writerIndex_;
    }

    ssize_t readFd(int fd,int* saveErrno);
    ssize_t writeFd(int fd,int* saveErrno);
private:

    //buffer_.begin()重载的版本不一样
    char *begin() { return &(*buffer_.begin()); /*iterator*/}
    const char *begin() const { return &(*buffer_.begin()); /*const_iterator*/}
    void makeSpace(size_t len)
    {
        if (writerableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = (readerIndex_ + readable);
        }
    }
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};

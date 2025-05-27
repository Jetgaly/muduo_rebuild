#pragma once
#include "string"
#include "noncopyable.h"
// LOG_INFO("%s %d",arg1,arg2)
#define LOG_INFO(msgFormat, ...)                       \
    do                                                 \
    {                                                  \
        Logger &logger = Logger::getInstance();        \
        logger.setLogLevel(INFO);                      \
        char buf[1024];                                \
        snprintf(buf, 1024, msgFormat, ##__VA_ARGS__); \
        logger.log(buf);                               \
    } while (0)

// LOG_INFO("%s %d",arg1,arg2)
#define LOG_ERROR(msgFormat, ...)                      \
    do                                                 \
    {                                                  \
        Logger &logger = Logger::getInstance();        \
        logger.setLogLevel(ERROR);                     \
        char buf[1024];                                \
        snprintf(buf, 1024, msgFormat, ##__VA_ARGS__); \
        logger.log(buf);                               \
    } while (0)

// LOG_INFO("%s %d",arg1,arg2)
#define LOG_FATAL(msgFormat, ...)                      \
    do                                                 \
    {                                                  \
        Logger &logger = Logger::getInstance();        \
        logger.setLogLevel(FATAL);                     \
        char buf[1024];                                \
        snprintf(buf, 1024, msgFormat, ##__VA_ARGS__); \
        logger.log(buf);                               \
        exit(-1);                                      \
    } while (0)

#ifdef MUDEBUG // 要有MUDEBUG宏才输出调试信息
// LOG_INFO("%s %d",arg1,arg2)
#define LOG_DEBUG(msgFormat, ...)                      \
    do                                                 \
    {                                                  \
        Logger &logger = Logger::getInstance();        \
        logger.setLogLevel(DEBUG);                     \
        char buf[1024];                                \
        snprintf(buf, 1024, msgFormat, ##__VA_ARGS__); \
        logger.log(buf);                               \
    } while (0)
#else
#define LOG_DEBUG(msgFormat, ...)
#endif

// 定义日志级别 INFO ERROR FATAL DEBUG
enum LogLevel
{
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};

class Logger : noncopyable // 不用写拷贝构造delete 和 operator= delete了
{
public:
    static Logger &getInstance();

    void setLogLevel(int level);

    // 写日志
    void log(std::string msg);

private:
    int logLevel_;
    Logger() {}
};
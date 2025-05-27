#include "Logger.h"
#include "TimeStamp.h"
#include <iostream>


Logger &Logger::getInstance()
{
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(int level)
{
    logLevel_ = level;
}

// 写日志
void Logger::log(std::string msg)
{
    switch (logLevel_)
    {
    case INFO:
        std::cout << "[INFO]";
        break;

    case ERROR:
        std::cout << "[ERROR]";
        break;

    case FATAL:
        std::cout << "[FATAL]";
        break;

    case DEBUG:
        std::cout << "[DEBUG]";
        break;

    default:
        break;
    }

    // print time and msg
    std::cout << TimeStamp::now().toString() << " : " << msg << std::endl;
}

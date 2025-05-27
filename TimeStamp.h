#pragma once
#include <iostream>
#include <string>
class TimeStamp
{
private:
    /* data */
    int64_t microSecondsSinceEpoch;

public:
    TimeStamp();
    //禁止隐式转换
    explicit TimeStamp(int64_t microSecondsSinceEpoch);
    
    static TimeStamp now();
    std::string toString() const;
};

#pragma once

class noncopyable
{
protected:
     noncopyable() = default;//==noncopyable(){};
    ~noncopyable() = default;
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
    //继承了这个类的子类，派生类对象可以正常构造和析构，但是不能拷贝构造和赋值
    //因为子类对象构造会先调用父类对应的构造函数
};



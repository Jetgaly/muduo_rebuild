#pragma once
#include<unistd.h>

namespace CurrentThread{
    extern __thread int t_cachedTid;
/*
64 位系统(tid)
范围：1 ~ 2^22 - 1（默认 32768，可扩展到 4194304 或更高）

理论上最大 2^22 - 1 = 4194303，但可以调整到更大（如 4M 或更高）
TID = 0：保留给 swapper（内核 idle 线程）
*/
    void cacheTid();
    inline int tid(){
        if(__builtin_expect(t_cachedTid==0,0)){
            cacheTid();
        }
        return t_cachedTid;
    }
}
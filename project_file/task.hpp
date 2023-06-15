#pragma once

#include <iostream>
#include "process_protocol.hpp"

class Task {
private:
    int _sock;
    CallBack _handler; //设置回调

public:
    Task()
    {}

    Task(int sock)
        : _sock(sock)
    {}

    ~Task()
    {}

public:
    //处理任务
    void ProcessOn() {
        _handler(_sock); //这里调用的就是他的仿函数 // process_protocol.hpp -> HandlerRequest(sock)
    }

};
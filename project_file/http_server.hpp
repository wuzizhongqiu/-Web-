#pragma once 

#include "tcp_server.hpp"
#include "log.hpp"
#include "task.hpp"
#include "thread_pool.hpp"

#include <pthread.h>
#include <signal.h>

#define PORT 8081

class HttpServer {
private:
    int _port;
    bool _stop;

public:
    HttpServer(int port = PORT)
        : _port(port)
        , _stop(false)
    {}

    ~HttpServer()
    {}

public:
    void InitServer() { 
        //信号SIGPIPE需要忽略，如果不忽略，在写入失败的时候，服务端可能会崩溃
        signal(SIGPIPE, SIG_IGN);    
    }

    void Loop() { //循环接收请求
        LOG(INFO, "loop begin...");
        
        while (!_stop) {
            struct sockaddr_in peer;
            socklen_t len = sizeof(peer);
            int sock = accept(TcpServer::getinstance(_port)->Sock(), (struct sockaddr*)&peer, &len);
            if(sock < 0) {
                continue;
            }

            LOG(INFO, "get a new link...");
            Task task(sock);
            ThreadPool::GetInstance()->PushTask(task);
        }
    }
};

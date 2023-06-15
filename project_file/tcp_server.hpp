#pragma once

#include "log.hpp"

#include <iostream>
#include <cstdlib> 
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> 

#define BACKLOG 5

class TcpServer { //这里就是用套接字构建一个tcp的服务
private:
    int _port;
    int _listen_sock;
    static TcpServer* svr;

private: //单例模式
    TcpServer(int port)
        : _port(port)
        , _listen_sock(-1)
    {}

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

public:
    ~TcpServer()
    {
        if(_listen_sock >= 0) close(_listen_sock);
    }

public:
    static TcpServer* getinstance(int port) { //构建单例
        static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        if(nullptr == svr) {
            pthread_mutex_init(&lock, nullptr);
            if(nullptr == svr) { //这里的二次检测的知识点有点忘记了！！！ 
                svr = new TcpServer(port);
                svr->InitServer();
            }
            pthread_mutex_destroy(&lock);
        }
        return svr;
    }

    void InitServer() { //基本的套接字操作
        Socket();
        Bind();
        Listen();
        LOG(INFO, "tcp_server init success...");
    }

    void Socket() { 
        _listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if(_listen_sock < 0) {
            LOG(FATAL, "socket error!");
            exit(1);
        }

        int opt = 1;
        setsockopt(_listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); //立即重启服务（解决bind error问题）
        LOG(INFO, "create socket success...");
    }

    void Bind() {
        struct sockaddr_in local;
        memset(&local, 0 ,sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(_port);
        local.sin_addr.s_addr = INADDR_ANY; //云服务器不能直接绑定公网IP

        if(bind(_listen_sock, (struct sockaddr*)&local, sizeof(local)) < 0) {
            LOG(FATAL, "bind error!");
            exit(2);
        }
        LOG(INFO, "bind socket sucess...");
    }

    void Listen() {
        if(listen(_listen_sock, BACKLOG) < 0) {
            LOG(FATAL, "listen error!");
            exit(3);
        }
        LOG(INFO, "listen socket success...");
    }
    
    int Sock() { return _listen_sock; }

};

TcpServer* TcpServer::svr = nullptr;

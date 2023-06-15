#include "http_server.hpp"

#include <iostream>
#include <string>
#include <memory>

static void Usage() {
    std::cout << "你需要安装这个格式输入：\n\t" << "./tcp_server port" << std::endl;
}

int main(int argc, char* argv[]) 
{
    if(argc != 2) {
        Usage();
        exit(4);
    }

    int port = atoi(argv[1]);

    std::shared_ptr<HttpServer> http_server(new HttpServer(port)); 

    http_server->InitServer();
    http_server->Loop(); // loop accept

    return 0;
}

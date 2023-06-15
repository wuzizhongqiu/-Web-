#pragma once

#include "my_tool.hpp"
#include "log.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/socket.h>

#define SEP ": "
#define WEB_ROOT "wwwroot"
#define HOME_PAGE "index.html"
#define HTTP_VERSION "HTTP/1.0"
#define LINE_END "\r\n"
#define PAGE_404 "404.html"

#define OK 200
#define BAD_REQUEST 400
#define NOT_FOUND 404
#define SERVER_ERROR 500

// #define DEBUG 1 //测试时开放

//根据状态码形成字符串形式的状态描述返回
static std::string Code2Desc(int code) {
    std::string desc;
    switch (code) {
        case 200:
            desc = "OK";
            break;
        case 404:
            desc = "Not Found";
            break;
        default:
            break;
    }
    return desc;
}

//根据文件后缀映射处理对应文件的方法，默认是：text/html
static std::string Suffix2Desc(const std::string& suffix) { 
    static std::unordered_map<std::string, std::string> Suffix2Desc = {
        {".html", "text/html"}, 
        {".css", "text/css"}, 
        {".js", "application/javascript"}, 
        {".jpg", "application/x-jpg"}, 
        {".xml", "application/xml"}, 
    };

    auto iter  = Suffix2Desc.find(suffix);
    if(iter != Suffix2Desc.end()) {
        return iter->second;
    }
    return "text/html";
}

// HTTP请求 
struct HttpRequest { 
    std::string request_line;                //请求行
    std::vector<std::string> request_header; //请求报头
    std::string blank;                       //空行
    std::string request_body;                //请求报文

    //解析完毕后的结果
    std::string method;   // 请求方法
    std::string uri;      // path？args
    std::string version;  // 版本

    std::unordered_map<std::string, std::string> header_kv; //解析报头形成的kv

    int content_length = 0;

    std::string path;         //文件路径
    std::string suffix;       //文件后缀
    std::string query_string; //对文件发送的数据请求（path?args）中的args这部分
    int size;                 //被请求的文件的大小

    bool cgi = false;         //是否需要使用CGI技术（1. GET方法URI带参，2. POST方法）

};

// HTTp响应
struct HttpResponse { //响应
    std::string status_line;                  //状态行
    std::vector<std::string> response_header; //响应报头
    std::string blank = LINE_END;             //空行
    std::string response_body;                //响应报文

    int status_code = OK; //状态描述
    int fd = -1;          //默认返回的静态网页的文件描述符

};

//读取请求，分析请求，构建响应并发送 + IO通信
class Feature {
private:
    int _sock; //通信时打开的sock文件
    bool _stop;
    HttpRequest _http_request;
    HttpResponse _http_response;

private: //封装接口实现
    //读请求行
    bool RecvHttpRequestLine() { 
        auto& line = _http_request.request_line;
        if(Tool::ReadLine(_sock, line) > 0) {
            line.resize(line.size() - 1); // 清理最后的 \n
            LOG(INFO, line);
        }
        else {
            _stop = true;
        }
        return _stop;
    }

    //读请求报头
    bool RecvHttpRequestHeader() { 
        std::string line;
        while(true) {
            line.clear(); //记得清理line （一开始忘记了，出现line长度爆了的情况）
            if(Tool::ReadLine(_sock, line) <= 0) {
                _stop = true;
                break;
            }
            if(line == "\n") {
                _http_request.blank = line;
                break;
            }
            line.resize(line.size() - 1); // 清理最后的 \n
            _http_request.request_header.push_back(line);
            // LOG(INFO, line); 
        }
        return _stop;
    }

    //解析请求行 // 方法 URI 版本 
    void ParseHttpRequestLine() { 
        auto& line = _http_request.request_line;
        std::stringstream ss(line);
        ss >> _http_request.method >> _http_request.uri >> _http_request.version;

        // 别人的方法格式可能不是大写，我们的判断是大写的，所以需要转成大写
        auto& method = _http_request.method;
        std::transform(method.begin(), method.end(), method.begin(), ::toupper);
    }

    //解析报头，形成kv
    void ParseHttpRequestHeader() { 
        std::string key;
        std::string value;
        for(auto& iter : _http_request.request_header) {
            if(Tool::CutString(iter, key, value, SEP)) {
                _http_request.header_kv.insert({key, value});
            }
        }
    }

    //是否需要读取请求正文（请求正文是否存在）
    bool IsNeedRecvHttpRequestBody() { 
        auto& method = _http_request.method;
        if(method == "POST") {
            auto& header_kv = _http_request.header_kv;
            auto iter = header_kv.find("Content-Length");
            if(iter != header_kv.end()) {
                _http_request.content_length = atoi(iter->second.c_str());
                return true;
            }
        }
        return false;
    }

    //读报文（请求正文）POST方法
    bool RecvHttpRequestBody() {
        if(IsNeedRecvHttpRequestBody()) { //只有POST方法有请求正文（我这里只考虑GET和POST这两个方法）
            int content_length = _http_request.content_length;
            auto& body = _http_request.request_body;

            char ch = 0;
            while(content_length) {
                ssize_t s = recv(_sock, &ch, 1, 0);
                if(s > 0) {
                    body.push_back(ch);
                    content_length--;
                }
                else {
                    _stop = true;
                    break;
                }
            }
            LOG(INFO, "body: " + body);
        }
        return _stop;
    }

    //这里设置的是错误页面
    void HandlerError(std::string page) {
        _http_request.cgi = false;
        _http_response.fd = open(page.c_str(), O_RDONLY);
        if(_http_response.fd > 0) {
            struct stat st;
            stat(page.c_str(), &st);
            _http_request.size = st.st_size;  
            std::string line = "Content-Type: text/html";
            line += LINE_END;
            _http_response.response_header.push_back(line);

            line = "Content-Lenght: ";
            line += std::to_string(st.st_size);
            line += LINE_END; 
            _http_response.response_header.push_back(line); 
        }
    }

    //返回响应报文
    void BuildOKResponse() {
        std::string line = "Content-Type: ";
        line += Suffix2Desc(_http_request.suffix);
        line += LINE_END;
        _http_response.response_header.push_back(line);

        line = "Content-Lenght: ";
        if(_http_request.cgi) {
            line += std::to_string(_http_response.response_body.size());
        }
        else {
            line += std::to_string(_http_request.size);
        }
        line += LINE_END;
        _http_response.response_header.push_back(line); 
    }

    //构建响应
    void BuildHttpResponseHelper() {
        auto& code = _http_response.status_code;

        //响应的状态行
        auto& status_line = _http_response.status_line;
        status_line += HTTP_VERSION;
        status_line += " ";
        status_line += std::to_string(code);
        status_line += " ";
        status_line += Code2Desc(code);
        status_line += LINE_END;

        //构建响应正文（可能包含报头）
        std::string path = WEB_ROOT;
        path += "/";
        switch (code) { 
        case OK:
            BuildOKResponse(); 
            break;
        case BAD_REQUEST: //这里错误信息暂时都直接按照404返回，之后有需求可以添加其它网页
            path += PAGE_404;
            HandlerError(path);
            break;
        case NOT_FOUND:
            path += PAGE_404;
            HandlerError(path);
            break;
        case SERVER_ERROR:
            path += PAGE_404;
            HandlerError(path);
            break;
        default:
            break;
        }

    }

    //用CGI技术处理数据
    int ProcessCgi() {
        LOG(INFO, "process cgi method!");
        int code = OK;

        //父进程数据
        auto& method = _http_request.method;               //请求方法
        auto& query_string = _http_request.query_string;   //GET方法需要的数据
        auto& body_text = _http_request.request_body;      //POST方法需要的数据
        auto& response_body = _http_response.response_body;
        int content_length = _http_request.content_length; //别人发送的数据的长度

        //要让子进程执行的目标程序，一定是存在的（前面写的代码保证的）
        auto& bin = _http_request.path; //查找的文件路径                

        std::string query_string_env; 
        std::string method_env;
        std::string content_length_env;

        //站在父进程角度，父子间匿名管道通信，让子进程调用这些程序
        int input[2];
        int output[2];

        if(pipe(input) < 0) {
            LOG(ERROR, "pipe input error");
            code = SERVER_ERROR;
            return code;
        }
        if(pipe(output) < 0) {
            LOG(ERROR, "pipe output error");
            code = SERVER_ERROR;
            return code;
        }

        pid_t pid = fork();
        if(pid == 0) { //子进程 
            close(input[0]);
            close(output[1]);

            //不要在这个位置进行重定向！！！
            // //站在子进程的角度，重定向
            // //input[1]: 写  1 -> input[1]
            // //output[0]: 读 0 -> output[0]
            // dup2(input[1], 1); //将标准输出重定向成input[1]写入
            // dup2(output[0], 0); //将标准输入重定向成output[0]读取

            //子进程需要知道是什么方法
            method_env = "METHOD=";
            method_env += method;
            putenv((char*)method_env.c_str());

            //我们直接通过环境变量将数据传到程序替换之后的代码
            if(method == "GET") {
                query_string_env = "QUERY_STRING=";
                query_string_env += query_string;
                putenv((char*)query_string_env.c_str()); //putenv((char*)query_string.c_str()) //这里，传参传错了！！！
                LOG(INFO, "Get method, add query_sting env");
            }
            else if(method == "POST") {
                content_length_env = "CONTENT_LENGTH=";
                content_length_env += std::to_string(content_length);
                putenv((char*)content_length_env.c_str());
                LOG(INFO, "POST method, add Content-Length");
            }
            else {
                //do nothing
            }
 
            //注意！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
            //如果先重定向了文件的读取和写入，上面我们用来调试的日志就打印不出来了
            //注意！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！

            //站在子进程的角度，重定向
            //input[1]: 写  1 -> input[1]
            //output[0]: 读 0 -> output[0] 
            dup2(input[1], 1); //将标准输出重定向成input[1]写入
            dup2(output[0], 0); //将标准输入重定向成output[0]读取

            //这样替换成功之后，就不需要知道子进程的文件描述符，只需要知道是：读0，写1即可。
            execl(bin.c_str(), bin.c_str(), nullptr); //程序替换，执行
            exit(1); 
        }
        else if(pid < 0) { //创建失败
            LOG(ERROR, "fork error!");
            code = SERVER_ERROR;
            return code; 
        }
        else { //父进程
            close(input[1]);
            close(output[0]);

            //通讯信道已经建立好了，开始通讯
            if(method == "POST") { //请求的数据在报文
                const char* start = body_text.c_str();
                int total = 0; //已经写入的数据大小
                int size = 0;  //每次写入的大小
                //循环写入，直到写完
                while(total < content_length && (size = write(output[1], start + total, body_text.size() - total)) > 0) {
                    total += size;
                }
            }

            //将最后取得的结果塞进response_body里面
            char ch = 0;
            while(read(input[0], &ch, 1) > 0) {
                response_body.push_back(ch);
            }

            int status = 0;
            pid_t ret = waitpid(pid, &status, 0);
            if(ret == pid) {
                if(WIFEXITED(status)) { //子进程退出正常
                    if(WEXITSTATUS(status) == 0) { //退出码没有问题
                        code = OK;
                    }
                    else {
                        code = BAD_REQUEST;
                    }
                }
                else {
                    code = SERVER_ERROR;
                }
            }

            close(input[0]);
            close(output[1]);
        }
        return code;
    }

    //不需要使用CGI技术处理数据
    int ProcessNonCgi() {
        _http_response.fd = open(_http_request.path.c_str(), O_RDONLY);
        if(_http_response.fd >= 0) { //成功打开文件
            LOG(INFO, _http_request.path + " open success!");
            return OK;
        }
        return NOT_FOUND;
    }

    //构建响应
    void _BuildHttpResponse() { 
        std::string path;
        int fd_size; //需要发送的文件的大小（报文资源）
        std::size_t found = 0; //查找后缀

        auto& code = _http_response.status_code;
        if(_http_request.method != "GET" && _http_request.method != "POST") {
            //非法请求
            LOG(WARNING, "method is not right");
            code = BAD_REQUEST;
            goto END;
        }
        if(_http_request.method == "GET") {
            size_t pos = _http_request.uri.find('?');
            if(pos != std::string::npos) { //有问号
                Tool::CutString(_http_request.uri, _http_request.path, _http_request.query_string, "?");
                _http_request.cgi = true;
            }
            else { //没有问号，证明uri里面没有带任何参数
                _http_request.path = _http_request.uri;
            }
        }
        else if(_http_request.method == "POST") {
            _http_request.cgi = true;
            _http_request.path = _http_request.uri; //！！！在这里，设置好使用POST方法的时候的路径！！！
        }
        else {
            // Do nothing
        }

        path = _http_request.path;
        _http_request.path = WEB_ROOT;
        _http_request.path += path;

        if(_http_request.path[_http_request.path.size() - 1] == '/') {
            _http_request.path += HOME_PAGE;
        }

        struct stat st;
        if(stat(_http_request.path.c_str(), &st) == 0) { //该文件资源存在
            if(S_ISDIR(st.st_mode)) { //请求的是一个目录
                _http_request.path += "/";
                _http_request.path += HOME_PAGE; //给用户返回该目录的首页（我这里默认规定的首页都是 index.html )
                stat(_http_request.path.c_str(), &st); //更新stat文件信息（因为前面给用户返回了新的首页文件）
            }
            if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) { //这是个可执行文件，需要特殊处理
                _http_request.cgi = true;
            }
            _http_request.size = st.st_size;
        } 
        else { //说明文件资源不存在
            LOG(WARNING, _http_request.path + " NOT_FOUND");
            code = NOT_FOUND;
            goto END;
        }
        
        // 提取后缀
        found = _http_request.path.rfind(".");
        if(found == std::string::npos) {
            _http_request.suffix = ".html";
        }
        else {
            _http_request.suffix = _http_request.path.substr(found);
        }

        if(_http_request.cgi) { 
            code = ProcessCgi(); //执行目标程序，拿到结果：_http_response.response_body
        }
        else {
            code = ProcessNonCgi();
        }

END:
        //构建响应
        BuildHttpResponseHelper();  
    }

    //发送响应
    void _SendHttpResponse() { 
        send(_sock, _http_response.status_line.c_str(), _http_response.status_line.size(), 0); //状态行
        for(auto iter : _http_response.response_header) { //报头
            send(_sock, iter.c_str(), iter.size(), 0);
        }
        send(_sock, _http_response.blank.c_str(), _http_response.blank.size(), 0); //空行

        if(_http_request.cgi) {
            auto& response_body = _http_response.response_body;
            const char* start = response_body.c_str();
            size_t size = 0;
            size_t total = 0;
            while(total < response_body.size() && (size = send(_sock, start + total, response_body.size() - total, 0)) > 0) {
                total += size;
            }
        }
        else {
            sendfile(_sock, _http_response.fd, nullptr, _http_request.size); //报文资源
            close(_http_response.fd);
        }
    }

public:
    Feature(int sock)
        : _sock(sock)
        , _stop(false)
    {}

    ~Feature()
    {
        close(_sock);
    }

    bool IsStop() {
        return _stop;
    }

public:
    void RecvHttpRequest() { //读取(并解析)请求
        //读行                      //读报头
        if(RecvHttpRequestLine() || RecvHttpRequestHeader()) { //当他们都为false的时候，才会进else语句，否则就是出错
            //读取失败
        }
        else {
            ParseHttpRequestLine();  //解析行
            ParseHttpRequestHeader();//解析报头
            RecvHttpRequestBody();   //读报文
        }
    }

    void BuildHttpResponse() { //构建响应
        _BuildHttpResponse();
    }
    
    void SendHttpResponse() { //发送响应
        _SendHttpResponse();
    }
};

class CallBack { //入口
public:
    CallBack()
    {}

    ~CallBack()
    {}

    //仿函数
    void operator()(int sock) {
        HandlerRequest(sock);
    }

public: 
    void HandlerRequest(int sock) {
        LOG(INFO, "handler request begin...");

#ifdef DEBUG
        char buffer[4096];2
        recv(sock, buffer, sizeof(buffer), 0); //接收http报文，用于观察

        std::cout << buffer << std::endl;
#else
        Feature* fea = new Feature(sock);
        fea->RecvHttpRequest();
        if(!fea->IsStop()) {
            LOG(INFO, "Recv no error, Begin Build and Send");
            fea->BuildHttpResponse();
            fea->SendHttpResponse();
        }
        else {
            LOG(WARNING, "Recv error, Stop Build and Send");
        }

        delete fea;
#endif

        LOG(INFO, "handler request end...");
    }
};

#include <iostream>
#include <cstdlib>
#include <unistd.h>

//获取数据
bool GetQueryString(std::string& query_string) {
    bool ret = false;

    std::string method = getenv("METHOD");

    if(method == "GET") {\
        query_string = getenv("QUERY_STRING");
        ret = true;
    }
    else if(method == "POST") {
        int content_length = atoi(getenv("CONTENT_LENGTH"));
        std::cerr << content_length << std::endl;
        char c = 0;
        while(content_length--) {
            read(0, &c, 1);
            query_string.push_back(c);
        }
        ret = true;
    }
    else {
        //do nothing
    }
    return ret;
}

//处理方法：切分字符串
void CutString(std::string& in, const std::string& sep, std::string& out1, std::string& out2) {
    auto pos = in.find(sep);
    if(std::string::npos != pos) {
        out1 = in.substr(0, pos);
        out2 = in.substr(pos + sep.size());
    }
}

int main()
{
    std::string query_string;
    GetQueryString(query_string);

    // a=100&b=200
    std::string str1;
    std::string str2;
    CutString(query_string, "&", str1, str2);

    std::string name1;
    std::string value1;
    CutString(str1, "=", name1, value1);

    std::string name2;
    std::string value2;
    CutString(str2, "=", name2, value2);

    std::cout << name1 << "=" << value1 << std::endl;
    std::cout << name2 << "=" << value2 << std::endl;

    std::cerr << name1 << "=" << value1 << std::endl;
    std::cerr << name2 << "=" << value2 << std::endl;

    int x = atoi(value1.c_str());
    int y = atoi(value2.c_str());

    //可能进行计算，搜索，存储（注册）
    std::cout << "<html>" << std::endl;
    std::cout << "<head><meta charset=\"utf-8\"></head>";
    std::cout << "<body>" << std::endl;
    std::cout << "<h1>" << value1 << " + " << value2 << " = " << x + y << "</h1>" << std::endl;
    std::cout << "<h1>" << value1 << " - " << value2 << " = " << x - y << "</h1>" << std::endl;
    std::cout << "<h1>" << value1 << " * " << value2 << " = " << x * y << "</h1>" << std::endl;
    std::cout << "<h1>" << value1 << " / " << value2 << " = " << x / y << "</h1>" << std::endl;

    std::cout << "</body>" << std::endl;
    std::cout << "</html>" << std::endl;

    return 0;
}
bin=httpserver# 可执行程序
cgi=test_cgi# 需要执行的CGI程序
cc=g++# 编译器
lib=-std=c++11# 需要调的C++库
pthread=-lpthread# 需要调的线程库

src=main.cc# 主函数
pwd=$(shell pwd)# 暂时不需要

ALL:$(bin) $(cgi)# ALL依赖于这两个可执行程序
.PHONY:ALL

$(bin):$(src) 
	$(cc) -o $@ $^ $(lib) $(pthread)

$(cgi):cgi/test_cgi.cc
	$(cc) -o $@ $^ $(lib)

.PHONY:clean
clean:
	rm -rf $(bin) $(cgi)
	rm -rf output

.PHONY:output# 打包
output:
	mkdir -p output
	cp $(bin) output
	cp -rf wwwroot output
	cp $(cgi) output/wwwroot
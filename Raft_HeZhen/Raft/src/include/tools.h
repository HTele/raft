#ifndef TOOLS_H
#define TOOLS_H
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include "confdeal.h"
#include <functional>
#include"threadpool.h"
#include <chrono>
#include <random>
#include <string>
#include <sstream>


//一个转换函数，把自己id(x)发送给其他节点在send_fd的index(y)时转换为自己在对应节点地send_fd里的index，只适用于port（id）是连号的情况
int turn_id(int x,int y){
    if(y+1<x)return x-2;
    else return x-1;
}


//一个转换函数，根据自己id(x)和其他节点id(y)转换为自己在对应节点地send_fd里的index，只适用于port（id）是连号的情况
int fd_id(int x,int y){
    if(y<x)return y-1;
    else  return y-2;
}

//解析Redis协议格式的字符串
std::vector<std::string> parse_string(const std::string& str) {
    //cout<<"解析客户端请求"<<endl;
    std::vector<std::string> result;
    size_t pos = 0;
    while (pos < str.size()) {
        if (str[pos] == '*') {
            // 读取参数数量
            size_t end_pos = str.find("\r\n", pos+1);
            int count = std::stoi(str.substr(pos+1, end_pos-pos-1));
            pos = end_pos + 2;
            for (int i = 0; i < count; ++i) {
                // 读取参数长度
                end_pos = str.find("\r\n", pos+1);
                int len = std::stoi(str.substr(pos+1, end_pos-pos-1));
                pos = end_pos + 2;
                // 读取参数值
                result.emplace_back(str.substr(pos, len));
                pos += len + 2;
            }
        } else if (str[pos] == '$') {
            // 读取参数长度
            size_t end_pos = str.find("\r\n", pos+1);
            int len = std::stoi(str.substr(pos+1, end_pos-pos-1));
            pos = end_pos + 2;
            // 读取参数值
            result.emplace_back(str.substr(pos, len));
            pos += len + 2;
        } else {
            // 无效的格式
            break;
        }
    }
    //cout<<"解析完成"<<endl;
    return result;
}

//将字符串转换为Redis协议格式的字符串
std::string to_redis_string(const std::string& str) {
    std::stringstream ss(str);
    std::string word;
    std::string result;
    int count = 0;
    while (ss >> word) {
        ++count;
        result += "$" + std::to_string(word.size()) + "\r\n"; // 参数长度
        result += word + "\r\n"; // 参数值
    }
    result = "*" + std::to_string(count) + "\r\n" + result; // 参数数量
    return result;
}

std::string infoToRedisProtocol(const Info& info) {
    std::ostringstream oss;
    /*if (std::strcmp(info.key, "get") != 0 || std::strcmp(info.value, "fleet_info") != 0) {
        std::cout << "解析字符" << std::endl;
        std::cout << info.key << std::endl << info.value << std::endl << info.method << std::endl
                  << info.hashKey << std::endl << info.groupId << std::endl;
        std::cout << " ---------222---------  " << std::endl;
    }*/
    // 计算命令中的总元素数（这里是 4：命令名、键、值）
    oss << "*" << 3 << "\r\n";
    // 添加命令（SET、GET 等）
    oss << "$" << std::strlen(info.method) << "\r\n" << info.method << "\r\n";
    // 添加键
    oss << "$" << std::strlen(info.key) << "\r\n" << info.key << "\r\n";
    // 添加值
    oss << "$" << std::strlen(info.value) << "\r\n" << info.value << "\r\n";
    return oss.str();
}



#endif

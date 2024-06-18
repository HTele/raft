#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include <unistd.h>  // 对于 Unix/Linux 系统
// #include <Windows.h>  // 如果是 Windows 系统，请使用这个库来替代 unix 的 unistd.h
#include <sys/socket.h>  // 套接字库, 根据你的系统/环境可能需要不同的头文件
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>


using json = nlohmann::json;
using namespace std;

void send_receive(int sock, const json& message) {
    string serialized_message = message.dump();  // 序列化 JSON 对象为字符串
    send(sock, serialized_message.c_str(), serialized_message.size(), 0);  // 发送 JSON 字符串
    char buffer[1024] = {0};
    recv(sock, buffer, 1024, 0);  // 接收响应
    cout << "Response: " << buffer << endl;  // 打印服务器响应
}

int main() {
    // 获取目标 IP 地址和端口号
    string host;
    int port;
    cout << "请输入目标IP地址：";
    cin >> host;
    cout << "请输入目标端口号：";
    cin >> port;

    // 创建套接字并连接到服务器
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(host.c_str());

    if (connect(sock, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cout << "连接服务器失败" << endl;
        return -1;
    }
    while (true) {
        string command;
        cout << "请输入命令（set/del/get）：";
        cin >> command;
        json message;
        cout << "请输入哈希键（hashKey）：";
        uint64_t hashKey;
        cin >> hashKey;
        cout << "请输入组ID（groupId）：";
        int groupId;
        cin >> groupId;
        message["hashKey"] = hashKey;
        message["groupId"] = groupId;
        if (command == "set") {
            cout << "请输入键：";
            string key;
            cin >> key;
            cout << "请输入值：";
            string value;
            cin >> value;

            message["method"] = "SET";
            message["key"] = key;
            message["value"] = value;
        } else if (command == "del") {
            cout << "请输入要删除的键：";
            string key;
            cin >> key;
            message["method"] = "DEL";
            message["key"] = key;
            message["value"] = "";  // DEL操作时value为空
        } else if (command == "get") {
            cout << "请输入要获取的键：";
            string key;
            cin >> key;
            message["method"] = "GET";
            message["key"] = key;
            message["value"] = "";  // GET操作时value为空
        } else {
            cout << "无效命令，请重新输入" << endl;
            continue;
        }
        send_receive(sock, message);
        sleep(1);  // 等待1秒
    }
    return 0;
}

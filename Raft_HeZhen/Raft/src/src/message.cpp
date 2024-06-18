#include"message.h"
#include <netinet/tcp.h>
#include <fcntl.h>
#include <signal.h>
#include <nlohmann/json.hpp>
#include <cstring> // For strcpy


using json = nlohmann::json;
Message toMessage(MessageType type, void* data, size_t size) {
    Message message;
    message.type = type;
    std::memcpy(message.data, data, size);
    return message;
}

//还原：
// 从 Message 中还原 RequestVote 结构体
RequestVote getRequestVote(const Message& message) {
    RequestVote requestVote;
    std::memcpy(&requestVote, message.data, sizeof(requestVote));
    return requestVote;
}

// 从 Message 中还原 VoteResponse 结构体
VoteResponse getVoteResponse(const Message& message) {
    VoteResponse voteResponse;
    std::memcpy(&voteResponse, message.data, sizeof(voteResponse));
    return voteResponse;
}

// 从 Message 中还原 AppendEntries 结构体
AppendEntries getAppendEntries(const Message& message) {
    AppendEntries appendEntries;
    appendEntries = *reinterpret_cast<const AppendEntries*>(message.data);
    return appendEntries;
}

// 从 Message 中还原 AppendResponse 结构体
AppendResponse getAppendResponse(const Message& message) {
    AppendResponse appendResponse;
    std::memcpy(&appendResponse, message.data, sizeof(appendResponse));
    return appendResponse;
}

// 从 Message 中还原 ClientResponse 结构体
ClientResponse getClientResponse(const Message& message) {
    ClientResponse clientResponse;
    clientResponse = *reinterpret_cast<const ClientResponse*>(message.data);
    return clientResponse;
}

ClientRequest getClientRequest(const Message& message) {
    ClientRequest clientRequest;
    clientRequest = *reinterpret_cast<const ClientRequest*>(message.data);
    return clientRequest;
}

Info getInfo(const Message& message){
    Info json_info;
    json_info=*reinterpret_cast<const Info*>(message.data);
    return json_info;
}


string getString(const Message& message) {
    string result=message.data;
    return result;
}

void handle_for_sigpipe(){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE, &sa, NULL))
        return;
}


int sendMessage(int sockfd,Message message) {
    handle_for_sigpipe(); // 设置SIGPIPE信号处理方式为忽略
    const char* data = reinterpret_cast<const char*>(&message);
    size_t total_sent = 0;
    size_t length = sizeof(message);
    while (total_sent < length) {
        int ret = send(sockfd, data + total_sent, length - total_sent, 0);
        if (ret < 0) {
            if (errno == EPIPE || errno == ECONNRESET) {
                // 对方已经关闭了连接
                return -1;
            } else if (errno == EINTR) {
                // 信号中断，重试发送
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 发送缓冲区已满，可以选择等待一段时间再重试，或立即返回错误
                perror("send - try again later");
                return -2;
            } else {
                // 其他错误
                perror("send");
                return -3;
            }
        } else if (ret == 0) {
            // send 返回 0，表示连接已关闭
            return -4;
        }
        total_sent += ret;
    }
    return 1; // 成功发送所有数据
}


union Data {
    Message message;
    char stringData[sizeof(Message)];
};




bool parseJsonToInfo(const std::string& jsonStr, Info& info) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        
        // Copy values to char arrays
        std::strncpy(info.key, j.at("key").get<std::string>().c_str(), sizeof(info.key) - 1);
        info.key[sizeof(info.key) - 1] = '\0'; // Ensure null-termination
        
        std::strncpy(info.value, j.at("value").get<std::string>().c_str(), sizeof(info.value) - 1);
        info.value[sizeof(info.value) - 1] = '\0'; // Ensure null-termination
        
        std::strncpy(info.method, j.at("method").get<std::string>().c_str(), sizeof(info.method) - 1);
        info.method[sizeof(info.method) - 1] = '\0'; // Ensure null-termination
        
        info.hashKey = j.at("hashKey").get<uint64_t>();
        info.groupId = j.at("groupId").get<int>();
        
        /*if (std::strcmp(info.key, "get") != 0 || std::strcmp(info.value, "fleet_info") != 0) {
            std::cout << "接收json " << jsonStr << std::endl;
            std::cout << info.key << std::endl << info.value << std::endl << info.method << std::endl
                      << info.hashKey << std::endl << info.groupId << std::endl;
            std::cout << " -----------111-----------  " << std::endl;
        }*/
        
        return true;
    } catch (const std::exception& e) {
        // Handle errors (e.g., log them)
        return false;
    }
}

int recvMessage(int sockfd, Message& message) {
    if (sockfd == -1) {
        return -1;
    }

    Data data;
    int total_received = 0;
    int length = static_cast<int>(sizeof(data.stringData));

    while (total_received < length) {
        int bytes = recv(sockfd, reinterpret_cast<char*>(&data) + total_received, length - total_received, 0);
        if (bytes < 0) {
            if (errno == EINTR) {
                // 信号中断，重试接收
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 接收缓冲区空，可以选择等待一段时间再重试，或立即返回错误
                perror("recv - try again later");
                return -2;
            } else {
                // 其他错误
                perror("recv");
                return -3;
            }
        } else if (bytes == 0) {
            // recv 返回 0，表示连接已关闭
            return -4;
        }
        total_received += bytes;

        // 如果接收到的数据长度小于 Message 结构体大小，并且 recv 返回的数据字节数小于预期，认为消息已全部接收
        if (total_received < static_cast<int>(sizeof(Message)) && bytes < length - total_received) {
            break;
        }
    }

    // 判断接收到的数据长度
    if (total_received == static_cast<int>(sizeof(Message))) {
        // 如果接收到的数据长度大于等于 Message 结构体的大小
        message = data.message;
    } else {
        cout<<"receive json"<<endl;
        std::string jsonStr(data.stringData, total_received); // 使用接收到的实际字节数构造字符串
        Info json_info;
        if (parseJsonToInfo(jsonStr, json_info)) {
            // JSON 解析成功
            message=toMessage(info,&json_info,sizeof(json_info));
        }
    }
    memset(data.stringData, 0, sizeof(data.stringData));
    return 1;
}

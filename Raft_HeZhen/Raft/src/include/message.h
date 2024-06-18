#ifndef MESSAGE_H
#define MESSAGE_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;

/*在这个文件中，定义了统一的消息格式message
包括了具体的消息
RequestVote,
VoteResponse,
AppendEntries,
AppendResponse,
ClientResponse

单个message的收发函数：
bool sendMessage(int sockfd, Message message) 
bool recvMessage(int sockfd, Message& message)


转换函数：
message2special：......
special2message：Message toMessage(MessageType type, void* data, size_t size)
*/
// 定义信息类型的枚举值
enum MessageType {
    requestvote,
    voteresponse,
    appendentries,
    appendresponse,
    clientresponse,
    clientrequest,
    info
};



struct Message {
    MessageType type;
    char data[500];
};


struct RequestVote {
    int term;
    int candidate_id;
    int last_log_index;
    int last_log_term;
};//表示请求投票的消息类型，包括当前任期、候选者 ID、候选者的最后一条日志索引和任期等信息



struct VoteResponse {
    int term;
    bool vote_granted;
};//表示投票响应的消息类型，包括当前任期和是否投票给候选者

struct AppendEntries {
    int term;
    int leader_id;
    int prev_log_index;
    int prev_log_term;
    int leader_commit;
    int entries_term[3];
    char entries[3][100];
    bool identify;
    int seq;

};//表示日志追加的消息类型，
//包括当前任期、领导者 ID、前一条日志条目的索引和任期、
//待追加的日志条目以及领导者提交的最后一条日志条目索引

/*leader_commit 是领导者已经提交的最高日志条目的索引。
它的作用是帮助跟随者（follower）判断哪些日志已经被提交了。
领导者会在 AppendEntries 消息中发送自己的 leader_commit 值，
跟随者收到消息后会将自己的 commit_index（已提交的最高日志索引）与 leader_commit 进行比较，
如果跟随者的 commit_index 比 leader_commit 小，则认为跟随者需要提交更多的日志条目，
如果跟随者的 commit_index 比 leader_commit 大，则跟随者可以直接提交对应的日志条目。
这个机制可以确保所有的节点最终都会提交相同的日志序列，从而保证系统的一致性。*/

struct AppendResponse {
    int follower_id;
    int log_index;
    int follower_commit;
    int term;
    bool success;
    bool identify;
    int ack;
};//表示日志追加响应的消息类型，包括当前任期和是否成功追加


struct ClientResponse{
    int client_fd; 
    int node_id;
    char message[100];
};

struct ClientRequest{
    int client_fd;
    int node_id;
    char message[100];
};

struct Info {
    char key[100];
    char value[100];
    char method[100];
    uint64_t hashKey;
    int groupId;
};


// 将特定类型的数据转换为 Message 数据
//RequestVoteData requestVoteData = {1, 2, 3, 4};
//Message requestVoteMessage = toMessage(requestvote, &requestVoteData, sizeof(requestVoteData));
// 将特定类型的数据转换为 Message 数据
Message toMessage(MessageType type, void* data, size_t size);


//还原：
// 从 Message 中还原 RequestVote 结构体
RequestVote getRequestVote(const Message& message);


// 从 Message 中还原 VoteResponse 结构体
VoteResponse getVoteResponse(const Message& message);

// 从 Message 中还原 AppendEntries 结构体
AppendEntries getAppendEntries(const Message& message);

// 从 Message 中还原 AppendResponse 结构体
AppendResponse getAppendResponse(const Message& message) ;


// 从 Message 中还原 ClientResponse 结构体
ClientResponse getClientResponse(const Message& message) ;

ClientRequest getClientRequest(const Message& message) ;


Info getInfo(const Message& message);

string getString(const Message& message) ;

void handle_for_sigpipe();
int sendMessage(int sockfd, Message message);

int recvMessage(int sockfd, Message& message);




#endif

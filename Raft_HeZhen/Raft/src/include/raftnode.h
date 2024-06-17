#ifndef RAFTNODE_H
#define RAFTNODE_H
#include <vector>
#include <mutex>
#include <iostream>
#include <thread>
#include <condition_variable>
#include <cstring>
#include <unordered_map>
#include<string>
#include "confdeal.h"
#include"kvstore.h"
#include"message.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include"threadpool.h"
#include "log.h"
#include <atomic>
#define MAXREQ 256
#define MAX_EVENT 20
#define THREAD_NUM 30

enum NodeState {
    FOLLOWER,
    CANDIDATE,
    LEADER
};//节点的状态，包括 FOLLOWER、CANDIDATE 和 LEADER 三种状态。


class Node {
public:
    Node(string config_path);
    void Run();//节点运行
    ~Node()
    {
        if (accept_thread.joinable()) {
            accept_thread.join();
        }
        for(int i=0;i<num-1;i++)
        {
            close(send_fd[i]);
        }
    }
private:
    void accept_connections();//管理连接，epoll机制
    void work(int fd);//接收信息
    void do_log();//处理日志
    

    void FollowerLoop();//跟随者状态循环，包括等待一段时间并转换为候选者状态
    void CandidateLoop();//候选者状态循环，
    //向其他节点发送 RequestVote 消息并等待投票结果，如果获得多数选票，则成为 Leader
    void LeaderLoop();//领导者状态循环，包括向其他节点发送 AppendEntries 消息并等待响应，如果收到多数节点的确认，则提交日志条目
    void sendmsg(int &fd,struct sockaddr_in addr,Message msg);//发送信息

private:

    //all 通用：
    int id;//节点id
    int num;//整个集群节点数量
    //集群中的leader的id
    atomic<int> leader_id{0};
    int listenfd;//监听套接字
    thread accept_thread;//用于管理连接的线程
    thread deal_log_thread;//用于处理日志的线程
    struct sockaddr_in servaddr;//自己的地址
    vector<int> send_fd;//用于send信息给其他节点的socket
    atomic<NodeState> state{FOLLOWER};//当前状态
    atomic<int> current_term {0};//当前任期
    vector<sockaddr_in> others_addr;//其他节点的地址
    Log log;//该节点维护的日志
    mutex mtx_append;
    kvstore kv;//该节点维护的数据库
    mutex send_mutex_;//发送锁，one by one，不能冲突
    

    //leader专属：
    atomic<int> live{0};//leader沙漏，刚开始成为leader时置为5，如果五个周期收不到任何回应，会丢失leader身份
    mutex mtx_match;
    vector<int> match_index;//match_index_[i]表示leader节点已经复制给节点i的最高日志条目的索引号
    vector<int> match_term;//match_index_[i]表示leader节点已经复制给节点i的最高日志条目的term
    atomic<int> seq{0};
    atomic<int> response_node_num{0};

    //candidate专属：
    int num_votes = 0;//已获得的票数
    atomic<bool> voted{false};//是否已经在该term投票


    //follower专属：
    mutex mtx_del;
    atomic<bool> recv_heartbeat{false};//接收到心跳|同步信息的标志
    atomic<int> leader_commit_index{0};//当前集群中leader提交的条目index，自己不能超过这个
    atomic<int> ack{0};

   
};
#endif







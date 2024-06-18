#include "raftnode.h"
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

#include <nlohmann/json.hpp>

using json = nlohmann::json;
#include"tools.h"

ThreadPool pool(THREAD_NUM);//线程池，用于work(回复收到的message)

std::default_random_engine generator1;//计时器，follower
std::uniform_int_distribution<int> distribution1(3000, 3001);//变化比较小，稳定
int delay1;

std::random_device rd;//真随机，竞争者的周期变化大，可以错开，避免所有node一直同时进入竞争者状态不给别人投票选不出leader
std::default_random_engine generator2(rd());
std::uniform_int_distribution<int> distribution2(500, 1000);
int delay2;

std::default_random_engine generator3;//计时器，leader
std::uniform_int_distribution<int> distribution3(1520, 1521);//变化比较小，稳定
int delay3;


//节点初始化，包括初始自己id和地址，监听接口，监听线程以及其他节点地址
Node::Node(string config_path){
    // 解析配置文件并初始化节点信息
    std::vector<std::vector<std::string>> node_info = parse_config(config_path);//解析配置文件
    if (node_info.empty()) {
        std::cerr << "100:Error: failed to parse config file" << std::endl;
        return;
    }

    //初始化log的日志文件（用于本地查看调试）
    log.file_name_=node_info[0][1]+".txt";
    std::ofstream outfile(log.file_name_, std::ios::trunc);// 以 trunc 模式打开文件，清空文件内容（初始化）
    outfile.close();  // 立即关闭文件

    // 初始化节点（自己）地址
    num = node_info.size();
    id=stoi(node_info[0][1])%10;//取port的尾数为id，这里要求集群的port是连号的

    cout<<id<<endl;
    cout<<id<<"start: follower"<<endl;
    send_fd.resize(num-1,-1);//初始化用于send 信息给其他节点的socket
   
    //初始化监听fd
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(std::stoi(node_info[0][1])); // 将第一个节点的端口号作为监听端口号
     /* Enable address reuse */
    int on = 1;
    // 打开 socket 端口复用, 防止测试的时候出现 Address already in use
    int result = setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
    if (-1 == result) {
        perror ("Set socket");
        return ;
    }
    result = bind(listenfd, (const struct sockaddr *)&servaddr, sizeof (servaddr));
    if (-1 == result) {
        perror("Bind port");
        return ;
    }
    result = listen(listenfd, 15);
    if (-1 == result) {
        perror("Start listen");
        return ;
    }
    accept_thread = std::thread(std::bind(&Node::accept_connections, this));//开一个线程来进行监听，recv
    deal_log_thread=std::thread(std::bind(&Node::do_log, this));//开一个线程处理日志

    // 初始化其他节点信息（地址）
    for (int i = 1; i < num; ++i) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        //cout<<node_info[i][0].c_str()<<" "<<node_info[i][1].c_str()<<endl;
        inet_pton(AF_INET, node_info[i][0].c_str(), &addr.sin_addr);
        addr.sin_port = htons(std::stoi(node_info[i][1]));
        others_addr.push_back(addr);
    }
    //handle_for_sigpipe();// 设置SIGPIPE信号处理方式为忽略，这样当send_fd中的fd失效时，node send不会导致程序崩溃
}




//节点运行
void Node::Run() {
    while (true) {
        if (state == FOLLOWER) {
            FollowerLoop();
        } 
        else if (state== CANDIDATE) {
            CandidateLoop();
        } 
        else if (state== LEADER) {
            LeaderLoop();
        }
    }
}



//follower，周期检验
void Node::FollowerLoop() {
    while (state==FOLLOWER) {
        //cout<<"now:follower"<<endl;
        //倒计时
        delay1 = distribution1(generator1);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay1));
        //检验
        if(recv_heartbeat==false)//如果一个周期结束了还没有收到心跳，那么转换为竞选者
        {
            state=CANDIDATE;
            current_term++;
            leader_id=0;//只有一个节点后，暂时不发送
            //if(current_term==1000000)current_term=0;//未考虑到超范围还

            cout<<id<<" become candidate"<<endl;
            cout<<"---------------"<<endl;
        }
        else{//收到心跳了
            recv_heartbeat=false;//重置心跳标志
        }
    }
}




//candidate，周期操作+检验
void Node::CandidateLoop() {
    while (state==CANDIDATE) {
        //cout<<"now:candidate"<<endl;
        //发起投票
        num_votes=1;//首先投自己一票
        voted=true;//已经投票，不给别人投了
        for(int i=0;i<num-1;i++)//向其他节点发送投票请求
        {
            RequestVote need_vote{current_term,turn_id(id,i),log.latest_index(),log.latest_term()};
            //竞选者当前term，id（转换过的），日志最新条目索引，日志最新条目索引的term
            Message message=toMessage(requestvote,&need_vote,sizeof(need_vote));//转换为通用信息格式
		    sendmsg(send_fd[i],others_addr[i],message);//发送给其他所有节点
        }
        //倒计时
        delay2 = distribution2(generator2);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay2));
        //检验
        if(state==CANDIDATE)//如果还没成为leader，那么变回follower
        {
            state=FOLLOWER;
            cout<<id<<" become follower"<<endl;
            cout<<"---------------"<<endl;
            leader_commit_index=0;//重置，因为后续根据leader的心跳信息可更正
        }
        voted=false;//不是竞选者后，可为其他节点投票
    }
}


//leader，周期操作+检验
void Node::LeaderLoop() {
    while (state==LEADER) {
        //cout<<"now:leader"<<endl;
        //发送心跳信息
        response_node_num=0;
        seq=(seq+1)%10;
        for(int i=0;i<num-1;i++)
        {
            AppendEntries heartbeat;
            heartbeat.seq=(seq)%10;
            heartbeat.leader_id=id;//leader的id，用于follower返回
            heartbeat.term=current_term;//leader的term，用于同步所有节点的term
            heartbeat.leader_commit=log.committed_index();//leader最新提交的目录索引
            mtx_match.lock();
            heartbeat.prev_log_index=match_index[i];//leader最近一条复制到follower（i)的条目的索引
            heartbeat.prev_log_term=log.term_at(match_index[i]);//leader最近一条复制到follower（i)条目的周期
            mtx_match.unlock();
            for(int j=1;j<=3;j++){
                if(match_index[i]+j<=log.latest_index()){//同步的entry
                    strncpy(heartbeat.entries[j-1],log.entry_at(match_index[i]+j).c_str(), sizeof(heartbeat.entries[j-1]));//要复制给follower（i）的条目的index
                    //heartbeat.entries[j][sizeof(heartbeat.entries[j]) - 1] = '\0';
                    heartbeat.entries_term[j-1]=log.term_at(match_index[i]+j);//要复制给follower（i）的条目的term
                }
                else{//如果复制给follower（i）的条目已经是leader的最新条目了，则用F标识（如果三条entry都是F，则是纯粹的心跳信息）
                    heartbeat.entries[j-1][0]='F';
                    heartbeat.entries_term[j-1]=0;
                }
            }
            Message message=toMessage(appendentries,&heartbeat,sizeof(heartbeat));
	    sendmsg(send_fd[i],others_addr[i],message);
           
        }
        //倒计时
        delay3 = distribution3(generator3);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay3));
        //检验
        if(live==0)//如果五次没有follower响应，leader变成follower
        {
            state=FOLLOWER;
            cout<<id<<" not response become follower"<<endl;
            cout<<"---------------"<<endl;
            //leader_commit_index=log.latest_index();
        }
        else{
            live--;//活跃标志倒计（一共5*dealy3）
            //cout<<"heartbeat ------------------------------------------"<<endl;
        }
        
    }
}



//根据收到的信息进行对应的处理
void Node::work(int fd)
{
    Message message_recv{};//收到的message
    //cout<<"size: "<<recv_fd.size()<<endl;
    int ret=recvMessage(fd, message_recv);
    MessageType type=message_recv.type;//节点根据message的type来进行对应的处理
    if(ret!=-1){
        if(type==requestvote){//请求投票类型，candidtae->follwer
            if(state==FOLLOWER)
            {
                if(voted==false&&recv_heartbeat==false)//还没有投票的话,而且当前集群无leader
                {       
                    RequestVote recv_info;
                    recv_info=getRequestVote(message_recv);
                    if(recv_info.term>current_term)current_term=recv_info.term;//更新为candidate的term（更大的）
                    if((recv_info.term>=current_term)&&(recv_info.last_log_index>=log.latest_index())&&(recv_info.last_log_term>=log.latest_term()))
                    {//投票条件：candidate的term大于等于当前term，最新的log更新（或一样），最新log的term更大（或相等）
                        VoteResponse give_vote{current_term,true};
                        Message message=toMessage(voteresponse,&give_vote,sizeof(give_vote));
                        sendmsg(send_fd[recv_info.candidate_id],others_addr[recv_info.candidate_id],message);
                        voted=true;//表示已投票
                    }
                    recv_heartbeat=true;//重置倒计时
                }
            }
        }
        else if(type==voteresponse){//投票类型，follower->candidate
            if(state==CANDIDATE)
            {
                VoteResponse recv_info;
                if(recv_info.term>current_term)current_term=recv_info.term;//更新为leader的term（如果更大）
                recv_info=getVoteResponse(message_recv);
                if(recv_info.vote_granted==true)//这里还没考虑term！
                {
                    num_votes++;
                }
                if(num_votes>num/2){ 
                    cout<<id<<" become a leader"<<endl;
                    cout<<"---------------"<<endl;
                    match_index.resize(num-1,log.latest_index());
                    match_term.resize(num-1,log.term_at(log.latest_index()));
                    live=10;
                    seq=0;
                    state=LEADER;
                }//一旦票数大于一半，就变成leader
            }
        }
        else if(type==appendentries)//心跳（条目数为0）|日志同步信息（更新条目），leader->follower
        {
        	//cout<<"ok"<<endl;
            AppendEntries recv_info;
            recv_info=getAppendEntries(message_recv);
            ack=recv_info.seq;
            if(state==FOLLOWER)
            {
                AppendResponse res;          
                res.follower_id=id;//告诉leader自己的id
                //cout<<"get heartbeat from leader:"<<recv_info.leader_id<<endl;
                recv_heartbeat=true;//设置心跳标志
                if(recv_info.term>current_term)current_term=recv_info.term;//如果比leader的term小，那么更新为leader的term
                res.term=current_term;//返回自己的term（可能比leader的大）
                res.follower_commit=log.committed_index();
                if(recv_info.leader_commit>=log.committed_index())
                    leader_id=recv_info.leader_id;//更新leader的id
                leader_commit_index=recv_info.leader_commit;//设置leader的最后一条提交的日志（赞同数超过一半）索引（follower提交不能超过这个数）
                if(((log.latest_index()==recv_info.prev_log_index)&&(log.term_at(log.latest_index())==recv_info.prev_log_term)))//||(recv_info.prev_log_index==0)
                {//follower节点的最新条目的索引和term与leader发过来的验证信息匹配，或者leader发现follower日志为空（或者全都不匹配）
                    int i=0;
                    //cout<<"follower "<<id<<" 响应leader "<<leader_id<<"同步请求: "<<endl;
                    //cout<<"ack:("<<ack<<") id:("<<id<<") 最新的条目索引 ("<<log.latest_index()<<") leader确认索引 ("<<recv_info.prev_log_index<<")"<<endl;
                    //cout<<"---------------------------------------------------------------------"<<endl;
                    //cout<<recv_info.prev_log_index<<"+ "<<i+1<<": "<<recv_info.entries[i]<<endl;
                    while((i<3)&&(recv_info.entries[i][0]!='F'))//根据leader发送的条目进行更新(有效条目)
                    {
                        string str(recv_info.entries[i]);
                        mtx_append.lock();
                        log.append(str,recv_info.entries_term[i]);//把leader的条目索引和周期加入自己的日志
                        mtx_append.unlock();
                        i++;
                    }
                    //res.log_index=recv_info.prev_log_index+i;//follower成功复制到自己的日志的条目的索引
                    res.log_index=log.latest_index();
                    res.success=true;//同步成功     
                }
                else//如果未匹配
                {
                    if(log.latest_index()>recv_info.prev_log_index)//follwer的index比leader确认的大，（leader还没收到response)
                    {
                        if(recv_info.leader_commit>log.committed_index()){
                            //该node的最新条目数没有leader多，同步，删除自己不匹配的那些条目
                            //如果自己的日志比leader的多，那么删了多的，一般用在leader转换成follower后收到新的leader的同步信息
                            cout<<id<<": 不匹配，删掉了 "<<recv_info.prev_log_index+1<<" 到 "<<log.latest_index()<<"的日志"<<endl;
                            log.erase(recv_info.prev_log_index+1,log.latest_index());
                        }
                        res.log_index=-1;
                    }
                    else if(log.latest_index()<recv_info.prev_log_index){//follwer的index比leader确认的小，（follower需要leader回滚)
                        res.log_index=recv_info.prev_log_index-1;//回滚？
                        cout<<id<<": 请求leader回滚,因为："<<endl;
                        cout<<"我的最新的条目索引 "<<log.latest_index()<<"leader确认索引 "<<recv_info.prev_log_index<<endl;
                        cout<<"我的最新的条目周期 "<<log.term_at(log.latest_index())<<"leader确认周期 "<<recv_info.prev_log_term<<endl;
                        cout<<"不相等"<<endl;
                    }  
                    res.success=false;//同步失败
                } 
                res.ack=recv_info.seq;    
                Message msg=toMessage(appendresponse,&res,sizeof(res));
                sendmsg(send_fd[fd_id(id,leader_id)],others_addr[fd_id(id,leader_id)],msg);
            }
            else if(state==LEADER)//1,如果一个leader长时间未发送心跳给follower，那么其他节点会选出新的leader，这种情况下会收到心跳信息
            {
                if(recv_info.leader_commit>log.committed_index())//选择提交数目更多的
                {//自己已经不是最新的 Leader，会立即放弃自己的 Leader 身份，并更新自己的任期为接收到的 AppendEntries 消息中的任期
                    state=FOLLOWER;
                    current_term=recv_info.term;
                    cout<<id<<"become a follower"<<endl;
                    cout<<"---------------"<<endl;
                    leader_commit_index=0;
                }
            }
        }
        else if(type==appendresponse)//回应心跳|同步信息 follower->leader
        {
            if(state==LEADER)
            {
                AppendResponse recv_info;
                recv_info=getAppendResponse(message_recv);
                //cout<<recv_info.ack<<" "<<seq<<endl;
                //cout<<"get response from follower:"<<recv_info.follower_id<<endl;
                if(recv_info.ack!=seq)return;
                response_node_num++;
                if(recv_info.term>current_term)current_term=recv_info.term;//如果follower的term比leader大。那么更新为大的term
                if(recv_info.success==true)
                {
                    for(int i=match_index[fd_id(id,recv_info.follower_id)]+1;i<=recv_info.log_index;i++)//刚刚成功复制给follower的条目
                    {
                        log.add_num(i-1,recv_info.follower_id);
                        //cout<<"条目"<<i<<"获得了"<<recv_info.follower_id<<"的响应"<<endl;
                    }  
                }    

                if(recv_info.follower_commit>log.committed_index()){
                    state=FOLLOWER;
                    cout<<id<<"不是提交条目数最多的leader"<<endl;
                    cout<<id<<"become a follower"<<endl;
                    cout<<"---------------"<<endl;
                    leader_commit_index=0;
                    return ;
                }
                mtx_match.lock();
                if(recv_info.log_index!=-1)//代表follwer把多余的删了，此时match不用变，等待回滚匹配，以免造成死循环 0  3 2 1|回滚| 3 0 3 2 1|回滚|  3 0 ...
                {  
                    match_index[fd_id(id,recv_info.follower_id)]=recv_info.log_index;//匹配follower最新更新到的条目索引
                    if(match_index[fd_id(id,recv_info.follower_id)]!=0)//如果follower返回的index不为0，则更新相应node的match index为返回值
                        match_term[fd_id(id,recv_info.follower_id)]=log.term_at(match_index[fd_id(id,recv_info.follower_id)]);//match index相应的term
                    else{
                        match_term[fd_id(id,recv_info.follower_id)]=0;//index为0，说明follower中没有匹配的或者为空，此时baterm设置为0来匹配follower
                    }     
                }
                mtx_match.unlock();
                if(response_node_num>=num/2)live=5;//翻转leader沙漏
            }
        }
        else if(type==info){//客户端client的请求，client->leader（直接回应）|follower（中转）|candidate(fail)
            cout<<message_recv.data<<endl;
            Info json_info=getInfo(message_recv);
            
            string s=infoToRedisProtocol(json_info);
            //cout<<id<<"收到"<<s<<endl;

            if(state==CANDIDATE){//竞争者状态返回error
                json response = {
                    {"code", -1},
                    {"value", "error1"}
                };
                // 将JSON对象转为字符串并添加换行符
                string msg = response.dump() + "\r\n";
                // 发送消息
                int ret = send(fd, msg.c_str(), msg.size(), 0);
            }
            if(state==FOLLOWER)//client->follower
            {
                std::vector<std::string> result = parse_string(s);
                if (result[0] == "GET") {
                    cout << "get a get ask(leader)" << endl;
                    string value = kv.get(result[1]);
                    if(result[1]=="fleet_info"){
                        json nodes = json::array();
                        int fleet_leader_id=leader_id;
                        nodes.push_back(json({{"id", json::array({1})}, {"ip", "115.157.197.178:8001"}}));
                        nodes.push_back(json({{"id", json::array({2})}, {"ip", "115.157.197.178:8002"}}));
                        nodes.push_back(json({{"id", json::array({3})}, {"ip", "115.157.197.178:8003"}}));

                        json groups = json::array();
                        groups.push_back(json({{"id", 1}, {"leader",fleet_leader_id}, {"nodes", json::array({1, 2, 3})}}));

                        json originalJson = {
                            {"nodes", nodes},
                            {"fleetLeader", fleet_leader_id},
                            {"groups", groups}
                        };

                        // 创建包含编码后 JSON 字符串的 JSON 对象
                        json responseJson = {
                            {"code", 1},
                            {"value", originalJson.dump()}
                        };
                        string msg;
                        msg = responseJson.dump() + "\r\n";
                        int ret = send(fd, msg.c_str(), msg.size(), 0);
                        if(ret!=-1)cout<<id<<" 成功响应客户端fleet_info请求 (follower)"<<": "<<msg<<endl;
                    }
                }
            }
            else if(state==LEADER){//client->leader

            //leader_work2:
                //cout<<"get a client ask(leader)"<<endl;
                while(log.committed_index()<log.latest_index()){
                
                }
                mtx_append.lock();
                log.append(s,current_term);//提交日志
                mtx_append.unlock();
                std::vector<std::string> result = parse_string(s);
                //cout<<result[0]<<endl;
                string msg;
                if (result[0] == "GET") {
                    cout << "get a get ask(leader)" << endl;
                    string value = kv.get(result[1]);
                    if(result[1]=="fleet_info"){
                        json nodes = json::array();
                        int fleet_leader_id=id;
                        nodes.push_back(json({{"id", json::array({1})}, {"ip", "115.157.197.178:8001"}}));
                        nodes.push_back(json({{"id", json::array({2})}, {"ip", "115.157.197.178:8002"}}));
                        nodes.push_back(json({{"id", json::array({3})}, {"ip", "115.157.197.178:8003"}}));

                        json groups = json::array();
                        groups.push_back(json({{"id", 1}, {"leader",fleet_leader_id}, {"nodes", json::array({1, 2, 3})}}));

                        json originalJson = {
                            {"nodes", nodes},
                            {"fleetLeader", fleet_leader_id},
                            {"groups", groups}
                        };

                        // 创建包含编码后 JSON 字符串的 JSON 对象
                        json responseJson = {
                            {"code", 1},
                            {"value", originalJson.dump()}
                        };
                        msg = responseJson.dump() + "\r\n";
                    }
                    else if (value == "") {
                        msg = json({
                            {"code", 0},
                            {"value", "nil"}
                        }).dump() + "\r\n";
                    } else {
                        msg = json({
                            {"code", 1},
                            {"value", value}
                        }).dump() + "\r\n";
                    }
                } else if (result[0] == "DEL") {
                    int count = 0;
                    for (int k = 1; k <= static_cast<int>(result.size()) - 1; k++) {
                        if (kv.get(result[k]) != "")
                            count++;
                    }
                    cout << "get a del ask(leader)" << endl;
                    msg = json({
                        {"code", count}
                    }).dump() + "\r\n";
                } else if (result[0] == "SET") {
                    cout << "get a set ask(leader)" << endl;
                    msg = json({
                        {"code", 1},
                        {"value", ""}
                    }).dump() + "\r\n";
                }
                while(log.committed_index()<log.latest_index()){
                    //请求被提交了才返回信息给客户端
                }
                int ret = send(fd, msg.c_str(), msg.size(), 0);
                if(ret!=-1)cout<<id<<" 成功响应客户端(leader)"<<": "<<msg<<endl;
            }
        }
        else{
        	cout<<message_recv.type<<endl;
        }
    }
}



//单独一个线程来处理日志上面的内容
void Node::do_log()
{
    while(1)
    {
        mtx_append.lock();
        int start=log.committed_index()+1;
        mtx_append.unlock();
        if(start>log.latest_index())//条件一：日志中有未提交的条目
        {
            continue;//不提交（跳过）
        }
        if(state==FOLLOWER){//条件二：当node是follower时，提交的日志不能比leader提交的日志条目的index高
            if(start>leader_commit_index){
                continue;
            }
            else{
                //cout<<id<<" 提交第"<<start<<"条目(follower)"<<endl;
            }
        }
        if(state==LEADER){//条件三：当node是leader时，可以提交的条目的需要满足已经成功复制给超过半数节点
            if(log.get_num(start)<(num-1)/2)
            {
                continue;
            }
            else{
                //cout<<id<<" 提交第"<<start<<"条目(leader)"<<endl;
            }
        }
        cout<<"node("<<id<<")开始处理log("<<start<<")"<<endl;
        cout<<log.entry_at(start)<<endl;
        std::vector<std::string> result = parse_string(log.entry_at(start));
        if(result[0]=="GET")
        {
            //kv.get(result[1]);
            log.commit(start);
        }
        else if(result[0]=="SET")
        {
            string value=result[2];
            for(int i=3;i<static_cast<int>(result.size());i++)
            {
                value+=" "+result[i];
            }
            kv.set(result[1],value);
            log.commit(start);
        }
        else if(result[0]=="DEL"){
            for(int i=1;i<static_cast<int>(result.size());i++)
            {
                kv.del(result[i]);
            }
            log.commit(start);
        }
        start++;
    
    }
}




//管理连接，监听收到的消息
void Node::accept_connections() {
    // epoll 实例 file describe
    int epfd = 0;
    int result = 0;
    struct epoll_event ev, event[MAX_EVENT];
    // 创建epoll实例
    epfd = epoll_create1(0);
    if (1 == epfd) {
        perror("Create epoll instance");
        return ;
    }
    ev.data.fd = listenfd;
    ev.events = EPOLLIN;
    // 设置epoll的事件
    result = epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    if(-1 == result) {
        perror("Set epoll_ctl");
        return ;
    }
    while (1) {
        int wait_count;
        // 等待事件
        wait_count = epoll_wait(epfd, event, MAX_EVENT, -1);
        for (int i = 0 ; i < wait_count; i++) {
            uint32_t events = event[i].events;
            int __result;
            if (events & EPOLLERR) {
                // 发生错误
                printf("Epoll error on fd %d\n", event[i].data.fd);
                close(event[i].data.fd);
                continue;
            } else if (events & EPOLLHUP) {
                // 挂起，可能是对端关闭连接
                printf("Epoll hang-up on fd %d\n", event[i].data.fd);
                close(event[i].data.fd);
                continue;
            } else if (listenfd == event[i].data.fd) {
                // listen的 file describe 事件触发， accpet事件
                struct sockaddr_in client_addr;
                socklen_t addr_size = sizeof(client_addr);
                int accp_fd = accept(listenfd, (struct sockaddr *)&client_addr, &addr_size);
                if (-1 == accp_fd) {
                    perror("Accept");
                    continue;
                }
                else{
                    //cout<<"find a node!"<<endl;
                }
                //recv_fd.push_back(accp_fd);
                ev.data.fd = accp_fd;
                ev.events = EPOLLIN | EPOLLET;
                // 为新accept的 file describe 设置epoll事件
                __result = epoll_ctl(epfd, EPOLL_CTL_ADD, accp_fd, &ev);
                if (-1 == __result) {
                    perror("epoll_ctl");
                    return ;
                }
            } else {//收到消息，使用work来对应处理收到的信息
                //<<"recv info---"<<endl;
                int fd=event[i].data.fd;
                pool.enqueue([this,fd] {
                    work(fd);
                });
            }
        }
    }
}

std::string messageTypeToString(int type) {
    switch (type) {
        case requestvote:     return "requestvote";
        case voteresponse:    return "voteresponse";
        case appendentries:   return "appendentries";
        case appendresponse:  return "appendresponse";
        case clientresponse:  return "clientresponse";
        case clientrequest:   return "clientrequest";
        case info:            return "info";
        default:              return "unknown";
    }
}


//用于节点来send信息的函数（包括创建连接,重连,这样可以通过发送信息（心跳，投票等)来快速和新加入的节点连接）
//当连接断开后，也不会中断（sendMessage(fd,msg)防止中断）
void Node::sendmsg(int &fd,struct sockaddr_in addr,Message msg){
    //cout<<"send mseeage via: fd("<<fd<<") to port("<<ntohs(addr.sin_port)<<")"<<endl;
    std::lock_guard<std::mutex> lock(send_mutex_);  // 加锁
    int ret = -1;
    if (fd != -1) {
        ret = sendMessage(fd,msg);
    }
    //std::cout << "ret: " << ret << " fd: " << fd << std::endl;
    if ((fd == -1) || (ret < 0)) { // 未建立连接或者发送消息失败
        // 关闭旧的套接字
        if(ret < 0) close(fd);
        // 创建新的套接字
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            //perror("socket");
            fd = -1;
            return;
        }
        // 连接服务器
        ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
        if (ret < 0) { // 重新连接失败
            //perror("connect");
            close(fd);
            fd = -1;
            //std::cout << "type("<<messageTypeToString(msg.type)<<")reconnect failed" << std::endl;
        } else { // 重新连接成功
            //std::cout << "连接node成功" << std::endl;
            // 发送消息
            ret = sendMessage(fd,msg);
            if (ret < 0) { // 发送消息失败
            	//std::cout << "type("<<messageTypeToString(msg.type)<<") fail reason("<<ret<<") send message failed" << std::endl;
            }
        }
    }
    else{
    	//cout<<"type("<<messageTypeToString(msg.type)<<") send success"<<endl;
    }
    //cout<<"--------------------------------"<<endl;
}

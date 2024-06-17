#include"confdeal.h"

std::vector<std::vector<std::string>> parse_config(const std::string& filename)
{
    std::ifstream infile(filename);
    std::string line;
    std::vector<std::vector<std::string>> node_info;

    while (std::getline(infile, line)) {
        if (line.find("follower_info") != std::string::npos) {
            // 解析节点信息
            size_t pos = line.find(" ");
            std::string info = line.substr(pos + 1);
            std::string ip = info.substr(0, info.find(":"));
            std::string port = info.substr(info.find(":") + 1);
            node_info.push_back({ip, port}); // 将节点信息插入向量
        }
    }

    return node_info;
}

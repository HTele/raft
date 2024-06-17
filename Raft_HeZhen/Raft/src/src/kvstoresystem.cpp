#include"raftnode.h"
int main(int argc, char **argv){ 
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " --config_path <config_file_path>" << std::endl;
        return 1;
    }
    std::string config_path=argv[2];
    Node new_node(config_path);
    new_node.Run();
    return 0;
}
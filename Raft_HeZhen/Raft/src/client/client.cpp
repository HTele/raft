#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <random>
using namespace std;

void send_receive(int sock, string message) {
    send(sock, message.c_str(), message.size(), 0);
    char buffer[1024] = {0};
    recv(sock, buffer, 1024, 0);
    cout << buffer << endl;
}

string generate_random_string(size_t length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    string result;
    result.resize(length);

    for (size_t i = 0; i < length; ++i) {
        result[i] = charset[rand() % (sizeof(charset) - 1)];
    }

    return result;
}

void client_thread(string host, int port, int id) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Socket creation failed for thread " << id << endl;
        return;
    }

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(host.c_str());

    if (connect(sock, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        cerr << "Connection failed for thread " << id << endl;
        close(sock);
        return;
    }

    // Setup for better random number generation
    random_device rd;  // Obtain a random number from hardware
    mt19937 eng(rd()); // Seed the generator
    uniform_int_distribution<> distr(0, 2); // Define the range

    vector<string> commands = {"set", "del", "get"};

    for (int i = 0; i < 10; ++i) {
        int index = distr(eng); // Generate a random index
        const string& command = commands[index];

        if (command == "set") {
            string key = generate_random_string(8);
            string value1 = generate_random_string(5);
            string value2 = generate_random_string(9);

            string message = "*4\r\n$3\r\nSET\r\n$" + to_string(key.size()) + "\r\n" + key + "\r\n$" +
                             to_string(value1.size()) + "\r\n" + value1 + "\r\n$" + 
                             to_string(value2.size()) + "\r\n" + value2 + "\r\n";
            send_receive(sock, message);
        } else if (command == "del") {
            string key1 = generate_random_string(8);
            string key2 = generate_random_string(5);
            string message = "*3\r\n$3\r\nDEL\r\n$" + to_string(key1.size()) + "\r\n" + key1 + "\r\n$" +
                             to_string(key2.size()) + "\r\n" + key2 + "\r\n";
            send_receive(sock, message);
        } else if (command == "get") {
            string key = generate_random_string(8);
            string message = "*2\r\n$3\r\nGET\r\n$" + to_string(key.size()) + "\r\n" + key + "\r\n";
            send_receive(sock, message);
        }
    }


    close(sock);
}

int main() {
    srand(time(0)); // Seed the random number generator

    string host;
    int port, thread_count;
    cout << "请输入目标IP地址：";
    cin >> host;
    cout << "请输入目标端口号：";
    cin >> port;
    cout << "请输入并发线程数量：";
    cin >> thread_count;

    vector<thread> threads;
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(client_thread, host, port, i);
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    return 0;
}

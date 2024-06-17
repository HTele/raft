#ifndef KVSTORE_H
#define KVSTORE_H
#include <iostream>
#include <map>
#include <string>

class kvstore {
public:
    void set(const std::string& key, const std::string& value) {
        data_[key] = value;
    }
    std::string get(const std::string& key) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            return it->second;
        } else {
            return "";
        }
    }
    void del(const std::string& key) {
        data_.erase(key);
    }
private:
    std::map<std::string, std::string> data_;
};
/*
    用法：
    kvstore store;
    store.set("name", "Alice");
    store.set("age", "30");
    std::cout << "name: " << store.get("name") << std::endl;
    std::cout << "age: " << store.get("age") << std::endl;
    store.del("age");
    std::cout << "age: " << store.get("age") << std::endl;
    return 0;
*/
#endif
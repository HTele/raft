#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
using namespace std;
class ThreadPool {
public:
    ThreadPool();//默认构造函数，将线程数设置为系统支持的最大线程数
    explicit ThreadPool(std::size_t num_threads);//带参数构造函数，将线程数设置为指定数量

    template <typename F>
    void enqueue(F&& task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(task));
        }
        condition.notify_one();
    }

    ~ThreadPool();//析构函数，停止所有线程并等待它们退出

private:
    std::vector<std::thread> threads;//线程池中的线程对象，使用std::vector容器保存
    std::queue<std::function<void()>> tasks;//线程池中的任务队列，使用std::queue容器保存std::function<void()>类型的可调用对象

    std::mutex queue_mutex;//保护任务队列的互斥锁
    std::condition_variable condition;//等待可执行任务时使用的条件变量。

    bool stop;//标志线程池是否已停止
};

#endif // THREAD_POOL_H
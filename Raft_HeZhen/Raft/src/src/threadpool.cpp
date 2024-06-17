#include "threadpool.h"

ThreadPool::ThreadPool() : ThreadPool(std::thread::hardware_concurrency()) {}
/*线程池的默认构造函数，将线程数设置为系统支持的最大线程数（通过std::thread::hardware_concurrency()获取）。
在初始化列表中调用其他构造函数进行初始化*/

ThreadPool::ThreadPool(std::size_t num_threads) : stop(false) {//线程池的带参数构造函数，将线程数设置为指定数量。同时，初始化stop标志为false
    for (std::size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                    if (this->stop && this->tasks.empty())
                        return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}
/*使用循环创建指定数量的线程。对于每个线程，先创建一个std::function<void()>类型的变量task，用于保存待执行的任务。
然后，使用unique_lockstd::mutex保护任务队列，使用condition_variable等待任务队列中有可执行的任务。如果线程池已经被停止，且任务队列为空，则退出循环。
否则，从任务队列中取出一个任务，将其移动到task变量中，并从队列中删除该任务。最后，执行task中保存的任务函数*/


/*将任务添加到任务队列中。使用unique_lockstd::mutex保护任务队列，
将任务移动到队列中，并使用condition_variable通知等待的线程有可执行的任务。*/

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread& thread : threads)
        thread.join();
}
/*线程池的析构函数，停止所有线程并等待它们退出。使用unique_lockstd::mutex保护stop标志，将其设置为true，表示停止线程池。
然后，使用condition_variable通知所有等待的线程有停止信号。最后，使用join()方法等待所有线程退出。*/
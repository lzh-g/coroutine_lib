#ifndef _THREAD_H_
#define _THREAD_H_

#include <mutex>
#include <condition_variable>
#include <functional>

namespace sylar
{
    // 用于线程方法间的同步
    class Semaphore
    {
    public:
        // 信号量初始化为0
        explicit Semaphore(int count = 0) : m_count(count) {}

        // P操作
        void wait()
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            while (m_count == 0)
            {
                // wait for signals
                m_cv.wait(lock); // 这里会自动解锁
            }
            --m_count;
        }

        // V操作
        void signal()
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            ++m_count;
            m_cv.notify_one(); // signal
            // 这里的one可能是向多个线程发起信号，但最终只会有一个线程获取资源
        }

    private:
        std::mutex m_mutex;
        std::condition_variable m_cv;
        int m_count;
    };

    // 共两种线程：1、由系统自动创建的主线程；2、由Thread类创建的线程
    class Thread
    {
    public:
        Thread(std::function<void()> cb, const std::string &name);
        ~Thread();

        pid_t getId() const
        {
            return m_id;
        }

        const std::string &getName() const
        {
            return m_name;
        }

        void join();

    public:
        // 获取系统分配的线程id
        static pid_t GetThreadId();

        // 获取当前所在线程
        static Thread *GetThis();

        // 获取当前线程名字
        static const std::string &GetName();

        // 设置当前线程的名字
        static void SetName(const std::string &name);

    private:
        // 线程函数
        static void *run(void *arg);

    private:
        pid_t m_id = -1;
        pthread_t m_thread = 0;

        // 线程需要运行的函数
        std::function<void()> m_cb;
        std::string m_name;

        Semaphore m_semaphore;
    };

} // namespace sylar

#endif // !_THREAD_H_
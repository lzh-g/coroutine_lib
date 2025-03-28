#include "thread.h"

#include <sys/syscall.h>
#include <iostream>
#include <unistd.h>

namespace sylar
{
    // 线程信息
    static thread_local Thread *t_thread = nullptr;            // 当前线程的Thread对象指针
    static thread_local std::string t_thread_name = "UNKNOWN"; // 当前线程的名称

    Thread::Thread(std::function<void()> cb, const std::string &name) : m_cb(cb), m_name(name)
    {
        int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
        if (rt)
        {
            std::cerr << "pthread_create thread fail, rt = " << rt << " name = " << name;
            throw std::logic_error("pthread_create error");
        }
        // 等待线程函数完成初始化
        m_semaphore.wait();
    }

    Thread::~Thread()
    {
        if (m_thread)
        {
            pthread_detach(m_thread);
            m_thread = 0;
        }
    }

    void Thread::join()
    {
        if (m_thread)
        {
            int rt = pthread_join(m_thread, nullptr);
            if (rt)
            {
                std::cerr << "pthread_join failed, rt = " << rt << ", name = " << m_name << std::endl;
                throw std::logic_error("pthread_join error");
            }
            m_thread = 0;
        }
    }

    pid_t Thread::GetThreadId()
    {
        // 系统调用，用于获取当前线程的唯一id，SYS_gettid是linux特定的系统调用编号，用来获取线程ID(tid)。
        // pid_t是一个数据类型，用于表示进程id或线程id
        return syscall(SYS_gettid);
    }

    Thread *Thread::GetThis()
    {
        return t_thread;
    }

    const std::string &Thread::GetName()
    {
        return t_thread_name;
    }

    void Thread::SetName(const std::string &name)
    {
        if (t_thread)
        {
            t_thread->m_name = name;
        }
        t_thread_name = name;
    }

    void *Thread::run(void *arg)
    {
        Thread *thread = (Thread *)arg;

        t_thread = thread;
        t_thread_name = thread->m_name;
        thread->m_id = GetThreadId();
        // pthread_self()获取当前线程id，设置m_name的前15个字节
        // 目的就是设置线程的名称方便调试
        // 为什么是(0, 15)，因为linux中线程的最大名字之恩那个是15，最后有个\0共16
        pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

        std::function<void()> cb;
        cb.swap(thread->m_cb); // swap -> 可以减少m_cb中智能指针的引用计数

        // 初始化完成，确保主线程创建出来一个工作线程，供协程使用，否则可能出现协程在未初始化的线程上使用
        thread->m_semaphore.signal();

        cb(); // 真正执行函数的地方
        return 0;
    }

} // namespace sylar

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "../fiber/fiber.h"
#include "../thread/thread.h"

#include <mutex>
#include <vector>

namespace sylar
{
    class Scheduler
    {
    public:
        // threads指定线程数量，use_caller指定是否将主线程作为工作线程，name是调度器的名称
        Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name = "Scheduler");

        // 防止出现资源泄露，基类指针删除派生类对象时不完全销毁的问题
        virtual ~Scheduler();

        // 获取调度器名字
        const std::string &getName() const { return m_name; }

    public:
        // 获取正在运行的调度器
        static Scheduler *GetThis();

    protected:
        // 设置正在运行的调度器
        void SetThis();

    public:
        // 添加任务到任务队列
        template <class FiberOrCb>
        void scheduleLock(FiberOrCb fc, int thread = -1)
        {
            bool need_tickle;

            std::lock_guard<std::mutex> lock(m_mutex);
            // empty -> all thread is idle -> need to be waken up
            need_tickle = m_tasks.empty();

            // 创建Task的任务对象
            ScheduleTask task(fc, thread);
            if (task.fiber || task.cb)
            {
                m_tasks.push_back(task);
            }

            // 如果检查出了队列为空，就唤醒线程
            if (need_tickle)
            {
                tickle();
            }
        }

        // 启动线程池
        virtual void start();

        // 关闭线程池
        virtual void stop();

    protected:
        // 唤醒线程
        virtual void tickle();

        // 线程函数
        virtual void run();

        // 空闲协程函数，无任务调度时执行idle
        virtual void idle();

        // 是否可以关闭
        virtual bool stopping();

        // 返回是否有空闲线程
        // 当调度协程进入idle时空闲线程数+1，从idle协程返回时空闲线程数-1
        bool hasIdleThreads() { return m_idleThreadCount > 0; }

    private:
        // 任务
        struct ScheduleTask
        {
            std::shared_ptr<Fiber> fiber;
            std::function<void()> cb;
            int thread; // 指定任务需要运行的线程

            ScheduleTask()
            {
                fiber = nullptr;
                cb = nullptr;
                thread = -1;
            }

            ScheduleTask(std::shared_ptr<Fiber> f, int thr)
            {
                fiber = f;
                thread = thr;
            }

            ScheduleTask(std::shared_ptr<Fiber> *f, int thr)
            {
                // 将内容转移，即指针内部的转移和上面的赋值不通，引用计数不会增加
                fiber.swap(*f);
                thread = thr;
            }

            ScheduleTask(std::function<void()> f, int thr)
            {
                cb = f;
                thread = thr;
            }

            ScheduleTask(std::function<void()> *f, int thr)
            {
                cb.swap(*f);
                thread = thr;
            }

            // 重置
            void reset()
            {
                fiber = nullptr;
                cb = nullptr;
                thread = -1;
            }
        };

    private:
        // 调度器名称
        std::string m_name;

        // 互斥锁 -> 保护任务队列
        std::mutex m_mutex;

        // 线程池
        std::vector<std::shared_ptr<Thread>> m_threads;

        // 任务队列
        std::vector<ScheduleTask> m_tasks;

        // 存储工作线程的线程id
        std::vector<int> m_threadIds;

        // 需要额外创建的线程数
        size_t m_threadCount = 0;

        // 活跃线程数
        std::atomic<size_t> m_activeThreadCount = {0};

        // 空闲线程数
        std::atomic<size_t> m_idleThreadCount = {0};

        // 主线程是否用作工作线程
        bool m_useCaller;

        // 如果是 -> 需要额外创建调度协程
        std::shared_ptr<Fiber> m_schedulerFiber;

        // 如果是 -> 记录主线程的线程id
        int m_rootThread = -1;

        // 是否正在关闭
        bool m_stopping = false;
    };

} // namespace sylar

#endif //!_SCHEDULER_H_
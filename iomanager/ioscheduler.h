#ifndef _SYLAR_IOMANAGER_H_
#define _SYLAR_IOMANAGER_H_

#include "scheduler.h"
#include "timer.h"
#include <shared_mutex>

namespace sylar
{
    // work flow
    // 1.register one event -> 2.wait for it to ready -> 3.schedule the callback -> 4.unregister the event -> 5.run the callback
    class IOManager : public Scheduler, public TimerManager
    {
    public:
        enum Event
        {
            NONE = 0x0, // 没有事件
            // READ == EPOLLIN
            READ = 0x1, // 读事件
            // WRITE == EPOLLOUT
            WRITE = 0x4 // 写事件
        };

    public:
        IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "IOManager");
        ~IOManager();

        /**
         * @brief 事件管理方法
         */
        // add one event at a time
        int addEvent(int fd, Event event, std::function<void()> cb = nullptr); // 添加一个事件到文件描述符fd上，并关联一个回调函数
        // delete event
        bool delEvent(int fd, Event event); // 删除文件描述符fd上的某个事件
        // delete the event and trigger its callback
        bool cancelEvent(int fd, Event event); // 取消文件描述符上的某个事件，并触发其回调函数
        // delete all events and trigger its callback
        bool cancelAll(int fd); // 取消文件描述符fd上的所有事件，并触发所有回调函数

        // 获得当前线程的调度器对象
        static IOManager *GetThis();

    protected:
        // 通知调度器调度任务
        void tickle() override;
        // 判断调度器是否可以停止
        bool stopping() override;
        // 重写scheduler的idle，收集所有已触发的fd回调函数并将其加入调度器的任务队列，真正的执行实际是idle协程退出后，调度器在下一轮调度时执行
        void idle() override;
        // 因为Timer类的成员函数重写当有新的定时器插入到前面时的处理逻辑
        void onTimerInsertedAtFront() override;
        // 调整文件描述符上下文数组的大小
        void contextResize(size_t size);

    private:
        /**
         * @brief 用于描述一个文件描述符的事件上下文，每个socket fd都对应一个FdContext，包括df的值、事件以及读写上下文
         */
        struct FdContext
        {
            // 描述一个具体事件的上下文，如读/写事件
            struct EventContext
            {
                // scheduler
                Scheduler *scheduler = nullptr; // 关联的调度器
                // callback fiber
                std::shared_ptr<Fiber> fiber; // 关联的回调线程（协程）
                // callback function
                std::function<void()> cb; // 关联的回调函数（注册为协程对象）
            };

            // read event context
            EventContext read; // 读事件上下文
            // write event context
            EventContext write; // 写事件上下文
            int fd = 0;         // 事件关联的fd
            // events registed
            Event events = NONE; // 注册的事件
            std::mutex mutex;

            // 根据事件类型获取上下文
            EventContext &getEventContext(Event event);
            // 重置事件上下文
            void resetEventContext(EventContext &ctx);
            // 触发事件，根据实现类型调用对应的EventContext中的调度器去调度协程/函数
            void triggerEvent(Event event);
        };

    private:
        int m_epfd = 0; // epoll文件描述符
        // fd[0] read, fd[1] write
        int m_tickleFds[2];                            // 用于线程间通信的管道文件描述符，fd[0]是读端，fd[1]是写端
        std::atomic<size_t> m_pendingEventCount = {0}; // 原子计数器
        std::shared_mutex m_mutex;                     // 读写锁
        // store fdcontexts for each fd
        std::vector<FdContext *> m_fdContexts; // 文件描述符上下文数组，存储每个文件描述符的FdContext
    };
} // namespace sylar

#endif // !_SYLAR_IOMANAGER_H_
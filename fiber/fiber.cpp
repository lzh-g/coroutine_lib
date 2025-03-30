#include "fiber.h"

static bool debug = false;

namespace sylar
{
    // 当前线程上的协程控制信息

    // 正在运行的协程
    static thread_local Fiber *t_fiber = nullptr;
    // 主协程
    static thread_local std::shared_ptr<Fiber> t_thread_fiber = nullptr;
    // 调度协程
    static thread_local Fiber *t_scheduler_fiber = nullptr;

    // 全局协程id计数器
    static std::atomic<uint64_t> s_fiber_id{0};
    // 活跃协程数量计数器
    static std::atomic<uint64_t> s_fiber_count{0};

    Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool run_in_scheduler) : m_cb(cb), m_runInScheduler(run_in_scheduler)
    {
        // 初始化状态
        m_state = READY;

        // 分配协程栈空间
        m_staciSize = stacksize ? stacksize : 128000;
        m_stack = malloc(m_staciSize);

        if (getcontext(&m_ctx))
        {
            std::cerr << "Fiber(std::function<void()> cb, size_t stackeize, bool run_in_scheduler) failed" << std::endl;
            pthread_exit(nullptr);
        }

        // 没设置后继上下文，运行完MainFunc后协程推出，会调用依次yield返回主协程
        m_ctx.uc_link = nullptr;
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_staciSize;
        makecontext(&m_ctx, &Fiber::MainFunc, 0);

        m_id = s_fiber_id++;
        s_fiber_count++;
        if (debug)
        {
            std::cout << "Fiber(): child id = " << m_id << std::endl;
        }
    }

    Fiber::~Fiber()
    {
        // 减少活跃协程计数器
        s_fiber_count--;

        if (m_stack)
        {
            free(m_stack);
        }
        if (debug)
        {
            std::cout << "~Fiber(): id = " << m_id << std::endl;
        }
    }

    void Fiber::reset(std::function<void()> cb)
    {
        assert(m_stack != nullptr && m_state == TERM);

        m_state = READY;
        m_cb = cb;

        if (getcontext(&m_ctx))
        {
            std::cerr << "reset() failed" << std::endl;
            pthread_exit(nullptr);
        }

        m_ctx.uc_link = nullptr;
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_staciSize;
        makecontext(&m_ctx, &Fiber::MainFunc, 0);
    }

    void Fiber::resume()
    {
        assert(m_state == READY);

        m_state = RUNNING;

        if (m_runInScheduler)
        {
            SetThis(this);
            if (swapcontext(&(t_scheduler_fiber->m_ctx), &m_ctx))
            {
                std::cerr << "resume() to t_scheduler_fiber failed" << std::endl;
                pthread_exit(nullptr);
            }
        }
        else
        {
            SetThis(this);
            if (swapcontext(&(t_thread_fiber->m_ctx), &m_ctx))
            {
                std::cerr << "resume() to t_thread_fiber failed" << std::endl;
                pthread_exit(nullptr);
            }
        }
    }

    void Fiber::yield()
    {
        assert(m_state == RUNNING || m_state == TERM);

        if (m_state != TERM)
        {
            m_state = READY;
        }

        if (m_runInScheduler)
        {
            SetThis(t_scheduler_fiber);
            if (swapcontext(&m_ctx, &(t_scheduler_fiber->m_ctx)))
            {
                std::cerr << "yield() to t_scheduler_fiber failed" << std::endl;
                pthread_exit(nullptr);
            }
        }
        else
        {
            SetThis(t_thread_fiber.get());
            if (swapcontext(&m_ctx, &(t_thread_fiber->m_ctx)))
            {
                std::cerr << "yield() to t_thread_fiber failed" << std::endl;
                pthread_exit(nullptr);
            }
        }
    }

    void Fiber::SetThis(Fiber *f)
    {
        t_fiber = f;
    }

    // 首先运行该函数创建主协程
    std::shared_ptr<Fiber> Fiber::GetThis()
    {
        if (t_fiber)
        {
            return t_fiber->shared_from_this();
        }

        std::shared_ptr<Fiber> main_fiber(new Fiber());
        t_thread_fiber = main_fiber;
        t_scheduler_fiber = main_fiber.get(); // 除非主动设置 主协程默认为调度协程

        // 用于判断t_fiber是否等于main_fiber，是：继续执行；否：程序终止
        assert(t_fiber == main_fiber.get());
        return t_fiber->shared_from_this();
    }

    void Fiber::SetSchedulerFiber(Fiber *f)
    {
        t_scheduler_fiber = f;
    }

    uint64_t Fiber::GetFiberId()
    {
        if (t_fiber)
        {
            return t_fiber->getId();
        }
        // (uint64_t)-1会转化为UINT64_MAX，用来表示错误的情况
        return (uint64_t)-1;
    }

    void Fiber::MainFunc()
    {
        std::shared_ptr<Fiber> curr = GetThis();
        assert(curr != nullptr);

        curr->m_cb();
        curr->m_cb = nullptr;
        curr->m_state = TERM;

        // 运行完毕 -> 让出执行权
        auto raw_ptr = curr.get();
        curr.reset(); // shared_ptr中的方法，释放当前管理的对象，只能指针引用计数-1
        raw_ptr->yield();
    }

    Fiber::Fiber()
    {
        SetThis(this);
        m_state = RUNNING;

        if (getcontext(&m_ctx))
        {
            std::cerr << "Fiber() failed" << std::endl;
            pthread_exit(nullptr);
        }

        m_id = s_fiber_id++;
        s_fiber_count++;
        if (debug)
        {
            std::cout << "Fiber(): main id = " << m_id << std::endl;
        }
    }

} // namespace sylar

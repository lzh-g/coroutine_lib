#include "timer.h"

namespace sylar
{
    bool Timer::cancel()
    {
        // 删除一个定时器的回调函数，并将其从时间堆中移除
        std::unique_lock<std::shared_mutex> write_lock(m_manager->m_mutex);

        if (!m_cb)
        {
            return false;
        }
        else
        {
            m_cb = nullptr;
        }

        // 获取定时器
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it != m_manager->m_timers.end())
        {
            // 删除定时器
            m_manager->m_timers.erase(it);
        }
        return true;
    }

    bool Timer::refresh()
    {
        // refresh只会向后调整
        std::unique_lock<std::shared_mutex> write_lock(m_manager->m_mutex);
        if (!m_cb)
        {
            return false;
        }

        // 获取当前定时器
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end())
        {
            return false;
        }

        // 移除该定时器
        m_manager->m_timers.erase(it);
        // 更新该定时器的超时时间
        m_next = std::chrono::system_clock::now() + std::chrono::milliseconds(m_ms);
        // 重新加入时间堆中
        m_manager->m_timers.insert(shared_from_this());
        return true;
    }

    bool Timer::reset(uint64_t ms, bool from_now)
    {
        if (ms == m_ms && !from_now)
        {
            // 不需要重置
            return true;
        }

        std::unique_lock<std::shared_mutex> write_lock(m_manager->m_mutex);

        if (!m_cb)
        {
            // 该定时器已被取消/未初始化，无法重置
            return false;
        }

        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end())
        {
            return false;
        }
        m_manager->m_timers.erase(it);

        // reinsert
        auto start = from_now ? std::chrono::system_clock::now() : m_next - std::chrono::milliseconds(m_ms); // 设置是从当前时间/上次超时时间开始计算
        m_ms = ms;
        m_next = start + std::chrono::milliseconds(m_ms);
        // insert with lock
        m_manager->addTimer(shared_from_this());
        return true;
    }

    Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager) : m_ms(ms), m_cb(cb), m_recurring(recurring), m_manager(manager)
    {
        auto now = std::chrono::system_clock::now();
        m_next = now + std::chrono::milliseconds(m_ms);
    }

    bool Timer::Comparator::operator()(const std::shared_ptr<Timer> &lhs, const std::shared_ptr<Timer> &rhs) const
    {
        assert(lhs != nullptr && rhs != nullptr);
        return lhs->m_next < rhs->m_next;
    }

    TimerManager::TimerManager()
    {
        m_previousTime = std::chrono::system_clock::now();
    }

    TimerManager::~TimerManager()
    {
    }

    std::shared_ptr<Timer> TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring)
    {
        std::shared_ptr<Timer> timer(new Timer(ms, cb, recurring, this));
        addTimer(timer);
        return timer;
    }

    // 若条件存在，执行cb()
    static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb)
    {
        // 如果条件存在，执行cb()
        std::shared_ptr<void> tmp = weak_cond.lock();
        if (tmp)
        {
            cb();
        }
    }

    std::shared_ptr<Timer> TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring)
    {
        return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
    }

    uint64_t TimerManager::getNextTimer()
    {
        std::shared_lock<std::shared_mutex> read_lock(m_mutex);

        // reset m_tickled
        m_tickled = false;

        if (m_timers.empty())
        {
            // 返回最大值
            return ~0ull;
        }

        auto now = std::chrono::system_clock::now();
        auto time = (*m_timers.begin())->m_next;

        if (now >= time)
        {
            // 已经有timer超时
            return 0;
        }
        else
        {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(time - now);
            return static_cast<uint64_t>(duration.count()); // 返回毫秒
        }
    }

    void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs)
    {
        auto now = std::chrono::system_clock::now();

        std::unique_lock<std::shared_mutex> write_lock(m_mutex);

        // 判断是否出现系统时间错误
        bool rollover = detectClockRollover();

        // 回退 -> 清理所有timer || 超时 -> 清理超时timer
        while (!m_timers.empty() && rollover || !m_timers.empty() && (*m_timers.begin())->m_next <= now)
        {
            std::shared_ptr<Timer> temp = *m_timers.begin();
            m_timers.erase(m_timers.begin());

            cbs.push_back(temp->m_cb);

            if (temp->m_recurring)
            {
                // 重新加入时间堆
                temp->m_next = now + std::chrono::milliseconds(temp->m_ms);
                m_timers.insert(temp);
            }
            else
            {
                // 清理cb
                temp->m_cb = nullptr;
            }
        }
    }

    bool TimerManager::hasTimer()
    {
        std::shared_lock<std::shared_mutex> read_lock(m_mutex);
        return !m_timers.empty();
    }

    // lock + tickle()
    void TimerManager::addTimer(std::shared_ptr<Timer> timer)
    {
        // 标识是否是最早超时的定时器
        bool at_front = false;

        std::unique_lock<std::shared_mutex> write_lock(m_mutex);
        auto it = m_timers.insert(timer).first;
        // 判断是否是最早超时的朝时期
        at_front = (it == m_timers.begin()) && !m_tickled;

        // only tickle once till one thread wakes up and runs getNextTimer()
        if (at_front)
        {
            m_tickled = true;
            // wake up
            onTimerInsertedAtFront();
        }
    }

    bool TimerManager::detectClockRollover()
    {
        bool rollover = false;
        auto now = std::chrono::system_clock::now();
        if (now < (m_previousTime - std::chrono::milliseconds(60 * 60 * 1000)))
        {
            rollover = true;
        }
        m_previousTime = now;
        return rollover;
    }

} // namespace sylar

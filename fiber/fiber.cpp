#include "fiber.h"

sylar::Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool run_in_scheduler)
{
}

sylar::Fiber::~Fiber()
{
}

void sylar::Fiber::reset(std::function<void()> cb)
{
}

void sylar::Fiber::resume()
{
}

void sylar::Fiber::yield()
{
}

void sylar::Fiber::SetThis(Fiber *f)
{
}

std::shared_ptr<Fiber> sylar::Fiber::GetThis()
{
    return std::shared_ptr<Fiber>();
}

void sylar::Fiber::SetSchedulerFiber(Fiber *f)
{
}

uint64_t sylar::Fiber::GetFiberId()
{
    return 0;
}

void sylar::Fiber::MainFunc()
{
}

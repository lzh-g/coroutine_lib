#ifndef _HOOK_H_
#define _HOOK_H_

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <fcntl.h>

namespace sylar
{
    bool is_hook_enable();
    void set_hook_enable(bool flag);
} // namespace sylar

extern "C"
{
    // track the original version
    typedef unsigned int (*sleep_fun)(unsigned int second);
    extern sleep_fun sleep_f;

    typedef int (*usleep_fun)(useconds_t usec);
    extern usleep_fun usleep_;
}

#endif
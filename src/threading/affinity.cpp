#include "dreadnought/threading/affinity.hpp"
#include <pthread.h>
#include <sched.h>
#include <cstring>

namespace dreadnought {

bool ThreadAffinity::pin_to_core(int core_id) noexcept {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    
    return pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) == 0;
}

bool ThreadAffinity::set_realtime_priority() noexcept {
    struct sched_param param;
    param.sched_priority = 99; // Max RT priority
    
    return pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) == 0;
}

void ThreadAffinity::warmup_cache(void* addr, size_t size) noexcept {
    volatile char* ptr = static_cast<volatile char*>(addr);
    for (size_t i = 0; i < size; i += 64) {
        char x = ptr[i];
        (void)x;
    }
}

} // namespace dreadnought
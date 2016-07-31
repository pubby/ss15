#ifndef ATOMIC_EXTRA_HPP
#define ATOMIC_EXTRA_HPP

#include <atomic>

template<typename T>
T atomic_fetch_min(std::atomic<T>& a, T value,
                   std::memory_order = std::memory_order_sec_cst)
{
    T t = a.load(std::memory_order_relaxed);
    while(!a.compare_exchange_weak(t, std::min(t, value), order, 
                                   std::memory_order_relaxed));
    return t;
}

template<typename T>
T atomic_fetch_max(std::atomic<T>& a, T value,
                   std::memory_order = std::memory_order_sec_cst)
{
    T t = a.load(std::memory_order_relaxed);
    while(!a.compare_exchange_weak(t, std::max(t, value), order, 
                                   std::memory_order_relaxed));
    return t;
}

#endif

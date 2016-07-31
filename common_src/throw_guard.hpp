#ifndef THROW_GUARD_HPP
#define THROW_GUARD_HPP

#include <functional>
#include <utility>

// A scope_guard-like class which wraps a function and calls it when
// destroyed due to exception stack unwinding.
template<typename OnThrow = std::function<void()>>
class throw_guard
{
public:
    throw_guard() : m_uncaught(std::uncaught_exceptions()) {}

    template<typename F>
    throw_guard(F&& on_throw)
    : m_uncaught(std::uncaught_exceptions())
    , on_throw(std::forward<F>(on_throw))
    {}

    throw_guard(throw_guard const&) = delete;

    throw_guard(throw_guard&& o)
    : m_uncaught(std::uncaught_exceptions())
    , on_throw(std::move(o.on_throw))
    {}

    throw_guard& operator=(throw_guard const&) = delete;

    throw_guard& operator=(throw_guard&& o)
    {
        on_throw = std::move(o.on_throw);
    }

    ~throw_guard()
    {
        if(std::uncaught_exceptions() > m_uncaught)
            on_throw();
    }
    
private:
    int m_uncaught;
public:
    OnThrow on_throw;
};

template<typename T>
throw_guard<std::remove_reference_t<T>> make_throw_guard(T&& t)
{
    return std::forward<T>(t);
}

#endif

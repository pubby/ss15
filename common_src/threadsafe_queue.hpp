#ifndef THREADSAFE_QUEUE_HPP
#define THREADSAFE_QUEUE_HPP

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>

// Protects a queue-like container with a mutex.
// Implementation is minimal; add member functions as needed.
template<typename T, typename Container = std::deque<T>>
class threadsafe_queue
{
public:
    using container_type = Container;
    using value_type = T;

    threadsafe_queue(std::size_t max_size = SIZE_MAX)
    : m_max_size(max_size)
    {}

    threadsafe_queue(threadsafe_queue const& o)
    : m_queue(o.container())
    , m_max_size(o.max_size)
    {}

    // Can't be nothrow because swap can throw.
    threadsafe_queue(threadsafe_queue&& o)
    : threadsafe_queue()
    {
        swap(o);
    }

    threadsafe_queue& operator=(threadsafe_queue o) { swap(o); }

    bool empty() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    template<typename V>
    bool push(V&& v)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if(m_queue.size() >= m_max_size)
            return false;
        m_queue.push_back(std::forward<V>(v));
        m_condition.notify_one();
        return true;
    }

    void pop(value_type& dest)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while(m_queue.empty())
            m_condition.wait(lock);
        dest = std::move(m_queue.front());
        m_queue.pop_front();
    }

    bool try_pop(value_type& dest)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if(m_queue.empty())
            return false;
        dest = std::move(m_queue.front());
        m_queue.pop_front();
        return true;
    }

    // Empties the queue.
    // Returns the underlying container before it was emptied.
    container_type flush()
    {
        container_type ret;
        std::unique_lock<std::mutex> lock(m_mutex);
        using std::swap;
        swap(m_queue, ret);
        return ret;
    }

    // Returns the underlying container by value.
    // This allows reading the copy without locking the mutex.
    container_type container() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue;
    }

    // Can't be nothrow because std::mutex can throw.
    void swap(threadsafe_queue& o)
    {
        if(this == &o)
            return;

        std::lock(m_mutex, o.m_mutex);
        std::unique_lock<std::mutex> lock1(m_mutex, std::adopt_lock);
        std::unique_lock<std::mutex> lock2(o.m_mutex, std::adopt_lock);

        using std::swap;
        swap(m_queue, o.m_queue);
        swap(m_max_size, o.m_max_size);

        if(!m_queue.empty())
            m_condition.notify_all();
        lock1.unlock();
        if(!o.m_queue.empty())
            o.m_condition.notify_all();
    }

private:
    container_type m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::size_t m_max_size;
};

// A threadsafe, fixed-size circular queue that allows pushing
// values out of order. Example use: storing received UDP packets.
// Implementation is minimal; add member functions as needed.
template<typename T, std::size_t Size>
class out_of_order_queue
{
public:
    using value_type = T;
    static constexpr std::size_t size = Size;

    out_of_order_queue(std::size_t starting_index = 0)
    : m_circular_queue{}
    , m_awaiting(starting_index)
    {}

    // Returns the index of the popped value.
    std::size_t pop(value_type& dest)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while(!m_circular_queue[m_awaiting % size].exists)
            m_condition.wait(lock);
        dest = std::move(m_circular_queue[m_awaiting % size].value);
        m_circular_queue[m_awaiting % size].exists = false;
        return m_awaiting++;
    }

    // Forcibly skip the next 'pop', regardless of whether or not 
    // the value exists.
    std::size_t skip()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_circular_queue[m_awaiting % size].exists = false;
        if(m_circular_queue[(m_awaiting+1) % size].exists)
            m_condition.notify_all();
        return m_awaiting++;
    }

    // 'set' is the closest thing this class has to 'push'.
    // The next call to 'pop' will retrieve the value set by this function.
    template<typename V>
    void set(V&& v)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.notify_all();
        m_circular_queue[m_awaiting % size].value = std::forward<V>(v);
    }

    // This function sets a future value that 'pop' will 
    // retrieve when m_await == index.
    template<typename V>
    bool set(V&& v, std::size_t index)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if(index >= m_awaiting + size)
            return false; // Return false when the queue overflows; error!
        if(index < m_awaiting)
            return true; // Underflows are OK though; just ignore them.
        if(index == m_awaiting)
            m_condition.notify_all();
        m_circular_queue[index % size].value = std::forward<V>(v);
        m_circular_queue[index % size].exists = true;
        return true;
    }

    bool has(std::size_t index) const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if(index >= m_awaiting + size)
            return false;
        if(index < m_awaiting)
            return true;
        return m_circular_queue[index % size].exists;
    }

private:
    // Rather than using std::optional, the implementation instead
    // uses pairs consisting of value+bool. This means that
    // popping a value does not call destructors and setting a value
    // does not call constructors. Instead, everything is done with,
    // move-assigns. That's a caveat, so take note!
    struct mapped_t
    {
        T value;
        bool exists;
    };

    std::array<mapped_t, size> m_circular_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::size_t m_awaiting;
};

#endif

#ifndef THREADSAFE_MAP_HPP
#define THREADSAFE_MAP_HPP

#include <shared_mutex>

#include <boost/container/flat_map.hpp>

#include "optional.hpp"

// Protects a map-like container with a shared_mutex.
// Implementation is minimal; add member functions as needed.
template<typename Key, typename T, 
         typename Compare = std::less<Key>,  // TODO: use std::default_order_t
         typename Container = boost::container::flat_map<Key, T, Compare> >
class threadsafe_map
{
public:
    // The default container is Boost's flat_map as it's fast to copy.
    using container_type = Container;
    using key_type = Key;
    using mapped_type = T;
    using value_type = typename container_type::value_type;
    using key_compare = Compare;

    threadsafe_map() = default;
    threadsafe_map(threadsafe_map const& o) : map(o.container()) {}
    // Can't be nothrow because swap can throw.
    threadsafe_map(threadsafe_map&& o) { swap(o); }
    threadsafe_map& operator=(threadsafe_map o) { swap(o); return *this; }

    template<typename Pair>
    bool insert(Pair&& pair)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        return map.insert(std::forward<Pair>(pair)).second;
    }

    template<typename K, typename M>
    bool insert_or_assign(K&& k, M&& m)
    {
        return insert_or_assign<container_type>(
            0, std::forward<K>(k), std::forward<M>(m));
    }

    template<typename... Args>
    bool emplace(Args&&... args)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        return map.emplace(std::forward<Args>(args)...).second;
    }

    bool erase(key_type const& k)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        return map.erase(k);
    }

    void clear()
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        return map.clear();
    }

    // Can't be nothrow because std::shared_mutex can throw.
    void swap(threadsafe_map& o)
    {
        if(this == &o)
            return;

        std::lock(mutex, o.mutex);
        std::unique_lock<std::mutex> lock1(mutex, std::adopt_lock);
        std::unique_lock<std::mutex> lock2(o.mutex, std::adopt_lock);

        using std::swap;
        swap(map, o.map);
    }

    template<class K>
    std::size_t count(K const& k) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return map.count(k);
    }

    optional<T> try_get(key_type const& k) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        auto it = map.find(k);
        return it == map.cend() ? nullopt : it->second;
    }

    template<typename M>
    bool try_set(key_type const& k, M&& t)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        auto it = map.find(k);
        if(it != map.cend())
            it->second = std::forward<M>(t);
        return it != map.cend();
    }

    // Returns the underlying container by value.
    // This allows reading the copy without locking the mutex.
    container_type container() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return map;
    }

    // Allows arbitrary modification to the container map.
    template<typename Func>
    auto with_container(Func func)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        return func(map);
    }

    template<typename Func>
    auto with_container(Func func) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return func(map);
    }

    template<typename Func>
    auto with_container_const(Func func) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return func(map);
    }
    
    template<typename Func>
    void for_each(Func func) const
    {
        for(auto const& pair : container())
            func(pair);
    }

private:
    // Not every map container containes C++17's insert_or_assign.
    // Thus, use SFINAE to use it only when availible.
    template<typename C, typename K, typename M>
    auto insert_or_assign(int, K&& k, M&& m)
    -> decltype(&C::insert_or_assign, bool())
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        return map.insert_or_assign(std::forward<K>(k), 
                                    std::forward<M>(m)).second;
    }

    template<typename C, typename K, typename M>
    bool insert_or_assign(long, K&& k, M&& m)
    {
        std::unique_lock<std::shared_mutex> lock(mutex);
        value_type pair(std::forward<K>(k), std::forward<M>(m));
        auto result = map.insert(pair);
        if(!result.second)
            *result.first = std::move(pair);
        return result.second;
    }

    // This class's critical segments are more than a few instructions
    // and so std::shared_mutex is worth using.
    // To improve parellism:
    // Reduce critical segment size and switch to std::mutex.
    mutable std::shared_mutex mutex;
    container_type map;
};

#endif

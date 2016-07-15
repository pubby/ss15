#ifndef POOL_HPP
#define POOL_HPP

// Fixed-size, thread-safe memory pools, intended to be used for 
// allocating buffers for ASIO without std::malloc overhead/fragmentation.

#include <array>
#include <atomic>
#include <cassert>
#include <type_traits>

template<typename BlockType, std::size_t NumBlocks>
class pool_template
{
static_assert(std::is_standard_layout<BlockType>::value, 
              "pool type must be standard layout"); 
public:
    using block_type = BlockType;
    using value_type = typename BlockType::value_type;
    static constexpr std::size_t size = NumBlocks;

    pool_template() : blocks{} {}
    pool_template(pool_template const&) = delete;
    pool_template& operator=(pool_template const&) = delete;

    value_type* malloc() noexcept
    {
        for(block_type& block : blocks)
            if(!block.allocated.test_and_set(std::memory_order_acquire))
                return &block.value;
        return nullptr;
    }

    void free(value_type* ptr) noexcept
    {
        if(ptr)
            reinterpret_cast<block_type*>(ptr)->allocated.clear(
                std::memory_order_release);
    }

private:
    std::array<block_type, NumBlocks> blocks;
};

template<typename T>
struct simple_pool_block
{
    using value_type = T;
    value_type value; // Must be first member.
    std::atomic_flag allocated;
};

template<typename T, std::size_t NumBlocks>
struct sharable_pool_block
{
    using value_type = T;
    value_type value; // Must be first member.
    std::atomic_flag allocated;
    std::atomic<int> refcount;
    pool_template<sharable_pool_block, NumBlocks>* owner;
};

template<typename T, std::size_t NumBlocks>
using memory_pool = pool_template<simple_pool_block<T>, NumBlocks>;

// Sharable pool allows the allocation of shared_pooled_ptrs.
template<typename T, std::size_t NumBlocks>
using sharable_pool 
    = pool_template<sharable_pool_block<T, NumBlocks>, NumBlocks>;

// A smart pointer that behaves like shared_ptr, but is designed to
// be used with sharable_pool instead.
// This class _should_ be as thread-safe as std::shared_ptr is.
// See libc++ for the cleanest/easiest implementation of
// std::shared_ptr (to copy from), and cppreference.com for some details.
// (Stay away from Boost's implementation!)
template<typename T, std::size_t NumBlocks>
class shared_pooled_ptr
{
template<typename P>
friend shared_pooled_ptr<typename P::value_type, P::size> 
make_shared_from_pool(P& pool);
public:
    using pool_type = sharable_pool<T, NumBlocks>;
    using block_type = typename pool_type::block_type;
    using element_type = T;

    shared_pooled_ptr() noexcept : block_ptr(nullptr) {}

    shared_pooled_ptr(shared_pooled_ptr const& other) noexcept
    : block_ptr(other.block_ptr)
    {
        // Most shared_ptr implementations use memory_order_relaxed for
        // the increments, and memory_order_acq_rel for decrements.
        if(block_ptr)
            block_ptr->refcount.fetch_add(1, std::memory_order_relaxed);
    }

    shared_pooled_ptr(shared_pooled_ptr&& other) noexcept
    : block_ptr(other.block_ptr)
    {
        other.block_ptr = nullptr;
    }

    ~shared_pooled_ptr() noexcept
    {
        if(block_ptr && block_ptr
           ->refcount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            if(block_ptr->owner)
                block_ptr->owner->free(&block_ptr->value);
            else
                delete block_ptr;
        }
    }

    shared_pooled_ptr& operator=(shared_pooled_ptr const& o) noexcept
    {
        shared_pooled_ptr(o).swap(*this);
        return *this;
    }

    shared_pooled_ptr& operator=(shared_pooled_ptr&& o) noexcept
    {
        shared_pooled_ptr(std::move(o)).swap(*this);
        return *this;
    }

    void reset() noexcept { shared_pooled_ptr().swap(*this); }

    void swap(shared_pooled_ptr& o) noexcept
    {
        std::swap(block_ptr, o.block_ptr);
    }

    T* get() const noexcept { return block_ptr ? &block_ptr->value : nullptr; }

    T& operator*() const noexcept { return block_ptr->value; }
    T* operator->() const noexcept { return &block_ptr->value; }
private:
    shared_pooled_ptr(block_type& block) noexcept
    : block_ptr(&block) { block.refcount.store(1); }

    block_type* block_ptr;
};

template<typename P>
shared_pooled_ptr<typename P::value_type, P::size> 
make_shared_from_pool(P& pool)
{
    using block_type = typename P::block_type;
    using value_type = typename P::value_type;

    block_type* block_ptr;
    if(value_type* value_ptr = pool.malloc())
    {
        block_ptr = reinterpret_cast<block_type*>(value_ptr);
        block_ptr->owner = &pool;
    }
    else
    {
        // If the pool is full, allocate the value with 'new' instead.
        // (Make sure to 'delete' it in ~shared_pooled_ptr()!)
        block_ptr = new block_type;
        block_ptr->owner = nullptr;
    }
    return shared_pooled_ptr<value_type, P::size>(*block_ptr);
}

#endif

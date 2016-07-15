#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <memory>

// A buffer with an immutable size.
// Uses shared_ptr internally and has same thread safety rules as shared_ptr.
// Copies like a shared_ptr too.
class shared_buffer_t
{
public:
    explicit shared_buffer_t(std::size_t size)
    : m_data(new char[size], std::default_delete<char[]>())
    , m_end_ptr(m_data.get() + size)
    {}

    shared_buffer_t(shared_buffer_t const&) = default;
    shared_buffer_t(shared_buffer_t&&) = default;

    shared_buffer_t& operator=(shared_buffer_t const&) = default;
    shared_buffer_t& operator=(shared_buffer_t&&) = default;

    friend void swap(shared_buffer_t&, shared_buffer_t&);

    char const* data() const { return m_data.get(); }
    char* data() { return m_data.get(); }
    
    char const& operator[](std::size_t i) const { return data()[i]; }
    char& operator[](std::size_t i) { return data()[i]; }
    
    std::size_t size() const { return m_end_ptr - data(); }
    
    using const_iterator = char const*;
    using iterator = char*;
    
    const_iterator cbegin() const { return data(); }
    const_iterator begin() const { return data(); }
    iterator begin() { return data(); }

    const_iterator cend() const { return m_end_ptr; }
    const_iterator end() const { return m_end_ptr; }
    iterator end() { return m_end_ptr; }

private:
    std::shared_ptr<char> m_data;
    char* m_end_ptr;
};

// A buffer with an immutable size, based on std::unique_ptr.
class unique_buffer_t
{
public:
    explicit unique_buffer_t(std::size_t size)
    : m_data(new char[size])
    , m_end_ptr(m_data.get() + size)
    {}

    unique_buffer_t(unique_buffer_t const&) = delete;
    unique_buffer_t(unique_buffer_t&&) = default;

    unique_buffer_t& operator=(unique_buffer_t const&) = delete;
    unique_buffer_t& operator=(unique_buffer_t&&) = default;
    
    friend void swap(unique_buffer_t&, unique_buffer_t&);
    
    char const* data() const { return m_data.get(); }
    char* data() { return m_data.get(); }
    
    char const& operator[](std::size_t i) const { return data()[i]; }
    char& operator[](std::size_t i) { return data()[i]; }
    
    std::size_t size() const { return m_end_ptr - data(); }
    
    using const_iterator = char const*;
    using iterator = char*;
    
    const_iterator cbegin() const { return data(); }
    const_iterator begin() const { return data(); }
    iterator begin() { return data(); }

    const_iterator cend() const { return m_end_ptr; }
    const_iterator end() const { return m_end_ptr; }
    iterator end() { return m_end_ptr; }

private:
    std::unique_ptr<char[]> m_data;
    char* m_end_ptr;
};

inline void swap(shared_buffer_t& a, shared_buffer_t& b)
{
    using std::swap;
    swap(a.m_data, b.m_data);
    swap(a.m_end_ptr, b.m_end_ptr);
}

inline void swap(unique_buffer_t& a, unique_buffer_t& b)
{
    using std::swap;
    swap(a.m_data, b.m_data);
    swap(a.m_end_ptr, b.m_end_ptr);
}


#endif

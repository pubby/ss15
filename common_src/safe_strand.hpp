#ifndef SAFE_STRAND_HPP
#define SAFE_STRAND_HPP

#include <utility>

#include <boost/asio.hpp>

template<typename Tag>
class safe_strand;

// This class is intended to make programming with asio::strand 
// slightly easier/safer.
// Posting through a safe_strand constructs a new strand_key and passes it as
// an argument to the posted function.
// The function can then call other functions and pass its strand_key, which
// signifies that they are being called through the strand too.
// Note that strand_key can only be passed by rvalue. Use std::move.
template<typename Tag>
class strand_key
{
friend class safe_strand<Tag>;
public:
    strand_key(strand_key const&) = delete;
    strand_key(strand_key&&) = default;
    strand_key& operator=(strand_key const&) = delete;
    strand_key& operator=(strand_key&&) = default;
private:
    strand_key() = default;
};

template<typename Tag>
class safe_strand
{
public:
    using key = strand_key<Tag>;

    safe_strand(boost::asio::io_service& io_service)
    : m_strand(io_service)
    {}
    
    boost::asio::io_service& get_io_service()
    {
        return m_strand.get_io_service();
    }

    bool running_in_this_thread() const
    {
        return m_strand.running_in_this_thread();
    }

private:
    template<typename Func>
    struct helper
    {
        Func m_func;
        template<typename... Args>
        auto operator()(Args&&... args)
        {
            return m_func(strand_key<Tag>(), std::forward<Args>(args)...);
        }
    };

    boost::asio::strand m_strand;
public:
    template<typename Func>
    void post(Func func)
    {
        m_strand.post([func]() mutable { func(strand_key<Tag>()); });
    }

    template<typename Func>
    void dispatch(Func func)
    {
        m_strand.dispatch([func]() mutable { func(strand_key<Tag>()); });
    }

    template<typename Func>
    auto wrap(Func func)
    {
        return m_strand.wrap(helper<Func>{std::move(func)});
    }
};

#endif

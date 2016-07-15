#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio.hpp>

#include "buffer.hpp"
#include "game.hpp"
#include "net.hpp"
#include "pool.hpp"
#include "safe_strand.hpp"
#include "serialize.hpp"
#include "threadsafe_queue.hpp"

namespace asio = boost::asio;
namespace posix_time = boost::posix_time;
namespace ip = boost::asio::ip;

using error_code_t = boost::system::error_code;
using system_error = boost::system::system_error;

constexpr std::size_t MAX_UDP_PAYLOAD = 1400;
using udp_buffer_t = std::array<char, MAX_UDP_PAYLOAD>;

class client_t
{
public:
    struct udp_receiver_t
    {
        ip::udp::endpoint endpoint;
        udp_buffer_t buffer;
    };

    static constexpr std::size_t udp_pool_size = 32;
    using shared_udp_receiver_t = shared_pooled_ptr<udp_receiver_t, 
                                                    udp_pool_size>;
    using udp_pool_t = shared_udp_receiver_t::pool_type;

    client_t
    ( asio::io_service& io_service
    , std::string const& address
    , std::string const& port);
  
    /// Run the server's io_service loop.
    void run();

    void send_input(cts_input_t input);
private:
    struct udp_socket_tag {};
    using udp_socket_key_t = strand_key<udp_socket_tag>;
    struct tcp_socket_tag {};
    using tcp_socket_key_t = strand_key<tcp_socket_tag>;

    ////////////////////////////////////
    // Error handling

    void report(char const* fmt, ...) const
    {
        va_list args;
        va_start(args, fmt);
        std::fputs("error: ", stderr);
        std::vfprintf(stderr, fmt, args);
        std::fputc('\n', stderr);
        va_end(args);
    }

    ////////////////////////////////////
    // General send/receive functions

    template<typename Handler>
    void tcp_send
    ( tcp_socket_key_t key
    , shared_buffer_t shared_buffer
    , Handler handler);

    template<typename Handler>
    void tcp_send_message
    ( tcp_socket_key_t key
    , cts_tcp_message_t message
    , Handler handler);

    template<typename Handler>
    void tcp_read
    ( tcp_socket_key_t key
    , std::size_t bytes
    , Handler handler);

    template<typename Handler>
    void tcp_read_message
    ( tcp_socket_key_t key
    , Handler handler);

    template<typename Handler>
    void tcp_read_message_body
    ( tcp_socket_key_t key
    , stc_tcp_header_t header
    , Handler handler);

    template<typename Handler>
    void udp_send
    ( udp_socket_key_t key
    , shared_buffer_t shared_buffer
    , Handler handler);

    template<typename Handler>
    void udp_send_message
    ( udp_socket_key_t key
    , cts_udp_message_t message
    , Handler handler);

    void udp_receive(udp_socket_key_t key);

    void handle_udp_receive
    ( shared_udp_receiver_t shared_receiver
    , std::size_t bytes_received);

    ////////////////////////////////////
    // Startup messages (in sequential order)

    void tcp_read_server_info(tcp_socket_key_t key);

    void tcp_send_login(tcp_socket_key_t key);

    void tcp_read_game_state(tcp_socket_key_t key);

    void udp_read_updates(udp_socket_key_t key);

    void attempt_join();
    //void do_read_join_response();
    //void handle_join_response(stc_header, shared_buffer_t buf);
    
private:
    asio::io_service& m_io_service;

    ip::tcp::socket m_tcp_socket;
    safe_strand<tcp_socket_tag> m_tcp_socket_strand;

    ip::udp::socket m_udp_socket;
    ip::udp::endpoint m_udp_endpoint;
    safe_strand<udp_socket_tag> m_udp_socket_strand;
    std::unique_ptr<udp_pool_t> m_udp_pool;

    std::atomic<std::uint32_t> m_sequence_number;

public:
    std::promise<game_state_t> game_state_promise;
    out_of_order_queue<diff_t, 16> update_queue;
};

/*
template<typename T>
struct client_t::read_message_length
{
    void operator()
    ( tcp_socket_key_t key
    , client_t* self
    , shared_buffer_t shared_buffer) const
    {
        assert(shared_buffer.size() > sizeof(std::uint32_t));
        auto asio_buffer = asio::buffer(shared_buffer.data(),
                                        sizeof(std::uint32_t));
        asio::async_read(
            self->m_tcp_socket,
            asio_buffer,
            m_tcp_socket_strand.wrap(
                [this, shared_buffer = std::move(shared_buffer), handler]
                (tcp_socket_key_t key, error_code_t const& e, std::size_t)
                mutable
                {
                    if(e)
                        throw system_error(e);

                    std::uint32_t message_size;
                    serialize<std::uint32_t>::read(
                        shared_buffer.begin(),
                        shared_buffer.end(),
                        message_size);

                    if(message_size > shared_buffer.size())
                        shared_buffer = shared_buffer_t(message_size);

                    self->read_message<T>(std::move(shared_buffer));
                }));
    }

template<typename T>
struct client_t::read_message_length<T, std::void_t<decltype(T::const_size)>>
{
    void operator()(client_t* self, shared_buffer_t shared_buffer) const
    {
        if(T::const_size > shared_buffer.size())
            shared_buffer = shared_buffer_t(T::const_size);
        self->read_message<T>(std::move(shared_buffer));
    }
};
*/

template<typename Handler>
void client_t::tcp_send
( tcp_socket_key_t key
, shared_buffer_t shared_buffer
, Handler handler)
{
    auto asio_buffer = asio::buffer(shared_buffer.data(),
                                    shared_buffer.size());
    asio::async_write(
        m_tcp_socket,
        asio_buffer,
        m_tcp_socket_strand.wrap(
            [this, shared_buffer = std::move(shared_buffer), handler]
            (tcp_socket_key_t key, error_code_t const& e, std::size_t) mutable
            {
                if(e)
                    throw system_error(e);
                handler(std::move(key));
            }));
}

template<typename Handler>
void client_t::tcp_send_message
( tcp_socket_key_t key
, cts_tcp_message_t message
, Handler handler)
{
    using header_serialize = serialize<cts_tcp_header_t>;
    using message_serialize = serialize<cts_tcp_message_t, void>;

    cts_tcp_header_t header = 
    {
        message.which(),
        message_serialize::size(message),
    };

    shared_buffer_t shared_buffer(
        header_serialize::size(header)
        + message_serialize::size(message));

    auto it = header_serialize::write(header, shared_buffer.begin());
    it = message_serialize::write(message, it);

    tcp_send(std::move(key), std::move(shared_buffer), handler);
}

template<typename Handler>
void client_t::tcp_read
( tcp_socket_key_t key
, std::size_t bytes
, Handler handler)
{
    shared_buffer_t shared_buffer(bytes);
    auto asio_buffer = asio::buffer(
        shared_buffer.data(), 
        shared_buffer.size());

    asio::async_read(
        m_tcp_socket,
        asio_buffer,
        m_tcp_socket_strand.wrap(
            [this, shared_buffer = std::move(shared_buffer), handler]
            (tcp_socket_key_t key, error_code_t const& e, std::size_t) mutable
            {
                if(e)
                    throw system_error(e);
                handler(std::move(key), std::move(shared_buffer));
            }));
}

template<typename Handler>
void client_t::tcp_read_message
( tcp_socket_key_t key
, Handler handler)
{
    // First read the header:
    using header_serialize = serialize<stc_tcp_header_t>;
    tcp_read(
        std::move(key),
        header_serialize::const_size,
        [this, handler](tcp_socket_key_t key, shared_buffer_t shared_buffer)
        {
            stc_tcp_header_t header;
            header_serialize::read(
                shared_buffer.begin(),
                shared_buffer.end(),
                header);

            // Then read the message body:
            tcp_read_message_body(
                std::move(key),
                std::move(header),
                handler);
        });
}

template<typename Handler>
void client_t::tcp_read_message_body
( tcp_socket_key_t key
, stc_tcp_header_t header
, Handler handler)
{
    tcp_read(
        std::move(key),
        header.payload_size,
        [this, header, handler]
        (tcp_socket_key_t key, shared_buffer_t shared_buffer)
        {
            stc_tcp_message_t message;
            
            using vs = variant_serializer<stc_tcp_message_t>;
            runtime_type_indexer<stc_tcp_message_t> indexer;
            indexer.operator()<vs::reader>(
                header.opcode,
                shared_buffer.begin(),
                shared_buffer.end(),
                &message);

            handler(std::move(key), std::move(message));
        });
}

template<typename Handler>
void client_t::udp_send
( udp_socket_key_t key
, shared_buffer_t shared_buffer
, Handler handler)
{
    std::printf("sending %lu\n", shared_buffer.size());

    auto asio_buffer = asio::buffer(shared_buffer.data(),
                                    shared_buffer.size());
    m_udp_socket.async_send_to(
        asio_buffer,
        m_udp_endpoint,
        [this, shared_buffer = std::move(shared_buffer), handler]
        (error_code_t const& e, std::size_t) mutable
        {
            if(e)
                throw system_error(e);
            handler();
        });
}

template<typename Handler>
void client_t::udp_send_message
( udp_socket_key_t key
, cts_udp_message_t message
, Handler handler)
{
    using serialize_t = serialize<cts_udp_message_t>;
    shared_buffer_t shared_buffer(serialize_t::size(message));
    serialize_t::write(message, shared_buffer.begin());
    udp_send(std::move(key), std::move(shared_buffer), handler);
}

/*
template<typename Handler>
void client::do_read_n(std::size_t n, Handler handler)
{
    shared_buffer_t buf(n);

    m_strand.dispatch(
        [this, handler, buf]() mutable
        {
            boost::asio::async_read(
                m_socket,
                boost::asio::buffer(
                    buf.data(),
                    buf.size()),
                m_strand.wrap(
                    [this, handler, buf]
                    (boost::system::error_code const& e, std::size_t)
                    {
                        if(!e)
                        {
                            handler(buf);
                        }
                        else
                        {
                            std::cout << "read error: " << e.message() << std::endl;
                        }
                    }));
        });
}

template<typename Handler>
void client::do_read_header(Handler handler)
{
    do_read_n(
        const_serialized_size<stc_header>::value,
        [this, handler](shared_buffer_t const buf)
        {
            stc_header header;
            buf.unserialize(header);
            handler(header);
        });
}

template<typename Handler>
void client::do_read_message(Handler handler)
{
    do_read_header(
        [this, handler](stc_header header)
        {
            // Read the payload
            do_read_n(
                header.length,
                [this, handler, header](shared_buffer_t const buf)
                {
                    handler(header, buf);
                });
        });
};
*/

/*
template<typename Handler>
void client::do_send_message(cts_message_t message, Handler handler)
{
    shared_buffer_t buf(serialized_size(message.header));
    write_serialized(message.header, buf.begin());
    do_send_buffer(buf, handler);
}
*/

#if 0
// TODO: use this shit
struct server::connection_t::get_bytes_t
{
    template<typename ReturnBytesFunc>
    void operator()(std::size_t size, ReturnBytesFunc return_bytes) const
    {
        /* TODO
        conn->async_read(
            key,
            conn,
            size,
            [return_bytes](strand_key key, conn_ptr conn, 
                           shared_buffer_t const buf)
            {
                return_bytes(buf.begin());
            });
            */
    }

    strand_key key;
    conn_ptr conn;
};

template<cts_opcode_t Opcode>
struct server::connection_t::read_cts_message
{
    using message_body_t = cts_message_body<Opcode>;

    void operator()() const
    {
        read_serialized<message_body_t>(
            get_bytes_t{ key, conn },
            [return_func=this->return_func](message_body_t message_body)
            {
                return_func({ Opcode, std::move(message_body) });
            });
    }

    strand_key key;
    conn_ptr conn;
    std::function<void(cts_message_t)> return_func;
};
#endif


#endif

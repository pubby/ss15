#ifndef SERVER_HPP
#define SERVER_HPP

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "buffer.hpp"
#include "game.hpp"
#include "net.hpp"
#include "pool.hpp"
#include "safe_strand.hpp"
#include "threadsafe_map.hpp"


namespace asio = boost::asio;
namespace posix_time = boost::posix_time;
namespace ip = boost::asio::ip;

using error_code_t = boost::system::error_code;
using system_error = boost::system::system_error;

constexpr std::size_t MAX_UDP_PAYLOAD = 1400;
using udp_buffer_t = std::array<char, MAX_UDP_PAYLOAD>;

class latest_received_sequence_t
{
public:
    latest_received_sequence_t(std::uint32_t sequence = 0)
    : latest(sequence)
    {}

    std::uint32_t update(std::uint32_t received_sequence)
    {
        std::unique_lock<std::mutex> lock(latest_mutex);
        std::uint32_t const prev_latest = latest;
        if(received_sequence > latest)
            latest = received_sequence;
        return prev_latest;
    }
private:
    std::atomic<std::uint32_t> latest;
    mutable std::mutex latest_mutex;
};

class server_t
{
private:
    // Allows connection_t to access certain server_t member functions
    // and prevents it from accessing others.
    class server_key;

    class connection_t;
    struct udp_receiver_t
    {
        ip::udp::endpoint endpoint;
        udp_buffer_t buffer;
    };

    static constexpr std::size_t udp_pool_size = 32;
    using shared_udp_receiver_t = shared_pooled_ptr<udp_receiver_t, 
                                                    udp_pool_size>;
    using udp_pool_t = shared_udp_receiver_t::pool_type;

    using address_map_t = threadsafe_map<ip::address, 
                                         std::weak_ptr<connection_t>>;

    using shared_connection_t = std::shared_ptr<connection_t>;

    struct udp_socket_tag {};
    using udp_socket_key_t = strand_key<udp_socket_tag>;
public:
    server_t(std::string address, std::string port, std::size_t num_threads);

    void run();

    asio::io_service const& io_service(server_key) const;
    asio::io_service& io_service(server_key);
private:
    shared_connection_t make_connection(ip::tcp::socket&& socket);
    shared_connection_t get_connection(ip::address const& address) const;

    void stop();
    void do_tcp_accept();
    void udp_receive(udp_socket_key_t key);

    template<typename Handler>
    void udp_send
    ( udp_socket_key_t key
    , ip::udp::endpoint endpoint
    , shared_buffer_t shared_buffer
    , Handler handler);

    template<typename Handler>
    void udp_send_message
    ( udp_socket_key_t key
    , ip::udp::endpoint endpoint
    , stc_udp_message_t message
    , Handler handler);
private:

    std::size_t m_num_threads;

    asio::io_service m_io_service;
    asio::signal_set m_terminate_signals;
    ip::tcp::acceptor m_tcp_acceptor;
    
    ip::tcp::socket m_new_tcp_connection_socket;
    ip::udp::socket m_udp_socket;
    safe_strand<udp_socket_tag> m_udp_socket_strand;
    std::unique_ptr<udp_pool_t> m_udp_pool;

    address_map_t m_address_map;

    game_state_t m_game_state;
};

template<typename Handler>
void server_t::udp_send
( udp_socket_key_t key
, ip::udp::endpoint endpoint
, shared_buffer_t shared_buffer
, Handler handler)
{
    auto asio_buffer = asio::buffer(shared_buffer.data(),
                                    shared_buffer.size());
    asio::async_write(
        m_udp_socket,
        asio_buffer,
        m_udp_socket_strand.wrap(
            [shared_buffer = std::move(shared_buffer), handler]
            (udp_socket_key_t key, error_code_t const& e, std::size_t) mutable
            {
                if(!e)
                    handler(std::move(key));
                else
                {
                    std::fprintf(stderr, "udp_send error: %s\n", 
                                 e.message().c_str());
                }
            }));
}

template<typename Handler>
void server_t::udp_send_message
( udp_socket_key_t key
, ip::udp::endpoint endpoint
, stc_udp_message_t message
, Handler handler)
{
    using message_serialize = serialize<stc_udp_message_t>;
    shared_buffer_t buffer(message_serialize::size(message));
    message_serialize::write(message, buffer.begin());
    udp_send(
        std::move(key),
        std::move(endpoint),
        std::move(buffer),
        handler);
}
        
struct server_t::server_key
{
friend class connection_t;
public:
    server_key(server_key const&) = default;
    server_key(server_key&&) = default;
    server_key& operator=(server_key const&) = default;
    server_key& operator=(server_key&&) = default;
private:
    server_key() = default;
};

class server_t::connection_t
{
public:
    connection_t(server_t& server, asio::ip::tcp::socket&& tcp_socket);
    connection_t(connection_t const&) = delete;
    connection_t(connection_t&&) = default;

    static void start(shared_connection_t shared_connection);
    bool stopped() const;

    //static void enqueue_game_message(conn_ptr conn, shared_buffer buffer);

    static void handle_udp_receive
    ( shared_connection_t shared_connection
    , shared_udp_receiver_t shared_receiver
    , std::size_t bytes_received);

private:
    struct tcp_socket_tag {};
    struct timer_tag {};
    using tcp_socket_key_t = strand_key<tcp_socket_tag>;
    using timer_key_t = strand_key<timer_tag>;

    connection_t
    ( server_t& server
    , asio::ip::tcp::socket&& socket
    , asio::io_service& io_service);

    static void stop(shared_connection_t shared_connection);

    ///////////////////////////////////
    // Error handling

    void report(char const* fmt, ...)
    {
        std::va_list args;
        va_start(args, fmt);
        std::vfprintf(stderr, fmt, args);
        va_end(args);
    }

    ///////////////////////////////////
    // General send/receive functions

    template<typename Handler>
    static void tcp_send
    ( tcp_socket_key_t key
    , shared_connection_t shared_connection
    , shared_buffer_t shared_buffer
    , Handler handler);

    template<typename Handler>
    static void tcp_send_message
    ( tcp_socket_key_t key
    , shared_connection_t shared_connection
    , stc_tcp_message_t message
    , Handler handler);

    template<typename Handler>
    static void tcp_read
    ( tcp_socket_key_t key
    , shared_connection_t shared_connection
    , std::size_t bytes_to_read
    , Handler handler);

    template<typename Handler>
    static void tcp_read_message
    ( tcp_socket_key_t key
    , shared_connection_t shared_connection
    , Handler handler);

    template<typename Handler>
    static void tcp_read_message_body
    ( tcp_socket_key_t key
    , shared_connection_t shared_connection
    , cts_tcp_header_t header
    , Handler handler);

    ///////////////////////////////////
    // Startup messages (in sequential order)

    static void tcp_send_server_info
    ( tcp_socket_key_t key
    , shared_connection_t shared_connection);

    static void tcp_read_login
    ( tcp_socket_key_t key
    , shared_connection_t shared_connection);

    static void tcp_send_game_state
    ( tcp_socket_key_t key
    , shared_connection_t shared_connection);

private:
    server_t& m_server;

    asio::ip::tcp::socket m_tcp_socket;
    safe_strand<tcp_socket_tag> m_tcp_socket_strand;

    latest_received_sequence_t latest_received_sequence;
};

template<typename Handler>
void server_t::connection_t::tcp_send
( tcp_socket_key_t key
, shared_connection_t shared_connection
, shared_buffer_t shared_buffer
, Handler handler)
{
    connection_t& connection = *shared_connection;
    auto asio_buffer = asio::buffer(shared_buffer.data(),
                                    shared_buffer.size());
    asio::async_write(
        connection.m_tcp_socket,
        asio_buffer,
        connection.m_tcp_socket_strand.wrap(
            [ shared_connection = std::move(shared_connection)
            , shared_buffer = std::move(shared_buffer)
            , handler]
            (tcp_socket_key_t key, error_code_t const& e, std::size_t) mutable
            {
                if(!e)
                    handler(std::move(key), std::move(shared_connection));
                else
                {
                    std::fprintf(stderr, "tcp_send error: %s\n", 
                                 e.message().c_str());
                }
            }));
}

template<typename Handler>
void server_t::connection_t::tcp_send_message
( tcp_socket_key_t key
, shared_connection_t shared_connection
, stc_tcp_message_t message
, Handler handler)
{
    using header_serialize = serialize<stc_tcp_header_t>;
    using message_serialize = serialize<stc_tcp_message_t, void>;

    stc_tcp_header_t header = 
    {
        message.which(),
        message_serialize::size(message),
    };

    shared_buffer_t shared_buffer(
        header_serialize::size(header)
        + message_serialize::size(message));

    auto it = header_serialize::write(header, shared_buffer.begin());
    it = message_serialize::write(message, it);

    tcp_send(
        std::move(key),
        std::move(shared_connection),
        std::move(shared_buffer),
        handler);
}

template<typename Handler>
void server_t::connection_t::tcp_read
( tcp_socket_key_t key
, shared_connection_t shared_connection
, std::size_t bytes
, Handler handler)
{
    connection_t& connection = *shared_connection;
    shared_buffer_t shared_buffer(bytes);
    auto asio_buffer = asio::buffer(shared_buffer.data(), 
                                    shared_buffer.size());
    asio::async_read(
        connection.m_tcp_socket,
        asio_buffer,
        connection.m_tcp_socket_strand.wrap(
            [ shared_connection = std::move(shared_connection)
            , shared_buffer = std::move(shared_buffer)
            , handler]
            (tcp_socket_key_t key, error_code_t const& e, std::size_t) mutable
            {
                if(!e)
                {
                    handler(
                        std::move(key),
                        std::move(shared_connection),
                        std::move(shared_buffer));
                }
                else
                {
                    std::fprintf(stderr, "tcp_read error: %s\n",
                                 e.message().c_str());
                }
            }));
}

template<typename Handler>
void server_t::connection_t::tcp_read_message
( tcp_socket_key_t key
, shared_connection_t shared_connection
, Handler handler)
{
    // First read the header:
    using header_serialize = serialize<cts_tcp_header_t>;
    tcp_read(
        std::move(key),
        std::move(shared_connection),
        header_serialize::const_size,
        [handler]
        (tcp_socket_key_t key
        , shared_connection_t shared_connection
        , shared_buffer_t shared_buffer)
        {
            cts_tcp_header_t header;
            header_serialize::read(
                shared_buffer.begin(),
                shared_buffer.end(),
                header);

            // Then read the message body:
            tcp_read_message_body(
                std::move(key),
                std::move(shared_connection),
                std::move(header),
                handler);
        });
}

template<typename Handler>
void server_t::connection_t::tcp_read_message_body
( tcp_socket_key_t key
, shared_connection_t shared_connection
, cts_tcp_header_t header
, Handler handler)
{
    tcp_read(
        std::move(key),
        std::move(shared_connection),
        header.payload_size,
        [header, handler]
        (tcp_socket_key_t key
        , shared_connection_t shared_connection
        , shared_buffer_t shared_buffer)
        {
            cts_tcp_message_t message;
            
            using vs = variant_serializer<cts_tcp_message_t>;
            runtime_type_indexer<cts_tcp_message_t> indexer;
            indexer.operator()<vs::reader>(
                header.opcode,
                shared_buffer.begin(),
                shared_buffer.end(),
                &message);

            handler(
                std::move(key),
                std::move(shared_connection),
                std::move(message));
        });
}

#endif

#include "server.hpp"

server_t::server_t(std::string address, std::string port, 
                   std::size_t num_threads)
: m_num_threads(num_threads)
, m_io_service(num_threads)
, m_terminate_signals(m_io_service)
, m_tcp_acceptor(m_io_service)
, m_new_tcp_connection_socket(m_io_service)
, m_udp_socket(m_io_service, 
               ip::udp::endpoint(ip::udp::v6(), std::stoi(port)))
, m_udp_socket_strand(m_io_service)
, m_udp_pool(new udp_pool_t())
, m_game_state(dimen_t{ 100, 100 })
{
    m_terminate_signals.add(SIGINT);
    m_terminate_signals.add(SIGTERM);
    #ifdef SIGQUIT
        m_terminate_signals.add(SIGQUIT);
    #endif
    m_terminate_signals.async_wait(std::bind(&server_t::stop, this));

    ip::tcp::resolver tcp_resolver(m_io_service);
    ip::tcp::resolver::query tcp_query(address, port);
    ip::tcp::endpoint tcp_endpoint = *tcp_resolver.resolve(tcp_query);
    m_tcp_acceptor.open(tcp_endpoint.protocol());
    // Know SO_REUSEADDR to understand this line:
    m_tcp_acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
    m_tcp_acceptor.bind(tcp_endpoint);
    m_tcp_acceptor.listen();

    do_tcp_accept();

    using namespace std::placeholders;
    m_udp_socket_strand.post(std::bind(&server_t::udp_receive, this, _1));
}

asio::io_service const& server_t::io_service(server_key) const
{
    return m_io_service;
}

asio::io_service& server_t::io_service(server_key)
{
    return m_io_service;
}

void server_t::stop()
{
    m_io_service.stop();
}

void server_t::run()
{
    if(m_num_threads <= 0)
        throw std::logic_error("running server on zero threads");

    // Create a pool of threads to run all of the io_services.
    std::vector<std::thread> threads;
    threads.reserve(m_num_threads);
    for(std::size_t i = 0; i < m_num_threads; ++i)
        threads.emplace_back([this](){ m_io_service.run(); });

    // Wait for all threads in the pool to finish.
    for(std::size_t i = 0; i < threads.size(); ++i)
        threads[i].join();
}

// This function should be called strictly sequentially.
// Concurrent accept is NOT implemented.
void server_t::do_tcp_accept()
{  
    m_tcp_acceptor.async_accept(
        m_new_tcp_connection_socket,
        [this](error_code_t const& error)
        {
            if(!error)
            {
                std::printf("new connection\n");
                connection_t::start(
                    make_connection(std::move(m_new_tcp_connection_socket)));
            }
            else
            {
                // TODO
            }
            do_tcp_accept();
        });
}

void server_t::udp_receive(udp_socket_key_t key)
{
    shared_udp_receiver_t shared_receiver =
        make_shared_from_pool(*m_udp_pool);
    udp_receiver_t& receiver = *shared_receiver;

    m_udp_socket.async_receive_from(
        asio::buffer(receiver.buffer),
        receiver.endpoint,
        [shared_receiver = std::move(shared_receiver), this]
        (error_code_t const& error, std::size_t bytes_received) mutable
        {
            using namespace std::placeholders;
            m_udp_socket_strand.post(
                std::bind(&server_t::udp_receive, this, _1));

            if(!error)
            {
                udp_receiver_t& receiver = *shared_receiver;

                // Check if we're connected to this endpoint.
                shared_connection_t shared_connection
                    = get_connection(receiver.endpoint.address());
                if(!shared_connection)
                    return; // Not connected; discard the packet.

                // A connection exists, so have the connection handle it.
                connection_t::handle_udp_receive(
                    std::move(shared_connection),
                    std::move(shared_receiver),
                    bytes_received);
            }
            else
            {
                // TODO
            }
        });
}

auto server_t::get_connection(ip::address const& address) const
-> shared_connection_t 
{
    return m_address_map.with_container_const(
        [&address](auto const& map)
        {
            auto it = map.find(address);
            return (it == map.cend() 
                    ? std::shared_ptr<connection_t>() 
                    : it->second.lock());
        });
}


// This should be the only way to construct new connection_ts.
// It inserts additional code for both construction and destruction.
auto server_t::make_connection(ip::tcp::socket&& socket) 
-> shared_connection_t 
{
    ip::address address = socket.remote_endpoint().address();

    shared_connection_t shared_connection(
        new connection_t(*this, std::move(socket)),
        [this](connection_t* ptr)
        {
            // dtor stuff that depends on server can go here
            std::printf("connection_t killed\n");
            delete ptr;
        });

    // Add the connection to the address map.
    m_address_map.insert_or_assign(address, shared_connection);

    assert(shared_connection);
    return shared_connection;
}

///////////////////////////////////////////////////////////////////////////////
// server_t::connection_t

// Should only be called by "make_connection_t"!!!
server_t::connection_t::connection_t
( server_t& server
, asio::ip::tcp::socket&& tcp_socket)
: connection_t(server, std::move(tcp_socket), server.io_service(server_key()))
{
     // Turn off nagle.
    m_tcp_socket.set_option(ip::tcp::no_delay(true));
}

server_t::connection_t::connection_t
( server_t& server
, asio::ip::tcp::socket&& tcp_socket
, asio::io_service& io_service)
: m_server(server)
, m_tcp_socket(std::move(tcp_socket))
, m_tcp_socket_strand(io_service)
{
    assert(&server.io_service(server_key()) == &io_service);
    assert(&m_tcp_socket.get_io_service() == &io_service);
}

void server_t::connection_t::start(shared_connection_t shared_connection)
{
    connection_t& connection = *shared_connection;

    connection.m_tcp_socket_strand.dispatch(
        [shared_connection](tcp_socket_key_t key)
        {
            connection_t& connection = *shared_connection;

            std::printf("started\n");

            tcp_send_server_info(
                std::move(key),
                std::move(shared_connection));
        });
}

void server_t::connection_t::stop(shared_connection_t shared_connection)
{
    connection_t& connection = *shared_connection;

    connection.m_tcp_socket_strand.dispatch(
        [shared_connection](tcp_socket_key_t key)
        {
            connection_t& connection = *shared_connection;
            boost::system::error_code e;
            connection.m_tcp_socket.close(e);
            if(e)
            {
                // TODO
            }
        });
}

void server_t::connection_t::handle_udp_receive
( shared_connection_t shared_connection
, shared_udp_receiver_t shared_receiver
, std::size_t bytes_received)
{
    connection_t& connection = *shared_connection;
    udp_receiver_t& receiver = *shared_receiver;

    try
    {
        std::printf("udp size: %lu\n", bytes_received);

        auto it = receiver.buffer.cbegin();
        auto const end = receiver.buffer.cend() + bytes_received;

        cts_udp_header_t header;
        it = serialize<cts_udp_header_t>::read(it, end, header);

        if(connection.latest_received_sequence
           .update(header.sequence_number) > header.sequence_number)
        {
            // Discard the packet if we've seen a more recent one.
            return;
        }

        cts_udp_message_body_t body;
        it = serialize<cts_udp_message_body_t>::read(it, end, body);

        std::printf("received udp\n");

        // TODO: actually read shit.
        // Then push shit to a queue or something
        // I dunno
    }
    catch(std::exception& e)
    {
        // TODO
        std::printf("udp receive error: %s\n", e.what());
        //stop();
    }
}

void server_t::connection_t::tcp_send_server_info
( tcp_socket_key_t key
, shared_connection_t shared_connection)
{
    tcp_send_message(
        std::move(key),
        std::move(shared_connection),
        stc_tcp_server_info_t
        {
            version_t::this_version(),
        },
        tcp_read_login);
}

void server_t::connection_t::tcp_read_login
( tcp_socket_key_t key
, shared_connection_t shared_connection)
{
    tcp_read_message(
        std::move(key),
        std::move(shared_connection),
        []
        ( tcp_socket_key_t key
        , shared_connection_t shared_connection
        , cts_tcp_message_t message)
        {
            if(auto* login_details = message.target<cts_tcp_login_t>())
            {
                tcp_send_game_state(
                    std::move(key),
                    std::move(shared_connection));
            }
            else
            {
                std::printf("bad login message");
            }
        });
}

void server_t::connection_t::tcp_send_game_state
( tcp_socket_key_t key
, shared_connection_t shared_connection)
{
    connection_t& connection = *shared_connection;
    tcp_send_message(
        std::move(key),
        std::move(shared_connection),
        stc_tcp_game_state_t
        {
            connection.m_server.m_game_state.serialized()
        },
        tcp_read_login);
}

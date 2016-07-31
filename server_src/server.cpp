#include "server.hpp"

server_t::server_t
( asio::io_service& io_service
, std::string address
, std::string port
, std::size_t num_threads
)
: m_num_threads(num_threads)
, m_io_service(io_service)
, m_terminate_signals(m_io_service)
, m_tcp_acceptor(m_io_service)
, m_new_tcp_connection_socket(m_io_service)
, m_udp_socket(m_io_service, 
               ip::udp::endpoint(ip::udp::v6(), std::stoi(port)))
, m_udp_socket_strand(m_io_service)
, m_udp_pool(new udp_pool_t())
, m_tick_timer(io_service)
{
    m_port = m_udp_socket.endpoint().port();

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

std::deque<std::thread> server_t::run()
{
    if(m_num_threads <= 0)
        throw std::logic_error("running server on zero threads");

    // Create a pool of threads to run all of the io_services.
    std::deque<std::thread> threads;
    for(std::size_t i = 0; i < m_num_threads; ++i)
        threads.emplace_back([this](){ m_io_service.run(); });

    return threads;
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

void server_t::launch_tick()
{
    m_tick_timer.expires_from_now(posix_time::seconds(1));
    m_tick_timer.async_wait(
        [this](error_code_t const& error)
        {
            handle_tick();
            launch_tick();
        });
}

void server_t::handle_tick()
{
    if(!m_game_state)
        return;
    game_state_t& game_state = *m_game_state;

    // Find the most recently received input and ignore the rest.
    // (UDP packets may be out-of-order, missing, etc)

    struct received_input_t
    {
        std::uint64_t sequence_number;
        cts_input_t input;
    };

    std::vector<cts_udp_received_t> udp_received = m_udp_received.flush();
    std::unordered_map<player_id_t, received_input_t> received_input_map;

    for(auto& received : udp_received)
    {
        auto& most_recent = received_input_map[received.player_id];
        if(most_recent.sequence_number 
           < received.message.header.sequence_number)
        {
            most_recent.sequence_number 
                = received.message.header.sequence_number;
            most_recent.input = received.message.body.input;
        }
    }

    // player_ids will be iterated in a random order. Prepare for that now.

    std::vector<player_id_t> randomized_player_ids;
    randomized_player_ids.reserve(game_state.player_map.size());
    for(auto const& pair : game_state.player_map)
        randomized_player_ids.push_back(pair.first);
    std::shuffle(randomized_player_ids.begin(), 
                 randomized_player_ids.end(), m_rng);

    // Push the received messages onto the Lua stack.

    lua_State* L = game_state.L.get();
    std::size_t Li = 1;
    lua_getglobal(L, "tickMain");
    lua_rawseti (L, -2, 1);

    for(player_id_t player_id : randomized_player_ids)
    {
        lua_createtable(L, 0, 8);
        lua_pushnumber(L, player_id);
        lua_setfield(L, -2, "playerId");
        lua_pushstring(L, "move");
        lua_setfield(L, -2, "input");
        lua_rawseti(L, -2, Li++);
    }

    // Call into Lua and run the tickMain game code.

    if(lua_pcall(L, 1, 0, 0))
    {
        printf("%s\n", lua_tostring(L, -1));
        // TODO
    }

    // Add the tick updates to history.

    m_frame_history.pop_back();
    for(frame_t& past_frame : m_frame_history)
    {
        for(auto&& pair : game_state.frame.updated)
            past_frame.updated[pair.first] = pair.second;

        for(int id : game_state.frame.destroyed)
            past_frame.updated.erase(id);
        past_frame.destroyed.insert(game_state.frame.destroyed.begin(),
                                    game_state.frame.destroyed.end());
    }

    game_state.frame.updated.clear();
    game_state.frame.destroyed.clear();

    // Create outgoing messages based on the history.

    std::deque<shared_buffer_t> update_buffers;
    for(frame_t const& frame : m_frame_history)
    {
        std::deque<update_t> updates;

        for(auto const& pair : frame.updated)
        {
            player_id_t const object_id = pair.first;
            object_t const& object_past = pair.second;

            auto it = game_state.object_map.find(object_id);
            assert(it != game_state.object_map.end());
            object_t const& object = it->second->object;

            if(object.position != object_past.position)
            {
                updates.push_back(
                    update_object_position_t
                    {
                        object_id,
                        object.position,
                    });
            }
        }

        for(object_id_t object_id : frame.destroyed)
            updates.push_back(update_destroy_object_t{ object_id });

        using serialize_t = serialize<std::deque<update_t>>;
        shared_buffer_t shared_buffer(serialize_t::size(updates));
        serialize_t::write(updates, shared_buffer.begin());
        
        update_buffers.push_back(std::move(shared_buffer));
    }

    // Send the messages.

    for(auto const& pair : m_address_map.container())
    {
        shared_connection_t connection = pair.second.lock(); 
        if(!connection)
            continue;

        aut_t const delta_time = game_state.time - last_received_time;

        constexpr aut_t delta_time_max = 16; // TODO
        if(delta_time > delta_time_max)
            throw 0; // TODO

        auto it = buffers.find(delta_time);
        if(it == buffers.end())
        {
            // TODO
        }

        std::vector<char>& buffer = it->second;

        stc_udp_header_t header;
        header.time = game_state.time;
        header.delta_time = delta_time;
        header.last_received_sequence = todo;

        serialize<stc_udp_header_t>::write(header, buffer.begin());

        udp::endpoint endpoint(pair.first, m_port);
        connection.m_udp_socket.send_to(
            asio::buffer(buffer.data(), buffer.size()),
            endpoint);


    }
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

        /*
        if(connection.latest_received_sequence
           .update(header.sequence_number) > header.sequence_number)
        {
            // Discard the packet if we've seen a more recent one.
            return;
        }
        */

        cts_udp_message_body_t body;
        it = serialize<cts_udp_message_body_t>::read(it, end, body);

        std::printf("received udp\n");

        connection.m_server.m_udp_received.emplace_back(cts_udp_received_t
        {
            0, // TODO
            { header, body }
        });

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
            //connection.m_server.m_game_state.serialized()
        },
        tcp_read_login);
}

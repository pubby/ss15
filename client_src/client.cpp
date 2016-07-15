#include "client.hpp"

client_t::client_t
( asio::io_service& io_service
, std::string const& address
, std::string const& port)
: m_io_service(io_service)
, m_tcp_socket(io_service)
, m_tcp_socket_strand(io_service)
, m_udp_socket(io_service)
, m_udp_socket_strand(io_service)
, m_udp_pool(new udp_pool_t())
{
    m_tcp_socket.open(ip::tcp::v6());
    ip::tcp::resolver tcp_resolver(m_io_service);
    asio::connect(m_tcp_socket, tcp_resolver.resolve({ address, port }));
    m_tcp_socket.set_option(ip::tcp::no_delay(true)); // Disable nagle.

    ip::udp::resolver udp_resolver(m_io_service);
    m_udp_endpoint = *udp_resolver.resolve({ address, port });
    m_udp_socket.open(ip::udp::v6());

    using namespace std::placeholders;
    m_tcp_socket_strand.post(
        std::bind(&client_t::tcp_read_server_info, this, _1));

                             /*
    cts_tcp_message_t message = cts_tcp_login_t
    {
        "buppy",
    };
    m_tcp_socket_strand.post(
        [this, message](tcp_socket_key_t key)
        {
            tcp_read_message(
                std::move(key),
                [](tcp_socket_key_t, stc_tcp_message_t message)
                {
                });
        });
        */

    /*
    m_tcp_socket_strand.post(
        [this](tcp_socket_key_t key)
        {
            tcp_read_message(
                std::move(key),
                [](tcp_socket_key_t key, stc_tcp_message_t message)
                {
                });

        });
        */
}

void client_t::run()
{
    m_io_service.run();
}

void client_t::udp_receive(udp_socket_key_t key)
{
    shared_udp_receiver_t shared_receiver
        = make_shared_from_pool(*m_udp_pool);
    udp_receiver_t& receiver = *shared_receiver;

    m_udp_socket.async_receive_from(
        asio::buffer(receiver.buffer),
        receiver.endpoint,
        [shared_receiver = std::move(shared_receiver), this]
        (error_code_t const& error, std::size_t bytes_received) mutable
        {
            if(error)
                throw system_error(error);

            using namespace std::placeholders;
            m_udp_socket_strand.post(
                std::bind(&client_t::udp_receive, this, _1));

            // Check if we're connected to this endpoint.
            if(shared_receiver->endpoint != m_udp_endpoint)
                return; // Not connected; discard the packet.

            handle_udp_receive(std::move(shared_receiver), bytes_received);
        });
}

void client_t::handle_udp_receive
( shared_udp_receiver_t shared_receiver
, std::size_t bytes_received)
{
    udp_receiver_t& receiver = *shared_receiver;
    auto it = receiver.buffer.cbegin();
    auto const end = receiver.buffer.cend() + bytes_received;

    stc_udp_header_t header;
    it = serialize<stc_udp_header_t>::read(it, end, header);

    // Check for duplicate packets. This can return false negatives.
    if(update_queue.has(header.time))
        return;

    stc_udp_message_body_t body;
    it = serialize<stc_udp_message_body_t>::read(it, end, body);
}

void client_t::attempt_join()
{
    /*
    cts_message_t message = make_cts_message(
        cts_message_body<cts_opcode_logon>{});

    do_send_message(
        message,
        [this]()
        {
            std::fprintf(stderr, "sent message\n");
            attempt_join();
            //do_read_join_response();
        });
        */
}

void client_t::tcp_read_server_info(tcp_socket_key_t key)
{
    tcp_read_message(
        std::move(key),
        [this](tcp_socket_key_t key, stc_tcp_message_t message)
        {
            if(auto* server_info = message.target<stc_tcp_server_info_t>())
            {
                if(server_info->version.correct())
                    tcp_send_login(std::move(key));
                else
                    report("Client/server version mismatch.");
            }
            else
                report("Bad message. Expected server info.");
        });
}

void client_t::tcp_send_login(tcp_socket_key_t key)
{
    using namespace std::placeholders;
    tcp_send_message(
        std::move(key),
        cts_tcp_login_t
        {
            "buppy"
        },
        std::bind(&client_t::tcp_read_game_state, this, _1));
}

void client_t::tcp_read_game_state(tcp_socket_key_t key)
{
    std::printf("waiting for game state\n");
    tcp_read_message(
        std::move(key),
        [this](tcp_socket_key_t key, stc_tcp_message_t message)
        {
            if(auto* payload = message.target<stc_tcp_game_state_t>())
            {
                game_state_t game_state(payload->game_state);

                // TODO
                //game_state_promise.set_value(*game_state);
                std::printf("read game state! %i\n", game_state.dimensions().w);
            }
            else
                report("Bad message. Expected game state.");
        });
}

void client_t::udp_read_updates(udp_socket_key_t key)
{
}

void client_t::send_input(cts_input_t input)
{
    cts_udp_message_t message;
    message.header.sequence_number = m_sequence_number.fetch_add(1);
    message.header.last_received_time = 0;
    message.body.input = input;

    using namespace std::placeholders;
    m_udp_socket_strand.post(
        [this, message](udp_socket_key_t key)
        {
            udp_send_message(std::move(key), message, [](){});
        });
}

#ifndef NET_HPP
#define NET_HPP

#include <eggs/variant.hpp>

#include "serialize.hpp"

using cts_tcp_message_t = eggs::variant
< struct cts_tcp_login_t
>;

using stc_tcp_message_t = eggs::variant
< struct stc_tcp_server_info_t
, struct stc_tcp_game_state_t
>;

struct version_t
{
    // This value should be changed for different forks of the game.
    static std::uint32_t const correct_magic_number = 0xDEADBEEF;
    // This value should be incremented as the netcode protocol gets updated
    // with breaking changes.
    static std::uint32_t const correct_protocol_version = 1;

    SERIALIZED_DATA
    (
        ((std::uint32_t) (magic_number)     ())
        ((std::uint32_t) (protocol_version) ())
    )

    bool correct() const
    {
        return (magic_number == correct_magic_number
                && protocol_version == correct_protocol_version);
    }

    static constexpr version_t this_version()
    {
        return { correct_magic_number, correct_protocol_version };
    }
};

///////////////////////////////////////
// cts_tcp

struct cts_tcp_header_t
{
    SERIALIZED_DATA
    (
        ((std::size_t) (opcode)       (std::uint8_t))
        ((std::size_t) (payload_size) (std::uint32_t))
    )
};

struct cts_tcp_login_t
{
    SERIALIZED_DATA
    (
        ((std::string) (username) ())
    )
};

///////////////////////////////////////
// stc_tcp

struct stc_tcp_header_t
{
    SERIALIZED_DATA
    (
        ((std::size_t) (opcode)       (std::uint8_t))
        ((std::size_t) (payload_size) (std::uint32_t))
    )
};

struct stc_tcp_server_info_t
{
    SERIALIZED_DATA
    (
        ((version_t) (version) ())
    )
};

struct stc_tcp_game_state_t
{
    SERIALIZED_DATA
    (
        //((game_state_t::serialized_t) (game_state) ())
    )
};

///////////////////////////////////////
// udp

enum cts_input_t : std::uint8_t
{
    CTS_INPUT_NONE,
    CTS_INPUT_UP,
    CTS_INPUT_DOWN,
    CTS_INPUT_LEFT,
    CTS_INPUT_RIGHT,
};

struct cts_udp_header_t
{
    SERIALIZED_DATA
    (
        ((std::uint64_t) (sequence_number)    (std::uint16_t))
        ((std::uint64_t) (last_received_time) (std::uint16_t))
    )
};

struct cts_udp_message_body_t
{
    SERIALIZED_DATA
    (
        ((cts_input_t) (input) ())
    )
};

struct cts_udp_message_t
{
    SERIALIZED_DATA
    (
        ((cts_udp_header_t)       (header) ())
        ((cts_udp_message_body_t) (body)   ())
    )
};

struct cts_udp_received_t
{
    int player_id;
    cts_udp_message_t message;
};

struct stc_udp_header_t
{
    SERIALIZED_DATA
    (
        ((std::uint16_t) (time)       ())
        ((std::uint8_t)  (delta_time) ())
        ((std::uint16_t) (last_received_sequence) ())
    )
};

struct stc_udp_message_body_t
{
    SERIALIZED_DATA
    (
        ((std::uint16_t) (foo) ())
    )
};

struct stc_udp_message_t
{
    SERIALIZED_DATA
    (
        ((stc_udp_header_t) (header)     ())
        ((stc_udp_message_body_t) (body) ())
    )
};

/*
read_serialized(std::deque<update_t> updates)
{
}

std::size_t serialized_size(std::deque<update_t> const& updates)
{
    std::size_t size = sizeof(std::uint16_t);
    for(update_t const& update : updates)
        size += serialized_size(update);
    return update;
}
*/


#if 0
struct cts_udp_opcode_t
{
    CTS_UDP_OPCODE(nop)
    CTS_UDP_OPCODE(move_up)
    CTS_UDP_OPCODE(move_down)
    CTS_UDP_OPCODE(move_left)
    CTS_UDP_OPCODE(move_right)
};

std::uint32_t adjust_sequence(std::uint32_t new_sequence16, 
                              std::uint32_t last_sequence)
{
    std::uint32_t const last_sequence16 = last_sequence % (1<<16);
    if(new_sequence16 > last_sequence16)
    {
        if(new_sequence16 - last_sequence16 < 1<<15)
            return last_sequence + new_sequence16 - last_sequence16;
        else
            return last_sequence + new_sequence16 - last_sequence16 - 1<<16;
    }
    else
    {
        if(last_sequence16 - new_sequence < 1<<15)
            return last_sequence + new_sequence16 - last_sequence16;
        else
            return last_sequence + new_sequence16 - last_sequence16 + 1<<16;
    }
}

void server_read_udp(shared_buffer_t buffer)
{
    cts_udp_header_t header;
    auto it = read_serialized_from_buffer(buffer.cbegin(), buffer.cend(), 
                                          header);

    std::uint32_t const adjusted_sequence
        = adjust_sequence(header.sequence_number, last_received_sequence);

    if(last_received_sequence < adjusted_sequence)
    {
        std::uint32_t const delta_sequence 
            = adjusted_sequence - last_received_sequence;

        for(std::uint32_t i = 0; i < delta_sequence; ++i)
        {
            cts_udp_message_t message;
            it = read_serialized_from_buffer(it, buffer.cend(), message);

            map[adjusted_sequence - i] = message;

            if(it == buffer.cend())
                break;
        }

        last_received_sequence = adjusted_sequence;
    }
}







//flat_map<std::uint8_t, shared_buffer>;

struct foo
{
    void receive_packet(shared_buffer_t buffer)
    {
        header = TODO;

        auto it = incomplete_map.emplace(header.game_time).first;
        incomplete_message_t& incomplete_message = *it;
        incomplete_message.packet_map.emplace(header.packet_index, 
                                              std::move(buffer));
        if(header.packet_index == 0)
        {
            packet0_header = TODO;
            incomplete_message.expected_packets = packet0_header.num_packets;
        }

        if(incomplete_message.packet_map.size()
           == incomplete_message.expected_packets)
        {
            incomplete_map.erase(incomplete_map.begin(), it);
        }
    }

    struct incomplete_message_t
    {
        int expected_packets;
        flat_map<std::uint8_t, shared_buffer_t> packet_map;
    };

    aut_t most_recent_game_time;
    std::map<aut_t, incomplete_message_t> incomplete_map;

    int most_recent_expected;
    int prev_recent_expected;
};
#endif
#endif

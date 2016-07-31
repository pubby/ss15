#include <cstdlib>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <boost/container/flat_map.hpp>

#include "server.hpp"
#include "pool.hpp"

using boost::container::flat_map;

/*
std::vector<cts_udp_message_t> udp_received = server.udp_received.flush();
for(auto it = message.rbegin(); it != messages.rend(); ++it)
{
    prev.erase(it->player_id);

}

std::vector<int> player_order;
for(cts_message_t const& message : udp_received)
{
    auto it = foo.find(message.player_id);
    if(it == foo.end())
    {
        player_order.push_back(message.player_id);
        it = foo.emplace(message.player_id);
    }

    it->push_back(message);

    foo& = map[message.player_id];
    foo.reset();
}
*/


/*

class runner
{
public:

    void tick()
    {
        m_tick_timer.expires_from_now(posix_time::seconds(1));
        m_tick_timer.async_wait(
            [this](error_t const& error)
            {
                handle_tick();
                tick();
            });
    }

    void handle_tick()
    {
        // Find the most recently received messages and ignore the rest.
        // (UDP packets may be out-of-order, missing, etc)

        std::vector<cts_udp_received_t> udp_received 
            = m_server.udp_received.flush();

        for(auto& received : udp_received)
        {
            auto& most_recent = received_map[received.player_id];
            if(most_recent.header.sequence_number 
               < received.header.sequence_number)
            {
                most_recent = std::move(received);
            }
        }

        std::vector<int> randomized_ids;
        std::shuffle(randomized_ids.begin(), randomized_ids.end(), rng);

        // Push the received messages onto the Lua stack.

        std::size_t Li = 1;
        lua_getglobal(L(), "tickMain");
        lua_rawseti (L(), -2, 1);

        for(auto player_id : randomized_ids)
        {
            lua_createtable(L(), 0, 8);
            lua_pushnumber(L(), player_id);
            lua_setfield(L(), -2, "playerId");
            lua_pushstring(L(), "move");
            lua_setfield(L(), -2, "input");
            lua_rawseti(L(), -2, Li++);
        }

        // Call into Lua and run the tickMain game code.

        if(lua_pcall(L, 1, 0, 0))
        {
            printf("%s\n", lua_tostring(L, -1));
        }

        // Add the tick updates to history.

        m_history.pop_back();
        for(frame_t& past_frame : m_history)
        {
            for(auto&& pair : new_frame.updated)
                past_frame.updated[pair.first] = pair.second;

            for(int id : new_frame.destroyed)
                past_frame.updated.erase(id);
            past_frame.destroyed.insert(new_frame.destroyed.begin(),
                                        new_frame.destroyed.end());
        }

        new_frame.updated.clear();
        new_frame.destroyed.clear();

        // Create outgoing messages based on the history.

        for(frame_t const& frame : m_history)
        {
        }

        // Send the messages.
    }

private:
    lua_State const* L() const { return m_lua_state.get(); }
    lua_State* L() { return m_lua_state.get(); }

    asio::deadline_timer m_tick_timer;
    free_list_pool<player_bk_t> player_pool;
    free_list_pool<object_bk_t> object_pool;
    std::deque<frame_t> m_history;
    std::unique_ptr<lua_State, lua_closer> m_lua_state;
}

*/

int main(int argc, char* argv[])
{
    if(argc != 4)
    {
        std::fprintf(stderr, "usage: %s <address> <port> <threads>\n",
                     argc ? argv[0] : "server");
        return EXIT_FAILURE;
    }

    try
    {
        asio::io_service io_service;
        std::size_t num_threads = std::stoi(argv[3]);
        server_t server(io_service, argv[1], argv[2], num_threads);
        std::deque<std::thread> threads = server.run();

        

        // Wait for all threads to finish.
        for(std::thread& thread : threads)
            thread.join();
    }
    catch(std::exception& e)
    {
        std::cerr << "exception: " << e.what() << "\n";
    }

    return EXIT_SUCCESS;
}


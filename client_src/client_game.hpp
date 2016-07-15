#ifndef CLIENT_GAME_HPP
#define CLIENT_GAME_HPP

class client_game_t
{
public:

    // Thread-safe. Can be called by multiple threads concurrently.
    template<typename It>
    void enqueue_updates(It begin, It end)
    {
        std::unique_lock<std::mutex> lock(m_update_queue_mutex);
        for(It it = begin; it != end; ++it)
            m_update_queue.push_back(*it);
    }

    // Not thread-safe.
    void dequeue_updates()
    {
        std::deque<update_t> updates;
        {
            std::unique_lock<std::mutex> lock(m_update_queue_mutex);
            updates.swap(m_update_queue);
        }

        for(update_t const& update : updates)
        {
            eggs::variants::apply(
                [&](auto update) { m_game_state.apply_update(update); },
                m_game_state);
        }
    }

private:
    game_state_t m_game_state;

    std::deque<update_t> m_update_queue;
    mutable std::mutex m_update_queue_mutex;
};

#endif

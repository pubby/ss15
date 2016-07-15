#ifndef GAME_HPP
#define GAME_HPP

#include <deque>
#include <exception>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>

#include <eggs/variant.hpp>

#include <int2d/units.hpp>
#include <int2d/geometry.hpp>
#include <int2d/grid.hpp>

#include "serialize.hpp"

using namespace int2d;
using namespace boost::container;

using player_id_t = std::uint16_t;
using object_id_t = std::uint32_t;
using aut_t = std::uint32_t;

using action_t = eggs::variant
< struct action_move_up_t
, struct action_move_down_t
, struct action_move_left_t
, struct action_move_right_t
>;

using update_t = eggs::variant
< struct update_create_object_t
, struct update_destroy_object_t
, struct update_object_position_t
, struct update_create_player_t
>;

////////////////////////////////////////
// Actions

struct action_move_up_t {};
struct action_move_down_t {};
struct action_move_left_t {};
struct action_move_right_t {};

////////////////////////////////////////
// Updates

struct update_create_object_t
{
    SERIALIZED_DATA
    (
        ((object_id_t) (object_id) ())
        ((coord_t)     (position)  ())
    )
};

struct update_destroy_object_t
{
    SERIALIZED_DATA
    (
        ((object_id_t) (object_id) ())
    )
};

struct update_object_position_t
{
    SERIALIZED_DATA
    (
        ((object_id_t) (object_id) ())
        ((coord_t)     (position)  ())
    )
};

struct update_create_player_t
{
    SERIALIZED_DATA
    (
        ((player_id_t) (player_id) ())
        ((object_id_t) (object_id) ())
    )
};


struct diff_t
{
    SERIALIZED_DATA
    (
        ((aut_t)                (update_from) ())
        ((std::deque<update_t>) (updates)     ())
    )
};

////////////////////////////////////////

////////////////////////////////////////
// Objects

class player_t;

class object_t
{
friend class game_state_t;
public:
    object_t() = delete;
    object_t
    ( object_id_t id
    , coord_t position)
    : m_id(id)
    , m_player(nullptr)
    , m_position(position)
    {}

    object_id_t id() const { return m_id; }
    player_t* player() const { return m_player; }
    player_id_t player_id() const;
    coord_t position() const { return m_position; }

    update_create_object_t serialized() const
    {
        return { id(), position() };
    }

private:
    object_id_t m_id;
    player_t* m_player;
    coord_t m_position;
};

class player_t
{
friend class game_state_t;
public:
    player_t(player_id_t id)
    : m_id(id)
    , m_object(nullptr)
    {}
    
    player_id_t id() const { return m_id; }
    object_t* object() const { return m_object; }
    object_id_t object_id() const { return m_object ? m_object->id() : 0; }

    update_create_player_t serialized() const
    {
        return { id(), object_id() };
    }

private:
    player_id_t m_id;
    object_t* m_object;
};

inline player_id_t object_t::player_id() const
{
    return m_player ? m_player->id() : 0;
}

template<typename T>
class id_allocator
{
public:
    id_allocator() : next_id(1) {}
    T new_id() { return next_id++; }
private:
    T next_id;
};

class game_state_t
{
public:
    struct serialized_t;

    game_state_t() = delete;
    explicit game_state_t(dimen_t dimen) : m_object_grid(dimen) {}
    game_state_t(game_state_t const&);
    game_state_t(serialized_t const& serialized);

    dimen_t dimensions() const { return m_object_grid.dimensions(); }

    player_t const* get_player_ptr(player_id_t id) const;
    player_t* get_player_ptr(player_id_t id);

    object_t const* get_object_ptr(object_id_t id) const;
    object_t* get_object_ptr(object_id_t id);

    player_t const& get_player(player_id_t id) const;
    player_t& get_player(player_id_t id);

    object_t const& get_object(object_id_t id) const;
    object_t& get_object(object_id_t id);

    player_t& add_player(player_t player);
    void remove_player(player_id_t player_id);

    object_t& add_object(object_t object);
    void remove_object(object_id_t object_id);
    void move_object(object_t& object, coord_t to);
    void set_player_object(player_t& player, object_t* object);
    flat_set<object_id_t> const& objects_at(coord_t coord) const;

    std::deque<update_t> diff(game_state_t const& prev) const;

    void apply_update(update_create_object_t update);
    void apply_update(update_destroy_object_t update);
    void apply_update(update_object_position_t update);
    void apply_update(update_create_player_t update);

    bool do_action(player_t& player, action_move_up_t action);

    game_state_t const* const_this() const { return this; }
private:
    struct object_bk_t
    {
        object_t object;
        mutable bool visited;
    };

    std::map<player_id_t, player_t> m_player_map;
    grid<flat_set<object_id_t>> m_object_grid;
    std::map<object_id_t, object_bk_t> m_object_map;

public:
    struct serialized_t
    {
        SERIALIZED_DATA
        (
            ((dimen_t) (dimen) (std::uint8_t))
            ((std::vector<update_create_object_t>) (objects) ())
            ((std::vector<update_create_player_t>) (players) ())
        )
    };

    serialized_t serialized() const
    {
        serialized_t serialized;
        serialized.dimen = dimensions();

        serialized.objects.reserve(m_object_map.size());
        for(auto const& pair : m_object_map)
            serialized.objects.push_back(pair.second.object.serialized());

        serialized.players.reserve(m_player_map.size());
        for(auto const& pair : m_player_map)
            serialized.players.push_back(pair.second.serialized());

        return serialized;
    }
};

inline player_t const& game_state_t::get_player(player_id_t id) const
{
    if(player_t const* ptr = get_player_ptr(id))
        return *ptr;
    throw std::out_of_range("Player doesn't exist.");
}

inline player_t& game_state_t::get_player(player_id_t id)
{
    return const_cast<player_t&>(const_this()->get_player(id));
}

inline object_t const& game_state_t::get_object(object_id_t id) const
{
    if(object_t const* ptr = get_object_ptr(id))
        return *ptr;
    throw std::out_of_range("Object doesn't exist.");
}

inline object_t& game_state_t::get_object(object_id_t id)
{
    return const_cast<object_t&>(const_this()->get_object(id));
}

inline player_t const* game_state_t::get_player_ptr(player_id_t id) const
{
    auto const it = m_player_map.find(id);
    return it == m_player_map.end() ? nullptr : &it->second;
}

inline player_t* game_state_t::get_player_ptr(player_id_t id)
{
    return const_cast<player_t*>(const_this()->get_player_ptr(id));
}

inline object_t const* game_state_t::get_object_ptr(object_id_t id) const
{
    auto const it = m_object_map.find(id);
    return it == m_object_map.end() ? nullptr : &it->second.object;
}

inline object_t* game_state_t::get_object_ptr(object_id_t id)
{
    return const_cast<object_t*>(const_this()->get_object_ptr(id));
}

inline flat_set<object_id_t> const& 
game_state_t::objects_at(coord_t coord) const
{
    return m_object_grid[coord];
}

////////////////////////////////////////

struct request_t
{
    player_id_t player;
    action_t action;
};


/*
struct server_game_t
{
    void push_request(player_id_t player_id, action_t action)
    {

    }

    std::deque<request_t> m_requests;
    mutable std::mutex m_requests_mutex;

    threadsafe_map<player_id_t, std::shared_ptr<foo>>;

    game_state_t m_game_state;
};
*/

#endif

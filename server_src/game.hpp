#ifndef GAME_HPP
#define GAME_HPP

#include <cstdint>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/container/flat_map.hpp>
#include <eggs/variant.hpp>
#include <luajit-2.0/lua.hpp>
#include <int2d/units.hpp>

#include "pool.hpp"
#include "serialize.hpp"

using boost::container::flat_map;
using namespace int2d;

using aut_t = std::uint32_t;
using player_id_t = std::uint32_t;
using object_id_t = std::uint32_t;

struct object_t;

struct player_t
{
    int id;
    object_t* object;
};

struct player_bk_t
{
    player_t player;
    bool updated;
};

struct object_t
{
    int id;
    player_t* player;
    coord_t position;
    flat_map<std::uint32_t, int> storage;
};

struct object_bk_t
{
    object_t object;
    bool updated;
};

struct frame_t
{
    std::unordered_map<object_id_t, object_t> updated;
    std::unordered_set<object_id_t> destroyed;
};

struct lua_closer
{
    void operator()(lua_State* ptr) const { lua_close(ptr); }
};

struct game_state_t
{
    aut_t time;
    free_list_pool<player_bk_t> player_pool;
    free_list_pool<object_bk_t> object_pool;
    std::unordered_map<player_id_t, player_bk_t*> player_map;
    std::unordered_map<object_id_t, object_bk_t*> object_map;
    frame_t frame;
    std::unique_ptr<lua_State, lua_closer> L;
};

#pragma GCC visibility push(default)
extern "C"
{

struct object_bk_t* create_object(struct game_state_t* game);
void destroy_object(struct game_state_t* game, struct object_bk_t* bk);
void free_object(struct game_state_t* game, struct object_bk_t* bk);
int get_id(struct object_bk_t* bk);
void set_xy(struct game_state_t* game, struct object_bk_t* bk, int x, int y);
int get_x(struct object_bk_t* bk);
int get_y(struct object_bk_t* bk);

} // extern "C"
#pragma GCC visibility pop

using update_t = eggs::variant
< struct update_destroy_object_t
, struct update_object_position_t
>;

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
        ((coord_t)     (position)  (std::uint8_t))
    )
};

#endif

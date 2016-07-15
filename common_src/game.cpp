#include "game.hpp"

game_state_t::game_state_t(game_state_t const& o)
: m_player_map(o.m_player_map)
, m_object_grid(o.m_object_grid)
, m_object_map(o.m_object_map)
{
    // Fix-up pointers
    for(auto& pair: m_player_map)
    {
        player_t& player = pair.second;
        player.m_object = get_object_ptr(player.m_object->id());
        player.m_object->m_player = &player;
    }
}

game_state_t::game_state_t(serialized_t const& serialized)
: m_object_grid(serialized.dimen)
{
    for(update_create_object_t const& update : serialized.objects)
        apply_update(update);

    for(update_create_player_t const& update : serialized.players)
        apply_update(update);
}

player_t& game_state_t::add_player(player_t player)
{
    auto pair = m_player_map.emplace(player.id(), player);

    if(!pair.second)
    {
        throw std::runtime_error(
            "Can't add player; player already exists.");
    }

    if(player.m_object)
        player.m_object->m_player = &pair.first->second;

    return pair.first->second;
}

void game_state_t::remove_player(player_id_t player_id)
{
    auto it = m_player_map.find(player_id);
    if(it == m_player_map.end())
    {
        throw std::runtime_error(
            "Can't remove player; player doesn't exist.");
    }
    m_player_map.erase(it);
}

object_t& game_state_t::add_object(object_t object)
{
    assert(!object.m_player);

    auto pair = m_object_map.emplace(
        object.id(),
        object_bk_t{ object, false });

    if(!pair.second)
    {
        throw std::runtime_error(
            "Can't add object; object already exists.");
    }

    object_t& added_object = pair.first->second.object;
    m_object_grid[added_object.position()].insert(added_object.id());
    return added_object;
}

void game_state_t::remove_object(object_id_t object_id)
{
    auto it = m_object_map.find(object_id);
    if(it == m_object_map.end())
    {
        throw std::runtime_error(
            "Can't remove object; object doesn't exist.");
    }
    {
        object_t& object = it->second.object;
        m_object_grid[object.position()].erase(object_id);
        if(object.m_player)
            object.m_player->m_object = nullptr;
    }
    m_object_map.erase(it);
}

void game_state_t::move_object(object_t& object, coord_t to)
{
    if(to == object.position())
        return;
    if(in_bounds(to, m_object_grid.dimensions()))
       m_object_grid[to].insert(object.id());
    if(in_bounds(object.position(), m_object_grid.dimensions()))
        m_object_grid[object.position()].erase(object.id());
    object.m_position = to;
}

void game_state_t::set_player_object(player_t& player, object_t* object)
{
    if(player.m_object)
        player.m_object->m_player = nullptr;
    player.m_object = object;
    if(object)
        object->m_player = &player;
}

std::deque<update_t> game_state_t::diff(game_state_t const& prev) const
{
    std::deque<update_t> updates;

    for(auto const& pair : m_object_map)
    {
        auto const& object = pair.second.object;
        auto prev_it = prev.m_object_map.find(pair.first);

        if(prev_it == prev.m_object_map.end())
        {
            updates.push_back(
                update_create_object_t
                {
                    object.id(),
                    object.position(),
                });
        }
        else
        {
            auto const& prev_object = prev_it->second.object;

            if(object.position() != prev_object.position())
            {
                updates.push_back(
                    update_object_position_t
                    {
                        object.id(),
                        object.position(),
                    });
            }

            prev_it->second.visited = true;
        }
    }

    for(auto const& pair : prev.m_object_map)
    {
        if(!pair.second.visited)
        {
            object_t const& prev_object = pair.second.object;
            updates.push_back(
                update_destroy_object_t
                {
                    prev_object.id()
                });
        }
        pair.second.visited = false;
    }

    return updates;
}

void game_state_t::apply_update(update_create_object_t update)
{
    add_object(object_t(
        update.object_id,
        update.position));
}

void game_state_t::apply_update(update_destroy_object_t update)
{
    remove_object(update.object_id);
}

void game_state_t::apply_update(update_object_position_t update)
{
    move_object(get_object(update.object_id), update.position);
}

bool game_state_t::do_action(player_t& player, action_move_up_t action)
{
    object_t& object = *player.m_object;
    move_object(object, up1(object.position()));
    return true;
}

void game_state_t::apply_update(update_create_player_t update)
{
    player_t& player = add_player(player_t(update.player_id));
    set_player_object(player, get_object_ptr(update.object_id));
}

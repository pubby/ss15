#include "game.hpp"

static void begin_update(struct game_state_t* game, struct object_bk_t* bk)
{
    if(!bk->updated)
    {
        bk->updated = true;
        game->frame.updated.emplace(bk->object.id, bk->object);
    }
}

extern "C"
{

struct object_bk_t* create_object(struct game_state_t* game)
{
    object_bk_t* bk = game->object_pool.alloc();
    game->frame.updated.emplace(bk->object.id, bk->object);
    bk->updated = true;
    return bk;
}

void destroy_object(struct game_state_t* game, struct object_bk_t* bk)
{
    if(!bk->object.id)
        return;

    game->frame.destroyed.insert(bk->object.id);
    bk->updated = true;
    bk->object = {};
    game->object_pool.free(bk);
}

void free_object(struct game_state_t* game, struct object_bk_t* bk)
{
    destroy_object(game, bk);
    game->object_pool.free(bk);
}

int get_id(struct object_bk_t* bk)
{
    return bk->object.id;
}

int get_x(struct object_bk_t* bk)
{
    return bk->object.position.x;
}

int get_y(struct object_bk_t* bk)
{
    return bk->object.position.y;
}

void set_xy(struct game_state_t* game, struct object_bk_t* bk, int x, int y)
{
    begin_update(game, bk);
    bk->object.position.x = x;
    bk->object.position.y = y;
}

} // extern "C"

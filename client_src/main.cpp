#include <cstdlib>
#include <cstdio>
#include <string>
#include <thread>

#include <boost/asio.hpp>

#include <SFML/Graphics.hpp>

#include "client.hpp"
//#include "client_game.hpp"
#include "game.hpp"

namespace asio = boost::asio;
namespace posix_time = boost::posix_time;
namespace ip = boost::asio::ip;

void render_game_state
( sf::RenderTarget& render_target
, sf::Texture const& texture
, game_state_t const& game_state)
{
    render_target.clear();

    constexpr int tile_size = 32;

    sf::Sprite sprite;
    sprite.setTexture(texture);
    for(coord_t coord : rect_range(to_rect(game_state.dimensions())))
    {
        sprite.setPosition(coord.x * tile_size, coord.y * tile_size);
        for(object_id_t object_id : game_state.objects_at(coord))
        {
            //object_t const& object = game_state.get_object(object_id);
            /*
            sf::IntRect tex_rect;
            tex_rect.left = object.sprite_id() % 32;
            tex_rect.top = object.sprite_id() / 32;
            tex_rect.width = 32;
            tex_rect.height = 32;
            sprite.setTextureRect(tex_rect);
            */
            render_target.draw(sprite);
        }
    }
}

int main(int argc, char* argv[])
{
    if(argc != 3)
    {
        std::fprintf(stderr, "usage: %s <address> <port>\n",
                     argc ? argv[0] : "client");
        return EXIT_FAILURE;
    }

    asio::io_service io_service;

    //client_game_t client_game;
    game_state_t game_state(dimen_t{ 256, 256 });

    object_t o(1, { 3, 3 });
    game_state.add_object(o);

    //client c(client_game, argv[1], argv[2]);
    client_t client(io_service, argv[1], argv[2]);

    std::thread net_thread(
        [&client]()
        {
            client.run();
        });

    client.send_input(CTS_INPUT_UP);
    client.send_input(CTS_INPUT_UP);

    net_thread.join();
    return 0;

    sf::RenderWindow window(sf::VideoMode(800, 600), "ss15");
    sf::Texture tiles;
    tiles.loadFromFile("tiles.png");

    while(true)
    {
        // Process events
        sf::Event event;
        while(window.pollEvent(event))
        {
            switch(event.type)
            {
            case sf::Event::Closed:
                window.close();
                return EXIT_SUCCESS;
            case sf::Event::Resized:
                window.setView(sf::View(
                    sf::FloatRect(0, 0, event.size.width, 
                                  event.size.height)));
                break;
            default:
                break;
            }
        }

        /*
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
        {
            request_t request = { REQUEST_MOVE, client.player.id };
            request.move.position
                = left1(client.player_object().position);
            server.pend_request(request);
        }

        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
        {
            request_t request = { REQUEST_MOVE, client.player.id };
            request.move.position
                = right1(client.player_object().position);
            server.pend_request(request);
        }

        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
        {
            request_t request = { REQUEST_MOVE, client.player.id };
            request.move.position
                = up1(client.player_object().position);
            server.pend_request(request);
        }

        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
        {
            request_t request = { REQUEST_MOVE, client.player.id };
            request.move.position
                = down1(client.player_object().position);
            server.pend_request(request);
        }

        client.pending_updates = server.advance_tick();
        client.advance_tick();
        */

        render_game_state(window, tiles, game_state);
        window.display();
    }

    return EXIT_SUCCESS;
}

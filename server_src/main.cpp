#include <cstdlib>
#include <cstdio>
#include <memory>
#include <string>

#include "server.hpp"
#include "pool.hpp"

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
        std::size_t num_threads = std::stoi(argv[3]);
        server_t server(argv[1], argv[2], num_threads);
        server.run();
    }
    catch(std::exception& e)
    {
        std::cerr << "exception: " << e.what() << "\n";
    }

    return EXIT_SUCCESS;
}


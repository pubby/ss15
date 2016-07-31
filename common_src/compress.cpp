#include "compress.hpp"

#include <stdexcept>

#include <zlib.h>

static void check_zlib_error(int err)
{
    if(err == Z_OK)
        return;
    if(err == Z_MEM_ERROR)
        throw std::runtime_error("compression error (Z_MEM_ERROR)");
    if(err == Z_BUF_ERROR)
        throw std::runtime_error("compression error (Z_BUF_ERROR)");
    if(err == Z_STREAM_ERROR)
        throw std::runtime_error("compression error (Z_STREAM_ERROR)");
    throw std::runtime_error("compression error");
}


std::size_t compress_bound(std::size_t uncompressed_size)
{
    throw 0;
    //return compressBound(uncompressed_size);
}

std::size_t compress(char const* dest, std::size_t dest_size, 
                     char* src, std::size_t src_size)
{
    throw 0;
    /*
    int err = compress2(dest, &dest_size, src, src_size, Z_BEST_COMPRESSION);
    check_zlib_error(err);
    return dest_size;
    */
}

std::size_t decompress(char const* dest, std::size_t dest_size, 
                       char* src, std::size_t src_size)
{
    throw 0;
    /*
    int err = uncompress(dest, &dest_size, src, src_size);
    check_zlib_error(err);
    */
}

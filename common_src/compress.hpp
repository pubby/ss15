#ifndef COMPRESS_HPP
#define COMPRESS_HPP

#include <cstdint>

std::size_t compress_bound(std::size_t uncompressed_size);

std::size_t compress(char const* dest, std::size_t dest_size, 
                     char* src, std::size_t src_size);

std::size_t decompress(char const* dest, std::size_t dest_size, 
                       char* src, std::size_t src_size);

#endif

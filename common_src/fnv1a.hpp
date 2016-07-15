#ifndef FNV1A_HPP
#define FNV1A_HPP

// A decently-fast, endian-agnostic, non-cryptographic hash function.
// Hyperlinks:
//   http://create.stephan-brumme.com/fnv-hash/
//   http://isthe.com/chongo/tech/comp/fnv/

namespace fnv1a
{
    constexpr std::uint64_t prime64 = 1099511628211;
    constexpr std::uint64_t seed64  = 14695981039346656037;

    /// Hash a single byte.
    [[gnu::always_inline]]
    inline std::uint64_t hash64(unsigned char byte,
                                std::uint64_t hash = seed64)
    {
      return (byte ^ hash) * prime;
    }

    inline std::uint64_t hash64(char const* data, std::size_t size,
                                std::uint64_t hash = seed64)
    {
        while(size--)
            hash = hash64(*data++, hash);
        return hash;
    }
}

#endif

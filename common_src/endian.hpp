#ifndef ENDIAN_HPP
#define ENDIAN_HPP

// Functions for converting to and from different endians.

#include <cassert>
#include <cstdint>
#include <climits>
#include <iterator>
#include <type_traits>

template<typename T, int N = sizeof(T) - 1>
struct endian_impl
{
    static_assert(CHAR_BIT == 8, "Only 8-bit bytes are supported.");
    static_assert(std::is_integral<T>::value,
                  "endian conversion only works on integers");

    // Note: t should be set to 0 before calling.
    template<typename CharIt>
    static CharIt from_little_endian(CharIt it, T& t)
    {
        static_assert(sizeof(decltype(*it)) == 1,
                      "iterator should point to char type");
        std::make_unsigned_t<T> v = static_cast<unsigned char>(*it);
        v <<= 8 * (sizeof(T) - 1 - N);
        t |= v;
        ++it;
        return endian_impl<T, N-1>::from_little_endian(it, t);
    }

    // Note: t should be set to 0 before calling.
    template<typename CharIt>
    static CharIt from_big_endian(CharIt it, T& t)
    {
        static_assert(sizeof(decltype(*it)) == 1,
                      "iterator should point to char type");
        std::make_unsigned_t<T> v = static_cast<unsigned char>(*it);
        v <<= 8 * N; 
        t |= v;
        ++it;
        return endian_impl<T, N-1>::from_big_endian(it, t);
    }

    template <typename CharIt>
    static CharIt to_little_endian(T t, CharIt it)
    {
        T const shift_amount = (sizeof(T) - 1 - N) * 8;
        std::make_unsigned_t<T> const FF = 0xFF;
        *it = (t & (FF << shift_amount)) >> shift_amount;
        ++it;
        return endian_impl<T, N-1>::to_little_endian(t, it);
    }

    template <typename CharIt>
    static CharIt to_big_endian(T t, CharIt it)
    {
        T const shift_amount = N * 8;
        std::make_unsigned_t<T> const FF = 0xFF;
        *it = (t & (FF << shift_amount)) >> shift_amount;
        ++it;
        return endian_impl<T, N-1>::to_big_endian(t, it);
    }

};

template<typename T>
struct endian_impl<T, -1>
{
    template<typename CharIt>
    static CharIt from_little_endian(CharIt it, T&) { return it; }

    template<typename CharIt>
    static CharIt from_big_endian(CharIt it, T&) { return it; }

    template <typename CharIt>
    static CharIt to_little_endian(T, CharIt it) { return it; }

    template <typename CharIt>
    static CharIt to_big_endian(T, CharIt it) { return it; }
};

template<typename CharIt, typename T>
inline CharIt from_little_endian(CharIt it, T& t)
{
    t = 0;
    return endian_impl<T>::from_little_endian(it, t);
}

template<typename CharIt>
inline CharIt from_little_endian(CharIt it, bool& t)
{
    char c = 0;
    it = from_little_endian(it, c);
    t = c;
    return it;
}

template<typename CharIt, typename T>
inline CharIt from_big_endian(CharIt it, T& t)
{
    t = 0;
    return endian_impl<T>::from_big_endian(it, t);
}

template<typename CharIt>
inline CharIt from_big_endian(CharIt it, bool& t)
{
    char c = 0;
    it = from_big_endian(it, c);
    t = c;
    return it;
}

template<typename CharIt, typename T>
inline CharIt to_little_endian(T t, CharIt it)
{
    return endian_impl<T>::to_little_endian(t, it);
}

template<typename CharIt>
inline CharIt to_little_endian(bool t, CharIt it)
{
    return endian_impl<char>::to_little_endian(t, it);
}

template<typename CharIt, typename T>
inline CharIt to_big_endian(T t, CharIt it)
{
    return endian_impl<T>::to_big_endian(t, it);
}

template<typename CharIt>
inline CharIt to_big_endian(bool t, CharIt it)
{
    return endian_impl<char>::to_big_endian(t, it);
}

#endif

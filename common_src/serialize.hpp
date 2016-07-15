#ifndef SERIALIZE_HPP
#define SERIALIZE_HPP

#include <array>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include <eggs/variant.hpp>

#include <int2d/units.hpp>

#include "serialize_impl.hpp"
#include "type_index.hpp"

template<typename T>
struct variant_serializer;

template<typename T, typename... Params>
struct serialize
: serialize_impl::base<T, Params...>
{};

template<typename T, std::size_t N, typename... P>
struct serialize<std::array<T, N>, P...>
: serialize_impl::with_params<P...>
  ::template array_size<std::array<T, N>, P...>
{
    using type = std::array<T, N>;

    template<typename It>
    static It read(It const begin, It const end, type& dest)
    {
        auto it = begin;
        for(auto& v : dest)
            it = serialize<T, P...>::read(it, end, v);
        return it;
    }

    template<typename It>
    static It write(type const& src, It const dest)
    {
        auto it = dest;
        for(T const& v : src)
            it = serialize<T, P...>::write(v, it);
        return it;
    }
};

template<>
struct serialize<std::string>
{
    using type = std::string;

    template<typename It>
    static It read(It const begin, It const end, type& dest)
    {
        auto it = begin;
        while(*it != '\0')
        {
            if(it == end)
            {
                throw std::range_error(
                    "serialize::read range too small");
            }
            ++it;
        }
        dest.assign(begin, it);
        return std::next(it);
    }

    template<typename It>
    static It write(type const& src, It const dest)
    {
        return std::copy(src.data(), src.data() + src.size() + 1, dest);
    }

    static std::size_t size(type const& t)
    {
        return t.size() + 1;
    }
};

template<typename... Ts>
struct serialize<eggs::variant<Ts...>>
: serialize<eggs::variant<Ts...>, std::uint8_t>
{};

template<typename Int, typename... Ts>
struct serialize<eggs::variant<Ts...>, Int>
{
    using type = eggs::variant<Ts...>;

    static_assert(sizeof...(Ts) < std::numeric_limits<Int>::max(),
                  "variant must be smaller than integer type");

    template<typename It>
    static It read(It const begin, It const end, type& dest)
    {
        Int which;
        auto it = serialize<Int>::read(begin, end, which);
        return runtime_type_index<reader, Ts...>(
            which,
            it,
            end,
            &dest);
    }

    template<typename It>
    static It write(type const& src, It const dest)
    {
        auto it = serialize<Int>::write(src.which(), dest);
        return serialize<type, void>::write(src, it);
    }

    static std::size_t size(type const& t)
    {
        return serialize<type, void>::size(t) + serialize<Int>::const_size;
    }

private:
    template<typename V>
    using reader = typename variant_serializer<type>::template reader<V>;
};

// This specialization does not serialize the variant's stored index,
// thus it lacks 'read' functionality.
template<typename... Ts>
struct serialize<eggs::variant<Ts...>, void>
{
    using type = eggs::variant<Ts...>;

    template<typename It>
    static It write(type const& src, It const dest)
    {
        return eggs::variants::apply<It>(
            [dest](auto const& v) -> It
            {
                using V = std::remove_reference_t<decltype(v)>;
                return serialize<V>::write(v, dest);
            }, src);
    }

    static std::size_t size(type const& t)
    {
        return eggs::variants::apply<std::size_t>(
            [](auto const& v) -> std::size_t
            {
                using V = std::remove_reference_t<decltype(v)>;
                return serialize<V>::size(v);
            }, t);
    }
};

template<typename Int>
struct serialize<int2d::coord_t, Int>
{
    using type = int2d::coord_t;

    static constexpr std::size_t const_size
        = serialize<int, Int>::const_size * 2;

    template<typename It>
    static It read(It const begin, It const end, type& dest)
    {
        auto it = serialize<int, Int>::read(begin, end, dest.x);
        return serialize<int, Int>::read(it, end, dest.y);
    }

    template<typename It>
    static It write(type const& src, It const dest)
    {
        auto it = serialize<int, Int>::write(src.x, dest);
        return serialize<int, Int>::write(src.y, it);
    }

    static std::size_t size(type const&) { return const_size; }
};

template<>
struct serialize<int2d::coord_t>
: serialize<int2d::coord_t, std::int32_t>
{};

template<typename Int>
struct serialize<int2d::dimen_t, Int>
{
    using type = int2d::dimen_t;

    static constexpr std::size_t const_size
        = serialize<int, Int>::const_size * 2;

    template<typename It>
    static It read(It const begin, It const end, type& dest)
    {
        auto it = serialize<int, Int>::read(begin, end, dest.w);
        return serialize<int, Int>::read(it, end, dest.h);
    }

    template<typename It>
    static It write(type const& src, It const dest)
    {
        auto it = serialize<int, Int>::write(src.w, dest);
        return serialize<int, Int>::write(src.h, it);
    }

    static std::size_t size(type const&) { return const_size; }
};

template<>
struct serialize<int2d::dimen_t>
: serialize<int2d::dimen_t, std::int32_t>
{};

template<typename T, typename = void>
struct serialize_has_const_size
: std::false_type
{};

template<typename T>
struct serialize_has_const_size<T, std::void_t<decltype(T::const_size)>>
: std::true_type
{};

template<typename T>
struct variant_serializer
{
    template<typename V>
    struct reader
    {
        template<typename It>
        It operator()(It begin, It end, T* t) const
        {
            V v;
            auto it = serialize<V>::read(begin, end, v);
            *t = std::move(v);
            return it;
        }
    };
};

#endif

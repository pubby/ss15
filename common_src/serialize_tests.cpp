#include "serialize.hpp"

#include <catch/catch.hpp>

#include <array>
#include <cstdint>
#include <iostream>

#include <arpa/inet.h>

#include "buffer.hpp"

#include <boost/preprocessor/seq/variadic_seq_to_seq.hpp>

#include <eggs/variant.hpp>

struct foo_t
{
    SERIALIZED_DATA
    (
        ((std::int8_t)   (x) ())
        ((std::int16_t)  (y) (std::int64_t))
        ((std::uint32_t) (z) (std::uint64_t))
    )

    bool operator==(foo_t const& o) const
    {
        return x == o.x && y == o.y && z == o.z;
    }

    bool operator!=(foo_t const& o) const
    {
        return !(*this == o);
    }
};

struct bar_t
{
    SERIALIZED_DATA
    (
        ((foo_t)                (foo1) ())
        ((std::uint64_t)        (x)    ())
        ((bool)                 (y)    ())
        ((foo_t)                (foo2) ())
        ((std::vector<int>)     (vec)  (std::uint8_t, std::int16_t))
        ((std::array<short, 8>) (arr)  (std::int64_t))
        ((eggs::variant<int>)   (v)    ())
    )
    bool operator==(bar_t const& o) const
    {
        return (foo1 == o.foo1 && x == o.x && y == o.y && foo2 == o.foo2
                && std::equal(vec.begin(), vec.end(), o.vec.begin())
                && arr == o.arr
                && v == o.v);
    }

    bool operator!=(bar_t const& o) const
    {
        return !(*this == o);
    }
};

struct qux_t
{
    SERIALIZED_DATA
    (
        ((std::vector<int>) (vec) (std::uint8_t, std::uint16_t))
    )
};

TEST_CASE("SERIALIZED_DATA", "[serialize]")
{
    SECTION("foo_t")
    {
        foo_t foo1 = { 122, -4302, 9038414 };
        foo_t foo2 = {};

        REQUIRE(foo1 != foo2);
        std::size_t const_size = serialize<foo_t>::const_size;
        REQUIRE(const_size == 17);
        REQUIRE(const_size == serialize<foo_t>::size(foo1));

        std::vector<char> buffer(serialize<foo_t>::size(foo1));
        serialize<foo_t>::write(foo1, buffer.begin());
        serialize<foo_t>::read(buffer.begin(), buffer.end(), foo2);

        REQUIRE(foo1 == foo2);
    }

    SECTION("bar_t")
    {
        bar_t bar1
            = { { 102, 9231, 3204 }, 390409384, true, { 0, 93, 2 },
                { 1,5,9 }, { 9, 3, -2, 394, 2, -3 }, 900 };
        bar_t bar2 = {};

        REQUIRE(bar1 != bar2);

        std::vector<char> buffer(serialize<bar_t>::size(bar1));
        serialize<bar_t>::write(bar1, buffer.begin());
        serialize<bar_t>::read(buffer.begin(), buffer.end(), bar2);

        REQUIRE(bar1 == bar2);
    }

    SECTION("qux_t")
    {
        qux_t qux = {{ 1, 2, 3, 4 }};
        REQUIRE(serialize<qux_t>::size(qux) 
                == sizeof(std::uint8_t) + sizeof(std::uint16_t) * 4);
    }

    REQUIRE((serialize<long long, std::uint16_t>::const_size == 2));
    REQUIRE((serialize<char, std::uint32_t>::const_size == 4));
}


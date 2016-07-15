#ifndef SERIALIZE_IMPL
#define SERIALIZE_IMPL

#include <cstdint>
#include <iterator>
#include <limits>
#include <list>
#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/seq/variadic_seq_to_seq.hpp>

#include "endian.hpp"

namespace std
{
    template<typename T, typename Allocator> class vector;
    template<typename T, typename Allocator> class deque;
}

template<typename T, typename... Params>
struct serialize;

namespace serialize_impl
{
    template<typename T>
    struct is_listlike : std::false_type {};

    template<typename... T>
    struct is_listlike<std::vector<T...>> : std::true_type {};

    template<typename... T>
    struct is_listlike<std::deque<T...>> : std::true_type {};

    template<typename... T>
    struct is_listlike<std::list<T...> > : std::true_type {};

    template<typename... P>
    struct with_params
    {
        template<typename A, typename = std::void_t<>>
        struct array_size
        {
            static std::size_t size(A const& t)
            {
                std::size_t size = 0;
                for(auto& v : t)
                    size += serialize<typename A::value_type, P...>::size(v);
                return size;
            }
        };

        template<typename A>
        struct array_size<A, std::void_t<
            decltype(serialize<typename A::value_type, P...>::const_size)>>
        {
            static constexpr std::size_t const_size 
                = (serialize<typename A::value_type, P...>::const_size 
                   * std::tuple_size<A>::value);

            static std::size_t size(A const&)
            {
                return const_size;
            }
        };
    };

    template<typename T, typename M, typename = std::void_t<>>
    struct members_size
    {
        static std::size_t size(T const& t)
        {
            return t.serialized_size();
        }
    };

    template<typename T>
    struct members_size<T, std::tuple<>, void>
    {
        static constexpr std::size_t const_size = 0;

        static std::size_t size(T const& t)
        {
            return 0;
        }
    };

    template<typename T, typename... Ms>
    struct members_size<T, std::tuple<Ms...>,
                        std::void_t<decltype((Ms::const_size + ...))>>
    {
        static constexpr std::size_t const_size = (Ms::const_size + ...);

        static std::size_t size(T const&)
        {
            return const_size;
        }
    };

    template<typename T, typename = void, typename = void>
    struct base_size
    {
        static std::size_t size(T const& t)
        {
            return t.serialized_size();
        }
    };

    template<typename T>
    struct base_size<T, std::void_t<decltype(T::const_size)>, void>
    {
        static constexpr std::size_t const_size = T::const_size;

        static std::size_t size(T const&)
        {
            return T::const_size;
        }
    };

    template<typename T>
    struct base_size<T, void, std::void_t<typename T::serialized_members>>
    : members_size<T, typename T::serialized_members>
    {};

    template<typename T, typename Underlying, typename Cast>
    struct int_serialize
    {
        using type = T;

        static constexpr std::size_t const_size
            = std::is_same<Cast, bool>::value ? 1 : sizeof(Cast);

        template<typename It>
        static It read(It const begin, It const end, T& dest)
        {
            if((std::size_t)std::distance(begin, end) < const_size)
                throw std::range_error("serialize::read range too small");

            Cast c;
            auto it = from_little_endian(begin, c);

            using common = std::common_type_t<Underlying, Cast>;
            if(common(c) < common(std::numeric_limits<Underlying>::min())
               || common(c) > common(std::numeric_limits<Underlying>::max()))
            {
                throw std::overflow_error("serialize::read overflow");
            }

            dest = static_cast<T>(c);
            return it;
        }

        template<typename It>
        static It write(T const& src, It const dest)
        {
            using common = std::common_type_t<Underlying, Cast>;
            if(common(src) < common(std::numeric_limits<Cast>::min())
               || common(src) > common(std::numeric_limits<Cast>::max()))
            {
                throw std::overflow_error("serialize::write overflow");
            }
            Cast c = src;
            return to_little_endian(c, dest);
        }

        static std::size_t size(T const&)
        {
            return const_size;
        }
    };

    template
    < typename T
    , bool IsIntegral
    , bool IsEnum
    , bool IsListlike
    , typename... Params>
    struct base_;

    template<typename T, typename... Params>
    using base = base_
    < T
    , std::is_integral<T>::value
    , std::is_enum<T>::value
    , is_listlike<T>::value
    , Params...>;

    template<typename T>
    struct base_<T, false, false, false>
    : base_size<T>
    {
        using type = T;

        template<typename It>
        static It read(It const begin, It const end, T& dest)
        {
            return dest.read_serialized(begin, end);
        }

        template<typename It>
        static It write(T const& src, It const dest)
        {
            return src.write_serialized(dest);
        }
    };

    template<typename T>
    struct base_<T, true, false, false>
    : int_serialize<T, T, T>
    {};

    template<>
    struct base_<bool, true, false, false>
    : int_serialize<bool, bool, unsigned char>
    {};

    template<typename T, typename Int>
    struct base_<T, true, false, false, Int>
    : int_serialize<T, T, Int>
    {};

    template<typename T>
    struct base_<T, false, true, false>
    : int_serialize<T, std::underlying_type_t<T>, std::underlying_type_t<T>>
    {};

    template<typename T, typename Int>
    struct base_<T, false, true, false, Int>
    : int_serialize<T, std::underlying_type_t<T>, Int>
    {};

    template<typename T>
    struct base_<T, false, false, true>
    : base_<T, false, false, true, std::uint16_t>
    {};

    template<typename T, typename SizeInt, typename... P>
    struct base_<T, false, false, true, SizeInt, P...>
    {
        using type = T;

        template<typename It>
        static It read(It const begin, It const end, T& dest)
        {
            std::size_t size;
            auto it = serialize<std::size_t, SizeInt>::read(begin, end, size);
            std::printf("size %lu\n", size);
            dest.resize(size);
            for(auto& v : dest)
                it = serialize<typename T::value_type, P...>::read(it, end, v);
            return it;
        }

        template<typename It>
        static It write(T const& src, It const dest)
        {
            auto it = serialize<std::size_t, SizeInt>::write(src.size(), dest);
            for(auto& v : src)
                it = serialize<typename T::value_type, P...>::write(v, it);
            std::printf("write size %lu\n", it-dest);
            return it;
        }

        static std::size_t size(T const& t)
        {
            std::size_t size = serialize<std::size_t, SizeInt>::const_size;
            for(auto& v : t)
                size += serialize<typename T::value_type, P...>::size(v);
            std::printf("size size %lu\n", size);
            return size;
        }
    };

    template<typename T>
    struct remove_first;

    template<typename T, typename... Ts>
    struct remove_first<std::tuple<T, Ts...>>
    {
        using type = std::tuple<Ts...>;
    };

    template<typename T>
    using remove_first_t = typename remove_first<T>::type;

    template<typename T>
    struct add_params
    {
        template<typename... P>
        using type = serialize<T, P...>;
    };
}

#define SERIALIZE_IMPL_GET(i, elem) \
BOOST_PP_TUPLE_ENUM(BOOST_PP_SEQ_ELEM(i, BOOST_PP_VARIADIC_SEQ_TO_SEQ(elem)))

#define SERIALIZE_IMPL_EXPAND_SERIALIZED_MEMBERS(r, data, elem) \
SERIALIZE_IMPL_GET(0, elem) SERIALIZE_IMPL_GET(1, elem);

#define SERIALIZE_IMPL_EXPAND_READ_SERIALIZED(r, it, elem) \
{\
    using S_ = ::serialize_impl::add_params<\
        SERIALIZE_IMPL_GET(0, elem)>::template type<\
        SERIALIZE_IMPL_GET(2, elem)>;\
    it = S_::read(it, end, SERIALIZE_IMPL_GET(1, elem));\
}

#define SERIALIZE_IMPL_EXPAND_WRITE_SERIALIZED(r, it, elem) \
{\
    using S_ = ::serialize_impl::add_params<\
        SERIALIZE_IMPL_GET(0, elem)>::template type<\
        SERIALIZE_IMPL_GET(2, elem)>;\
    it = S_::write(SERIALIZE_IMPL_GET(1, elem), it);\
}

#define SERIALIZE_IMPL_EXPAND_SERIALIZED_SIZE(r, data, elem) \
{\
    using S_ = ::serialize_impl::add_params<\
        SERIALIZE_IMPL_GET(0, elem)>::template type<\
        SERIALIZE_IMPL_GET(2, elem)>;\
    size += S_::size(SERIALIZE_IMPL_GET(1, elem));\
}

#define SERIALIZE_IMPL_EXPAND_SERIALIZED_TYPE_LIST(r, data, elem) \
, serialize_impl::add_params<SERIALIZE_IMPL_GET(0, elem)>\
  ::template type<SERIALIZE_IMPL_GET(2, elem)>

// Use this macro in a struct to get some serialization functions 
// defined for it. Pass in a PP sequence of 2-tuples for member variables.
// The functions are:
//   InputIt read_serialized(InputIt)
//   OutputIt write_serialized(OutputIt) const
//   std::size_t serialized_size() const
//   
//
// Example:
//  struct foo
//  {
//      SERIALIZED_DATA
//      (
//          ((int8_t,  x))
//          ((int16_t, y))
//          ((int32_t, z))
//      )
//  };
//
#define SERIALIZED_DATA(memseq) \
BOOST_PP_SEQ_FOR_EACH(SERIALIZE_IMPL_EXPAND_SERIALIZED_MEMBERS,, memseq)\
template<typename InputIt>\
InputIt read_serialized(InputIt begin, InputIt end)\
{\
    auto it = begin;\
    BOOST_PP_SEQ_FOR_EACH(SERIALIZE_IMPL_EXPAND_READ_SERIALIZED, it, memseq)\
    return it;\
}\
template<typename OutputIt>\
OutputIt write_serialized(OutputIt dest) const\
{\
    auto it = dest;\
    BOOST_PP_SEQ_FOR_EACH(SERIALIZE_IMPL_EXPAND_WRITE_SERIALIZED, it, memseq)\
    return it;\
}\
std::size_t serialized_size() const\
{\
    std::size_t size = 0;\
    BOOST_PP_SEQ_FOR_EACH(SERIALIZE_IMPL_EXPAND_SERIALIZED_SIZE,, memseq);\
    return size;\
}\
using serialized_members = serialize_impl::remove_first_t<std::tuple<void\
    BOOST_PP_SEQ_FOR_EACH(SERIALIZE_IMPL_EXPAND_SERIALIZED_TYPE_LIST,,\
                          memseq)>>;

#endif

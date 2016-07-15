#ifndef TYPE_INDEX_HPP
#define TYPE_INDEX_HPP

#include <type_traits>
#include <utility>

#include <eggs/variant.hpp>

namespace type_index_impl
{
    template<template<typename> class FuncObj, typename... Ts>
    struct runtime_type_index_;

    template<template<typename> class FuncObj, typename T, typename... Ts>
    struct runtime_type_index_<FuncObj, T, Ts...>
    {
        template<typename... Args>
        [[gnu::always_inline]]
        static auto func(std::size_t index, Args&&... args)
        {
            if(index == 0)
            {
                FuncObj<T> func;
                return func(std::forward<Args>(args)...);
            }
            return runtime_type_index_<FuncObj, Ts...>::func(
                index - 1,
                std::forward<Args>(args)...);
        }
    };

    template<template<typename> class FuncObj>
    struct runtime_type_index_<FuncObj>
    {
        template<typename... Args>
        [[gnu::always_inline]]
        static auto func(std::size_t index, Args&&... args)
        -> decltype(FuncObj<void>()(args...))
        {
            throw std::out_of_range("runtime_type_index out of range ");
        }
    };
}

// This is like std::variant_alternative except it gets the type at
// runtime instead of at compile time.
template<template<typename> class FuncObj, typename... Ts, typename... Args>
auto runtime_type_index(std::size_t index, Args&&... args)
{
    using namespace type_index_impl;
    return runtime_type_index_<FuncObj, Ts...>::func(
        index,
        std::forward<Args>(args)...);
}

template<typename T>
struct runtime_type_indexer;

template<typename... Ts>
struct runtime_type_indexer<eggs::variant<Ts...>>
{
    template<template<typename> class FuncObj, typename... Args>
    auto operator()(std::size_t index, Args&&... args) const
    {
        using namespace type_index_impl;
        return runtime_type_index_<FuncObj, Ts...>::func(
            index,
            std::forward<Args>(args)...);
    }
};

#endif

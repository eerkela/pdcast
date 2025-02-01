#ifndef BERTRAND_FUNC_H
#define BERTRAND_FUNC_H

#include <vector>
#include <unordered_map>

#include "bertrand/common.h"
#include "bertrand/bitset.h"
#include "bertrand/except.h"
#include "bertrand/static_str.h"
#include "bertrand/static_map.h"


namespace bertrand {


template <typename... Ts>
struct args;


template <typename F, typename... Fs>
    requires (
        !std::is_reference_v<F> &&
        !(std::is_reference_v<Fs> || ...)
    )
struct chain;


/* Introspect an annotated C++ function signature to extract compile-time type
information and allow matching functions to be called using Python-style conventions.
Also defines supporting data structures to allow for partial function application. */
template <typename T>
struct signature {
    static constexpr bool enable = false;
};


namespace impl {
    struct args_tag {};
    struct chain_tag {};
    struct comprehension_tag {};
    struct signature_tag {};
    struct signature_defaults_tag {};
    struct signature_partial_tag {};
    struct signature_bind_tag {};
    struct signature_vectorcall_tag {};
    struct signature_overloads_tag {};
    struct def_tag {};

    /* A compact bitset describing the kind (positional, keyword, optional, and/or
    variadic) of an argument within a C++ parameter list. */
    struct ArgKind {
        enum Flags : uint8_t {
            /// NOTE: the relative ordering of these flags is significant, as it
            /// dictates the order in which edges are stored within overload tries
            /// for the `bertrand::Function` class.  The order should always be such
            /// that POS < OPT POS < VAR POS < KW < OPT KW < VAR KW, to ensure a stable
            /// traversal order.
            OPT                 = 0b1,
            VAR                 = 0b10,
            POS                 = 0b100,
            KW                  = 0b1000,
        } flags;

        [[nodiscard]] constexpr ArgKind(uint8_t flags = 0) noexcept :
            flags(static_cast<Flags>(flags))
        {}

        [[nodiscard]] constexpr operator uint8_t() const noexcept {
            return flags;
        }

        [[nodiscard]] constexpr bool posonly() const noexcept {
            return (flags & ~OPT) == POS;
        }

        [[nodiscard]] constexpr bool pos() const noexcept {
            return (flags & (VAR | POS)) == POS;
        }

        [[nodiscard]] constexpr bool args() const noexcept {
            return flags == (VAR | POS);
        }

        [[nodiscard]] constexpr bool kwonly() const noexcept {
            return (flags & ~OPT) == KW;
        }

        [[nodiscard]] constexpr bool kw() const noexcept {
            return (flags & (VAR | KW)) == KW;
        }

        [[nodiscard]] constexpr bool kwargs() const noexcept {
            return flags == (VAR | KW);
        }

        [[nodiscard]] constexpr bool opt() const noexcept {
            return flags & OPT;
        }

        [[nodiscard]] constexpr bool variadic() const noexcept {
            return flags & VAR;
        }
    };

    template <typename Arg, typename... Ts>
    struct BoundArg;

    template <typename... Ts>
    struct ArgsBase : args_tag {};
    template <typename T, typename... Ts>
        requires (std::is_void_v<T> || (std::is_void_v<Ts> || ...))
    struct ArgsBase<T, Ts...> {};
    template <typename T, typename... Ts>
    struct ArgsBase<T, Ts...> : ArgsBase<Ts...> {
        std::conditional_t<
            std::is_lvalue_reference_v<T>,
            T,
            std::remove_reference_t<T>
        > value;
        constexpr ArgsBase(T value, Ts... ts) :
            ArgsBase<Ts...>(std::forward<Ts>(ts)...),
            value(std::forward<T>(value))
        {}
        constexpr ArgsBase(ArgsBase&& other) :
            ArgsBase<Ts...>(std::move(other)),
            value([](ArgsBase&& other) {
                if constexpr (std::is_lvalue_reference_v<T>) {
                    return other.value;
                } else {
                    return std::move(other.value);
                }
            }())
        {}
    };

}


namespace meta {

    template <typename T>
    struct arg_traits;

    namespace detail {
        template <typename, typename = void>
        struct detect_arg { static constexpr bool value = false; };
        template <typename T>
        struct detect_arg<T, std::void_t<typename T::_detect_arg>> {
            static constexpr bool value = true;
        };

        template <size_t, bertrand::static_str>
        constexpr bool validate_arg_name = true;
        template <size_t I, bertrand::static_str Name> requires (I < Name.size())
        constexpr bool validate_arg_name<I, Name> =
            (impl::char_isalnum(Name[I]) || Name[I] == '_') &&
            validate_arg_name<I + 1, Name>;

        template <bertrand::static_str Name>
        constexpr bool arg_name =
            !Name.empty() &&
            (impl::char_isalpha(Name[0]) || Name[0] == '_') &&
            validate_arg_name<1, Name>;

        template <bertrand::static_str Name>
        constexpr bool variadic_args_name =
            Name.size() > 1 &&
            Name[0] == '*' &&
            (impl::char_isalpha(Name[1]) || Name[1] == '_') &&
            validate_arg_name<2, Name>;

        template <bertrand::static_str Name>
        constexpr bool variadic_kwargs_name =
            Name.size() > 2 &&
            Name[0] == '*' &&
            Name[1] == '*' &&
            (impl::char_isalpha(Name[2]) || Name[2] == '_') &&
            validate_arg_name<3, Name>;

        template <bertrand::static_str...>
        constexpr bool arg_names_are_unique = true;
        template <bertrand::static_str Name, bertrand::static_str... Names>
        constexpr bool arg_names_are_unique<Name, Names...> =
            (Name.empty() || ((Name != Names) && ...)) && arg_names_are_unique<Names...>;

        template <typename T>
        constexpr bool enable_unpack_operator = false;

        template <typename T>
        constexpr bool enable_comprehension_operator = false;
    }

    template <typename T>
    concept arg = detail::detect_arg<std::remove_cvref_t<T>>::value;

    template <typename T>
    concept args = inherits<T, impl::args_tag>;

    template <typename T>
    concept chain = inherits<T, impl::chain_tag>;

    template <typename T>
    concept comprehension = inherits<T, impl::comprehension_tag>;

    template <typename T>
    concept unpack_operator = detail::enable_unpack_operator<std::remove_cvref_t<T>>;

    template <typename T>
    concept comprehension_operator =
        detail::enable_comprehension_operator<std::remove_cvref_t<T>>;

    template <typename Range, typename View>
    concept viewable = requires {
        std::views::all(std::declval<Range>()) | std::declval<View>();
    };

    template <typename Range, typename Func>
    concept transformable = !viewable<Range, Func> && requires {
        std::views::transform(std::declval<Range>(), std::declval<Func>());
    };

    template <typename T>
    concept signature = inherits<T, impl::signature_tag>;

    template <typename T>
    concept signature_defaults = inherits<T, impl::signature_defaults_tag>;

    template <typename T>
    concept signature_partial = inherits<T, impl::signature_partial_tag>;

    template <typename T>
    concept signature_bind = inherits<T, impl::signature_bind_tag>;

    template <typename T>
    concept def = inherits<T, impl::def_tag>;

    template <bertrand::static_str Name>
    concept arg_name = detail::arg_name<Name>;

    template <bertrand::static_str Name>
    concept variadic_args_name = detail::variadic_args_name<Name>;

    template <bertrand::static_str Name>
    concept variadic_kwargs_name = detail::variadic_kwargs_name<Name>;

    template <bertrand::static_str... Names>
    concept arg_names_are_unique = detail::arg_names_are_unique<Names...>;

    template <typename F>
    concept normalized_signature =
        bertrand::signature<F>::enable &&
        std::same_as<std::remove_cvref_t<F>, typename bertrand::signature<F>::type>;

}


/* Save a set of input arguments for later use.  Returns an args<> container, which
stores the arguments similar to a `std::tuple`, except that it is capable of storing
references and cannot be copied or moved.  Calling the args pack as an rvalue will
perfectly forward its values to an input function, without any extra copies, and at
most 2 moves per element (one when the pack is created and another when it is consumed).

Also provides utilities for compile-time argument manipulation wherever arbitrary lists
of types may be necessary. 

WARNING: Undefined behavior can occur if an lvalue is bound that falls out of scope
before the pack is consumed.  Such values will not have their lifetimes extended in any
way, and it is the user's responsibility to ensure that this is observed at all times.
Generally speaking, ensuring that no packs are returned out of a local context is
enough to satisfy this guarantee.  Typically, this class will be consumed within the
same context in which it was created, or in a downstream one where all of the objects
are still in scope, as a way of enforcing a certain order of operations.  Note that
this guidance does not apply to rvalues and temporaries, which are stored directly
within the pack for its natural lifetime. */
template <typename... Ts>
struct args : impl::ArgsBase<Ts...> {
private:

    template <typename>
    struct _concat;
    template <typename... Us>
    struct _concat<args<Us...>> { using type = args<Ts..., Us...>; };

    template <typename... packs>
    struct _product {
        /* permute<> iterates from left to right along the packs. */
        template <typename permuted, typename...>
        struct permute { using type = permuted; };
        template <typename... permuted, typename... types, typename... rest>
        struct permute<args<permuted...>, args<types...>, rest...> {

            /* accumulate<> iterates over the prior permutations and updates them
            with the types at this index. */
            template <typename accumulated, typename...>
            struct accumulate { using type = accumulated; };
            template <typename... accumulated, typename permutation, typename... others>
            struct accumulate<args<accumulated...>, permutation, others...> {

                /* append<> iterates from top to bottom for each type. */
                template <typename appended, typename...>
                struct append { using type = appended; };
                template <typename... appended, typename U, typename... Us>
                struct append<args<appended...>, U, Us...> {
                    using type = append<
                        args<appended..., typename permutation::template append<U>>,
                        Us...
                    >::type;
                };

                /* append<> extends the accumulated output at this index. */
                using type = accumulate<
                    typename append<args<accumulated...>, types...>::type,
                    others...
                >::type;
            };

            /* accumulate<> has to rebuild the output pack at each iteration. */
            using type = permute<
                typename accumulate<args<>, permuted...>::type,
                rest...
            >::type;
        };

        /* This pack is converted to a 2D pack to initialize the recursion. */
        using type = permute<args<args<Ts>...>, packs...>::type;
    };

    template <typename out, typename...>
    struct _unique { using type = out; };
    template <typename... Vs, typename U, typename... Us>
    struct _unique<args<Vs...>, U, Us...> {
        template <typename>
        struct helper { using type = args<Vs...>; };
        template <typename U2> requires (!(std::same_as<U2, Us> || ...))
        struct helper<U2> { using type = args<Vs..., U>; };
        using type = _unique<typename helper<U>::type, Us...>::type;
    };

    template <typename>
    struct _to_value;
    template <typename... Us>
    struct _to_value<args<Us...>> {
        template <typename out, typename...>
        struct filter { using type = out; };
        template <typename... Ws, typename V, typename... Vs>
        struct filter<args<Ws...>, V, Vs...> {
            template <typename>
            struct helper { using type = args<Ws...>; };
            template <typename V2>
                requires (!(std::same_as<std::remove_cvref_t<V2>, Ws> || ...))
            struct helper<V2> {
                using type = args<Ws..., std::conditional_t<
                    (std::same_as<
                        std::remove_cvref_t<V2>,
                        std::remove_cvref_t<Vs>
                    > || ...),
                    std::remove_cvref_t<V2>,
                    V2
                >>;
            };
            using type = filter<typename helper<V>::type, Vs...>::type;
        };
        using type = filter<args<>, Us...>::type;
    };

    template <typename out, size_t I>
    struct _get_base { using type = out; };
    template <typename... Us, size_t I> requires (I < sizeof...(Ts))
    struct _get_base<impl::ArgsBase<Us...>, I> {
        using type = _get_base<
            impl::ArgsBase<Us..., meta::unpack_type<I, Ts...>>,
            I + 1
        >::type;
    };
    template <size_t I> requires (I < sizeof...(Ts))
    using get_base = _get_base<impl::ArgsBase<>, I>::type;

    template <size_t I> requires (I < sizeof...(Ts))
    decltype(auto) forward() {
        if constexpr (std::is_lvalue_reference_v<meta::unpack_type<I, Ts...>>) {
            return get_base<I>::value;
        } else {
            return std::move(get_base<I>::value);
        }
    }

public:
    /* The total number of arguments being stored. */
    [[nodiscard]] static constexpr size_t size() noexcept { return sizeof...(Ts); }
    [[nodiscard]] static constexpr bool empty() noexcept { return !sizeof...(Ts); }

    template <typename T>
    static constexpr size_t index_of = meta::index_of<T, Ts...>;
    template <typename T>
    static constexpr bool contains = index_of<T> != size();

    /* Evaluate a control structure's `::enable` state by inserting this pack's
    template parameters. */
    template <template <typename...> class Control>
    static constexpr bool enable = Control<Ts...>::enable;

    /* Evaluate a control structure's `::type` state by inserting this pack's
    template parameters, assuming they are valid. */
    template <template <typename...> class Control> requires (enable<Control>)
    using type = Control<Ts...>::type;

    /* Get the type at index I. */
    template <size_t I> requires (I < size())
    using at = meta::unpack_type<I, Ts...>;

    /* Get a new pack with the type appended. */
    template <typename T>
    using append = args<Ts..., T>;

    /* Get a new pack that combines the contents of this pack with another. */
    template <meta::args T>
    using concat = _concat<T>::type;

    /* Get a pack of packs containing all unique permutations of the types in this
    parameter pack and all others, returning their Cartesian product.  */
    template <meta::args... packs> requires (size() > 0 && ((packs::size() > 0) && ...))
    using product = _product<packs...>::type;

    /* Get a new pack with exact duplicates filtered out, accounting for cvref
    qualifications. */
    using unique = _unique<args<>, Ts...>::type;

    /* Get a new pack with duplicates filtered out, replacing any types that differ
    only in cvref qualifications with an unqualified equivalent, thereby forcing a
    copy/move. */
    using to_value = _to_value<unique>::type;

    template <std::convertible_to<Ts>... Us>
    args(Us&&... args) : impl::ArgsBase<Ts...>(
        std::forward<Us>(args)...
    ) {}

    args(args&&) = default;

    args(const args&) = delete;
    args& operator=(const args&) = delete;
    args& operator=(args&&) = delete;

    /* Get the argument at index I. */
    template <size_t I> requires (I < size())
    [[nodiscard]] decltype(auto) get() && {
        if constexpr (std::is_lvalue_reference_v<meta::unpack_type<I, Ts...>>) {
            return get_base<I>::value;
        } else {
            return std::move(get_base<I>::value);
        }
    }

    /* Calling a pack as an rvalue will perfectly forward the input arguments to an
    input function that is templated to accept them. */
    template <typename Func>
        requires (!(std::is_void_v<Ts> || ...) && std::is_invocable_v<Func, Ts...>)
    decltype(auto) operator()(Func&& func) && {
        return [&]<size_t... Is>(std::index_sequence<Is...>) {
            return func(forward<Is>()...);
        }(std::index_sequence_for<Ts...>{});
    }
};


template <typename... Ts>
args(Ts&&...) -> args<Ts...>;


/* A higher-order function that merges a sequence of component functions into a single
operation.  When called, the chain will evaluate the first function with the input
arguments, then pass the result to the next function, and so on, until the final result
is returned. */
template <typename F, typename... Fs>
    requires (
        !std::is_reference_v<F> &&
        !(std::is_reference_v<Fs> || ...)
    )
struct chain : impl::chain_tag {
private:
    F func;

public:
    /* The number of component functions in the chain. */
    [[nodiscard]] static constexpr size_t size() noexcept { return 1; }

    /* Get the type of the component function at index I. */
    template <size_t I> requires (I < size())
    using at = F;

    template <meta::is<F> First>
    constexpr chain(First&& func) : func(std::forward<First>(func)) {}

    /* Invoke the function chain, piping the return value from the first function into
    the input for the second function, and so on. */
    template <typename Self, typename... A> requires (std::is_invocable_v<F, A...>)
    constexpr decltype(auto) operator()(this Self&& self, A&&... args) {
        return std::forward<Self>(self).func(std::forward<A>(args)...);
    }

    /* Get the component function at index I. */
    template <size_t I, typename Self> requires (I < size())
    [[nodiscard]] constexpr decltype(auto) get(this Self&& self) noexcept {
        return std::forward<Self>(self).func;
    }
};


template <typename F1, typename F2, typename... Fs>
    requires (
        !std::is_reference_v<F1> &&
        !std::is_reference_v<F2> &&
        !(std::is_reference_v<Fs> || ...)
    )
struct chain<F1, F2, Fs...> : chain<F2, Fs...> {
private:
    using base = chain<F2, Fs...>;

    F1 func;

    template <size_t I>
    struct _at { using type = base::template at<I - 1>; };
    template <>
    struct _at<0> { using type = F1; };

    template <typename R, typename...>
    struct _chainable { static constexpr bool value = true; };
    template <typename R, typename G, typename... Gs>
    struct _chainable<R, G, Gs...> {
        template <typename H>
        static constexpr bool invoke = false;
        template <typename H> requires (std::is_invocable_v<H, R>)
        static constexpr bool invoke<H> =
            _chainable<typename std::invoke_result_t<H, R>, Gs...>::value;
        static constexpr bool value = invoke<G>;
    };
    template <typename... A>
    static constexpr bool chainable =
        _chainable<typename std::invoke_result_t<F1, A...>, F2, Fs...>::value;

public:
    /* The number of component functions in the chain. */
    [[nodiscard]] static constexpr size_t size() noexcept { return base::size() + 1; }

    /* Get the type of the component function at index I. */
    template <size_t I> requires (I < size())
    using at = _at<I>::type;

    template <meta::is<F1> First, meta::is<F2> Next, meta::is<Fs>... Rest>
    constexpr chain(First&& first, Next&& next, Rest&&... rest) :
        base(std::forward<Next>(next), std::forward<Rest>(rest)...),
        func(std::forward<First>(first))
    {}

    /* Invoke the function chain, piping the return value from the first function into
    the input for the second function, and so on. */
    template <typename Self, typename... A>
        requires (std::is_invocable_v<F1, A...> && chainable<A...>)
    constexpr decltype(auto) operator()(this Self&& self, A&&... args) {
        return static_cast<meta::qualify<base, Self>>(std::forward<Self>(self))(
            std::forward<Self>(self).func(std::forward<A>(args)...)
        );
    }

    /* Get the component function at index I. */
    template <size_t I, typename Self> requires (I < size())
    [[nodiscard]] constexpr decltype(auto) get(this Self&& self) noexcept {
        if constexpr (I == 0) {
            return std::forward<Self>(self).func;
        } else {
            using parent = meta::qualify<base, Self>;
            return static_cast<parent>(std::forward<Self>(self)).template get<I - 1>();
        }
    }
};


template <typename F, typename... Fs>
chain(F, Fs...) -> chain<F, Fs...>;


template <meta::chain Self, meta::chain Next>
[[nodiscard]] constexpr auto operator>>(Self&& self, Next&& next) {
    return []<size_t... Is, size_t... Js>(
        std::index_sequence<Is...>,
        std::index_sequence<Js...>,
        auto&& self,
        auto&& next
    ) {
        return chain(
            std::forward<decltype(self)>(self).template get<Is>()...,
            std::forward<decltype(next)>(next).template get<Js>()...
        );
    }(
        std::make_index_sequence<std::remove_cvref_t<Self>::size()>{},
        std::make_index_sequence<std::remove_cvref_t<Next>::size()>{},
        std::forward<Self>(self),
        std::forward<Next>(next)
    );
}


template <meta::chain Self, typename Next>
[[nodiscard]] constexpr auto operator>>(Self&& self, Next&& next) {
    return []<size_t... Is>(std::index_sequence<Is...>, auto&& self, auto&& next) {
        return chain(
            std::forward<decltype(self)>(self).template get<Is>()...,
            std::forward<decltype(next)>(next)
        );
    }(
        std::make_index_sequence<std::remove_cvref_t<Self>::size()>{},
        std::forward<Self>(self),
        std::forward<Next>(next)
    );
}


template <typename Prev, meta::chain Self>
[[nodiscard]] constexpr auto operator>>(Prev&& prev, Self&& self) {
    return []<size_t... Is>(std::index_sequence<Is...>, auto&& prev, auto&& self) {
        return chain(
            std::forward<decltype(prev)>(prev),
            std::forward<decltype(self)>(self).template get<Is>()...
        );
    }(
        std::make_index_sequence<std::remove_cvref_t<Self>::size()>{},
        std::forward<Prev>(prev),
        std::forward<Self>(self)
    );
}


/* A range adaptor returned by the `->*` operator, which allows for Python-style
iterator comprehensions in C++.  This is essentially equivalent to a
`std::views::transform_view`, but with the caveat that any function which returns
another view will have its result flattened into the output, allowing for nested
comprehensions and filtering according to Python semantics. */
template <typename Range, typename Func> requires (meta::transformable<Range, Func>)
struct comprehension :
    std::ranges::view_interface<comprehension<Range, Func>>,
    impl::comprehension_tag
{
private:
    using View = decltype(std::views::transform(
        std::declval<Range>(),
        std::declval<Func>()
    ));
    using Value = std::ranges::range_value_t<View>;

    View view;

    template <typename>
    struct iterator {
        using Begin = std::ranges::iterator_t<const View>;
        using End = std::ranges::sentinel_t<const View>;
    };
    template <typename Value> requires (std::ranges::view<std::remove_cvref_t<Value>>)
    struct iterator<Value> {
        struct Begin {
            using iterator_category = std::input_iterator_tag;
            using difference_type = std::ranges::range_difference_t<const Value>;
            using value_type = std::ranges::range_value_t<const Value>;
            using pointer = value_type*;
            using reference = value_type&;

            using Iter = std::ranges::iterator_t<const View>;
            using End = std::ranges::sentinel_t<const View>;
            Iter iter;
            End end;
            struct Inner {
                using Iter = std::ranges::iterator_t<const Value>;
                using End = std::ranges::sentinel_t<const Value>;
                Value curr;
                Iter iter = std::ranges::begin(curr);
                End end = std::ranges::end(curr);
            } inner;

            Begin(Iter&& begin, End&& end) :
                iter(std::move(begin)),
                end(std::move(end)),
                inner([](Iter& begin, End& end) -> Inner {
                    while (begin != end) {
                        Inner curr = {*begin};
                        if (curr.iter != curr.end) {
                            return curr;
                        }
                        ++begin;
                    }
                    return {};
                }(this->iter, this->end))
            {}

            Begin& operator++() {
                if (++inner.iter == inner.end) {
                    while (++iter != end) {
                        inner.curr = *iter;
                        inner.iter = std::ranges::begin(inner.curr);
                        inner.end = std::ranges::end(inner.curr);
                        if (inner.iter != inner.end) {
                            break;
                        }
                    }
                }
                return *this;
            }

            Begin operator++(int) {
                Begin copy = *this;
                ++(*this);
                return copy;
            }

            decltype(auto) operator*() const { return *inner.iter; }
            decltype(auto) operator->() const { return inner.iter.operator->(); }

            friend bool operator==(const Begin& self, sentinel) {
                return self.iter == self.end;
            }

            friend bool operator==(sentinel, const Begin& self) {
                return self.iter == self.end;
            }

            friend bool operator!=(const Begin& self, sentinel) {
                return self.iter != self.end;
            }

            friend bool operator!=(sentinel, const Begin& self) {
                return self.iter != self.end;
            }
        };
        using End = sentinel;
    };

    using Begin = iterator<Value>::Begin;
    using End = iterator<Value>::End;

public:
    comprehension() = default;
    comprehension(Range range, Func func) :
        view(std::views::transform(
            std::forward<Range>(range),
            std::forward<Func>(func)
        ))
    {}

    [[nodiscard]] Begin begin() const {
        if constexpr (std::ranges::view<std::remove_cvref_t<Value>>) {
            return {std::ranges::begin(view), std::ranges::end(view)};
        } else {
            return std::ranges::begin(view);
        }
    }

    [[nodiscard]] End end() const {
        if constexpr (std::ranges::view<std::remove_cvref_t<Value>>) {
            return {};
        } else {
            return std::ranges::end(view);
        }
    }

    /* Implicitly convert the comprehension into any type that can be constructed from
    the iterator pair. */
    template <typename T> requires (std::constructible_from<T, Begin, End>)
    [[nodiscard]] operator T() const { return T(begin(), end()); }
};


template <typename Range, typename Func> requires (meta::transformable<Range, Func>)
comprehension(Range&&, Func&&) -> comprehension<Range, Func>;


/* A keyword parameter pack obtained by double-dereferencing a mapping-like container
within a Python-style function call. */
template <meta::mapping_like T>
    requires (
        meta::has_size<T> &&
        std::convertible_to<typename std::remove_reference_t<T>::key_type, std::string>
    )
struct kwarg_pack {
    using key_type = std::remove_reference_t<T>::key_type;
    using mapped_type = std::remove_reference_t<T>::mapped_type;

    static constexpr static_str name = "";
    static constexpr impl::ArgKind kind = impl::ArgKind::VAR | impl::ArgKind::KW;
    using type = mapped_type;
    template <typename... Vs>
    using bind = kwarg_pack;
    using bound_to = bertrand::args<>;
    using unbind = kwarg_pack;
    template <static_str N>
    using with_name = kwarg_pack;
    template <typename V>
    using with_type = kwarg_pack;

    T value;

private:
    template <typename, typename>
    friend struct meta::detail::detect_arg;
    using _detect_arg = void;

    template <typename U>
    static constexpr bool can_iterate =
        meta::yields_pairs_with<U, key_type, mapped_type> ||
        meta::has_items<U> ||
        (meta::has_keys<U> && meta::has_values<U>) ||
        (meta::yields<U, key_type> && meta::lookup_yields<U, mapped_type, key_type>) ||
        (meta::has_keys<U> && meta::lookup_yields<U, mapped_type, key_type>);

    auto transform() const {
        if constexpr (meta::yields_pairs_with<T, key_type, mapped_type>) {
            return value;

        } else if constexpr (meta::has_items<T>) {
            return value.items();

        } else if constexpr (meta::has_keys<T> && meta::has_values<T>) {
            return std::ranges::views::zip(value.keys(), value.values());

        } else if constexpr (
            meta::yields<T, key_type> && meta::lookup_yields<T, mapped_type, key_type>
        ) {
            return std::ranges::views::transform(
                value,
                [&](const key_type& key) {
                    return std::make_pair(key, value[key]);
                }
            );

        } else {
            return std::ranges::views::transform(
                value.keys(),
                [&](const key_type& key) {
                    return std::make_pair(key, value[key]);
                }
            );
        }
    }

public:

    [[nodiscard]] auto size() const { return std::ranges::size(value); }

    template <typename Self> requires (can_iterate<T>)
    [[nodiscard]] auto begin(this const Self& self) {
        return std::ranges::begin(self.transform());
    }

    template <typename Self> requires (can_iterate<T>)
    [[nodiscard]] auto cbegin(this const Self& self) {
        return self.begin();
    }

    template <typename Self> requires (can_iterate<T>)
    [[nodiscard]] auto end(this const Self& self) {
        return std::ranges::end(self.transform());
    }

    template <typename Self> requires (can_iterate<T>)
    [[nodiscard]] auto cend(this const Self& self) {
        return self.end();
    }
};


template <meta::mapping_like T>
    requires (
        meta::has_size<T> &&
        std::convertible_to<typename std::remove_reference_t<T>::key_type, std::string>
    )
kwarg_pack(T&&) -> kwarg_pack<T>;


/* A positional parameter pack obtained by dereferencing an iterable container within
a Python-style function call. */
template <meta::iterable T> requires (meta::has_size<T>)
struct arg_pack {
private:
    template <typename, typename>
    friend struct meta::detail::detect_arg;
    using _detect_arg = void;

public:
    static constexpr static_str name = "";
    static constexpr impl::ArgKind kind = impl::ArgKind::VAR | impl::ArgKind::POS;
    using type = meta::iter_type<T>;
    template <typename... Vs>
    using bind = arg_pack;
    using bound_to = bertrand::args<>;
    using unbind = arg_pack;
    template <static_str N>
    using with_name = arg_pack;
    template <typename V>
    using with_type = arg_pack;

    T value;

    [[nodiscard]] auto size() const { return std::ranges::size(value); }
    [[nodiscard]] auto begin() const { return std::ranges::begin(value); }
    [[nodiscard]] auto cbegin() const { return begin(); }
    [[nodiscard]] auto end() const { return std::ranges::end(value); }
    [[nodiscard]] auto cend() const { return end(); }

    template <typename Self>
        requires (meta::mapping_like<T> && std::convertible_to<
            typename std::remove_reference_t<T>::key_type,
            std::string
        >)
    [[nodiscard]] auto operator*(this const Self& self) {
        return kwarg_pack<T>{std::forward<Self>(self).value};
    }
};


template <meta::iterable T> requires (meta::has_size<T>)
arg_pack(T&&) -> arg_pack<T>;


/* A placeholder for a C++ template argument that can be used to back a `bertrand::Arg`
annotation.  Users can add custom predicates by inheriting from this type and
overriding the `::enable` field, in order to model arbitrary C++ template constraints.
The default always returns true, mimicking the unconstrained `typename`/`class`
keywords in a C++ template declaration. 


/// TODO: document precisely how this works

*/
struct generic {
    template <typename... Args>
    static constexpr bool enable = true;
};


/* A family of compile-time argument annotations that represent positional and/or
keyword arguments to a Python-style function.  Modifiers can be added to an argument
to indicate its kind, such as positional-only, keyword-only, optional, bound to a
partial value, or variadic (inferred from the argument name via leading `*` or `**`
prefixes).  The default (without any modifiers) is positional-or-keyword, unbound, and
required, similar to Python.

Note that this class makes careful use of aggregate initialization to extend the
lifetime of temporaries, meaning that it is safe to use with arbitrarily-qualified
reference types.  Such references are guaranteed to remain valid for their full,
natural lifespan, as if they were declared without the enclosing `Arg<>` wrapper.  In
particular, that allows `Arg<>` annotations to be freely declared as function
arguments without interfering with existing C++ parameter passing semantics. */
template <static_str Name, typename T>
    requires (
        meta::arg_name<Name> ||
        meta::variadic_args_name<Name> ||
        meta::variadic_kwargs_name<Name>
    )
struct Arg;


/* Specialization for standard args, with typical alphanumeric + underscore names. */
template <static_str Name, typename T> requires (meta::arg_name<Name>)
struct Arg<Name, T> {
private:
    template <typename, typename>
    friend struct meta::detail::detect_arg;
    using _detect_arg = void;

    using Value = std::remove_reference_t<T>;

    /// NOTE: there's a lot of code duplication here.  This is intentional, in order to
    /// maximize the clarity of error messages, restrict the ways in which annotations
    /// can be combined, and allow aggregate initialization to handle temporaries
    /// correctly.  The alternative would be macros, separate template helpers, or
    /// inheritance, all of which obfuscate errors, are inflexible, and/or complicate
    /// aggregate initialization.

public:
    static constexpr static_str name = Name;
    static constexpr impl::ArgKind kind = impl::ArgKind::POS | impl::ArgKind::KW;
    using type = T;
    using bound_to = bertrand::args<>;
    using unbind = Arg;
    template <static_str N> requires (meta::arg_name<N>)
    using with_name = Arg<N, T>;
    template <typename V> requires (!std::is_void_v<V>)
    using with_type = Arg<Name, V>;

    type value;

    [[nodiscard]] constexpr type operator*() && { std::forward<type>(value); }
    [[nodiscard]] constexpr Value& operator*() & { return value; }
    [[nodiscard]] constexpr Value* operator->() { return &value; }
    [[nodiscard]] constexpr const Value& operator*() const & { return value; }
    [[nodiscard]] constexpr const Value* operator->() const { return &value; }

    /* Argument rvalues are normally generated whenever a function is called.  Making
    them convertible to the underlying type means they can be used to call external
    C++ functions that are not aware of Python argument annotations, in a way that
    conforms to traits like `std::is_invocable<>`, etc. */
    [[nodiscard]] constexpr operator type() && { return std::forward<type>(value); }

    /* Conversions to other types are also allowed, as long as the argument is given as
    an rvalue and the underlying type supports it. */
    template <typename U> requires (std::convertible_to<type, U>)
    [[nodiscard]] constexpr operator U() && { return std::forward<type>(value); }

    /* Argument rvalues are also convertible to other annotated argument types, in
    order to ease the metaprogramming requirements.  This effectively means that all
    trailing annotation types are mutually interconvertible. */
    struct opt;
    struct pos;
    struct kw;

    template <typename... Vs>
    static constexpr bool can_bind = false;
    template <std::convertible_to<type> V>
    static constexpr bool can_bind<V> =
        !meta::arg_traits<V>::opt() &&
        !meta::arg_traits<V>::variadic() &&
        (meta::arg_traits<V>::name.empty() || meta::arg_traits<V>::name == name);

    /* Bind a partial value to this argument. */
    template <std::convertible_to<type>... Vs>
        requires (
            sizeof...(Vs) == 1 &&
            (!meta::arg_traits<Vs>::opt() && ...) &&
            (!meta::arg_traits<Vs>::variadic() && ...) &&
            ((meta::arg_traits<Vs>::name.empty() || meta::arg_traits<Vs>::name == name) && ...)
        )
    using bind = impl::BoundArg<Arg, Vs...>;

    template <typename U>
    [[nodiscard]] constexpr operator bind<U>() && {
        return {std::forward<type>(value)};
    }

    [[nodiscard]] constexpr operator opt() && { return {std::forward<type>(value)}; }
    template <typename U>
    [[nodiscard]] constexpr operator typename opt::template bind<U>() && {
        return {std::forward<type>(value)};
    }

    [[nodiscard]] constexpr operator pos() && { return {std::forward<type>(value)}; }
    template <typename U>
    [[nodiscard]] constexpr operator typename pos::template bind<U>() && {
        return {std::forward<type>(value)};
    }

    [[nodiscard]] constexpr operator typename pos::opt() && { return {std::forward<type>(value)}; }
    template <typename U>
    [[nodiscard]] constexpr operator typename pos::opt::template bind<U>() && {
        return {std::forward<type>(value)};
    }

    [[nodiscard]] constexpr operator kw() && { return {std::forward<type>(value)}; }
    template <typename U>
    [[nodiscard]] constexpr operator typename kw::template bind<U>() && {
        return {std::forward<type>(value)};
    }

    [[nodiscard]] constexpr operator typename kw::opt() && { return {std::forward<type>(value)}; }
    template <typename U>
    [[nodiscard]] constexpr operator typename kw::opt::template bind<U>() && {
        return {std::forward<type>(value)};
    }

    /* Marks the argument as optional. */
    struct opt {
    private:
        template <typename, typename>
        friend struct meta::detail::detect_arg;
        using _detect_arg = void;

    public:
        static constexpr static_str name = Name;
        static constexpr impl::ArgKind kind = Arg::kind | impl::ArgKind::OPT;
        using type = T;
        using bound_to = bertrand::args<>;
        using unbind = opt;
        template <static_str N> requires (meta::arg_name<N>)
        using with_name = Arg<N, T>::opt;
        template <typename V> requires (!std::is_void_v<V>)
        using with_type = Arg<Name, V>::opt;

        type value;

        [[nodiscard]] constexpr type operator*() && { std::forward<type>(value); }
        [[nodiscard]] constexpr Value& operator*() & { return value; }
        [[nodiscard]] constexpr Value* operator->() { return &value; }
        [[nodiscard]] constexpr const Value& operator*() const & { return value; }
        [[nodiscard]] constexpr const Value* operator->() const { return &value; }
        template <typename U> requires (std::convertible_to<Arg, U>)
        [[nodiscard]] constexpr operator U() && { return Arg{std::forward<type>(value)}; }

        template <typename... Vs>
        static constexpr bool can_bind = Arg::can_bind<Vs...>;

        template <std::convertible_to<type>... Vs>
            requires (
                sizeof...(Vs) == 1 &&
                (!meta::arg_traits<Vs>::opt() && ...) &&
                (!meta::arg_traits<Vs>::variadic() && ...) &&
                ((meta::arg_traits<Vs>::name.empty() || meta::arg_traits<Vs>::name == name) && ...)
            )
        using bind = impl::BoundArg<opt, Vs...>;
    };

    /* Marks the argument as positional-only. */
    struct pos {
    private:
        template <typename, typename>
        friend struct meta::detail::detect_arg;
        using _detect_arg = void;

    public:
        static constexpr static_str name = Name;
        static constexpr impl::ArgKind kind = impl::ArgKind::POS;
        using type = T;
        using bound_to = bertrand::args<>;
        using unbind = pos;
        template <static_str N> requires (meta::arg_name<N>)
        using with_name = Arg<N, T>::pos;
        template <typename V> requires (!std::is_void_v<V>)
        using with_type = Arg<Name, V>::pos;

        type value;

        [[nodiscard]] constexpr type operator*() && { std::forward<type>(value); }
        [[nodiscard]] constexpr Value& operator*() & { return value; }
        [[nodiscard]] constexpr Value* operator->() { return &value; }
        [[nodiscard]] constexpr const Value& operator*() const & { return value; }
        [[nodiscard]] constexpr const Value* operator->() const { return &value; }
        template <typename U> requires (std::convertible_to<Arg, U>)
        [[nodiscard]] constexpr operator U() && { return Arg{std::forward<type>(value)}; }

        template <typename... Vs>
        static constexpr bool can_bind = false;
        template <std::convertible_to<type> V>
        static constexpr bool can_bind<V> =
            meta::arg_traits<V>::posonly() &&
            !meta::arg_traits<V>::opt() &&
            !meta::arg_traits<V>::variadic() &&
            (meta::arg_traits<V>::name.empty() || meta::arg_traits<V>::name == name);

        template <std::convertible_to<type>... Vs>
            requires (
                sizeof...(Vs) == 1 &&
                (meta::arg_traits<Vs>::posonly() && ...) &&
                (!meta::arg_traits<Vs>::opt() && ...) &&
                (!meta::arg_traits<Vs>::variadic() && ...) &&
                ((meta::arg_traits<Vs>::name.empty() || meta::arg_traits<Vs>::name == name) && ...)
            )
        using bind = impl::BoundArg<pos, Vs...>;

        struct opt {
        private:
            template <typename, typename>
            friend struct meta::detail::detect_arg;
            using _detect_arg = void;

        public:
            static constexpr static_str name = Name;
            static constexpr impl::ArgKind kind = pos::kind | impl::ArgKind::OPT;
            using type = T;
            using bound_to = bertrand::args<>;
            using unbind = opt;
            template <static_str N> requires (meta::arg_name<N>)
            using with_name = Arg<N, T>::pos::opt;
            template <typename V> requires (!std::is_void_v<V>)
            using with_type = Arg<Name, V>::pos::opt;

            type value;

            [[nodiscard]] constexpr type operator*() && { std::forward<type>(value); }
            [[nodiscard]] constexpr Value& operator*() & { return value; }
            [[nodiscard]] constexpr Value* operator->() { return &value; }
            [[nodiscard]] constexpr const Value& operator*() const & { return value; }
            [[nodiscard]] constexpr const Value* operator->() const { return &value; }
            template <typename U> requires (std::convertible_to<Arg, U>)
            [[nodiscard]] constexpr operator U() && { return Arg{std::forward<type>(value)}; }

            template <typename... Vs>
            static constexpr bool can_bind = pos::can_bind<Vs...>;

            template <std::convertible_to<type>... Vs>
                requires (
                    sizeof...(Vs) == 1 &&
                    (meta::arg_traits<Vs>::posonly() && ...) &&
                    (!meta::arg_traits<Vs>::opt() && ...) &&
                    (!meta::arg_traits<Vs>::variadic() && ...) &&
                    ((meta::arg_traits<Vs>::name.empty() || meta::arg_traits<Vs>::name == name) && ...)
                )
            using bind = impl::BoundArg<opt, Vs...>;
        };
    };

    /* Marks the argument as keyword-only. */
    struct kw {
    private:
        template <typename, typename>
        friend struct meta::detail::detect_arg;
        using _detect_arg = void;

    public:
        static constexpr static_str name = Name;
        static constexpr impl::ArgKind kind = impl::ArgKind::KW;
        using type = T;
        using bound_to = bertrand::args<>;
        using unbind = kw;
        template <static_str N> requires (meta::arg_name<N>)
        using with_name = Arg<N, T>::kw;
        template <typename V> requires (!std::is_void_v<V>)
        using with_type = Arg<Name, V>::kw;

        type value;

        [[nodiscard]] constexpr type operator*() && { std::forward<type>(value); }
        [[nodiscard]] constexpr Value& operator*() & { return value; }
        [[nodiscard]] constexpr Value* operator->() { return &value; }
        [[nodiscard]] constexpr const Value& operator*() const & { return value; }
        [[nodiscard]] constexpr const Value* operator->() const { return &value; }
        template <typename U> requires (std::convertible_to<Arg, U>)
        [[nodiscard]] constexpr operator U() && { return Arg{std::forward<type>(value)}; }

        template <typename... Vs>
        static constexpr bool can_bind = false;
        template <std::convertible_to<type> V>
        static constexpr bool can_bind<V> =
            meta::arg_traits<V>::kw() &&
            !meta::arg_traits<V>::opt() &&
            !meta::arg_traits<V>::variadic() &&
            (meta::arg_traits<V>::name.empty() || meta::arg_traits<V>::name == name);

        template <std::convertible_to<type>... Vs>
            requires (
                sizeof...(Vs) == 1 &&
                (meta::arg_traits<Vs>::kw() && ...) &&
                (!meta::arg_traits<Vs>::opt() && ...) &&
                (!meta::arg_traits<Vs>::variadic() && ...) &&
                ((meta::arg_traits<Vs>::name.empty() || meta::arg_traits<Vs>::name == name) && ...)
            )
        using bind = impl::BoundArg<kw, Vs...>;

        struct opt {
        private:
            template <typename, typename>
            friend struct meta::detail::detect_arg;
            using _detect_arg = void;

        public:
            static constexpr static_str name = Name;
            static constexpr impl::ArgKind kind = kw::kind | impl::ArgKind::OPT;
            using type = T;
            using bound_to = bertrand::args<>;
            using unbind = opt;
            template <static_str N> requires (meta::arg_name<N>)
            using with_name = Arg<N, T>::kw::opt;
            template <typename V> requires (!std::is_void_v<V>)
            using with_type = Arg<Name, V>::kw::opt;

            type value;

            [[nodiscard]] constexpr type operator*() && { std::forward<type>(value); }
            [[nodiscard]] constexpr Value& operator*() & { return value; }
            [[nodiscard]] constexpr Value* operator->() { return &value; }
            [[nodiscard]] constexpr const Value& operator*() const & { return value; }
            [[nodiscard]] constexpr const Value* operator->() const { return &value; }
            template <typename U> requires (std::convertible_to<Arg, U>)
            [[nodiscard]] constexpr operator U() && { return Arg{std::forward<type>(value)}; }

            template <typename... Vs>
            static constexpr bool can_bind = kw::can_bind<Vs...>;

            template <std::convertible_to<type>... Vs>
                requires (
                    sizeof...(Vs) == 1 &&
                    (meta::arg_traits<Vs>::kw() && ...) &&
                    (!meta::arg_traits<Vs>::opt() && ...) &&
                    (!meta::arg_traits<Vs>::variadic() && ...) &&
                    ((meta::arg_traits<Vs>::name.empty() || meta::arg_traits<Vs>::name == name) && ...)
                )
            using bind = impl::BoundArg<opt, Vs...>;
        };
    };
};


/* Specialization for generic args, which have no definite type until they are
explicitly bound. */
template <static_str Name, std::derived_from<generic> Check>
    requires (meta::arg_name<Name>)
struct Arg<Name, Check> {
    /// TODO: same interface as other arguments, but cannot be constructed until
    /// ::bind<T> is used, at which point it becomes a normal Arg<Name, T>.  This will
    /// also have to be accounted for within the call logic somehow.
};


/* Specialization for variadic positional args, whose names are prefixed by a leading
asterisk. */
template <static_str Name, typename T> requires (meta::variadic_args_name<Name>)
struct Arg<Name, T> {
private:
    template <typename, typename>
    friend struct meta::detail::detect_arg;
    using _detect_arg = void;

public:
    static constexpr static_str name = static_str<>::removeprefix<Name, "*">();
    static constexpr impl::ArgKind kind = impl::ArgKind::VAR | impl::ArgKind::POS;
    using type = T;
    using bound_to = bertrand::args<>;
    using unbind = Arg;
    template <static_str N> requires (meta::arg_name<N>)
    using with_name = Arg<"*" + N, T>;
    template <typename V> requires (!std::is_void_v<V>)
    using with_type = Arg<Name, V>;

    using vec = std::vector<std::conditional_t<
        std::is_lvalue_reference_v<T>,
        std::reference_wrapper<T>,
        std::remove_reference_t<T>
    >>;

    vec value;

    [[nodiscard]] constexpr vec& operator*() { return value; }
    [[nodiscard]] constexpr const vec& operator*() const { return value; }
    [[nodiscard]] constexpr vec* operator->() { return &value; }
    [[nodiscard]] constexpr const vec* operator->() const { return &value; }

    template <typename... Vs>
    static constexpr bool can_bind = false;
    template <std::convertible_to<T>... Vs>
    static constexpr bool can_bind<Vs...> =
        (meta::arg_traits<Vs>::posonly() && ...) &&
        (!meta::arg_traits<Vs>::opt() && ...) &&
        (!meta::arg_traits<Vs>::variadic() && ...) &&
        meta::arg_names_are_unique<meta::arg_traits<Vs>::name...>;

    template <std::convertible_to<T>... Vs>
        requires (
            (meta::arg_traits<Vs>::posonly() && ...) &&
            (!meta::arg_traits<Vs>::opt() && ...) &&
            (!meta::arg_traits<Vs>::variadic() && ...) &&
            meta::arg_names_are_unique<meta::arg_traits<Vs>::name...>
        )
    using bind = impl::BoundArg<Arg, Vs...>;

    [[nodiscard]] constexpr operator vec() && { return std::move(value); }
    template <typename U> requires (std::convertible_to<vec, U>)
    [[nodiscard]] constexpr operator U() && { return std::move(value); }
    template <typename... Us>
    [[nodiscard]] constexpr operator bind<Us...>() && {
        return {std::forward<type>(value)};
    }

    [[nodiscard]] constexpr decltype(auto) operator[](vec::size_type i) {
        return value[i];
    }
    [[nodiscard]] constexpr decltype(auto) operator[](vec::size_type i) const {
        return value[i];
    }
};


/* Specialization for variadic positional args with generic types, whose names are
prefixed by a leading asterisk.  Like other generic arguments, these have no definite
type until they are explicitly bound. */
template <static_str Name, std::derived_from<generic> Check>
    requires (meta::variadic_args_name<Name>)
struct Arg<Name, Check> {
    /// TODO: same interface as other arguments, but cannot be constructed until
    /// ::bind<T> is used, at which point it becomes a normal Arg<Name, T>.  This will
};


/* Specialization for variadic keyword args, whose names are prefixed by 2 leading
asterisks. */
template <static_str Name, typename T> requires (meta::variadic_kwargs_name<Name>)
struct Arg<Name, T> {
private:
    template <typename, typename>
    friend struct meta::detail::detect_arg;
    using _detect_arg = void;

public:
    static constexpr static_str name = static_str<>::removeprefix<Name, "**">();
    static constexpr impl::ArgKind kind = impl::ArgKind::VAR | impl::ArgKind::KW;
    using type = T;
    using bound_to = bertrand::args<>;
    using unbind = Arg;
    template <static_str N> requires (meta::arg_name<N>)
    using with_name = Arg<"**" + N, T>;
    template <typename V> requires (!std::is_void_v<V>)
    using with_type = Arg<Name, V>;

    using map = std::unordered_map<std::string, std::conditional_t<
        std::is_lvalue_reference_v<type>,
        std::reference_wrapper<type>,
        std::remove_reference_t<type>
    >>;

    map value;

    [[nodiscard]] constexpr map& operator*() { return value; }
    [[nodiscard]] constexpr const map& operator*() const { return value; }
    [[nodiscard]] constexpr map* operator->() { return &value; }
    [[nodiscard]] constexpr const map* operator->() const { return &value; }

    template <typename... Vs>
    static constexpr bool can_bind = false;
    template <std::convertible_to<T>... Vs>
    static constexpr bool can_bind<Vs...> =
        (meta::arg_traits<Vs>::kw() && ...) &&
        (!meta::arg_traits<Vs>::opt() && ...) &&
        (!meta::arg_traits<Vs>::variadic() && ...) &&
        meta::arg_names_are_unique<meta::arg_traits<Vs>::name...>;

    template <std::convertible_to<T>... Vs>
        requires (
            (meta::arg_traits<Vs>::kw() && ...) &&
            (!meta::arg_traits<Vs>::opt() && ...) &&
            (!meta::arg_traits<Vs>::variadic() && ...) &&
            meta::arg_names_are_unique<meta::arg_traits<Vs>::name...>
        )
    using bind = impl::BoundArg<Arg, Vs...>;

    [[nodiscard]] constexpr operator map() && { return std::move(value); }
    template <typename U> requires (std::convertible_to<map, U>)
    [[nodiscard]] constexpr operator U() && { return std::move(value); }
    template <typename... Us>
    [[nodiscard]] constexpr operator bind<Us...>() && {
        return {std::forward<type>(value)};
    }

    [[nodiscard]] constexpr decltype(auto) operator[](const std::string& key) {
        return value.at(key);
    }
    [[nodiscard]] constexpr decltype(auto) operator[](std::string&& key) {
        return value.at(std::move(key));
    }

};


/* Specialization for variadic keyword args with generic types, whose names are
prefixed by 2 leading asterisks.  Like other generic arguments, these have no
definite type until they are explicitly bound. */
template <static_str Name, std::derived_from<generic> Check>
    requires (meta::variadic_kwargs_name<Name>)
struct Arg<Name, Check> {
    /// TODO: same interface as other arguments, but cannot be constructed until
    /// ::bind<T> is used, at which point it becomes a normal Arg<Name, T>.
};


namespace impl {

    template <typename Arg, typename T> requires (!meta::arg_traits<Arg>::variadic())
    struct BoundArg<Arg, T> {
    private:
        template <typename, typename>
        friend struct meta::detail::detect_arg;
        using _detect_arg = void;
        using Value = std::remove_reference_t<typename meta::arg_traits<Arg>::type>;

    public:
        static constexpr static_str name = meta::arg_traits<Arg>::name;
        static constexpr ArgKind kind = meta::arg_traits<Arg>::kind;
        using type = meta::arg_traits<Arg>::type;
        using bound_to = bertrand::args<T>;
        using unbind = Arg;
        template <static_str N> requires (meta::arg_name<N>)
        using with_name = BoundArg<
            typename meta::arg_traits<Arg>::template with_name<N>,
            T
        >;
        template <typename V>
            requires (
                !std::is_void_v<V> &&
                std::convertible_to<typename meta::arg_traits<T>::type, V>
            )
        using with_type = BoundArg<
            typename meta::arg_traits<Arg>::template with_type<V>,
            T
        >;

        type value;

        [[nodiscard]] constexpr Value& operator*() { return value; }
        [[nodiscard]] constexpr const Value& operator*() const { return value; }
        [[nodiscard]] constexpr Value* operator->() { return &value; }
        [[nodiscard]] constexpr const Value* operator->() const { return &value; }

        template <typename U> requires (std::convertible_to<unbind, U>)
        [[nodiscard]] constexpr operator U() && {
            if constexpr (meta::arg<unbind>) {
                return unbind{std::forward<type>(value)};
            } else {
                return std::forward<type>(value);
            }
        }

        template <typename... Vs>
        static constexpr bool can_bind = meta::arg_traits<Arg>::template can_bind<Vs...>;
        template <typename... Vs> requires (can_bind<Vs...>)
        using bind = meta::arg_traits<Arg>::template bind<Vs...>;
    };

    template <typename Arg, typename... Ts>
        requires (meta::arg_traits<Arg>::args() && sizeof...(Ts) > 0)
    struct BoundArg<Arg, Ts...> {
    private:
        template <typename, typename>
        friend struct meta::detail::detect_arg;
        using _detect_arg = void;

        template <typename, typename...>
        struct rebind;
        template <typename... curr, typename... Vs>
        struct rebind<bertrand::args<curr...>, Vs...> {
            static constexpr bool value =
                meta::arg_traits<Arg>::template can_bind<curr..., Vs...>;
            using type = meta::arg_traits<Arg>::template bind<curr..., Vs...>;
        };

    public:
        static constexpr static_str name = meta::arg_traits<Arg>::name;
        static constexpr ArgKind kind = meta::arg_traits<Arg>::kind;
        using type = meta::arg_traits<Arg>::type;
        using vec = std::vector<std::conditional_t<
            std::is_lvalue_reference_v<type>,
            std::reference_wrapper<type>,
            std::remove_reference_t<type>
        >>;
        using bound_to = bertrand::args<Ts...>;
        using unbind = Arg;
        template <static_str N> requires (meta::arg_name<N>)
        using with_name = BoundArg<
            typename meta::arg_traits<Arg>::template with_name<N>,
            Ts...
        >;
        template <typename V>
            requires (
                !std::is_void_v<V> &&
                (std::convertible_to<typename meta::arg_traits<Ts>::type, V> && ...)
            )
        using with_type = BoundArg<
            typename meta::arg_traits<Arg>::template with_type<V>,
            Ts...
        >;

        vec value;

        [[nodiscard]] constexpr vec& operator*() { return value; }
        [[nodiscard]] constexpr const vec& operator*() const { return value; }
        [[nodiscard]] constexpr vec* operator->() { return &value; }
        [[nodiscard]] constexpr const vec* operator->() const { return &value; }
        template <typename U> requires (std::convertible_to<unbind, U>)
        [[nodiscard]] constexpr operator U() && {
            return unbind{std::forward<type>(value)};
        }

        [[nodiscard]] constexpr decltype(auto) operator[](vec::size_type i) {
            return value[i];
        }
        [[nodiscard]] constexpr decltype(auto) operator[](vec::size_type i) const {
            return value[i];
        }

        template <typename... Vs>
        static constexpr bool can_bind = rebind<bound_to, Vs...>::value;
        template <typename... Vs> requires (can_bind<Vs...>)
        using bind = rebind<bound_to, Vs...>::type;
    };

    template <typename Arg, typename... Ts>
        requires (meta::arg_traits<Arg>::kwargs() && sizeof...(Ts) > 0)
    struct BoundArg<Arg, Ts...> {
    private:
        template <typename, typename>
        friend struct meta::detail::detect_arg;
        using _detect_arg = void;

        template <typename, typename...>
        struct rebind;
        template <typename... curr, typename... Vs>
        struct rebind<bertrand::args<curr...>, Vs...> {
            static constexpr bool value =
                meta::arg_traits<Arg>::template can_bind<curr..., Vs...>;
            using type = meta::arg_traits<Arg>::template bind<curr..., Vs...>;
        };

    public:
        static constexpr static_str name = meta::arg_traits<Arg>::name;
        static constexpr ArgKind kind = meta::arg_traits<Arg>::kind;
        using type = meta::arg_traits<Arg>::type;
        using map = std::unordered_map<std::string, std::conditional_t<
            std::is_lvalue_reference_v<type>,
            std::reference_wrapper<type>,
            std::remove_reference_t<type>
        >>;
        using bound_to = bertrand::args<Ts...>;
        using unbind = Arg;
        template <static_str N> requires (meta::arg_name<N>)
        using with_name = BoundArg<
            typename meta::arg_traits<Arg>::template with_name<N>,
            Ts...
        >;
        template <typename V>
            requires (
                !std::is_void_v<V> &&
                (std::convertible_to<typename meta::arg_traits<Ts>::type, V> && ...)
            )
        using with_type = BoundArg<
            typename meta::arg_traits<Arg>::template with_type<V>,
            Ts...
        >;

        map value;

        [[nodiscard]] constexpr map& operator*() { return value; }
        [[nodiscard]] constexpr const map& operator*() const { return value; }
        [[nodiscard]] constexpr map* operator->() { return &value; }
        [[nodiscard]] constexpr const map* operator->() const { return &value; }
        template <typename U> requires (std::convertible_to<unbind, U>)
        [[nodiscard]] constexpr operator U() && {
            return unbind{std::forward<type>(value)};
        }

        [[nodiscard]] constexpr decltype(auto) operator[](const std::string& key) {
            return value[key];
        }
        [[nodiscard]] constexpr decltype(auto) operator[](std::string&& key) {
            return value[std::move(key)];
        }

        template <typename... Vs>
        static constexpr bool can_bind = rebind<bound_to, Vs...>::value;
        template <typename... Vs> requires (can_bind<Vs...>)
        using bind = rebind<bound_to, Vs...>::type;
    };

    /* A singleton argument factory that allows keyword arguments to be constructed via
    familiar assignment syntax, which extends the lifetime of temporaries. */
    template <static_str Name>
    struct ArgFactory {
        template <typename T>
        constexpr Arg<Name, T>::kw operator=(T&& value) const {
            return {std::forward<T>(value)};
        }
    };

    template <typename R>
    struct signature_base : signature_tag {
        using Return = R;
    };

    inline std::string format_signature(
        const std::string& prefix,
        size_t max_width,
        size_t indent,
        std::vector<std::string>& components,
        size_t last_posonly,
        size_t first_kwonly
    ) {
        std::string param_open          = "(";
        std::string param_close         = ") -> ";
        std::string type_sep            = ": ";
        std::string default_sep         = " = ";
        std::string sep                 = ", ";
        std::string tab                 = std::string(indent, ' ');
        std::string line_sep            = "\n";
        std::string kwonly_sep          = "*";
        std::string posonly_sep         = "/";

        components.front() += param_open;
        components.back() = param_close + components.back();

        // add delimiters to parameters and compute hypothetical one-liner length
        size_t length = prefix.size() + components.front().size();
        if (components.size() > 2) {
            std::string& name = components[1];
            std::string& type = components[2];
            std::string& default_value = components[3];
            type = type_sep + type;
            if (!default_value.empty()) {
                default_value = default_sep + default_value;
            }
            length += name.size() + type.size() + default_value.size();
            if (length <= max_width) {
                for (size_t i = 4, end = components.size() - 1; i < end; i += 3) {
                    length += sep.size();
                    std::string& name = components[i];
                    std::string& type = components[i + 1];
                    std::string& default_value = components[i + 2];
                    name += type_sep;
                    if (!default_value.empty()) {
                        default_value = default_sep + default_value;
                    }
                    length += name.size() + type.size() + default_value.size();
                    size_t adjusted = (i - 4) / 3;
                    if (adjusted == last_posonly) {
                        length += sep.size() + posonly_sep.size();
                    } else if (adjusted == first_kwonly) {
                        length += sep.size() + kwonly_sep.size();
                    }
                }
            }
        }
        length += components.back().size();

        // if the whole signature fits on one line, return it as such
        if (length <= max_width) {
            std::string out;
            out.reserve(length);
            out += prefix;
            out += std::move(components.front());
            if (components.size() > 2) {
                size_t i = 1;
                size_t j = 0;
                if (j == first_kwonly) {
                    out += kwonly_sep + sep;
                }
                out += std::move(components[i++]);
                out += std::move(components[i++]);
                out += std::move(components[i++]);
                if (j == last_posonly) {
                    out += sep + posonly_sep;
                }
                ++j;
                for (size_t end = components.size() - 1; i < end; ++j) {
                    out += sep;
                    if (j == first_kwonly) {
                        out += kwonly_sep + sep;
                    }
                    out += std::move(components[i++]);
                    out += std::move(components[i++]);
                    out += std::move(components[i++]);
                    if (j == last_posonly) {
                        out += sep + posonly_sep;
                    }
                }
            }
            out += std::move(components.back());
            return out;
        }

        // otherwise, indent the parameters onto separate lines
        std::string out = prefix + components.front() + line_sep;
        std::string line = prefix + tab;
        if (components.size() > 2) {
            size_t i = 1;
            size_t j = 0;
            if (j == first_kwonly) {
                out += line + kwonly_sep + sep + line_sep;
            }
            std::string& name = components[i++];
            std::string& type = components[i++];
            std::string& default_value = components[i++];
            line += std::move(name);
            if (line.size() + type.size() <= max_width) {
                line += std::move(type);
            } else {
                out += std::move(line) + line_sep;
                line = prefix + tab + tab + std::move(type);
            }
            if (line.size() + default_value.size() <= max_width) {
                line += std::move(default_value);
            } else {
                out += std::move(line) + line_sep;
                line = prefix + tab + tab +
                    std::move(default_value).substr(1);  // remove leading space
            }
            out += line;
            if (j == last_posonly) {
                out += sep + line_sep + prefix + tab + posonly_sep;
            }
            for (size_t end = components.size() - 1; i < end; ++j) {
                out += sep + line_sep;
                line = prefix + tab;
                if (j == first_kwonly) {
                    out += line + kwonly_sep + sep + line_sep;
                }
                std::string& name = components[i++];
                std::string& type = components[i++];
                std::string& default_value = components[i++];
                line += std::move(name);
                if (line.size() + type.size() <= max_width) {
                    line += std::move(type);
                } else {
                    out += std::move(line) + line_sep;
                    line = prefix + tab + tab + std::move(type);
                }
                if (line.size() + default_value.size() <= max_width) {
                    line += std::move(default_value);
                } else {
                    out += std::move(line) + line_sep;
                    line = prefix + tab + tab +
                        std::move(default_value).substr(1);  // remove leading space
                }
                out += std::move(line);
                if (j == last_posonly) {
                    out += sep + line_sep + prefix + tab + posonly_sep;
                }
            }
            out += line_sep;
        }
        out += prefix + components.back();
        return out;
    }

    template <typename R, typename C, size_t I>
    struct chain_return_type { using type = R; };
    template <typename R, typename C, size_t I> requires (I < C::size())
    struct chain_return_type<R, C, I> {
        using type = chain_return_type<
            std::invoke_result_t<typename C::template at<I>, R>,
            C,
            I + 1
        >::type;
    };

}


/* A compile-time factory for binding keyword arguments with Python syntax.  constexpr
instances of this class can be used to provide an even more Pythonic syntax:

    constexpr auto x = arg<"x">;
    my_func(x = 42);
*/
template <static_str name> requires (meta::arg_name<name>)
constexpr impl::ArgFactory<name> arg {};


namespace meta {

    /* Manipulate a C++ argument annotation at compile time.  Unannotated types are
    treated as anonymous, positional-only, and required in order to preserve C++ style. */
    template <typename T>
    struct arg_traits {
    private:
        template <bertrand::static_str N>
        struct _with_name { using type = Arg<N, T>::pos; };
        template <bertrand::static_str N> requires (N.empty())
        struct _with_name<N> { using type = T; };

    public:
        using type                                  = T;
        static constexpr bertrand::static_str name  = "";
        static constexpr impl::ArgKind kind         = impl::ArgKind::POS;
        static constexpr bool posonly() noexcept    { return kind.posonly(); }
        static constexpr bool pos() noexcept        { return kind.pos(); }
        static constexpr bool args() noexcept       { return kind.args(); }
        static constexpr bool kwonly() noexcept     { return kind.kwonly(); }
        static constexpr bool kw() noexcept         { return kind.kw(); }
        static constexpr bool kwargs() noexcept     { return kind.kwargs(); }
        static constexpr bool bound() noexcept      { return bound_to::size() > 0; }
        static constexpr bool opt() noexcept        { return kind.opt(); }
        static constexpr bool variadic() noexcept   { return kind.variadic(); }

        template <typename... Vs>
        static constexpr bool can_bind = false;
        template <std::convertible_to<type> V>
        static constexpr bool can_bind<V> =
            arg_traits<V>::posonly() &&
            !arg_traits<V>::opt() &&
            !arg_traits<V>::variadic() &&
            (arg_traits<V>::name.empty() || arg_traits<V>::name == name);

        template <typename... Vs> requires (can_bind<Vs...>)
        using bind                                  = impl::BoundArg<T, Vs...>;
        using bound_to                              = bertrand::args<>;
        using unbind                                = T;
        template <bertrand::static_str N> requires (N.empty() || meta::arg_name<N>)
        using with_name                             = _with_name<N>::type;
        template <typename V> requires (!std::is_void_v<V>)
        using with_type                             = V;
    };

    /* Manipulate a C++ argument annotation at compile time.  Forwards to the annotated
    type's interface where possible. */
    template <meta::arg T>
    struct arg_traits<T> {
    private:
        using T2 = std::remove_cvref_t<T>;

        template <bertrand::static_str N>
        struct _with_name { using type = T2::template with_name<N>; };
        template <bertrand::static_str N> requires (N.empty())
        struct _with_name<N> { using type = T2::type; };

    public:
        using type                                  = T2::type;
        static constexpr bertrand::static_str name  = T2::name;
        static constexpr impl::ArgKind kind         = T2::kind;
        static constexpr bool posonly() noexcept    { return kind.posonly(); }
        static constexpr bool pos() noexcept        { return kind.pos(); }
        static constexpr bool args() noexcept       { return kind.args(); }
        static constexpr bool kwonly() noexcept     { return kind.kwonly(); }
        static constexpr bool kw() noexcept         { return kind.kw(); }
        static constexpr bool kwargs() noexcept     { return kind.kwargs(); }
        static constexpr bool bound() noexcept      { return bound_to::size() > 0; }
        static constexpr bool opt() noexcept        { return kind.opt(); }
        static constexpr bool variadic() noexcept   { return kind.variadic(); }

        template <typename... Vs>
        static constexpr bool can_bind = T2::template can_bind<Vs...>;

        template <typename... Vs> requires (can_bind<Vs...>)
        using bind                                  = T2::template bind<Vs...>;
        using unbind                                = T2::unbind;
        using bound_to                              = T2::bound_to;
        template <bertrand::static_str N> requires (N.empty() || meta::arg_name<N>)
        using with_name                             = _with_name<N>::type;
        template <typename V> requires (!std::is_void_v<V>)
        using with_type                             = T2::template with_type<V>;
    };

}


/* CTAD guide to simplify signature introspection.  Uses a dummy constructor, meaning
no work is done at runtime. */
template <typename T> requires (signature<T>::enable)
signature(const T&) -> signature<typename signature<T>::type>;


namespace impl {

    template <typename...>
    constexpr size_t n_posonly = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_posonly<T, Ts...> =
        n_posonly<Ts...> + meta::arg_traits<T>::posonly();

    template <typename...>
    constexpr size_t n_opt_posonly = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_opt_posonly<T, Ts...> =
        n_opt_posonly<Ts...> + (meta::arg_traits<T>::posonly() && meta::arg_traits<T>::opt());

    template <typename...>
    constexpr size_t n_partial_posonly = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_partial_posonly<T, Ts...> =
        n_partial_posonly<Ts...> + (meta::arg_traits<T>::posonly() && meta::arg_traits<T>::bound());

    template <typename...>
    constexpr size_t n_pos = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_pos<T, Ts...> =
        n_pos<Ts...> + meta::arg_traits<T>::pos();

    template <typename...>
    constexpr size_t n_opt_pos = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_opt_pos<T, Ts...> =
        n_opt_pos<Ts...> + (meta::arg_traits<T>::pos() && meta::arg_traits<T>::opt());

    template <typename...>
    constexpr size_t n_partial_pos = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_partial_pos<T, Ts...> =
        n_partial_pos<Ts...> + (meta::arg_traits<T>::pos() && meta::arg_traits<T>::bound());

    template <typename...>
    constexpr size_t n_partial_args = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_partial_args<T, Ts...> =
        meta::arg_traits<T>::args() ? meta::arg_traits<T>::bound_to::size() : n_partial_args<Ts...>;

    template <typename...>
    constexpr size_t n_kw = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_kw<T, Ts...> =
        n_kw<Ts...> + meta::arg_traits<T>::kw();

    template <typename...>
    constexpr size_t n_opt_kw = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_opt_kw<T, Ts...> =
        n_opt_kw<Ts...> + (meta::arg_traits<T>::kw() && meta::arg_traits<T>::opt());

    template <typename...>
    constexpr size_t n_partial_kw = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_partial_kw<T, Ts...> =
        n_partial_kw<Ts...> + (meta::arg_traits<T>::kw() && meta::arg_traits<T>::bound());

    template <typename...>
    constexpr size_t n_kwonly = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_kwonly<T, Ts...> =
        n_kwonly<Ts...> + meta::arg_traits<T>::kwonly();

    template <typename...>
    constexpr size_t n_opt_kwonly = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_opt_kwonly<T, Ts...> =
        n_opt_kwonly<Ts...> + (meta::arg_traits<T>::kwonly() && meta::arg_traits<T>::opt());

    template <typename...>
    constexpr size_t n_partial_kwonly = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_partial_kwonly<T, Ts...> =
        n_partial_kwonly<Ts...> + (meta::arg_traits<T>::kwonly() && meta::arg_traits<T>::bound());

    template <typename...>
    constexpr size_t n_partial_kwargs = 0;
    template <typename T, typename... Ts>
    constexpr size_t n_partial_kwargs<T, Ts...> =
        meta::arg_traits<T>::kwargs() ? meta::arg_traits<T>::bound_to::size() : n_partial_kwargs<Ts...>;

    template <typename...>
    constexpr size_t posonly_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t posonly_idx<T, Ts...> =
        meta::arg_traits<T>::posonly() ? 0 : posonly_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t opt_posonly_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t opt_posonly_idx<T, Ts...> =
        meta::arg_traits<T>::posonly() && meta::arg_traits<T>::opt() ? 0 : opt_posonly_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t partial_posonly_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t partial_posonly_idx<T, Ts...> =
        meta::arg_traits<T>::posonly() && meta::arg_traits<T>::bound() ? 0 : partial_posonly_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t pos_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t pos_idx<T, Ts...> =
        meta::arg_traits<T>::pos() ? 0 : pos_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t opt_pos_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t opt_pos_idx<T, Ts...> =
        meta::arg_traits<T>::pos() && meta::arg_traits<T>::opt() ? 0 : opt_pos_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t partial_pos_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t partial_pos_idx<T, Ts...> =
        meta::arg_traits<T>::pos() && meta::arg_traits<T>::bound() ? 0 : partial_pos_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t args_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t args_idx<T, Ts...> =
        meta::arg_traits<T>::args() ? 0 : args_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t kw_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t kw_idx<T, Ts...> =
        meta::arg_traits<T>::kw() ? 0 : kw_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t opt_kw_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t opt_kw_idx<T, Ts...> =
        meta::arg_traits<T>::kw() && meta::arg_traits<T>::opt() ? 0 : opt_kw_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t partial_kw_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t partial_kw_idx<T, Ts...> =
        meta::arg_traits<T>::kw() && meta::arg_traits<T>::bound() ? 0 : partial_kw_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t kwonly_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t kwonly_idx<T, Ts...> =
        meta::arg_traits<T>::kwonly() ? 0 : kwonly_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t opt_kwonly_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t opt_kwonly_idx<T, Ts...> =
        meta::arg_traits<T>::kwonly() && meta::arg_traits<T>::opt() ? 0 : opt_kwonly_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t partial_kwonly_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t partial_kwonly_idx<T, Ts...> =
        meta::arg_traits<T>::kwonly() && meta::arg_traits<T>::bound() ? 0 : partial_kwonly_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t kwargs_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t kwargs_idx<T, Ts...> =
        meta::arg_traits<T>::kwargs() ? 0 : kwargs_idx<Ts...> + 1;

    template <typename...>
    constexpr size_t opt_idx = 0;
    template <typename T, typename... Ts>
    constexpr size_t opt_idx<T, Ts...> =
        meta::arg_traits<T>::opt() ? 0 : opt_idx<Ts...> + 1;

    template <static_str, typename...>
    constexpr size_t arg_idx = 0;
    template <static_str N, typename A, typename... As>
    constexpr size_t arg_idx<N, A, As...> =
        N == meta::arg_traits<A>::name ? 0 : arg_idx<N, As...> + 1;

    template <typename... Ts>
    constexpr bool has_posonly = posonly_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_opt_posonly = opt_posonly_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_partial_posonly = partial_posonly_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_pos = pos_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_opt_pos = opt_pos_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_partial_pos = partial_pos_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_args = args_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_kw = kw_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_opt_kw = opt_kw_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_partial_kw = partial_kw_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_kwonly = kwonly_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_opt_kwonly = opt_kwonly_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_partial_kwonly = partial_kwonly_idx<Ts...> < sizeof...(Ts);
    template <typename... Ts>
    constexpr bool has_kwargs = kwargs_idx<Ts...> < sizeof...(Ts);

    template <typename... A>
    constexpr bool args_fit_within_bitset = sizeof...(A) <= MAX_ARGS;

    template <typename... A>
    constexpr bool no_qualified_args =
        !(meta::is_qualified<typename meta::arg_traits<A>::type> || ...);

    template <typename... A>
    constexpr bool no_qualified_arg_annotations =
        !((meta::arg<A> && meta::is_qualified<A>) || ...);

    template <size_t, typename...>
    constexpr bool _proper_argument_order = true;
    template <size_t I, typename... A> requires (I < sizeof...(A))
    constexpr bool _proper_argument_order<I, A...> = [] {
        using T = meta::unpack_type<I, A...>;
        constexpr size_t args_idx = impl::args_idx<A...>;
        constexpr size_t kw_idx = impl::kw_idx<A...>;
        constexpr size_t kwonly_idx = impl::kwonly_idx<A...>;
        constexpr size_t kwargs_idx = impl::kwargs_idx<A...>;
        constexpr size_t opt_idx = impl::opt_idx<A...>;
        return !((
            meta::arg_traits<T>::posonly() && (
                (I > std::min({args_idx, kw_idx, kwargs_idx})) ||
                (!meta::arg_traits<T>::opt() && I > opt_idx)
            )
        ) || (
            meta::arg_traits<T>::pos() && (
                (I > std::min({args_idx, kwonly_idx, kwargs_idx})) ||
                (!meta::arg_traits<T>::opt() && I > opt_idx)
            )
        ) || (
            meta::arg_traits<T>::args() && (I > std::min(kwonly_idx, kwargs_idx))
        ) || (
            meta::arg_traits<T>::kwonly() && (I > kwargs_idx)
        )) && _proper_argument_order<I + 1, A...>;
    }();
    template <typename... A>
    constexpr bool proper_argument_order = _proper_argument_order<0, A...>;

    template <size_t, typename...>
    constexpr bool _no_duplicate_args = true;
    template <size_t I, typename... A> requires (I < sizeof...(A))
    constexpr bool _no_duplicate_args<I, A...> = [] {
        using T = meta::unpack_type<I, A...>;
        constexpr size_t args_idx = impl::args_idx<A...>;
        constexpr size_t kwargs_idx = impl::kwargs_idx<A...>;
        if constexpr (meta::arg_traits<T>::name.empty()) {
            return !(
                (meta::arg_traits<T>::args() && I != args_idx) ||
                (meta::arg_traits<T>::kwargs() && I != kwargs_idx)
            ) && _no_duplicate_args<I + 1, A...>;
        } else {
            return !(
                (I != arg_idx<meta::arg_traits<T>::name, A...>) ||
                (meta::arg_traits<T>::args() && I != args_idx) ||
                (meta::arg_traits<T>::kwargs() && I != kwargs_idx)
            ) && _no_duplicate_args<I + 1, A...>;
        }
    }();
    template <typename... A>
    constexpr bool no_duplicate_args = _no_duplicate_args<0, A...>;

    template <typename... A>
    constexpr bitset<MAX_ARGS> required = 0;
    template <typename A, typename... As>
    constexpr bitset<MAX_ARGS> required<A, As...> =
        (required<As...> << 1) | !(meta::arg_traits<A>::opt() || meta::arg_traits<A>::variadic());

    /* A temporary container describing the contents of a `*` unpacking operator at a
    function's call site.  Encloses an iterator over the unpacked container, which is
    incremented every time an argument is consumed from the pack.  If it is not empty
    by the end of the call, then we know extra arguments were given that could not be
    matched. */
    template <typename Pack> requires (meta::arg_traits<Pack>::args())
    struct PositionalPack {
    private:
        template <typename, typename>
        friend struct meta::detail::detect_arg;
        using _detect_arg = void;

    public:
        static constexpr static_str name = Pack::name;
        static constexpr impl::ArgKind kind = Pack::kind;
        using type = Pack::type;
        template <typename... Vs>
        using bind = Pack::template bind<Vs...>;
        using bound_to = Pack::bound_to;
        using unbind = Pack::unbind;
        template <static_str N> requires (meta::arg_name<N>)
        using with_name = Pack::template with_name<N>;
        template <typename V> requires (!std::is_void_v<V>)
        using with_type = Pack::template with_type<V>;

        std::ranges::iterator_t<const Pack&> begin;
        std::ranges::sentinel_t<const Pack&> end;
        size_t size;

        PositionalPack(const Pack& pack) :
            begin(std::ranges::begin(pack)),
            end(std::ranges::end(pack)),
            size(std::ranges::size(pack))
        {}

        bool has_value() const {
            return begin != end;
        }

        decltype(auto) value() {
            decltype(auto) result = *begin;
            ++begin;
            return result;
        }

        void validate() {
            if (begin != end) {
                std::string message =
                    "too many arguments in positional parameter pack: ['" +
                    repr(*begin);
                while (++begin != end) {
                    message += "', '" + repr(*begin);
                }
                message += "']";
                throw TypeError(message);
            }
        }
    };

    /* A temporary container describing the contents of a `**` unpacking operator at a
    function's call site.  Encloses an unordered map of strings to values, which is
    destructively searched every time an argument is consumed from the pack.  If the
    map is not empty by the end of the call, then we know extra arguments were given
    that could not be matched. */
    template <typename Pack> requires (meta::arg_traits<Pack>::kwargs())
    struct KeywordPack {
    private:
        template <typename, typename>
        friend struct meta::detail::detect_arg;
        using _detect_arg = void;

        struct Hash {
            using is_transparent = void;
            static constexpr size_t operator()(std::string_view str) {
                return std::hash<std::string_view>{}(str);
            }
        };

        struct Equal {
            using is_transparent = void;
            static constexpr bool operator()(std::string_view lhs, std::string_view rhs) {
                return lhs == rhs;
            }
        };

    public:
        static constexpr static_str name = Pack::name;
        static constexpr impl::ArgKind kind = Pack::kind;
        using type = Pack::type;
        template <typename... Vs>
        using bind = Pack::template bind<Vs...>;
        using bound_to = Pack::bound_to;
        using unbind = Pack::unbind;
        template <static_str N> requires (meta::arg_name<N>)
        using with_name = Pack::template with_name<N>;
        template <typename V> requires (!std::is_void_v<V>)
        using with_type = Pack::template with_type<V>;

        using Map = std::unordered_map<
            std::string,
            typename Pack::mapped_type,
            Hash,
            Equal
        >;
        Map map;

        KeywordPack(const Pack& pack) :
            map([](const Pack& pack) {
                Map map;
                map.reserve(pack.size());
                for (auto&& [key, value] : pack) {
                    auto [it, inserted] = map.emplace(
                        std::forward<decltype(key)>(key),
                        std::forward<decltype(value)>(value)
                    );
                    if (!inserted) {
                        throw TypeError(
                            "duplicate keyword argument: '" + it->first + "'"
                        );
                    }
                }
                return map;
            }(pack))
        {}

        auto size() const { return map.size(); }
        auto empty() const { return map.empty(); }
        auto begin() { return map.begin(); }
        auto end() { return map.end(); }

        template <typename T>
        auto extract(T&& key) {
            return map.extract(std::forward<T>(key));
        }

        void validate() {
            if (!map.empty()) {
                auto it = map.begin();
                auto end = map.end();
                std::string message =
                    "unexpected keyword arguments: ['" + it->first;
                while (++it != end) {
                    message += "', '" + it->first;
                }
                message += "']";
                throw TypeError(message);
            }
        }
    };

    template <typename Pack>
    PositionalPack(const Pack&) -> PositionalPack<Pack>;
    template <typename Pack>
    KeywordPack(const Pack&) -> KeywordPack<Pack>;

    template <typename F, typename... A>
    constexpr decltype(auto) invoke_with_packs(F&& func, A&&... args) {
        static constexpr size_t n = sizeof...(A);
        static constexpr size_t args_idx = impl::args_idx<A...>;
        static constexpr size_t kwargs_idx = impl::kwargs_idx<A...>;

        if constexpr (args_idx < n && kwargs_idx < n) {
            return []<size_t... Prev, size_t... Next>(
                std::index_sequence<Prev...>,
                std::index_sequence<Next...>,
                auto&& func,
                auto&&... args
            ) {
                return std::forward<decltype(func)>(func)(
                    meta::unpack_arg<Prev>(
                        std::forward<decltype(args)>(args)...
                    )...,
                    impl::PositionalPack(meta::unpack_arg<args_idx>(
                        std::forward<decltype(args)>(args)...
                    )),
                    meta::unpack_arg<args_idx + 1 + Next>(
                        std::forward<decltype(args)>(args)...
                    )...,
                    impl::KeywordPack(meta::unpack_arg<kwargs_idx>(
                        std::forward<decltype(args)>(args)...
                    ))
                );
            }(
                std::make_index_sequence<args_idx>{},
                std::make_index_sequence<kwargs_idx - (args_idx + 1)>{},
                std::forward<F>(func),
                std::forward<A>(args)...
            );

        } else if constexpr (args_idx < n) {
            return []<size_t... Prev, size_t... Next>(
                std::index_sequence<Prev...>,
                std::index_sequence<Next...>,
                auto&& func,
                auto&&... args
            ) {
                return std::forward<decltype(func)>(func)(
                    meta::unpack_arg<Prev>(
                        std::forward<decltype(args)>(args)...
                    )...,
                    impl::PositionalPack(meta::unpack_arg<args_idx>(
                        std::forward<decltype(args)>(args)...
                    )),
                    meta::unpack_arg<args_idx + 1 + Next>(
                        std::forward<decltype(args)>(args)...
                    )...
                );
            }(
                std::make_index_sequence<args_idx>{},
                std::make_index_sequence<n - (args_idx + 1)>{},
                std::forward<F>(func),
                std::forward<A>(args)...
            );

        } else if constexpr (kwargs_idx < n) {
            return []<size_t... Prev>(
                std::index_sequence<Prev...>,
                auto&& func,
                auto&&... args
            ) {
                return std::forward<decltype(func)>(func)(
                    meta::unpack_arg<Prev>(
                        std::forward<decltype(args)>(args)...
                    )...,
                    impl::KeywordPack(meta::unpack_arg<kwargs_idx>(
                        std::forward<decltype(args)>(args)...
                    ))
                );
            }(
                std::make_index_sequence<kwargs_idx>{},
                std::forward<F>(func),
                std::forward<A>(args)...
            );

        } else {
            return std::forward<F>(func)(std::forward<A>(args)...);
        }
    }

    /* A single element stored in a signature::Partial or signature::Defaults tuple,
    which can be easily cross-referenced against the enclosing signature. */
    template <size_t I, static_str Name, typename T>
    struct SignatureElement {
        static constexpr size_t index = I;
        static constexpr static_str name = Name;
        using type = T;
        std::remove_cvref_t<type> value;
        constexpr meta::remove_rvalue<type> get() const { return value; }
        constexpr meta::remove_lvalue<type> get() && { return std::move(value); }
    };

    /* Build a sub-signature holding only the arguments marked as optional from an
    enclosing signature. */
    template <typename, typename...>
    struct _defaults_signature;
    template <typename... out, typename... Ts>
    struct _defaults_signature<args<out...>, Ts...> { using type = signature<void(out...)>; };
    template <typename... out, typename A, typename... As>
    struct _defaults_signature<args<out...>, A, As...> {
        template <typename>
        struct filter { using type = args<out...>; };
        template <typename T> requires (meta::arg_traits<T>::opt())
        struct filter<T> {
            using type = args<
                out...,
                typename Arg<meta::arg_traits<T>::name, typename meta::arg_traits<T>::type>::kw
            >;
        };
        using type = _defaults_signature<typename filter<A>::type, As...>::type;
    };
    template <typename... A>
    using defaults_signature = _defaults_signature<args<>, A...>::type;

    /* Build a std::tuple of SignatureElements to hold the default values themselves. */
    template <typename out, size_t, typename...>
    struct _defaults_tuple { using type = out; };
    template <typename... out, size_t I, typename A, typename... As>
    struct _defaults_tuple<std::tuple<out...>, I, A, As...> {
        template <typename>
        struct filter { using type = std::tuple<out...>; };
        template <typename T> requires (meta::arg_traits<T>::opt())
        struct filter<T> {
            using type = std::tuple<
                out...,
                SignatureElement<I, meta::arg_traits<T>::name, typename meta::arg_traits<T>::type>
            >;
        };
        using type = _defaults_tuple<typename filter<A>::type, I + 1, As...>::type;
    };
    template <typename... A>
    using defaults_tuple = _defaults_tuple<std::tuple<>, 0, A...>::type;

    /* Build a sub-signature holding only the bound arguments from an enclosing
    signature. */
    template <typename out, typename...>
    struct _partial_signature;
    template <typename... out, typename... Ts>
    struct _partial_signature<args<out...>, Ts...> { using type = signature<void(out...)>; };
    template <typename... out, typename A, typename... As>
    struct _partial_signature<args<out...>, A, As...> {
        template <typename>
        struct filter { using type = args<out...>; };
        template <typename T> requires (meta::arg_traits<T>::bound())
        struct filter<T> {
            template <typename>
            struct extend;
            template <typename... Ps>
            struct extend<args<Ps...>> {
                template <typename P>
                struct proper_name { using type = P; };
                template <typename P>
                    requires (!meta::arg_traits<T>::name.empty() && !meta::arg_traits<T>::variadic())
                struct proper_name<P> {
                    using base = Arg<meta::arg_traits<T>::name, typename meta::arg_traits<P>::type>;
                    using type = std::conditional_t<
                        meta::arg_traits<P>::pos(),
                        typename base::pos,
                        typename base::kw
                    >;
                };
                using type = args<out..., typename proper_name<Ps>::type...>;
            };
            using type = extend<typename meta::arg_traits<T>::bound_to>::type;
        };
        using type = _partial_signature<typename filter<A>::type, As...>::type;
    };
    template <typename... A>
    using partial_signature = _partial_signature<args<>, A...>::type;

    /* Build a std::tuple of Elements that hold the bound values in a way that can
    be cross-referenced with the target signature. */
    template <typename out, size_t, typename...>
    struct _partial_tuple { using type = out; };
    template <typename... out, size_t I, typename A, typename... As>
    struct _partial_tuple<std::tuple<out...>, I, A, As...> {
        template <typename>
        struct filter { using type = std::tuple<out...>; };
        template <typename T> requires (meta::arg_traits<T>::bound())
        struct filter<T> {
            template <typename>
            struct extend;
            template <typename... Ps>
            struct extend<args<Ps...>> {
                using type = std::tuple<
                    out...,
                    impl::SignatureElement<
                        I,
                        meta::arg_traits<Ps>::name,
                        typename meta::arg_traits<Ps>::type
                    >...
                >;
            };
            using type = extend<typename meta::arg_traits<T>::bound_to>::type;
        };
        using type = _partial_tuple<typename filter<A>::type, I + 1, As...>::type;
    };
    template <typename... A>
    using partial_tuple = _partial_tuple<std::tuple<>, 0, A...>::type;

    /* A single entry in a signature's parameter table, storing the argument name
    (which may be empty), kind (positional-only, optional, variadic, etc.), and its
    position within the enclosing parameter list.  Such parameters are typically
    returned by the index operator and associated accessors. */
    struct CppParam {
        template <size_t I, typename... Args>
        static constexpr CppParam create() noexcept {
            using T = meta::unpack_type<I, Args...>;
            return {
                .name = std::string_view(meta::arg_traits<T>::name),
                .kind = meta::arg_traits<T>::kind,
                .index = I,
            };
        }

        std::string_view name;
        ArgKind kind;
        size_t index;

        [[nodiscard]] constexpr bool posonly() const noexcept { return kind.posonly(); }
        [[nodiscard]] constexpr bool pos() const noexcept { return kind.pos(); }
        [[nodiscard]] constexpr bool args() const noexcept { return kind.args(); }
        [[nodiscard]] constexpr bool kw() const noexcept { return kind.kw(); }
        [[nodiscard]] constexpr bool kwonly() const noexcept { return kind.kwonly(); }
        [[nodiscard]] constexpr bool kwargs() const noexcept { return kind.kwargs(); }
        [[nodiscard]] constexpr bool opt() const noexcept { return kind.opt(); }
        [[nodiscard]] constexpr bool variadic() const noexcept { return kind.variadic(); }
    };

    /* Backs the pure-C++ `signature` class in a way that prevents unnecessary code
    duplication between specializations.  Python signatures can extend this base to
    avoid reimplementing C++ function logic internally. */
    template <typename Param, typename Return, typename... Args>
    struct CppSignature : signature_base<Return> {
        using type = Return(Args...);
        static constexpr bool enable = true;
        static constexpr bool python = false;
        static constexpr bool convertible_to_python = false;

        /* Dummy constructor for CTAD purposes.  This will be automatically inherited
        by all subclasses. */
        template <typename T> requires (signature<T>::enable)
        constexpr CppSignature(const T&) noexcept {}
        constexpr CppSignature() noexcept = default;

        template <typename... Values>
        struct Bind;

    protected:
        /* A flat array of callback objects whose indices are aligned to the enclosing
        parameter list. */
        using PositionalTable = std::array<Param, sizeof...(Args)>;
        static constexpr auto positional_table = []<size_t... Is>(std::index_sequence<Is...>) {
            return PositionalTable{Param::template create<Is, Args...>()...};
        }(std::index_sequence_for<Args...>{});

        /* In order to avoid superfluous compile errors, the perfect keyword hash map
        should not be created unless the signature is well-formed. */
        template <typename, size_t, typename...>
        struct get_names {
            using type = static_map<const Param&>;
            static constexpr type operator()(auto&&... callbacks) {
                return {};
            }
        };
        template <typename... out, size_t I, typename... Ts>
            requires (
                sizeof...(Ts) == 0 &&
                I < MAX_ARGS &&
                meta::strings_are_unique<meta::arg_traits<out>::name...> &&
                impl::minimal_perfect_hash<meta::arg_traits<out>::name...>::exists
            )
        struct get_names<args<out...>, I, Ts...> {
            using type = static_map<const Param&, meta::arg_traits<out>::name...>;
            static constexpr type operator()(const auto&... callbacks) {
                return {callbacks...};
            }
        };
        template <typename... out, size_t I, typename T, typename... Ts>
        struct get_names<args<out...>, I, T, Ts...> {
            template <typename>
            struct filter {
                using type = args<out...>;
                static constexpr auto operator()(const auto&... callbacks) noexcept {
                    return get_names<type, I + 1, Ts...>{}(callbacks...);
                }
            };
            template <typename U> requires (!meta::arg_traits<U>::name.empty())
            struct filter<U> {
                using type = args<out..., U>;
                static constexpr auto operator()(const auto&... callbacks) noexcept {
                    return get_names<type, I + 1, Ts...>{}(callbacks..., positional_table[I]);
                }
            };
            using type = get_names<typename filter<T>::type, I + 1, Ts...>::type;
            static constexpr auto operator()(const auto&... callbacks) noexcept {
                return filter<T>{}(callbacks...);
            }
        };

        using NameTable = get_names<args<>, 0, Args...>::type;
        static constexpr NameTable name_table = get_names<args<>, 0, Args...>{}();

        template <size_t I, typename T> requires (I < sizeof...(Args))
        static constexpr auto to_arg(T&& value) -> meta::unpack_type<I, Args...> {
            if constexpr (meta::arg<meta::unpack_type<I, Args...>>) {
                return {std::forward<T>(value)};
            } else {
                return std::forward<T>(value);
            }
        };

        template <typename out, typename...>
        struct _Unbind;
        template <typename R, typename... out, typename... Ts>
        struct _Unbind<R(out...), Ts...> { using type = signature<R(out...)>; };
        template <typename R, typename... out, typename A, typename... As>
        struct _Unbind<R(out...), A, As...> {
            using type = _Unbind<R(out..., typename meta::arg_traits<A>::unbind), As...>::type;
        };

    public:
        [[nodiscard]] static constexpr size_t size() noexcept { return sizeof...(Args); }
        [[nodiscard]] static constexpr bool empty() noexcept { return !sizeof...(Args); }
        [[nodiscard]] static auto begin() noexcept { return positional_table.begin(); }
        [[nodiscard]] static auto cbegin() noexcept { return positional_table.cbegin(); }
        [[nodiscard]] static auto rbegin() noexcept { return positional_table.rbegin(); }
        [[nodiscard]] static auto crbegin() noexcept { return positional_table.crbegin(); }
        [[nodiscard]] static auto end() noexcept { return positional_table.end(); }
        [[nodiscard]] static auto cend() noexcept { return positional_table.cend(); }
        [[nodiscard]] static auto rend() noexcept { return positional_table.rend(); }
        [[nodiscard]] static auto crend() noexcept { return positional_table.crend(); }

        static constexpr size_t n_posonly           = impl::n_posonly<Args...>;
        static constexpr size_t n_pos               = impl::n_pos<Args...>;
        static constexpr size_t n_kw                = impl::n_kw<Args...>;
        static constexpr size_t n_kwonly            = impl::n_kwonly<Args...>;
        static constexpr bool has_posonly           = impl::has_posonly<Args...>;
        static constexpr bool has_pos               = impl::has_pos<Args...>;
        static constexpr bool has_kw                = impl::has_kw<Args...>;
        static constexpr bool has_kwonly            = impl::has_kwonly<Args...>;
        static constexpr bool has_args              = impl::has_args<Args...>;
        static constexpr bool has_kwargs            = impl::has_kwargs<Args...>;
        static constexpr size_t posonly_idx         = impl::posonly_idx<Args...>;
        static constexpr size_t pos_idx             = impl::pos_idx<Args...>;
        static constexpr size_t kw_idx              = impl::kw_idx<Args...>;
        static constexpr size_t kwonly_idx          = impl::kwonly_idx<Args...>;
        static constexpr size_t args_idx            = impl::args_idx<Args...>;
        static constexpr size_t kwargs_idx          = impl::kwargs_idx<Args...>;
        static constexpr size_t opt_idx             = impl::opt_idx<Args...>;

        template <size_t I> requires (I < size())
        using at = meta::unpack_type<I, Args...>;

        template <typename R>
        using with_return = signature<R(Args...)>;

        template <typename... A>
        using with_args = signature<Return(A...)>;

        /* Check whether a given positional index is within the bounds of the enclosing
        signature. */
        template <size_t I>
        [[nodiscard]] static constexpr bool contains() noexcept { return I < size(); }
        [[nodiscard]] static constexpr bool contains(size_t i) noexcept { return i < size(); }

        /* Check whether a given argument name is present within the enclosing signature. */
        template <static_str Key>
        [[nodiscard]] static constexpr bool contains() noexcept {
            return NameTable::template contains<Key>();
        }
        template <typename T> requires (NameTable::template hashable<T>)
        [[nodiscard]] static constexpr bool contains(T&& key) noexcept {
            return name_table.contains(std::forward<T>(key));
        }

        /* Look up the callback object associated with the argument at index I if it is
        within range.  Fails to compile otherwise. */
        template <size_t I> requires (I < size())
        [[nodiscard]] static constexpr const Param& get() noexcept {
            return positional_table[I];
        }

        /* Look up the callback object associated with the argument at index I if it is
        within range.  Throws an `IndexError` otherwise. */
        [[nodiscard]] static constexpr const Param& get(size_t i) {
            if (i < size()) {
                return positional_table[i];
            } else {
                throw IndexError(std::to_string(i));
            }
        }

        /* Look up the callback object associated with the named argument if it is present
        within the signature.  Fails to compile otherwise. */
        template <static_str Key> requires (contains<Key>())
        [[nodiscard]] static constexpr const Param& get() noexcept {
            return name_table.template get<Key>();
        }

        /* Look up the callback object associated with the named argument if it is present
        within the signature.  Throws a `KeyError` otherwise. */
        template <typename T> requires (NameTable::template hashable<T>)
        [[nodiscard]] static constexpr const Param& get(T&& key) {
            if (const Param* result = name_table[std::forward<T>(key)]) {
                return *result;
            } else {
                throw KeyError(key);
            }
        }

        /* Get a pointer to the callback object for a given argument.  Returns nullptr if
        the index is out of range. */
        [[nodiscard]] static constexpr const Param* operator[](size_t i) noexcept {
            return i < size() ? &positional_table[i] : nullptr;
        }

        /* Get a pointer to the callback object for the named argument.  Returns nullptr if
        the name is not recognized. */
        template <typename T> requires (NameTable::template hashable<T>)
        [[nodiscard]] static constexpr const Param* operator[](T&& key) noexcept {
            return name_table[std::forward<T>(key)];
        }

        /* Find the index corresponding to the named argument. */
        template <static_str Key> requires (contains<Key>())
        [[nodiscard]] static constexpr size_t index() noexcept {
            return get<Key>().index;
        }
        template <typename T> requires (NameTable::template hashable<T>)
        [[nodiscard]] static constexpr size_t index(T&& key) {
            return get(std::forward<T>(key)).index;
        }

        /* True if a given function can be called with this signature's arguments and
        returns a compatible type, after accounting for implicit conversions. */
        template <typename Func>
        static constexpr bool invocable = std::is_invocable_r_v<Return, Func, Args...>;

        /* True if the return type lacks cvref qualifications. */
        static constexpr bool no_qualified_return = !meta::is_qualified<Return>;

        /* True if the arguments fit within the width of the bitset necessary to validate
        them during overload resolution (64). */
        static constexpr bool args_fit_within_bitset = impl::args_fit_within_bitset<Args...>;

        /* True if the arguments are given in the proper order (no positional after keyword,
        no required after optional, etc.). */
        static constexpr bool proper_argument_order = impl::proper_argument_order<Args...>;

        /* True if the argument types lack cvref qualifications. */
        static constexpr bool no_qualified_args = impl::no_qualified_args<Args...>;

        /* True if none of the `Arg<>` annotations are themselves cvref-qualified. */
        static constexpr bool no_qualified_arg_annotations =
            impl::no_qualified_arg_annotations<Args...>;

        /* True if there are no duplicate parameter names and at most one variadic
        positional/keyword argument, respectively. */
        static constexpr bool no_duplicate_args = impl::no_duplicate_args<Args...>;

        /* A bitmask with a 1 in the position of all of the required arguments in the
        parameter list.

        Each callback stores an index within the enclosing parameter list, which can be
        transformed into a one-hot encoded mask that will be progressively joined as each
        argument is processed.  The result can then be compared to this constant to quickly
        determine if all required arguments have been accounted for.  If that comparison
        fails, then further bitwise inspection can be done to determine exactly which
        arguments are missing, as well as their names for a comprehensive error message.

        Note that this mask effectively limits the maximum number of arguments that a
        function can accept, to an amount determined by the `MAX_ARGS` build flag. */
        static constexpr bitset<MAX_ARGS> required = impl::required<Args...>;

        /* A tuple holding a default value for every argument in the enclosing
        parameter list that is marked as optional.  One of these must be provided
        whenever a C++ function is invoked, and constructing one requires that the
        initializers match a sub-signature consisting only of the optional args as
        keyword-only parameters for clarity.  The result may be empty if there are no
        optional arguments in the enclosing signature, in which case the constructor
        will be optimized out. */
        struct Defaults : impl::signature_defaults_tag {
        protected:
            friend CppSignature;
            using Inner = impl::defaults_signature<Args...>;
            using Tuple = impl::defaults_tuple<Args...>;

            Tuple values;

            template <size_t, typename>
            static constexpr size_t _find = 0;
            template <size_t I, typename T, typename... Ts>
            static constexpr size_t _find<I, std::tuple<T, Ts...>> =
                I == T::index ? 0 : _find<I, std::tuple<Ts...>> + 1;

            template <typename D, size_t I>
            static constexpr bool _copy = I == D::size();
            template <typename D, size_t I> requires (I < std::tuple_size_v<Tuple>)
            static constexpr bool _copy<D, I> = [] {
                if constexpr (I < D::size()) {
                    using T = CppSignature::at<std::tuple_element_t<I, Tuple>::index>;
                    using U = D::template at<I>;
                    return std::same_as<
                        typename meta::arg_traits<T>::unbind,
                        typename meta::arg_traits<U>::unbind
                    > && _copy<D, I + 1>;
                } else {
                    return false;
                }
            }();
            template <meta::inherits<impl::signature_defaults_tag> D>
            static constexpr bool copy = _copy<std::remove_cvref_t<D>, 0>;

            template <size_t J, typename... A>
            static constexpr decltype(auto) build(A&&... args) {
                using T = std::tuple_element_t<J, Tuple>;
                constexpr size_t idx = impl::arg_idx<T::name, A...>;
                return meta::unpack_arg<idx>(std::forward<A>(args)...);
            }

        public:
            using type                              = signature<Return(Args...)>;
            static constexpr size_t n_posonly       = impl::n_opt_posonly<Args...>;
            static constexpr size_t n_pos           = impl::n_opt_pos<Args...>;
            static constexpr size_t n_kw            = impl::n_opt_kw<Args...>;
            static constexpr size_t n_kwonly        = impl::n_opt_kwonly<Args...>;
            static constexpr bool has_posonly       = impl::has_opt_posonly<Args...>;
            static constexpr bool has_pos           = impl::has_opt_pos<Args...>;
            static constexpr bool has_kw            = impl::has_opt_kw<Args...>;
            static constexpr bool has_kwonly        = impl::has_opt_kwonly<Args...>;
            static constexpr size_t posonly_idx     = impl::opt_posonly_idx<Args...>;
            static constexpr size_t pos_idx         = impl::opt_pos_idx<Args...>;
            static constexpr size_t kw_idx          = impl::opt_kw_idx<Args...>;
            static constexpr size_t kwonly_idx      = impl::opt_kwonly_idx<Args...>;

            /* The total number of optional arguments in the enclosing signature. */
            [[nodiscard]] static constexpr size_t size() noexcept { return Inner::size(); }
            [[nodiscard]] static constexpr bool empty() noexcept { return Inner::empty(); }

            /* Check whether a given index is within the bounds of the default value
            tuple. */
            template <size_t I>
            [[nodiscard]] static constexpr bool contains() noexcept { return I < size(); }
            [[nodiscard]] static constexpr bool contains(size_t i) noexcept {return i < size(); }

            /* Check whether the named argument is contained within the default value
            tuple. */
            template <static_str Key>
            [[nodiscard]] static constexpr bool contains() noexcept {
                return Inner::template contains<Key>();
            }
            template <typename T> requires (NameTable::template hashable<T>)
            [[nodiscard]] static constexpr bool contains(T&& key) noexcept {
                return Inner::contains(std::forward<T>(key));
            }
        
            /* Get the default value at index I of the tuple.  Use find<> to correlate
            an index from the enclosing signature if needed.  If the defaults container
            is used as an lvalue, then this will either directly reference the internal
            value if the corresponding argument expects an lvalue, or a copy if it
            expects an unqualified or rvalue type.  If the defaults container is given
            as an rvalue instead, then the copy will be optimized to a move. */
            template <size_t J> requires (J < size())
            constexpr decltype(auto) get() const {
                return std::get<J>(values).get();
            }
            template <size_t J> requires (J < size())
            constexpr decltype(auto) get() && {
                return std::move(std::get<J>(values)).get();
            }

            /* Get the default value associated with the named argument, if it is
            marked as optional.  If the defaults container is used as an lvalue, then
            this will either directly reference the internal value if the corresponding
            argument expects an lvalue, or a copy if it expects an unqualified or
            rvalue type.  If the defaults container is given as an rvalue instead, then
            the copy will be optimized to a move. */
            template <static_str Name> requires (contains<Name>())
            constexpr decltype(auto) get() const {
                return std::get<index<Name>()>(values).get();
            }
            template <static_str Name> requires (contains<Name>())
            constexpr decltype(auto) get() && {
                return std::move(std::get<index<Name>()>(values)).get();
            }

            /* Get the index of a named argument within the default values tuple. */
            template <static_str Key> requires (contains<Key>())
            [[nodiscard]] static constexpr size_t index() noexcept {
                return Inner::template index<Key>();
            }
            template <typename T> requires (NameTable::template hashable<T>)
            [[nodiscard]] static constexpr size_t index(T&& key) {
                return Inner::index(std::forward<T>(key));
            }

            /* Given an index into the enclosing signature, find the corresponding index
            in the defaults tuple if the corresponding argument is marked as optional. */
            template <size_t I> requires (meta::arg_traits<typename CppSignature::at<I>>::opt())
            static constexpr size_t find = _find<I, Tuple>;

            /* Given an index into the defaults tuple, find the corresponding index in
            the enclosing parameter list. */
            template <size_t J> requires (J < size())
            static constexpr size_t rfind = std::tuple_element_t<J, Tuple>::index;

            template <size_t J> requires (J < size())
            using at = CppSignature::at<rfind<J>>;

            /* Bind an argument list to the default values to enable the constructor. */
            template <typename... A>
            using Bind = Inner::template Bind<A...>;

            template <typename... A>
                requires (
                    !(meta::arg_traits<A>::variadic() || ...) &&
                    Bind<A...>::proper_argument_order &&
                    Bind<A...>::no_qualified_arg_annotations &&
                    Bind<A...>::no_duplicate_args &&
                    Bind<A...>::no_conflicting_values &&
                    Bind<A...>::no_extra_positional_args &&
                    Bind<A...>::no_extra_keyword_args &&
                    Bind<A...>::satisfies_required_args &&
                    Bind<A...>::can_convert
                )
            constexpr Defaults(A&&... args) : values(
                []<size_t... Js>(std::index_sequence<Js...>, auto&&... args) -> Tuple {
                    return {{build<Js>(std::forward<decltype(args)>(args)...)}...};
                }(std::index_sequence_for<A...>{}, std::forward<A>(args)...)
            ) {}

            template <meta::inherits<impl::signature_defaults_tag> D> requires (copy<D>)
            constexpr Defaults(D&& other) :
                values([]<size_t... Js>(std::index_sequence<Js...>, auto&& other) -> Tuple {
                    return {{std::forward<decltype(other)>(other).template get<Js>()}...};
                }(
                    std::make_index_sequence<std::remove_cvref_t<D>::size()>{},
                    std::forward<decltype(other)>(other)
                ))
            {}
        };

        /* Instance-level constructor for a `::Defaults` tuple. */
        template <typename... A>
            requires (
                !(meta::arg_traits<A>::variadic() || ...) &&
                Defaults::template Bind<A...>::proper_argument_order &&
                Defaults::template Bind<A...>::no_qualified_arg_annotations &&
                Defaults::template Bind<A...>::no_duplicate_args &&
                Defaults::template Bind<A...>::no_conflicting_values &&
                Defaults::template Bind<A...>::no_extra_positional_args &&
                Defaults::template Bind<A...>::no_extra_keyword_args &&
                Defaults::template Bind<A...>::satisfies_required_args &&
                Defaults::template Bind<A...>::can_convert
            )
        [[nodiscard]] static constexpr Defaults defaults(A&&... args) {
            return Defaults(std::forward<A>(args)...);
        }

        /* A tuple holding a partial value for every bound argument in the enclosing
        parameter list.  One of these must be provided whenever a C++ function is
        invoked, and constructing one requires that the initializers match a
        sub-signature consisting only of the bound args as positional-only and
        keyword-only parameters for clarity.  The result may be empty if there are no
        bound arguments in the enclosing signature, in which case the constructor will
        be optimized out. */
        struct Partial : impl::signature_partial_tag {
        protected:
            friend CppSignature;
            using Inner = impl::partial_signature<Args...>;
            using Tuple = impl::partial_tuple<Args...>;

            Tuple values;

            template <size_t K, typename... A>
            static constexpr decltype(auto) build(A&&... args) {
                using T = std::tuple_element_t<K, Tuple>;
                constexpr size_t idx = impl::arg_idx<T::name, A...>;
                if constexpr (!T::name.empty() && idx < sizeof...(A)) {
                    return meta::unpack_arg<idx>(std::forward<A>(args)...);
                } else {
                    return meta::unpack_arg<K>(std::forward<A>(args)...);
                }
            }

        public:
            using type                              = signature<Return(Args...)>;
            static constexpr size_t n_posonly       = impl::n_partial_posonly<Args...>;
            static constexpr size_t n_pos           = impl::n_partial_pos<Args...>;
            static constexpr size_t n_args          = impl::n_partial_args<Args...>;
            static constexpr size_t n_kw            = impl::n_partial_kw<Args...>;
            static constexpr size_t n_kwonly        = impl::n_partial_kwonly<Args...>;
            static constexpr size_t n_kwargs        = impl::n_partial_kwargs<Args...>;
            static constexpr bool has_posonly       = impl::has_partial_posonly<Args...>;
            static constexpr bool has_pos           = impl::has_partial_pos<Args...>;
            static constexpr bool has_args          = n_args > 0;
            static constexpr bool has_kw            = impl::has_partial_kw<Args...>;
            static constexpr bool has_kwonly        = impl::has_partial_kwonly<Args...>;
            static constexpr bool has_kwargs        = n_kwargs > 0;
            static constexpr size_t posonly_idx     = impl::partial_posonly_idx<Args...>;
            static constexpr size_t pos_idx         = impl::partial_pos_idx<Args...>;
            static constexpr size_t args_idx        = has_args ? CppSignature::args_idx : CppSignature::size();
            static constexpr size_t kw_idx          = impl::partial_kw_idx<Args...>;
            static constexpr size_t kwonly_idx      = impl::partial_kwonly_idx<Args...>;
            static constexpr size_t kwargs_idx      = has_kwargs ? CppSignature::kwargs_idx : CppSignature::size();

            /* The total number of partial arguments within the enclosing signature. */
            [[nodiscard]] static constexpr size_t size() noexcept { return Inner::size(); }
            [[nodiscard]] static constexpr bool empty() noexcept { return Inner::empty(); }

            /* Check whether a given index is within the bounds of the partial value
            tuple. */
            template <size_t I>
            [[nodiscard]] static constexpr bool contains() noexcept { return I < size(); }
            [[nodiscard]] static constexpr bool contains(size_t i) noexcept {return i < size(); }

            /* Check whether the named argument is contained within the partial value
            tuple. */
            template <static_str Key>
            [[nodiscard]] static constexpr bool contains() noexcept {
                return Inner::template contains<Key>();
            }
            template <typename T> requires (NameTable::template hashable<T>)
            [[nodiscard]] static constexpr bool contains(T&& key) noexcept {
                return Inner::contains(std::forward<T>(key));
            }

            /* Get the bound value at index K of the tuple.  If the partials are
            forwarded as an lvalue, then this will either directly reference the
            internal value if the corresponding argument expects an lvalue, or a copy
            if it expects an unqualified or rvalue type.  If the partials are given as
            an rvalue instead, then the copy will instead be optimized to a move. */
            template <size_t K> requires (K < size())
            [[nodiscard]] constexpr decltype(auto) get() const {
                return std::get<K>(values).get();
            }
            template <size_t K> requires (K < size())
            [[nodiscard]] constexpr decltype(auto) get() && {
                return std::move(std::get<K>(values)).get();
            }

            /* Get the bound value associated with the named argument, if it was given
            as a keyword argument.  If the partials are forwarded as an lvalue, then
            this will either directly reference the internal value if the corresponding
            argument expects an lvalue, or a copy if it expects an unqualified or rvalue
            type.  If the partials are given as an rvalue instead, then the copy will be
            optimized to a move. */
            template <static_str Name> requires (contains<Name>())
            [[nodiscard]] constexpr decltype(auto) get() const {
                return std::get<index<Name>()>(values).get();
            }
            template <static_str Name> requires (contains<Name>())
            [[nodiscard]] constexpr decltype(auto) get() && {
                return std::move(std::get<index<Name>()>(values)).get();
            }

            /* Get the index of a named argument within the partial values tuple. */
            template <static_str Key> requires (contains<Key>())
            [[nodiscard]] static constexpr size_t index() noexcept {
                return Inner::template index<Key>();
            }
            template <typename T> requires (NameTable::template hashable<T>)
            [[nodiscard]] static constexpr size_t index(T&& key) {
                return Inner::index(std::forward<T>(key));
            }

            /* Get the recorded name of the bound argument at index K of the partial
            tuple.  This may be empty if the argument was given as positional rather
            than keyword. */
            template <size_t K> requires (K < size())
            static constexpr static_str name = std::tuple_element_t<K, Tuple>::name;

            /* Given an index into the partial tuple, find the corresponding index in
            the enclosing parameter list. */
            template <size_t K> requires (K < size())
            static constexpr size_t rfind = std::tuple_element_t<K, Tuple>::index;

            template <size_t K> requires (K < size())
            using at = std::conditional_t<
                std::tuple_element_t<K, Tuple>::index == CppSignature::args_idx ||
                std::tuple_element_t<K, Tuple>::index == CppSignature::kwargs_idx,
                typename Inner::template at<K>,
                CppSignature::at<std::tuple_element_t<K, Tuple>::index>
            >;

            /* Bind an argument list to the partial values to enable the constructor. */
            template <typename... A>
            using Bind = Inner::template Bind<A...>;

            template <typename... A>
                requires (
                    !(meta::arg_traits<A>::variadic() || ...) &&
                    Bind<A...>::proper_argument_order &&
                    Bind<A...>::no_qualified_arg_annotations &&
                    Bind<A...>::no_duplicate_args &&
                    Bind<A...>::no_conflicting_values &&
                    Bind<A...>::no_extra_positional_args &&
                    Bind<A...>::no_extra_keyword_args &&
                    Bind<A...>::satisfies_required_args &&
                    Bind<A...>::can_convert
                )
            constexpr Partial(A&&... args) : values(
                []<size_t... Ks>(std::index_sequence<Ks...>, auto&&... args) -> Tuple {
                    return {{build<Ks>(std::forward<decltype(args)>(args)...)}...};
                }(std::index_sequence_for<A...>{}, std::forward<A>(args)...)
            ) {}

            /* Produce a new partial object with the given arguments in addition to any
            existing partial arguments.  This method is chainable, and the arguments will
            be interpreted as if they were passed to the signature's call operator.  They
            cannot include positional or keyword parameter packs. */
            template <typename... A>
                requires (
                    !(meta::arg_traits<A>::variadic() || ...) &&
                    CppSignature::Bind<A...>::proper_argument_order &&
                    CppSignature::Bind<A...>::no_qualified_arg_annotations &&
                    CppSignature::Bind<A...>::no_duplicate_args &&
                    CppSignature::Bind<A...>::no_extra_positional_args &&
                    CppSignature::Bind<A...>::no_extra_keyword_args &&
                    CppSignature::Bind<A...>::no_conflicting_values &&
                    CppSignature::Bind<A...>::can_convert
                )
            [[nodiscard]] constexpr auto bind(A&&... args) const {
                return CppSignature::Bind<A...>::template merge<0, 0, 0>::bind(
                    *this,
                    std::forward<A>(args)...
                );
            }
            template <typename... A>
                requires (
                    !(meta::arg_traits<A>::variadic() || ...) &&
                    CppSignature::Bind<A...>::proper_argument_order &&
                    CppSignature::Bind<A...>::no_qualified_arg_annotations &&
                    CppSignature::Bind<A...>::no_duplicate_args &&
                    CppSignature::Bind<A...>::no_extra_positional_args &&
                    CppSignature::Bind<A...>::no_extra_keyword_args &&
                    CppSignature::Bind<A...>::no_conflicting_values &&
                    CppSignature::Bind<A...>::can_convert
                )
            [[nodiscard]] constexpr auto bind(A&&... args) && {
                return CppSignature::Bind<A...>::template merge<0, 0, 0>::bind(
                    std::move(*this),
                    std::forward<A>(args)...
                );
            }

            /* Unbind any partial arguments that have been accumulated thus far. */
            [[nodiscard]] static constexpr auto unbind() noexcept {
                return typename CppSignature::Unbind::Partial{};
            }
        };

        /* Instance-level constructor for a `::Partial` tuple. */
        template <typename... A>
            requires (
                !(meta::arg_traits<A>::variadic() || ...) &&
                Partial::template Bind<A...>::proper_argument_order &&
                Partial::template Bind<A...>::no_qualified_arg_annotations &&
                Partial::template Bind<A...>::no_duplicate_args &&
                Partial::template Bind<A...>::no_conflicting_values &&
                Partial::template Bind<A...>::no_extra_positional_args &&
                Partial::template Bind<A...>::no_extra_keyword_args &&
                Partial::template Bind<A...>::satisfies_required_args &&
                Partial::template Bind<A...>::can_convert
            )
        [[nodiscard]] static constexpr Partial partial(A&&... args) {
            return Partial(std::forward<A>(args)...);
        }

        /* Bind a C++ argument list to the enclosing signature, inserting default values
        and partial arguments where necessary.  This enables and implements the signature's
        pure C++ call operator as a 3-way, compile-time merge between the partial
        arguments, default values, and given source arguments, provided they fulfill the
        enclosing signature.  Additionally, bound arguments can be saved and encoded into a
        partial signature in a chainable fashion, using the same infrastructure to simulate
        a normal function call at every step.  Any existing partial arguments will be
        folded into the resulting signature, facilitating higher-order function composition
        (currying, etc.) that can be done entirely at compile time. */
        template <typename... Values>
        struct Bind : impl::signature_bind_tag {
            static constexpr size_t n_pos           = impl::n_pos<Values...>;
            static constexpr size_t n_kw            = impl::n_kw<Values...>;
            static constexpr bool has_pos           = impl::has_pos<Values...>;
            static constexpr bool has_args          = impl::has_args<Values...>;
            static constexpr bool has_kw            = impl::has_kw<Values...>;
            static constexpr bool has_kwargs        = impl::has_kwargs<Values...>;
            static constexpr size_t args_idx        = impl::args_idx<Values...>;
            static constexpr size_t kw_idx          = impl::kw_idx<Values...>;
            static constexpr size_t kwargs_idx      = impl::kwargs_idx<Values...>;

            /* The total number of bound arguments. */
            static constexpr size_t size() noexcept { return sizeof...(Values); }
            static constexpr bool empty() noexcept { return !sizeof...(Values); }

            /* Check whether a given index is within the bounds of the default value
            tuple. */
            template <size_t I>
            [[nodiscard]] static constexpr bool contains() noexcept { return I < size(); }
            [[nodiscard]] static constexpr bool contains(size_t i) noexcept {return i < size(); }

            /* Check whether the named argument is contained within the default value
            tuple. */
            template <static_str Key>
            [[nodiscard]] static constexpr bool contains() noexcept {
                return impl::arg_idx<Key, Values...> < size();
            }

            /* Get the index of a named argument within the default values tuple. */
            template <static_str Key> requires (contains<Key>())
            [[nodiscard]] static constexpr size_t index() noexcept {
                return impl::arg_idx<Key, Values...>;
            }

            template <size_t I> requires (I < size())
            using at = meta::unpack_type<I, Values...>;

        protected:
            friend Partial;

            template <size_t I, size_t K>
            static constexpr bool _in_partial = false;
            template <size_t I, size_t K> requires (K < Partial::size())
            static constexpr bool _in_partial<I, K> =
                I == Partial::template rfind<K> || _in_partial<I, K + 1>;
            template <size_t I>
            static constexpr bool in_partial = _in_partial<I, 0>;

            template <size_t I, size_t>
            static constexpr bool _no_extra_positional_args = true;
            template <size_t I, size_t J>
                requires (J < std::min({args_idx, kw_idx, kwargs_idx}))
            static constexpr bool _no_extra_positional_args<I, J> = [] {
                return
                    I < std::min(CppSignature::kwonly_idx, CppSignature::kwargs_idx) &&
                    _no_extra_positional_args<I + 1, J + !in_partial<I>>;
            }();

            template <size_t>
            static constexpr bool _no_extra_keyword_args = true;
            template <size_t J> requires (J < kwargs_idx)
            static constexpr bool _no_extra_keyword_args<J> = [] {
                return
                    CppSignature::contains<meta::arg_traits<at<J>>::name>() &&
                    _no_extra_keyword_args<J + 1>;
            }();

            template <size_t, size_t>
            static constexpr bool _no_conflicting_values = true;
            template <size_t I, size_t J> requires (I < CppSignature::size() && J < size())
            static constexpr bool _no_conflicting_values<I, J> = [] {
                using T = CppSignature::at<I>;

                constexpr bool kw_conflicts_with_partial =
                    meta::arg_traits<at<J>>::kw() &&
                    Partial::template contains<meta::arg_traits<at<J>>::name>();

                constexpr bool kw_conflicts_with_positional =
                    !in_partial<I> && !meta::arg_traits<T>::name.empty() && (
                        meta::arg_traits<T>::posonly() || J < std::min(kw_idx, kwargs_idx)
                    ) && contains<meta::arg_traits<T>::name>();

                return
                    !kw_conflicts_with_partial &&
                    !kw_conflicts_with_positional &&
                    _no_conflicting_values<
                        has_args && J == args_idx ? std::min({
                            CppSignature::args_idx + 1,
                            CppSignature::kwonly_idx,
                            CppSignature::kwargs_idx
                        }) : I + 1,
                        CppSignature::has_args && I == CppSignature::args_idx ? std::min({
                            kw_idx,
                            kwargs_idx
                        }) : J + !in_partial<I>
                    >;
            }();

            template <size_t I, size_t J>
            static constexpr bool _satisfies_required_args = true;
            template <size_t I, size_t J> requires (I < CppSignature::size())
            static constexpr bool _satisfies_required_args<I, J> = [] {
                return (
                    in_partial<I> ||
                    meta::arg_traits<CppSignature::at<I>>::opt() ||
                    meta::arg_traits<CppSignature::at<I>>::variadic() ||
                    (
                        meta::arg_traits<CppSignature::at<I>>::pos() &&
                        J < std::min(kw_idx, kwargs_idx)
                    ) || (
                        meta::arg_traits<CppSignature::at<I>>::kw() &&
                        contains<meta::arg_traits<CppSignature::at<I>>::name>()
                    )
                ) && _satisfies_required_args<
                    has_args && J == args_idx ?
                        std::min(CppSignature::kwonly_idx, CppSignature::kwargs_idx) :
                        I + 1,
                    CppSignature::has_args && I == CppSignature::args_idx ?
                        std::min(kw_idx, kwargs_idx) :
                        J + !in_partial<I>
                >;
            }();

            template <size_t, size_t>
            static constexpr bool _can_convert = true;
            template <size_t I, size_t J> requires (I < CppSignature::size() && J < size())
            static constexpr bool _can_convert<I, J> = [] {
                if constexpr (meta::arg_traits<CppSignature::at<I>>::args()) {
                    constexpr size_t source_kw = std::min(kw_idx, kwargs_idx);
                    return
                        []<size_t... Js>(std::index_sequence<Js...>) {
                            return (std::convertible_to<
                                typename meta::arg_traits<at<J + Js>>::type,
                                typename meta::arg_traits<CppSignature::at<I>>::type
                            > && ...);
                        }(std::make_index_sequence<J < source_kw ? source_kw - J : 0>{}) &&
                        _can_convert<I + 1, source_kw>;

                } else if constexpr (meta::arg_traits<CppSignature::at<I>>::kwargs()) {
                    return
                        []<size_t... Js>(std::index_sequence<Js...>) {
                            return ((
                                CppSignature::contains<meta::arg_traits<at<kw_idx + Js>>::name>() ||
                                std::convertible_to<
                                    typename meta::arg_traits<at<kw_idx + Js>>::type,
                                    typename meta::arg_traits<CppSignature::at<I>>::type
                                >
                            ) && ...);
                        }(std::make_index_sequence<size() - kw_idx>{}) &&
                        _can_convert<I + 1, J>;

                } else if constexpr (in_partial<I>) {
                    return _can_convert<I + 1, J>;

                } else if constexpr (meta::arg_traits<at<J>>::posonly()) {
                    return std::convertible_to<
                        typename meta::arg_traits<at<J>>::type,
                        typename meta::arg_traits<CppSignature::at<I>>::type
                    > && _can_convert<I + 1, J + 1>;

                } else if constexpr (meta::arg_traits<at<J>>::kw()) {
                    constexpr static_str name = meta::arg_traits<at<J>>::name;
                    if constexpr (CppSignature::contains<name>()) {
                        if constexpr (!std::convertible_to<
                            typename meta::arg_traits<at<J>>::type,
                            typename meta::arg_traits<CppSignature::at<CppSignature::index<name>()>>::type
                        >) {
                            return false;
                        };
                    }
                    return _can_convert<I + 1, J + 1>;

                } else if constexpr (meta::arg_traits<at<J>>::args()) {
                    constexpr size_t target_kw =
                        std::min(CppSignature::kwonly_idx, CppSignature::kwargs_idx);
                    return
                        []<size_t... Is>(std::index_sequence<Is...>) {
                            return (
                                (
                                    in_partial<I + Is> || std::convertible_to<
                                        typename meta::arg_traits<at<J>>::type,
                                        typename meta::arg_traits<CppSignature::at<I + Is>>::type
                                    >
                                ) && ...
                            );
                        }(std::make_index_sequence<I < target_kw ? target_kw - I : 0>{}) &&
                        _can_convert<target_kw, J + 1>;

                } else if constexpr (meta::arg_traits<at<J>>::kwargs()) {
                    static constexpr size_t cutoff =
                        std::min({args_idx, kwonly_idx, kwargs_idx});
                    static constexpr size_t target_kw = has_args ?
                        CppSignature::kwonly_idx :
                        []<size_t... Ks>(std::index_sequence<Ks...>) {
                            return std::max(
                                CppSignature::kw_idx,
                                n_posonly + (0 + ... + (std::tuple_element_t<
                                    Ks,
                                    typename Partial::Tuple
                                >::target_idx < cutoff))
                            );
                        }(std::make_index_sequence<Partial::size()>{});
                    return
                        []<size_t... Is>(std::index_sequence<Is...>) {
                            return ((
                                in_partial<target_kw + Is> || contains<
                                    meta::arg_traits<CppSignature::at<target_kw + Is>>::name
                                >() || std::convertible_to<
                                    typename meta::arg_traits<at<J>>::type,
                                    typename meta::arg_traits<CppSignature::at<target_kw + Is>>::type
                                >
                            ) && ...);
                        }(std::make_index_sequence<CppSignature::size() - target_kw>{}) &&
                        _can_convert<I, J + 1>;

                } else {
                    static_assert(false, "invalid argument kind");
                    return false;
                }
            }();

            template <size_t I, size_t K>
            static constexpr bool use_partial = false;
            template <size_t I, size_t K> requires (K < Partial::size())
            static constexpr bool use_partial<I, K> = Partial::template rfind<K> == I;

            template <size_t I, size_t J, size_t K>
            struct merge {
            private:
                using T = CppSignature::at<I>;
                static constexpr static_str name = meta::arg_traits<T>::name;

                template <size_t K2>
                static constexpr bool use_partial = false;
                template <size_t K2> requires (K2 < Partial::size())
                static constexpr bool use_partial<K2> = Partial::template rfind<K2> == I;

                template <size_t K2>
                static constexpr size_t consecutive = 0;
                template <size_t K2> requires (K2 < Partial::size())
                static constexpr size_t consecutive<K2> = 
                    Partial::template rfind<K2> == I ? consecutive<K2 + 1> + 1 : 0;

                template <typename... A>
                static constexpr size_t pos_range = 0;
                template <typename A, typename... As>
                static constexpr size_t pos_range<A, As...> =
                    meta::arg_traits<A>::pos() ? pos_range<As...> + 1 : 0;

                template <typename... A>
                static constexpr void assert_no_kwargs_conflict(A&&... args) {
                    if constexpr (!name.empty() && impl::kwargs_idx<A...> < sizeof...(A)) {
                        auto&& pack = meta::unpack_arg<impl::kwargs_idx<A...>>(
                            std::forward<A>(args)...
                        );
                        if (auto node = pack.extract(name)) {
                            throw TypeError(
                                "conflicting value for parameter '" + name +
                                "' at index " + static_str<>::from_int<I>
                            );
                        }
                    }
                }

                template <size_t... Prev, size_t... Next, typename F>
                static constexpr std::invoke_result_t<F, Args...> forward_partial(
                    std::index_sequence<Prev...>,
                    std::index_sequence<Next...>,
                    auto&& parts,
                    auto&& defaults,
                    F&& func,
                    auto&&... args
                ) {
                    return merge<I + 1, J + 1, K + 1>{}(
                        std::forward<decltype(parts)>(parts),
                        std::forward<decltype(defaults)>(defaults),
                        std::forward<decltype(func)>(func),
                        meta::unpack_arg<Prev>(
                            std::forward<decltype(args)>(args)...
                        )...,
                        to_arg<I>(std::forward<decltype(parts)>(
                            parts
                        ).template get<K>()),
                        meta::unpack_arg<J + Next>(
                            std::forward<decltype(args)>(args)...
                        )...
                    );
                }

                template <size_t... Prev, size_t... Next, typename F>
                static constexpr std::invoke_result_t<F, Args...> forward_default(
                    std::index_sequence<Prev...>,
                    std::index_sequence<Next...>,
                    auto&& parts,
                    auto&& defaults,
                    F&& func,
                    auto&&... args
                ) {
                    return merge<I + 1, J + 1, K>{}(
                        std::forward<decltype(parts)>(parts),
                        std::forward<decltype(defaults)>(defaults),
                        std::forward<decltype(func)>(func),
                        meta::unpack_arg<Prev>(
                            std::forward<decltype(args)>(args)...
                        )...,
                        to_arg<I>(std::forward<decltype(defaults)>(
                            defaults
                        ).template get<Defaults::template find<I>>()),
                        meta::unpack_arg<J + Next>(
                            std::forward<decltype(args)>(args)...
                        )...
                    );
                }

                template <size_t... Prev, size_t... Next, typename F>
                static constexpr std::invoke_result_t<F, Args...> forward_positional(
                    std::index_sequence<Prev...>,
                    std::index_sequence<Next...>,
                    auto&& parts,
                    auto&& defaults,
                    F&& func,
                    auto&&... args
                ) {
                    return merge<I + 1, J + 1, K>{}(
                        std::forward<decltype(parts)>(parts),
                        std::forward<decltype(defaults)>(defaults),
                        std::forward<decltype(func)>(func),
                        meta::unpack_arg<Prev>(
                            std::forward<decltype(args)>(args)...
                        )...,
                        to_arg<I>(meta::unpack_arg<J>(
                            std::forward<decltype(args)>(args)...
                        )),
                        meta::unpack_arg<J + 1 + Next>(
                            std::forward<decltype(args)>(args)...
                        )...
                    );
                }

                template <size_t... Prev, size_t... Next, size_t... Rest, typename F>
                static constexpr std::invoke_result_t<F, Args...> forward_keyword(
                    std::index_sequence<Prev...>,
                    std::index_sequence<Next...>,
                    std::index_sequence<Rest...>,
                    auto&& parts,
                    auto&& defaults,
                    F&& func,
                    auto&&... args
                ) {
                    constexpr size_t idx = impl::arg_idx<name, decltype(args)...>;
                    return merge<I + 1, J + 1, K>{}(
                        std::forward<decltype(parts)>(parts),
                        std::forward<decltype(defaults)>(defaults),
                        std::forward<decltype(func)>(func),
                        meta::unpack_arg<Prev>(
                            std::forward<decltype(args)>(args)...
                        )...,
                        to_arg<I>(meta::unpack_arg<idx>(
                            std::forward<decltype(args)>(args)...
                        )),
                        meta::unpack_arg<J + Next>(
                            std::forward<decltype(args)>(args)...
                        )...,
                        meta::unpack_arg<idx + 1 + Rest>(
                            std::forward<decltype(args)>(args)...
                        )...
                    );
                }

                template <size_t... Prev, size_t... Next, typename F>
                static constexpr std::invoke_result_t<F, Args...> forward_from_pos_pack(
                    std::index_sequence<Prev...>,
                    std::index_sequence<Next...>,
                    auto&& parts,
                    auto&& defaults,
                    F&& func,
                    auto&&... args
                ) {
                    auto&& pack = meta::unpack_arg<impl::args_idx<decltype(args)...>>(
                        std::forward<decltype(args)>(args)...
                    );
                    return merge<I + 1, J + 1, K>{}(
                        std::forward<decltype(parts)>(parts),
                        std::forward<decltype(defaults)>(defaults),
                        std::forward<decltype(func)>(func),
                        meta::unpack_arg<Prev>(
                            std::forward<decltype(args)>(args)...
                        )...,
                        to_arg<I>(pack.value()),
                        meta::unpack_arg<J + Next>(
                            std::forward<decltype(args)>(args)...
                        )...
                    );
                }

                template <size_t... Prev, size_t... Next, typename F>
                static constexpr std::invoke_result_t<F, Args...> forward_from_kw_pack(
                    auto&& node,
                    std::index_sequence<Prev...>,
                    std::index_sequence<Next...>,
                    auto&& parts,
                    auto&& defaults,
                    F&& func,
                    auto&&... args
                ) {
                    return merge<I + 1, J + 1, K>{}(
                        std::forward<decltype(parts)>(parts),
                        std::forward<decltype(defaults)>(defaults),
                        std::forward<decltype(func)>(func),
                        meta::unpack_arg<Prev>(
                            std::forward<decltype(args)>(args)...
                        )...,
                        to_arg<I>(std::forward<typename meta::arg_traits<T>::type>(
                            node.mapped()
                        )),
                        meta::unpack_arg<J + Next>(
                            std::forward<decltype(args)>(args)...
                        )...
                    );
                }

                template <size_t... Prev, size_t... Next, typename F>
                static constexpr std::invoke_result_t<F, Args...> drop_empty_pack(
                    std::index_sequence<Prev...>,
                    std::index_sequence<Next...>,
                    auto&& parts,
                    auto&& defaults,
                    F&& func,
                    auto&&... args
                ) {
                    return merge{}(
                        std::forward<decltype(parts)>(parts),
                        std::forward<decltype(defaults)>(defaults),
                        std::forward<decltype(func)>(func),
                        meta::unpack_arg<Prev>(
                            std::forward<decltype(args)>(args)...
                        )...,
                        meta::unpack_arg<J + 1 + Next>(
                            std::forward<decltype(args)>(args)...
                        )...
                    );
                }

                template <typename P, typename... A>
                static auto variadic_positional(P&& parts, A&&... args) {
                    static constexpr size_t cutoff = std::min(
                        impl::kw_idx<A...>,
                        impl::kwargs_idx<A...>
                    );
                    static constexpr size_t diff = J < cutoff ? cutoff - J : 0;

                    // allocate variadic positional array
                    using vec = std::vector<typename meta::arg_traits<T>::type>;
                    vec out;
                    if constexpr (diff) {
                        if constexpr (impl::args_idx<A...> < sizeof...(A)) {
                            out.reserve(
                                consecutive<K> + 
                                (diff - 1) +
                                meta::unpack_arg<impl::args_idx<A...>>(
                                    std::forward<A>(args)...
                                ).size()
                            );
                        } else {
                            out.reserve(consecutive<K> + diff);
                        }
                    }

                    // consume partial args
                    []<size_t... Ks>(
                        std::index_sequence<Ks...>,
                        vec& out,
                        auto&& parts
                    ) {
                        (out.emplace_back(std::forward<decltype(parts)>(
                            parts
                        ).template get<K + Ks>()), ...);
                    }(
                        std::make_index_sequence<consecutive<K>>{},
                        out,
                        std::forward<P>(parts)
                    );

                    // consume source args + parameter packs
                    []<size_t J2 = J>(this auto&& self, vec& out, auto&&... args) {
                        if constexpr (J2 < cutoff) {
                            if constexpr (
                                impl::args_idx<A...> < sizeof...(A) &&
                                J2 == impl::args_idx<A...>
                            ) {
                                auto&& pack = meta::unpack_arg<J2>(
                                    std::forward<decltype(args)>(args)...
                                );
                                out.insert(out.end(), pack.begin, pack.end);
                            } else {
                                out.emplace_back(meta::unpack_arg<J2>(
                                    std::forward<decltype(args)>(args)...
                                ));
                                std::forward<decltype(self)>(self).template operator()<J2 + 1>(
                                    out,
                                    std::forward<decltype(args)>(args)...
                                );
                            }
                        }
                    }(out, std::forward<A>(args)...);

                    return out;
                }

                template <typename P, typename... A>
                static auto variadic_keywords(P&& parts, A&&... args) {
                    constexpr size_t diff = sizeof...(A) - J;

                    // allocate variadic keyword map
                    using map = std::unordered_map<
                        std::string,
                        typename meta::arg_traits<T>::type
                    >;
                    map out;
                    if constexpr (impl::kwargs_idx<A...> < sizeof...(A)) {
                        out.reserve(
                            consecutive<K> +
                            (diff - 1) +
                            meta::unpack_arg<impl::kwargs_idx<A...>>(
                                std::forward<A>(args)...
                            ).size()
                        );
                    } else {
                        out.reserve(consecutive<K> + diff);
                    }

                    // consume partial kwargs
                    []<size_t... Ks>(
                        std::index_sequence<Ks...>,
                        map& out,
                        auto&& parts
                    ) {
                        (out.emplace(
                            std::string(Partial::template name<K + Ks>),
                            std::forward<decltype(parts)>(
                                parts
                            ).template get<K + Ks>()
                        ), ...);
                    }(
                        std::make_index_sequence<consecutive<K>>{},
                        out,
                        std::forward<P>(parts)
                    );

                    // consume source kwargs + parameter packs
                    []<size_t J2 = J>(this auto&& self, map& out, auto&&... args) {
                        if constexpr (J2 < sizeof...(A)) {
                            if constexpr (
                                impl::kwargs_idx<A...> < sizeof...(A) &&
                                J2 == impl::kwargs_idx<A...>
                            ) {
                                auto&& pack = meta::unpack_arg<J2>(
                                    std::forward<decltype(args)>(args)...
                                );
                                auto it = pack.begin();
                                auto end = pack.end();
                                while (it != end) {
                                    // postfix++ required to increment before invalidation
                                    auto node = pack.extract(it++);
                                    auto rc = out.insert(node);
                                    if (!rc.inserted) {
                                        throw TypeError(
                                            "duplicate value for parameter '" +
                                            node.key() + "'"
                                        );
                                    }
                                }
                            } else {
                                out.emplace(
                                    std::string(meta::arg_traits<meta::unpack_type<J2, A...>>::name),
                                    *meta::unpack_arg<J2>(
                                        std::forward<decltype(args)>(args)...
                                    )
                                );
                                std::forward<decltype(self)>(self).template operator()<J2 + 1>(
                                    out,
                                    std::forward<decltype(args)>(args)...
                                );
                            }
                        }
                    }(out, std::forward<A>(args)...);

                    return out;
                }

            public:
                /* Produce a partial argument tuple for the enclosing signature using the
                built-up arguments from prior recursive calls.  Implements the `.bind()`
                method for partial functions, which is fully chainable, with existing
                partial arguments being folded in on prior recursive calls, and the return
                type being described above. */
                template <typename P, typename... A>
                static constexpr auto bind(P&& parts, A&&... args) {
                    if constexpr (meta::arg_traits<T>::args()) {
                        static constexpr size_t cutoff = impl::kw_idx<A...>;
                        return []<size_t... Prev, size_t... Ks, size_t... Next>(
                            std::index_sequence<Prev...>,
                            std::index_sequence<Ks...>,
                            std::index_sequence<Next...>,
                            auto&& parts,
                            auto&&... args
                        ) {
                            return merge<
                                I + 1,
                                cutoff + consecutive<K>,
                                K + consecutive<K>
                            >::bind(
                                std::forward<decltype(parts)>(parts),
                                meta::unpack_arg<Prev>(
                                    std::forward<decltype(args)>(args)...
                                )...,
                                std::forward<decltype(parts)>(
                                    parts
                                ).template get<K + Ks>()...,
                                meta::unpack_arg<cutoff + Next>(
                                    std::forward<decltype(args)>(args)...
                                )...
                            );
                        }(
                            std::make_index_sequence<J>{},
                            std::make_index_sequence<consecutive<K>>{},
                            std::make_index_sequence<sizeof...(A) - cutoff>{},
                            std::forward<P>(parts),
                            std::forward<A>(args)...
                        );

                    } else if constexpr (meta::arg_traits<T>::kwargs()) {
                        return []<size_t... Prev, size_t... Ks, size_t... Next>(
                            std::index_sequence<Prev...>,
                            std::index_sequence<Ks...>,
                            std::index_sequence<Next...>,
                            auto&& parts,
                            auto&&... args
                        ) {
                            return merge<
                                I + 1,
                                sizeof...(A) + consecutive<K>,
                                K + consecutive<K>
                            >::bind(
                                std::forward<decltype(parts)>(parts),
                                meta::unpack_arg<Prev>(
                                    std::forward<decltype(args)>(args)...
                                )...,
                                arg<Partial::template name<K + Ks>> =
                                    std::forward<decltype(parts)>(
                                        parts
                                    ).template get<K + Ks>()...,
                                meta::unpack_arg<J + Next>(
                                    std::forward<decltype(args)>(args)...
                                )...
                            );
                        }(
                            std::make_index_sequence<J>{},
                            std::make_index_sequence<consecutive<K>>{},
                            std::make_index_sequence<sizeof...(A) - J>{},
                            std::forward<P>(parts),
                            std::forward<A>(args)...
                        );

                    } else if constexpr (use_partial<K>) {
                        return []<size_t... Prev, size_t... Next>(
                            std::index_sequence<Prev...>,
                            std::index_sequence<Next...>,
                            auto&& parts,
                            auto&&... args
                        ) {
                            constexpr static_str name = Partial::template name<K>;
                            // demote keywords in the original partial into positional
                            // arguments in the new partial if the next source arg is
                            // positional and the target arg can be both positional or
                            // keyword
                            if constexpr (name.empty() || (
                                meta::arg_traits<T>::pos() &&
                                meta::arg_traits<T>::kw() &&
                                (impl::kw_idx<A...> == sizeof...(A) || J < impl::kw_idx<A...>)
                            )) {
                                return merge<I + 1, J + 1, K + 1>::bind(
                                    std::forward<decltype(parts)>(parts),
                                    meta::unpack_arg<Prev>(
                                        std::forward<decltype(args)>(args)...
                                    )...,
                                    std::forward<decltype(parts)>(
                                        parts
                                    ).template get<K>(),
                                    meta::unpack_arg<J + Next>(
                                        std::forward<decltype(args)>(args)...
                                    )...
                                );
                            } else {
                                return merge<I + 1, J + 1, K + 1>::bind(
                                    std::forward<decltype(parts)>(parts),
                                    meta::unpack_arg<Prev>(
                                        std::forward<decltype(args)>(args)...
                                    )...,
                                    arg<name> = std::forward<decltype(parts)>(
                                        parts
                                    ).template get<K>(),
                                    meta::unpack_arg<J + Next>(
                                        std::forward<decltype(args)>(args)...
                                    )...
                                );
                            }
                        }(
                            std::make_index_sequence<J>{},
                            std::make_index_sequence<sizeof...(A) - J>{},
                            std::forward<P>(parts),
                            std::forward<A>(args)...
                        );
                    } else {
                        return merge<I + 1, J + 1, K>::bind(
                            std::forward<P>(parts),
                            std::forward<A>(args)...
                        );
                    }
                }

                /* Invoking a C++ function involves a 3-way merge of the partial arguments,
                source arguments, and default values, in that order of precedence.  By the
                end, the parameters are guaranteed to exactly match the enclosing
                signature, such that it can be passed to a matching function with the
                intended semantics.  This is done by inserting, removing, and reordering
                parameters from the argument list at compile time using index sequences and
                fold expressions, which can be inlined into the final call. */
                template <typename P, typename D, typename F, typename... A>
                static constexpr std::invoke_result_t<F, Args...> operator()(
                    P&& parts,
                    D&& defaults,
                    F&& func,
                    A&&... args
                ) {
                    if constexpr (meta::arg_traits<T>::posonly()) {
                        if constexpr (use_partial<K>) {
                            assert_no_kwargs_conflict(std::forward<A>(args)...);
                            return forward_partial(
                                std::make_index_sequence<J>{},
                                std::make_index_sequence<sizeof...(A) - J>{},
                                std::forward<P>(parts),
                                std::forward<D>(defaults),
                                std::forward<F>(func),
                                std::forward<A>(args)...
                            );
                        } else if constexpr (J < pos_range<A...>) {
                            assert_no_kwargs_conflict(std::forward<A>(args)...);
                            return forward_positional(
                                std::make_index_sequence<J>{},
                                std::make_index_sequence<sizeof...(A) - (J + 1)>{},
                                std::forward<P>(parts),
                                std::forward<D>(defaults),
                                std::forward<F>(func),
                                std::forward<A>(args)...
                            );
                        } else if constexpr (J < sizeof...(A) && J == impl::args_idx<A...>) {
                            auto&& pack = meta::unpack_arg<J>(std::forward<A>(args)...);
                            if (pack.has_value()) {
                                assert_no_kwargs_conflict(std::forward<A>(args)...);
                                return forward_from_pos_pack(
                                    std::make_index_sequence<J>{},
                                    std::make_index_sequence<sizeof...(A) - J>{},
                                    std::forward<P>(parts),
                                    std::forward<D>(defaults),
                                    std::forward<F>(func),
                                    std::forward<A>(args)...
                                );
                            } else {
                                return drop_empty_pack(
                                    std::make_index_sequence<J>{},
                                    std::make_index_sequence<sizeof...(A) - (J + 1)>{},
                                    std::forward<P>(parts),
                                    std::forward<D>(defaults),
                                    std::forward<F>(func),
                                    std::forward<A>(args)...
                                );
                            }
                        } else if constexpr (meta::arg_traits<T>::opt()) {
                            assert_no_kwargs_conflict(std::forward<A>(args)...);
                            return forward_default(
                                std::make_index_sequence<J>{},
                                std::make_index_sequence<sizeof...(A) - J>{},
                                std::forward<P>(parts),
                                std::forward<D>(defaults),
                                std::forward<F>(func),
                                std::forward<A>(args)...
                            );
                        } else if constexpr (name.empty()) {
                            throw TypeError(
                                "no match for positional-only parameter at index " +
                                static_str<>::from_int<I>
                            );
                        } else {
                            throw TypeError(
                                "no match for positional-only parameter '" + name +
                                "' at index " + static_str<>::from_int<I>
                            );
                        }

                    } else if constexpr (meta::arg_traits<T>::pos()) {
                        if constexpr (use_partial<K>) {
                            assert_no_kwargs_conflict(std::forward<A>(args)...);
                            return forward_partial(
                                std::make_index_sequence<J>{},
                                std::make_index_sequence<sizeof...(A) - J>{},
                                std::forward<P>(parts),
                                std::forward<D>(defaults),
                                std::forward<F>(func),
                                std::forward<A>(args)...
                            );
                        } else if constexpr (J < pos_range<A...>) {
                            assert_no_kwargs_conflict(std::forward<A>(args)...);
                            return forward_positional(
                                std::make_index_sequence<J>{},
                                std::make_index_sequence<sizeof...(A) - (J + 1)>{},
                                std::forward<P>(parts),
                                std::forward<D>(defaults),
                                std::forward<F>(func),
                                std::forward<A>(args)...
                            );
                        } else if constexpr (J < sizeof...(A) && J == impl::args_idx<A...>) {
                            auto&& pack = meta::unpack_arg<J>(std::forward<A>(args)...);
                            if (pack.has_value()) {
                                assert_no_kwargs_conflict(std::forward<A>(args)...);
                                return forward_from_pos_pack(
                                    std::make_index_sequence<J>{},
                                    std::make_index_sequence<sizeof...(A) - J>{},
                                    std::forward<P>(parts),
                                    std::forward<D>(defaults),
                                    std::forward<F>(func),
                                    std::forward<A>(args)...
                                );
                            } else {
                                return drop_empty_pack(
                                    std::make_index_sequence<J>{},
                                    std::make_index_sequence<sizeof...(A) - (J + 1)>{},
                                    std::forward<P>(parts),
                                    std::forward<D>(defaults),
                                    std::forward<F>(func),
                                    std::forward<A>(args)...
                                );
                            }
                        } else if constexpr (impl::arg_idx<name, A...> < sizeof...(A)) {
                            assert_no_kwargs_conflict(std::forward<A>(args)...);
                            constexpr size_t idx = impl::arg_idx<name, A...>;
                            return forward_keyword(
                                std::make_index_sequence<J>{},
                                std::make_index_sequence<idx - J>{},
                                std::make_index_sequence<sizeof...(A) - (idx + 1)>{},
                                std::forward<P>(parts),
                                std::forward<D>(defaults),
                                std::forward<F>(func),
                                std::forward<A>(args)...
                            );
                        } else if constexpr (impl::kwargs_idx<A...> < sizeof...(A)) {
                            auto&& pack = meta::unpack_arg<impl::kwargs_idx<A...>>(
                                std::forward<A>(args)...
                            );
                            if (auto node = pack.extract(std::string_view(name))) {
                                return forward_from_kw_pack(
                                    std::move(node),
                                    std::make_index_sequence<J>{},
                                    std::make_index_sequence<sizeof...(A) - J>{},
                                    std::forward<P>(parts),
                                    std::forward<D>(defaults),
                                    std::forward<F>(func),
                                    std::forward<A>(args)...
                                );
                            } else {
                                if constexpr (meta::arg_traits<T>::opt()) {
                                    return forward_default(
                                        std::make_index_sequence<J>{},
                                        std::make_index_sequence<sizeof...(A) - J>{},
                                        std::forward<P>(parts),
                                        std::forward<D>(defaults),
                                        std::forward<F>(func),
                                        std::forward<A>(args)...
                                    );
                                } else {
                                    throw TypeError(
                                        "no match for parameter '" + name +
                                        "' at index " + static_str<>::from_int<I>
                                    );
                                }
                            }
                        } else if constexpr (meta::arg_traits<T>::opt()) {
                            assert_no_kwargs_conflict(std::forward<A>(args)...);
                            return forward_default(
                                std::make_index_sequence<J>{},
                                std::make_index_sequence<sizeof...(A) - J>{},
                                std::forward<P>(parts),
                                std::forward<D>(defaults),
                                std::forward<F>(func),
                                std::forward<A>(args)...
                            );
                        } else {
                            throw TypeError(
                                "no match for parameter '" + name +
                                "' at index " + static_str<>::from_int<I>
                            );
                        }

                    } else if constexpr (meta::arg_traits<T>::kw()) {
                        if constexpr (use_partial<K>) {
                            assert_no_kwargs_conflict(std::forward<A>(args)...);
                            return forward_partial(
                                std::make_index_sequence<J>{},
                                std::make_index_sequence<sizeof...(A) - J>{},
                                std::forward<P>(parts),
                                std::forward<D>(defaults),
                                std::forward<F>(func),
                                std::forward<A>(args)...
                            );
                        } else if constexpr (impl::arg_idx<name, A...> < sizeof...(A)) {
                            assert_no_kwargs_conflict(std::forward<A>(args)...);
                            constexpr size_t idx = impl::arg_idx<name, A...>;
                            return forward_keyword(
                                std::make_index_sequence<J>{},
                                std::make_index_sequence<idx - J>{},
                                std::make_index_sequence<sizeof...(A) - (idx + 1)>{},
                                std::forward<P>(parts),
                                std::forward<D>(defaults),
                                std::forward<F>(func),
                                std::forward<A>(args)...
                            );
                        } else if constexpr (impl::kwargs_idx<A...> < sizeof...(A)) {
                            auto&& pack = meta::unpack_arg<impl::kwargs_idx<A...>>(
                                std::forward<A>(args)...
                            );
                            if (auto node = pack.extract(std::string_view(name))) {
                                return forward_from_kw_pack(
                                    std::move(node),
                                    std::make_index_sequence<J>{},
                                    std::make_index_sequence<sizeof...(A) - J>{},
                                    std::forward<P>(parts),
                                    std::forward<D>(defaults),
                                    std::forward<F>(func),
                                    std::forward<A>(args)...
                                );
                            } else {
                                if constexpr (meta::arg_traits<T>::opt()) {
                                    return forward_default(
                                        std::make_index_sequence<J>{},
                                        std::make_index_sequence<sizeof...(A) - J>{},
                                        std::forward<P>(parts),
                                        std::forward<D>(defaults),
                                        std::forward<F>(func),
                                        std::forward<A>(args)...
                                    );
                                } else {
                                    throw TypeError(
                                        "no match for parameter '" + name +
                                        "' at index " + static_str<>::from_int<I>
                                    );
                                }
                            }
                        } else if constexpr (meta::arg_traits<T>::opt()) {
                            assert_no_kwargs_conflict(std::forward<A>(args)...);
                            return forward_default(
                                std::make_index_sequence<J>{},
                                std::make_index_sequence<sizeof...(A) - J>{},
                                std::forward<P>(parts),
                                std::forward<D>(defaults),
                                std::forward<F>(func),
                                std::forward<A>(args)...
                            );
                        } else {
                            throw TypeError(
                                "no match for keyword-only parameter '" + name +
                                "' at index " + static_str<>::from_int<I>
                            );
                        }

                    } else if constexpr (meta::arg_traits<T>::args()) {
                        static constexpr size_t cutoff = std::min(
                            impl::kw_idx<A...>,
                            impl::kwargs_idx<A...>
                        );
                        return []<size_t... Prev, size_t... Next>(
                            std::index_sequence<Prev...>,
                            std::index_sequence<Next...>,
                            auto&& parts,
                            auto&& defaults,
                            auto&& func,
                            auto&&... args
                        ) {
                            return merge<I + 1, J + 1, K + consecutive<K>>{}(
                                std::forward<decltype(parts)>(parts),
                                std::forward<decltype(defaults)>(defaults),
                                std::forward<decltype(func)>(func),
                                meta::unpack_arg<Prev>(
                                    std::forward<decltype(args)>(args)...
                                )...,
                                to_arg<I>(variadic_positional(
                                    std::forward<decltype(parts)>(parts),
                                    std::forward<decltype(args)>(args)...
                                )),
                                meta::unpack_arg<cutoff + Next>(
                                    std::forward<decltype(args)>(args)...
                                )...
                            );
                        }(
                            std::make_index_sequence<J>{},
                            std::make_index_sequence<sizeof...(A) - cutoff>{},
                            std::forward<P>(parts),
                            std::forward<D>(defaults),
                            std::forward<F>(func),
                            std::forward<A>(args)...
                        );

                    } else if constexpr (meta::arg_traits<T>::kwargs()) {
                        return []<size_t... Prev>(
                            std::index_sequence<Prev...>,
                            auto&& parts,
                            auto&& defaults,
                            auto&& func,
                            auto&&... args
                        ) {
                            return merge<I + 1, J + 1, K + consecutive<K>>{}(
                                std::forward<decltype(parts)>(parts),
                                std::forward<decltype(defaults)>(defaults),
                                std::forward<decltype(func)>(func),
                                meta::unpack_arg<Prev>(
                                    std::forward<decltype(args)>(args)...
                                )...,
                                to_arg<I>(variadic_keywords(
                                    std::forward<decltype(parts)>(parts),
                                    std::forward<decltype(args)>(args)...
                                ))
                            );
                        }(
                            std::make_index_sequence<J>{},
                            std::forward<P>(parts),
                            std::forward<D>(defaults),
                            std::forward<F>(func),
                            std::forward<A>(args)...
                        );

                    } else {
                        static_assert(false, "invalid argument kind");
                    }
                }
            };
            template <size_t J, size_t K>
            struct merge<CppSignature::size(), J, K> {
            private:
                /* Convert a terminal argument list into an equivalent partial signature,
                wherein the arguments are bound to their corresponding values in the
                enclosing signature. */
                template <typename... As>
                struct sig {
                    // elementwise traversal metafunction
                    template <typename out, size_t, size_t>
                    struct advance { using type = out; };
                    template <typename... out, size_t I2, size_t J2>
                        requires (I2 < CppSignature::size())
                    struct advance<Return(out...), I2, J2> {
                        template <typename>
                        struct maybe_bind;

                        template <typename T> requires (meta::arg_traits<T>::posonly())
                        struct maybe_bind<T> {
                            // If no matching partial exists, forward the unbound arg
                            template <size_t J3>
                            struct append {
                                using type = advance<Return(out..., T), I2 + 1, J3>::type;
                            };
                            // Otherwise, bind the partial and advance
                            template <size_t J3> requires (J3 < impl::kw_idx<As...>)
                            struct append<J3> {
                                using S = meta::unpack_type<J3, As...>;
                                using B = meta::arg_traits<T>::template bind<S>;
                                using type = advance<Return(out..., B), I2 + 1, J3 + 1>::type;
                            };
                            using type = append<J2>::type;
                        };

                        template <typename T> requires (meta::arg_traits<T>::pos() && meta::arg_traits<T>::kw())
                        struct maybe_bind<T> {
                            // If no matching partial exists, forward the unbound arg
                            template <size_t J3>
                            struct append {
                                using type = advance<Return(out..., T), I2 + 1, J3>::type;
                            };
                            // If a partial positional arg exists, bind it and advance
                            template <size_t J3> requires (J3 < impl::kw_idx<As...>)
                            struct append<J3> {
                                using S = meta::unpack_type<J3, As...>;
                                using B = meta::arg_traits<T>::template bind<S>;
                                using type = advance<Return(out..., B), I2 + 1, J3 + 1>::type;
                            };
                            // If a partial keyword arg exists, bind it and advance
                            template <size_t J3> requires (
                                J3 >= impl::kw_idx<As...> &&
                                impl::arg_idx<meta::arg_traits<T>::name, As...> < sizeof...(As)
                            )
                            struct append<J3> {
                                static constexpr static_str name = meta::arg_traits<T>::name;
                                static constexpr size_t idx = impl::arg_idx<name, As...>;
                                using S = meta::unpack_type<idx, As...>;
                                using B = meta::arg_traits<T>::template bind<S>;
                                using type = advance<Return(out..., B), I2 + 1, J3>::type;
                            };
                            using type = append<J2>::type;
                        };

                        template <typename T> requires (meta::arg_traits<T>::kwonly())
                        struct maybe_bind<T> {
                            // If no matching partial exists, forward the unbound arg
                            template <size_t J3>
                            struct append {
                                using type = advance<Return(out..., T), I2 + 1, J3>::type;
                            };
                            // If a partial keyword arg exists, bind it and advance
                            template <size_t J3>
                                requires (
                                    impl::arg_idx<meta::arg_traits<T>::name, As...> < sizeof...(As)
                                )
                            struct append<J3> {
                                static constexpr static_str name = meta::arg_traits<T>::name;
                                static constexpr size_t idx = impl::arg_idx<name, As...>;
                                using S = meta::unpack_type<idx, As...>;
                                using B = meta::arg_traits<T>::template bind<S>;
                                using type = advance<Return(out..., B), I2 + 1, J3>::type;
                            };
                            using type = append<J2>::type;
                        };

                        template <typename T> requires (meta::arg_traits<T>::args())
                        struct maybe_bind<T> {
                            // Recur until there are no more partial positional args to bind
                            template <typename result, size_t J3>
                            struct append {
                                template <typename>
                                struct collect;
                                // If no matching partials exist, forward the unbound arg
                                template <>
                                struct collect<args<>> {
                                    using type = advance<Return(out..., T), I2 + 1, J3>::type;
                                };
                                // Otherwise, bind the collected partials and advance
                                template <typename r2, typename... r2s>
                                struct collect<args<r2, r2s...>> {
                                    using B = meta::arg_traits<T>::template bind<r2, r2s...>;
                                    using type = advance<Return(out..., B), I2 + 1, J3>::type;
                                };
                                using type = collect<result>::type;
                            };
                            template <typename... result, size_t J3>
                                requires (J3 < impl::kw_idx<As...>)
                            struct append<args<result...>, J3> {
                                // Append remaining partial positional args to the output pack
                                using type = append<
                                    args<result..., meta::unpack_type<J3, As...>>,
                                    J3 + 1
                                >::type;
                            };
                            using type = append<args<>, J2>::type;
                        };

                        template <typename T> requires (meta::arg_traits<T>::kwargs())
                        struct maybe_bind<T> {
                            // Recur until there are no more partial keyword args to bind
                            template <typename result, size_t J3>
                            struct append {
                                template <typename>
                                struct collect;
                                // If no matching partials exist, forward the unbound arg
                                template <>
                                struct collect<args<>> {
                                    using type = advance<Return(out..., T), I2 + 1, J3>::type;
                                };
                                // Otherwise, bind the collected partials without advancing
                                template <typename r2, typename... r2s>
                                struct collect<args<r2, r2s...>> {
                                    using B = meta::arg_traits<T>::template bind<r2, r2s...>;
                                    using type = advance<Return(out..., B), I2 + 1, J3>::type;
                                };
                                using type = collect<result>::type;
                            };
                            template <typename... result, size_t J3> requires (J3 < sizeof...(As))
                            struct append<args<result...>, J3> {
                                // If the keyword arg is in the target signature, ignore
                                template <typename S>
                                struct collect {
                                    using type = args<result...>;
                                };
                                // Otherwise, append it to the output pack and continue
                                template <typename S>
                                    requires (!CppSignature::template contains<meta::arg_traits<S>::name>())
                                struct collect<S> {
                                    using type = args<result..., S>;
                                };
                                using type = append<
                                    typename collect<meta::unpack_type<J3, As...>>::type,
                                    J3 + 1
                                >::type;
                            };
                            // Start at the beginning of the partial keywords
                            using type = append<args<>, impl::kw_idx<As...>>::type;
                        };

                        // Feed in the unbound argument and return a possibly bound equivalent
                        using type = maybe_bind<
                            typename meta::arg_traits<CppSignature::at<I2>>::unbind
                        >::type;
                    };

                    // Start with an empty signature, which will be built up into an
                    // equivalent of the enclosing signature through elementwise binding
                    using type = signature<typename advance<Return(), 0, 0>::type>;
                };

            public:
                template <typename P, typename... As>
                static constexpr auto bind(P&& parts, As&&... args) {
                    return typename sig<As...>::type::Partial{std::forward<As>(args)...};
                }

                template <typename P, typename D, typename F, typename... A>
                static constexpr std::invoke_result_t<F, Args...> operator()(
                    P&& parts,
                    D&& defaults,
                    F&& func,
                    A&&... args
                ) {
                    // validate and remove positional parameter packs
                    if constexpr (impl::args_idx<A...> < sizeof...(A)) {
                        static constexpr size_t idx = impl::args_idx<A...>;
                        if constexpr (!CppSignature::has_args) {
                            auto&& pack = meta::unpack_arg<idx>(std::forward<A>(args)...);
                            pack.validate();
                        }
                        return []<size_t... Prev, size_t... Next>(
                            std::index_sequence<Prev...>,
                            std::index_sequence<Next...>,
                            auto&& parts,
                            auto&& defaults,
                            auto&& func,
                            auto&&... args
                        ) {
                            return merge{}(
                                std::forward<decltype(parts)>(parts),
                                std::forward<decltype(defaults)>(defaults),
                                std::forward<decltype(func)>(func),
                                meta::unpack_arg<Prev>(
                                    std::forward<decltype(args)>(args)...
                                )...,
                                meta::unpack_arg<idx + 1 + Next>(
                                    std::forward<decltype(args)>(args)...
                                )...
                            );
                        }(
                            std::make_index_sequence<J>{},
                            std::make_index_sequence<sizeof...(A) - (idx + 1)>{},
                            std::forward<P>(parts),
                            std::forward<D>(defaults),
                            std::forward<F>(func),
                            std::forward<A>(args)...
                        );

                    // validate and remove keyword parameter packs
                    } else if constexpr (impl::kwargs_idx<A...> < sizeof...(A)) {
                        static constexpr size_t idx = impl::kwargs_idx<A...>;
                        if constexpr (!CppSignature::has_kwargs) {
                            auto&& pack = meta::unpack_arg<idx>(std::forward<A>(args)...);
                            pack.validate();
                        }
                        return []<size_t... Prev, size_t... Next>(
                            std::index_sequence<Prev...>,
                            std::index_sequence<Next...>,
                            auto&& parts,
                            auto&& defaults,
                            auto&& func,
                            auto&&... args
                        ) {
                            return merge{}(
                                std::forward<decltype(parts)>(parts),
                                std::forward<decltype(defaults)>(defaults),
                                std::forward<decltype(func)>(func),
                                meta::unpack_arg<Prev>(
                                    std::forward<decltype(args)>(args)...
                                )...,
                                meta::unpack_arg<idx + 1 + Next>(
                                    std::forward<decltype(args)>(args)...
                                )...
                            );
                        }(
                            std::make_index_sequence<J>{},
                            std::make_index_sequence<sizeof...(A) - (idx + 1)>{},
                            std::forward<P>(parts),
                            std::forward<D>(defaults),
                            std::forward<F>(func),
                            std::forward<A>(args)...
                        );

                    // call the function with the final argument list
                    } else {
                        return std::forward<F>(func)(std::forward<A>(args)...);
                    }
                }
            };

        public:
            static constexpr bool proper_argument_order =
                impl::proper_argument_order<Values...>;

            static constexpr bool no_qualified_arg_annotations =
                impl::no_qualified_arg_annotations<Values...>;

            static constexpr bool no_duplicate_args =
                impl::no_duplicate_args<Values...>;

            static constexpr bool no_extra_positional_args =
                !has_pos || CppSignature::has_args || _no_extra_positional_args<0, 0>;

            static constexpr bool no_extra_keyword_args =
                !has_kw || CppSignature::has_kwargs || _no_extra_keyword_args<kw_idx>;

            static constexpr bool no_conflicting_values =
                _no_conflicting_values<0, 0>;

            static constexpr bool can_convert =
                _can_convert<0, 0>;

            static constexpr bool satisfies_required_args =
                _satisfies_required_args<0, 0>;

            /* Invoke a C++ function from C++ using Python-style arguments. */
            template <meta::inherits<Partial> P, meta::inherits<Defaults> D, typename F>
                requires (
                    CppSignature::invocable<F> &&
                    proper_argument_order &&
                    no_qualified_arg_annotations &&
                    no_duplicate_args &&
                    no_extra_positional_args &&
                    no_extra_keyword_args &&
                    no_conflicting_values &&
                    can_convert &&
                    satisfies_required_args
                )
            static constexpr Return operator()(
                P&& parts,
                D&& defaults,
                F&& func,
                Values... args
            ) {
                return impl::invoke_with_packs(
                    [](
                        auto&& parts,
                        auto&& defaults,
                        auto&& func,
                        auto&&... args
                    ) {
                        return merge<0, 0, 0>{}(
                            std::forward<decltype(parts)>(parts),
                            std::forward<decltype(defaults)>(defaults),
                            std::forward<decltype(func)>(func),
                            std::forward<decltype(args)>(args)...
                        );
                    },
                    std::forward<P>(parts),
                    std::forward<D>(defaults),
                    std::forward<F>(func),
                    std::forward<Values>(args)...
                );
            }
        };

        /* Unbinding a signature strips any partial arguments that have been encoded
        thus far and returns a new signature without them. */
        using Unbind = _Unbind<Return(), Args...>::type;

        /* Call a C++ function from C++ using Python-style arguments. */
        template <
            meta::inherits<Partial> P,
            meta::inherits<Defaults> D,
            typename F,
            typename... A
        >
            requires (
                invocable<F> &&
                Bind<A...>::proper_argument_order &&
                Bind<A...>::no_qualified_arg_annotations &&
                Bind<A...>::no_duplicate_args &&
                Bind<A...>::no_extra_positional_args &&
                Bind<A...>::no_extra_keyword_args &&
                Bind<A...>::no_conflicting_values &&
                Bind<A...>::can_convert &&
                Bind<A...>::satisfies_required_args
            )
        static constexpr Return operator()(
            P&& partial,
            D&& defaults,
            F&& func,
            A&&... args
        ) {
            return Bind<A...>{}(
                std::forward<P>(partial),
                std::forward<D>(defaults),
                std::forward<F>(func),
                std::forward<A>(args)...
            );
        }

        /* Capture a C++ function and generate a new function object that exactly matches
        the enclosing signature, without any partial arguments.  A function of this form
        can be used to initialize a `def` statement that delegates to an external C++
        function with an arbitrary signature, as long as it can be invoked with the
        enclosing arguments and returns a compatible type. */
        template <typename F> requires (invocable<F>)
        [[nodiscard]] static constexpr auto capture(F&& func) {
            struct Func {
                std::remove_cvref_t<F> func;
                constexpr Return operator()(typename meta::arg_traits<Args>::unbind... args) const {
                    return func(std::forward<typename meta::arg_traits<Args>::unbind>(args)...);
                }
            };
            return Func{std::forward<F>(func)};
        }

        /// TODO: revisit to_string to pull it out of the signature<> specializations and
        /// reduce code duplication.  It should also always produce valid C++ and/or Python
        /// source code, and if defaults are not given, they will be replaced with ellipsis
        /// in the Python style, such that the output can be written to a .pyi file.

        /* Produce a string representation of this signature for debugging purposes.  The
        provided `prefix` will be prepended to each output line, and if `max_width` is
        provided, then the algorithm will attempt to wrap the output to that width, with
        each parameter indented on a separate line.  If a single parameter exceeds the
        maximum width, then it will be wrapped onto multiple lines with an additional level
        of indentation for the extra lines.  Note that the maximum width is not a hard
        limit; individual components can exceed it, but never on the same line as another
        component.

        The output from this method is directly written to a .pyi file when bindings are
        generated, allowing static type checkers to validate C++ function signatures and
        provide high-quality syntax highlighting/autocompletion. */
        [[nodiscard]] static constexpr std::string to_string(
            const std::string& name,
            const std::string& prefix = "",
            size_t max_width = std::numeric_limits<size_t>::max(),
            size_t indent = 4
        ) {
            std::vector<std::string> components;
            components.reserve(size() * 3 + 2);
            components.emplace_back(name);

            /// TODO: appending the demangled type name is probably wrong, since it doesn't
            /// always yield valid Python source code.  Instead, I should probably try to
            /// convert the type to Python and return its qualified name?  That way, the
            /// .pyi file would be able to import the type correctly.  That will need some
            /// work, and demangling might be another option to the method that directs it
            /// to do this.  I'll probably have to revisit that when I actually try to
            /// build the .pyi files, and can test more directly.

            size_t last_posonly = std::numeric_limits<size_t>::max();
            size_t first_kwonly = std::numeric_limits<size_t>::max();
            []<size_t I = 0>(
                this auto&& self,
                auto&& defaults,
                std::vector<std::string>& components,
                size_t& last_posonly,
                size_t& first_kwonly
            ) {
                if constexpr (I < size()) {
                    using T = CppSignature::at<I>;
                    if constexpr (meta::arg_traits<T>::args()) {
                        components.emplace_back(std::string("*" + meta::arg_traits<T>::name));
                    } else if constexpr (meta::arg_traits<T>::kwargs()) {
                        components.emplace_back(std::string("**" + meta::arg_traits<T>::name));
                    } else {
                        if constexpr (meta::arg_traits<T>::posonly()) {
                            last_posonly = I;
                        } else if constexpr (meta::arg_traits<T>::kwonly() && !CppSignature::has_args) {
                            if (first_kwonly == std::numeric_limits<size_t>::max()) {
                                first_kwonly = I;
                            }
                        }
                        components.emplace_back(std::string(meta::arg_traits<T>::name));
                    }
                    components.emplace_back(
                        std::string(type_name<typename meta::arg_traits<T>::type>)
                    );
                    if constexpr (meta::arg_traits<T>::opt()) {
                        components.emplace_back("...");
                    } else {
                        components.emplace_back("");
                    }
                    std::forward<decltype(self)>(self).template operator()<I + 1>(
                        std::forward<decltype(defaults)>(defaults),
                        components
                    );
                }
            }(components);

            if constexpr (std::is_void_v<Return>) {
                components.emplace_back("None");
            } else {
                components.emplace_back(std::string(type_name<Return>));
            }

            return impl::format_signature(
                prefix,
                max_width,
                indent,
                components,
                last_posonly,
                first_kwonly
            );
        }

        template <meta::inherits<Defaults> D>
        [[nodiscard]] static constexpr std::string to_string(
            const std::string& name,
            D&& defaults,
            const std::string& prefix = "",
            size_t max_width = std::numeric_limits<size_t>::max(),
            size_t indent = 4
        ) {
            std::vector<std::string> components;
            components.reserve(size() * 3 + 2);
            components.emplace_back(name);

            size_t last_posonly = std::numeric_limits<size_t>::max();
            size_t first_kwonly = std::numeric_limits<size_t>::max();
            []<size_t I = 0>(
                this auto&& self,
                auto&& defaults,
                std::vector<std::string>& components,
                size_t& last_posonly,
                size_t& first_kwonly
            ) {
                if constexpr (I < size()) {
                    using T = CppSignature::at<I>;
                    if constexpr (meta::arg_traits<T>::args()) {
                        components.emplace_back(std::string("*" + meta::arg_traits<T>::name));
                    } else if constexpr (meta::arg_traits<T>::kwargs()) {
                        components.emplace_back(std::string("**" + meta::arg_traits<T>::name));
                    } else {
                        if constexpr (meta::arg_traits<T>::posonly()) {
                            last_posonly = I;
                        } else if constexpr (meta::arg_traits<T>::kwonly() && !CppSignature::has_args) {
                            if (first_kwonly == std::numeric_limits<size_t>::max()) {
                                first_kwonly = I;
                            }
                        }
                        components.emplace_back(std::string(meta::arg_traits<T>::name));
                    }
                    components.emplace_back(
                        std::string(type_name<typename meta::arg_traits<T>::type>)
                    );
                    if constexpr (meta::arg_traits<T>::opt()) {
                        components.emplace_back(repr(
                            defaults.template get<Defaults::template find<I>>()
                        ));
                    } else {
                        components.emplace_back("");
                    }
                    std::forward<decltype(self)>(self).template operator()<I + 1>(
                        std::forward<decltype(defaults)>(defaults),
                        components
                    );
                }
            }(components);

            if constexpr (std::is_void_v<Return>) {
                components.emplace_back("None");
            } else {
                components.emplace_back(std::string(type_name<Return>));
            }

            return impl::format_signature(
                prefix,
                max_width,
                indent,
                components,
                last_posonly,
                first_kwonly
            );
        }
    };

    /* If this control structure is enabled, the unary `call()` operator will accept
    functions that may have partial and/or default arguments and call them directly,
    rather than requiring the creation of a separate defaults tuple.  This allows
    `call()` to be used on `def` statements and other bertrand function objects. */
    template <typename F>
    struct call_passthrough { static constexpr bool enable = false; };
    template <meta::inherits<def_tag> F>
    struct call_passthrough<F> { static constexpr bool enable = true; };
    template <meta::inherits<chain_tag> F>
        requires (call_passthrough<typename std::remove_cvref_t<F>::template at<0>>::enable)
    struct call_passthrough<F> { static constexpr bool enable = true; };

}


/* The canonical form of `signature`, which encapsulates all of the internal call
machinery, as much as possible of which is evaluated at compile time.  All other
specializations should redirect to this form in order to avoid reimplementing the nuts
and bolts of the function ecosystem. */
template <typename Return, typename... Args>
struct signature<Return(Args...)> :
    impl::CppSignature<impl::CppParam, Return, Args...>
{};


/// NOTE: bertrand::signature<> contains all of the logic necessary to introspect and
/// invoke C++ functions with Python-style conventions.  By default, it is enabled for
/// all trivially-introspectable function types, meaning that the underlying function
/// does not accept template parameters or participate in an overload set.  However, it
/// is still possible to support these cases by specializing `bertrand::signature<>`
/// for the desired function types, and then redirecting to a canonical signature via
/// inheritance.  Doing so will allow a non-trivial function to be used as the
/// initializer for a `bertrand::def` statement.
template <typename R, typename... A>
struct signature<R(A...) noexcept>                          : signature<R(A...)> {};
template <typename R, typename... A>
struct signature<R(*)(A...)>                                : signature<R(A...)> {};
template <typename R, typename... A>
struct signature<R(*)(A...) noexcept>                       : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...)>                             : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) &>                           : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) noexcept>                    : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) & noexcept>                  : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) const>                       : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) const &>                     : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) const noexcept>              : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) const & noexcept>            : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) volatile>                    : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) volatile &>                  : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) volatile noexcept>           : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) volatile & noexcept>         : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) const volatile>              : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) const volatile &>            : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) const volatile noexcept>     : signature<R(A...)> {};
template <typename R, typename C, typename... A>
struct signature<R(C::*)(A...) const volatile & noexcept>   : signature<R(A...)> {};
template <meta::has_call_operator T>
struct signature<T> : signature<decltype(&std::remove_reference_t<T>::operator())> {};
template <meta::inherits<impl::def_tag> T>
struct signature<T> : std::remove_reference_t<T>::Signature {};
template <meta::inherits<impl::chain_tag> T>
    requires (signature<typename std::remove_reference_t<T>::template at<0>>::enable)
struct signature<T> : signature<
    typename std::remove_reference_t<T>::template at<0>
>::template with_return<
    typename impl::chain_return_type<
        typename signature<typename std::remove_reference_t<T>::template at<0>>::Return,
        std::remove_cvref_t<T>,
        1
    >::type
> {};


namespace meta {

    /* A template constraint that controls whether the `bertrand::call()` operator is
    enabled for a given C++ function and argument list. */
    template <typename F, typename... Args>
    concept callable =
        bertrand::signature<F>::enable &&
        bertrand::signature<F>::args_fit_within_bitset &&
        bertrand::signature<F>::proper_argument_order &&
        bertrand::signature<F>::no_qualified_arg_annotations &&
        bertrand::signature<F>::no_duplicate_args &&
        bertrand::signature<F>::template Bind<Args...>::proper_argument_order &&
        bertrand::signature<F>::template Bind<Args...>::no_qualified_arg_annotations &&
        bertrand::signature<F>::template Bind<Args...>::no_duplicate_args &&
        bertrand::signature<F>::template Bind<Args...>::no_conflicting_values &&
        bertrand::signature<F>::template Bind<Args...>::no_extra_positional_args &&
        bertrand::signature<F>::template Bind<Args...>::no_extra_keyword_args &&
        bertrand::signature<F>::template Bind<Args...>::satisfies_required_args &&
        bertrand::signature<F>::template Bind<Args...>::can_convert;

    /* A template constraint that controls whether the `bertrand::def()` operator is
    enabled for a given C++ function and argument list. */
    template <typename F, typename... Args>
    concept partially_callable =
        bertrand::signature<F>::enable &&
        !(meta::arg_traits<Args>::variadic() || ...) &&
        bertrand::signature<F>::proper_argument_order &&
        bertrand::signature<F>::no_qualified_arg_annotations &&
        bertrand::signature<F>::no_duplicate_args &&
        bertrand::signature<F>::template Bind<Args...>::proper_argument_order &&
        bertrand::signature<F>::template Bind<Args...>::no_qualified_arg_annotations &&
        bertrand::signature<F>::template Bind<Args...>::no_duplicate_args &&
        bertrand::signature<F>::template Bind<Args...>::no_conflicting_values &&
        bertrand::signature<F>::template Bind<Args...>::no_extra_positional_args &&
        bertrand::signature<F>::template Bind<Args...>::no_extra_keyword_args &&
        bertrand::signature<F>::template Bind<Args...>::can_convert;

    /* A template constraint that controls whether the `bertrand::Function()` type can
    be instantiated with a given Python-compatible signature. */
    template <typename F>
    concept py_function =
        bertrand::signature<F>::enable &&
        bertrand::signature<F>::python &&
        bertrand::signature<F>::args_fit_within_bitset &&
        bertrand::signature<F>::proper_argument_order &&
        bertrand::signature<F>::no_qualified_args &&
        bertrand::signature<F>::no_qualified_arg_annotations &&
        bertrand::signature<F>::no_duplicate_args;

}


/* Invoke a C++ function with Python-style calling conventions, including keyword
arguments and/or parameter packs, which are resolved at compile time.  Note that the
function signature cannot contain any template parameters (including auto arguments),
as the function signature must be known unambiguously at compile time to implement the
required matching. */
template <typename F, typename... Args>
    requires (
        meta::callable<F, Args...> && (
            impl::call_passthrough<F>::enable ||
            (signature<F>::Partial::empty() && signature<F>::Defaults::empty())
        )
    )
constexpr decltype(auto) call(F&& func, Args&&... args) {
    if constexpr (impl::call_passthrough<F>::enable) {
        return std::forward<F>(func)(std::forward<Args>(args)...);
    } else {
        return typename signature<F>::template Bind<Args...>{}(
            typename signature<F>::Partial{},
            typename signature<F>::Defaults{},
            std::forward<F>(func),
            std::forward<Args>(args)...
        );
    }
}


/* Invoke a C++ function with Python-style calling conventions, including keyword
arguments and/or parameter packs, which are resolved at compile time.  Note that the
function signature cannot contain any template parameters (including auto arguments),
as the function signature must be known unambiguously at compile time to implement the
required matching. */
template <typename F, typename... Args>
    requires (meta::callable<F, Args...> && signature<F>::Partial::empty())
constexpr decltype(auto) call(
    const typename signature<F>::Defaults& defaults,
    F&& func,
    Args&&... args
) {
    return typename signature<F>::template Bind<Args...>{}(
        typename signature<F>::Partial{},
        defaults,
        std::forward<F>(func),
        std::forward<Args>(args)...
    );
}


/* Invoke a C++ function with Python-style calling conventions, including keyword
arguments and/or parameter packs, which are resolved at compile time.  Note that the
function signature cannot contain any template parameters (including auto arguments),
as the function signature must be known unambiguously at compile time to implement the
required matching. */
template <typename F, typename... Args>
    requires (meta::callable<F, Args...> && signature<F>::Partial::empty())
constexpr decltype(auto) call(
    typename signature<F>::Defaults&& defaults,
    F&& func,
    Args&&... args
) {
    return typename signature<F>::template Bind<Args...>{}(
        typename signature<F>::Partial{},
        std::move(defaults),
        std::forward<F>(func),
        std::forward<Args>(args)...
    );
}


/* Construct a partial function object that captures a C++ function and a subset of its
arguments, which can be used to invoke the function later with the remaining arguments.
Arguments and default values are given in the same style as `call()`, and will be
stored internally within the partial object, forcing a copy in the case of lvalue
inputs.  When the partial is called, an additional copy may be made if the function
expects a temporary or rvalue reference, so as not to modify the stored arguments.  If
the partial is called as an rvalue (by moving it, for example), then the second copy
can be avoided, and the stored arguments will be moved directly into the function call.

Note that the function signature cannot contain any template parameters (including auto
arguments), as the function signature must be known unambiguously at compile time to
implement the required matching.

The returned partial is a thin proxy that only implements the call operator and a
handful of introspection methods.  It also allows transparent access to the decorated
function via the `*` and `->` operators. */
template <typename Func, typename... Args>
    requires (
        signature<Func>::enable &&
        signature<Func>::Partial::empty() &&
        meta::partially_callable<Func, Args...>
    )
struct def : impl::def_tag {
private:
    template <typename, typename, size_t>
    struct bind_type;
    template <typename... out, typename sig, size_t I>
    struct bind_type<args<out...>, sig, I> { using type = def<Func, out...>; };
    template <typename... out, typename sig, size_t I> requires (I < sig::size())
    struct bind_type<args<out...>, sig, I> {
        template <typename T>
        struct filter { using type = args<out...>; };
        template <typename T> requires (meta::arg_traits<T>::bound())
        struct filter<T> {
            template <typename>
            struct extend;
            template <typename... bound>
            struct extend<args<bound...>> { using type = args<out..., bound...>; };
            using type = extend<typename meta::arg_traits<T>::bound_to>::type;
        };
        using type = bind_type<
            typename filter<typename sig::template at<I>>::type,
            sig,
            I + 1
        >::type;
    };

public:
    using Function = std::remove_cvref_t<Func>;
    using Partial = decltype(
        std::declval<typename signature<Function>::Partial>().bind(std::declval<Args>()...)
    );
    using Signature = Partial::type;
    using Defaults = Signature::Defaults;

    Defaults defaults;
    Function func;
    Partial partial;

    /* Allows access to the template constraints and underlying implementation for the
    call operator. */
    template <typename... A>
    using Bind = Signature::template Bind<A...>;

    template <meta::is<Func> F> requires (signature<F>::Defaults::empty())
    constexpr def(F&& func, Args... args) :
        defaults(),
        func(std::forward<F>(func)),
        partial(std::forward<Args>(args)...)
    {}

    template <meta::is<Func> F>
    constexpr def(const Defaults& defaults, F&& func, Args... args) :
        defaults(defaults),
        func(std::forward<F>(func)),
        partial(std::forward<Args>(args)...)
    {}

    template <meta::is<Func> F>
    constexpr def(Defaults&& defaults, F&& func, Args... args) :
        defaults(std::move(defaults)),
        func(std::forward<F>(func)),
        partial(std::forward<Args>(args)...)
    {}

    template <std::convertible_to<Defaults> D, meta::is<Func> F, std::convertible_to<Partial> P>
    constexpr def(D&& defaults, F&& func, P&& partial) :
        defaults(std::forward<D>(defaults)),
        func(std::forward<F>(func)),
        partial(std::forward<P>(partial))
    {}

    /* Dereference a `def` object to access the underlying function. */
    [[nodiscard]] constexpr Function&& operator*() && noexcept { return std::move(func); }
    [[nodiscard]] constexpr Function& operator*() & noexcept { return func; }
    [[nodiscard]] constexpr Function* operator->() noexcept { return &func; }
    [[nodiscard]] constexpr const Function& operator*() const noexcept { return func; }
    [[nodiscard]] constexpr const Function* operator->() const noexcept { return &func; }

    /* Get the partial value at index I if it is within range. */
    template <size_t I, typename Self> requires (I < Partial::size())
    [[nodiscard]] constexpr decltype(auto) get(this Self&& self) {
        return std::forward<Self>(self).partial.template get<I>();
    }

    /* Get the partial value of the named argument if it is present. */
    template <static_str name, typename Self> requires (Partial::template contains<name>())
    [[nodiscard]] constexpr decltype(auto) get(this Self&& self) {
        return std::forward<Self>(self).partial.template get<name>();
    }

    /* Invoke the function, applying the semantics of the inferred signature. */
    template <typename Self, typename... A>
        requires (
            Bind<A...>::proper_argument_order &&
            Bind<A...>::no_qualified_arg_annotations &&
            Bind<A...>::no_duplicate_args &&
            Bind<A...>::no_extra_positional_args &&
            Bind<A...>::no_extra_keyword_args &&
            Bind<A...>::no_conflicting_values &&
            Bind<A...>::satisfies_required_args &&
            Bind<A...>::can_convert
        )
    constexpr decltype(auto) operator()(this Self&& self, A&&... args) {
        return Bind<A...>{}(
            std::forward<Self>(self).partial,
            std::forward<Self>(self).defaults,
            std::forward<Self>(self).func,
            std::forward<A>(args)...
        );
    }

    /* Generate a partial function with the given arguments filled in.  The method can
    be chained - any existing partial arguments will be carried over to the result, and
    will not be considered when binding the new arguments. */
    template <typename Self, typename... A>
        requires (
            !(meta::arg_traits<A>::variadic() || ...) &&
            Bind<A...>::proper_argument_order &&
            Bind<A...>::no_qualified_arg_annotations &&
            Bind<A...>::no_duplicate_args &&
            Bind<A...>::no_extra_positional_args &&
            Bind<A...>::no_extra_keyword_args &&
            Bind<A...>::no_conflicting_values &&
            Bind<A...>::can_convert
        )
    [[nodiscard]] constexpr auto bind(this Self&& self, A&&... args) {
        using sig = std::remove_cvref_t<
            decltype(partial.bind(std::forward<A>(args)...))
        >::type;
        return []<size_t... Is>(
            std::index_sequence<Is...>,
            auto&& defaults,
            auto&& func,
            auto&& partial
        ) {
            return typename bind_type<bertrand::args<>, sig, 0>::type(
                std::forward<decltype(defaults)>(defaults),
                std::forward<decltype(func)>(func),
                std::forward<decltype(partial)>(partial)
            );
        }(
            std::make_index_sequence<sig::Partial::size()>{},
            std::forward<Self>(self).defaults,
            std::forward<Self>(self).func,
            std::forward<Self>(self).partial.bind(std::forward<A>(args)...)
        );
    }

    /* Clear any partial arguments that have been accumulated thus far, returning a new
    function object without them. */
    template <typename Self>
    [[nodiscard]] constexpr def<Func> unbind(this Self&& self) {
        return def<Func>(std::forward<Self>(self).defaults, std::forward<Self>(self).func);
    }
};


template <typename F, typename... A>
    requires (
        signature<F>::Defaults::empty() &&
        signature<F>::Partial::empty() &&
        meta::partially_callable<F, A...>
    )
def(F, A&&...) -> def<F, A...>;
template <typename F, typename... A>
    requires (
        signature<F>::Partial::empty() &&
        meta::partially_callable<F, A...>
    )
def(typename signature<F>::Defaults&&, F, A&&...) -> def<F, A...>;
template <typename F, typename... A>
    requires (
        signature<F>::Partial::empty() &&
        meta::partially_callable<F, A...>
    )
def(const typename signature<F>::Defaults&, F, A&&...) -> def<F, A...>;


template <typename L, typename R>
    requires (
        (meta::inherits<L, impl::def_tag> || meta::inherits<R, impl::def_tag>) &&
        (!meta::inherits<L, impl::chain_tag> && !meta::inherits<R, impl::chain_tag>) &&
        signature<L>::enable &&
        std::is_invocable_v<R, typename signature<L>::Return>
    )
[[nodiscard]] constexpr auto operator>>(L&& lhs, R&& rhs) {
    return chain(std::forward<L>(lhs)) >> std::forward<R>(rhs);
}


}  // namespace bertrand


/* Specializing `std::tuple_size`, `std::tuple_element`, and `std::get` allows
argument packs, function chains, `def` objects, and signature tuples to be decomposed
using structured bindings. */
namespace std {

    template <bertrand::meta::args T>
    struct tuple_size<T> :
        std::integral_constant<size_t, std::remove_cvref_t<T>::size()>
    {};

    template <bertrand::meta::chain T>
    struct tuple_size<T> :
        std::integral_constant<size_t, std::remove_cvref_t<T>::size()>
    {};

    template <bertrand::meta::def T>
    struct tuple_size<T> :
        std::integral_constant<size_t, bertrand::signature<T>::size()>
    {};

    template <bertrand::meta::signature T>
    struct tuple_size<T> :
        std::integral_constant<size_t, std::remove_cvref_t<T>::size()>
    {};

    template <bertrand::meta::signature_defaults T>
    struct tuple_size<T> :
        std::integral_constant<size_t, std::remove_cvref_t<T>::size()>
    {};

    template <bertrand::meta::signature_partial T>
    struct tuple_size<T> :
        std::integral_constant<size_t, std::remove_cvref_t<T>::size()>
    {};

    template <size_t I, bertrand::meta::args T>
        requires (I < std::remove_cvref_t<T>::size())
    struct tuple_element<I, T> {
        using type = std::remove_cvref_t<T>::template at<I>;
    };

    template <size_t I, bertrand::meta::chain T>
        requires (I < std::remove_cvref_t<T>::size())
    struct tuple_element<I, T> {
        using type = decltype(std::declval<T>().template get<I>());
    };

    template <size_t I, bertrand::meta::def T>
        requires (I < bertrand::signature<T>::size())
    struct tuple_element<I, T> {
        using type = decltype(std::declval<T>().template get<I>());
    };

    template <size_t I, bertrand::meta::signature T>
    struct tuple_element<I, T> {
        using type = decltype(std::declval<T>().template get<I>());
    };

    template <size_t I, bertrand::meta::signature_defaults T>
        requires (I < std::remove_cvref_t<T>::size())
    struct tuple_element<I, T> {
        using type = decltype(std::declval<T>().template get<I>());
    };

    template <size_t I, bertrand::meta::signature_partial T>
        requires (I < std::remove_cvref_t<T>::size())
    struct tuple_element<I, T> {
        using type = decltype(std::declval<T>().template get<I>());
    };

    template <size_t I, bertrand::meta::args T>
        requires (!std::is_lvalue_reference_v<T> && I < std::remove_cvref_t<T>::size())
    [[nodiscard]] constexpr decltype(auto) get(T&& args) noexcept {
        return std::forward<T>(args).template get<I>();
    }

    template <size_t I, bertrand::meta::chain T>
        requires (I < std::remove_cvref_t<T>::size())
    [[nodiscard]] constexpr decltype(auto) get(T&& chain) {
        return std::forward<T>(chain).template get<I>();
    }

    template <size_t I, bertrand::meta::def T>
        requires (I < bertrand::signature<T>::size())
    [[nodiscard]] constexpr decltype(auto) get(T&& def) noexcept {
        return std::forward<T>(def).template get<I>();
    }

    template <size_t I, bertrand::meta::signature T>
        requires (I < std::remove_cvref_t<T>::size())
    [[nodiscard]] constexpr decltype(auto) get(T&& signature) noexcept {
        return std::forward<T>(signature).template get<I>();
    }

    template <size_t I, bertrand::meta::signature_defaults T>
        requires (I < std::remove_cvref_t<T>::size())
    [[nodiscard]] constexpr decltype(auto) get(T&& defaults) noexcept {
        return std::forward<T>(defaults).template get<I>();
    }

    template <size_t I, bertrand::meta::signature_partial T>
        requires (I < std::remove_cvref_t<T>::size())
    [[nodiscard]] constexpr decltype(auto) get(T&& partial) noexcept {
        return std::forward<T>(partial).template get<I>();
    }

    template <bertrand::static_str name, bertrand::meta::def T>
        requires (bertrand::signature<T>::template contains<name>())
    [[nodiscard]] constexpr decltype(auto) get(T&& def) noexcept {
        return std::forward<T>(def).template get<name>();
    }

    template <bertrand::static_str name, bertrand::meta::signature T>
        requires (std::remove_cvref_t<T>::template contains<name>())
    [[nodiscard]] constexpr decltype(auto) get(T&& signature) noexcept {
        return std::forward<T>(signature).template get<name>();
    }

    template <bertrand::static_str name, bertrand::meta::signature_defaults T>
        requires (std::remove_cvref_t<T>::template contains<name>())
    [[nodiscard]] constexpr decltype(auto) get(T&& defaults) noexcept {
        return std::forward<T>(defaults).template get<name>();
    }

    template <bertrand::static_str name, bertrand::meta::signature_partial T>
        requires (std::remove_cvref_t<T>::template contains<name>())
    [[nodiscard]] constexpr decltype(auto) get(T&& partial) noexcept {
        return std::forward<T>(partial).template get<name>();
    }

}


/* The dereference operator can be used to emulate Python container unpacking when
calling a Python-style function from C++.

A single unpacking operator passes the contents of an iterable container as positional
arguments to a function.  Unlike Python, only one such operator is allowed per call,
and it must be the last positional argument in the parameter list.  This allows the
compiler to ensure that the container's value type is minimally convertible to each of
the remaining positional arguments ahead of time, even though the number of arguments
cannot be determined until runtime.  Thus, if any arguments are missing or extras are
provided, the call will raise an exception similar to Python, rather than failing
statically at compile time.  This can be avoided by using standard positional and
keyword arguments instead, which can be fully verified at compile time, or by including
variadic positional arguments in the function signature, which will consume any
remaining arguments according to Python semantics.

A second unpacking operator promotes the arguments into keywords, and can only be used
if the container is mapping-like, meaning it possess both `::key_type` and
`::mapped_type` aliases, and that indexing it with an instance of the key type returns
a value of the mapped type.  The actual unpacking is robust, and will attempt to use
iterators over the container to produce key-value pairs, either directly through
`begin()` and `end()` or by calling the `.items()` method if present, followed by
zipping `.keys()` and `.values()` if both exist, and finally by iterating over the keys
and indexing into the container.  Similar to the positional unpacking operator, only
one of these may be present as the last keyword argument in the parameter list, and a
compile-time check is made to ensure that the mapped type is convertible to any missing
keyword arguments that are not explicitly provided at the call site.

In both cases, the extra runtime complexity results in a small performance degradation
over a typical function call, which is minimized as much as possible. */
template <bertrand::meta::iterable T> requires (bertrand::meta::unpack_operator<T>)
[[nodiscard]] constexpr auto operator*(T&& value) {
    return bertrand::arg_pack{std::forward<T>(value)};
}


/* Apply a C++ range adaptor to a container via the comprehension operator.  This is
similar to the C++-style `|` operator for chaining range adaptors, but uses the `->*`
operator to avoid conflicts with other operator overloads and apply higher precedence
than typical binary operators. */
template <typename T, typename V>
    requires (
        bertrand::meta::comprehension_operator<T> &&
        bertrand::meta::viewable<T, V>
    )
[[nodiscard]] constexpr auto operator->*(T&& value, V&& view) {
    return std::views::all(std::forward<T>(value)) | std::forward<V>(view);
}


/* Generate a C++ range adaptor that approximates a Python-style list comprehension.
This is done by piping a function in place of a C++ range adaptor, which will be
applied to each element in the sequence.  The function must be callable with the
container's value type, and may return any type.

If the function returns another range adaptor, then the adaptor's output will be
flattened into the parent range, similar to a nested `for` loop within a Python
comprehension.  Returning a range with no elements will effectively filter out the
current element, similar to a Python `if` clause within a comprehension.

Here's an example:

    std::vector vec = {1, 2, 3, 4, 5};
    std::vector new_vec = vec->*[](int x) {
        return std::views::repeat(x, x % 2 ? 0 : x);
    };
    for (int x : new_vec) {
        std::cout << x << ", ";  // 2, 2, 4, 4, 4, 4,
    }

This is functionally equivalent to `std::views::transform()` in C++ and uses that
implementation under the hood.  The only difference is the added logic for flattening
nested ranges, which is extremely lightweight. */
template <typename T, typename F>
    requires (
        bertrand::meta::comprehension_operator<T> &&
        bertrand::meta::transformable<T, F>
    )
[[nodiscard]] constexpr auto operator->*(T&& value, F&& func) {
    return bertrand::comprehension{std::forward<T>(value), std::forward<F>(func)};
}






namespace bertrand {

    template <typename T>
    constexpr bool meta::detail::enable_unpack_operator<std::vector<T>> = true;
    template <typename T>
    constexpr bool meta::detail::enable_comprehension_operator<std::vector<T>> = true;

    inline void test() {
        constexpr def sub(
            { arg<"x"> = 10, arg<"y"> = 2 },
            [](Arg<"x", int>::opt x, Arg<"y", int>::opt y) {
                return *x - *y;
            }
        );
        constexpr def div(
            { arg<"y"> = 2 },
            [](Arg<"x", int> x, Arg<"y", int>::opt y) {
                return *x / *y;
            }
        );
        static_assert(sub(10, 2) == 8);

        // constexpr auto chain = sub >> div.bind(arg<"y"> = 2) >> div;
        constexpr auto chain = sub >> div.bind(arg<"y"> = 2) >>  [](auto&& x) {
            return std::forward<decltype(x)>(x);
        };
        static_assert(chain(10, 2) == 4);
        static_assert(chain.get<1>().defaults.size() == 1);

        std::vector vec = {1, 2, 3};
        std::vector<int> new_vec = vec->*[](int x) { return x * 2; };
        auto view = vec->*std::views::transform([](int x) { return x * 2; });
        auto result = sub(*vec);
        for (int x : vec->*[](int x) { return x * 2; }) {
            std::cout << x << std::endl;
        }
    }

}


#endif  // BERTRAND_FUNC_H

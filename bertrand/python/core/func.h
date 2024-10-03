#ifndef BERTRAND_PYTHON_CORE_FUNC_H
#define BERTRAND_PYTHON_CORE_FUNC_H

#include "declarations.h"
#include "object.h"
#include "except.h"
#include "ops.h"
#include "access.h"


namespace py {


namespace impl {

    struct ArgKind {
        enum Flags : uint8_t {
            POS                 = 0b1,
            KW                  = 0b10,
            OPT                 = 0b100,
            VARIADIC            = 0b1000,
        } flags;

        constexpr ArgKind(uint8_t flags = 0) noexcept :
            flags(static_cast<Flags>(flags))
        {}
    
        constexpr operator uint8_t() const noexcept {
            return flags;
        }

        constexpr bool posonly() const noexcept {
            return (flags & ~OPT) == POS;
        }

        constexpr bool pos() const noexcept {
            return (flags & POS) & !(flags & VARIADIC);
        }

        constexpr bool args() const noexcept {
            return flags == (POS | VARIADIC);
        }

        constexpr bool kwonly() const noexcept {
            return (flags & ~OPT) == KW;
        }

        constexpr bool kw() const noexcept {
            return (flags & KW) & !(flags & VARIADIC);
        }

        constexpr bool kwargs() const noexcept {
            return flags == (KW | VARIADIC);
        }

        constexpr bool opt() const noexcept {
            return flags & OPT;
        }

        constexpr bool variadic() const noexcept {
            return flags & VARIADIC;
        }
    };

    template <typename T>
    struct OptionalArg {
        using type = T::type;
        using opt = OptionalArg;
        static constexpr StaticStr name = T::name;
        static constexpr ArgKind kind = T::kind | ArgKind::OPT;

        type value;

        [[nodiscard]] operator type() && {
            if constexpr (std::is_lvalue_reference_v<type>) {
                return value;
            } else {
                return std::move(value);
            }
        }

        template <typename U> requires (std::convertible_to<type, U>)
        [[nodiscard]] operator U() && {
            if constexpr (std::is_lvalue_reference_v<type>) {
                return value;
            } else {
                return std::move(value);
            }
        }
    };

    template <StaticStr Name, typename T>
    struct PositionalArg {
        using type = T;
        using opt = OptionalArg<PositionalArg>;
        static constexpr StaticStr name = Name;
        static constexpr ArgKind kind = ArgKind::POS;

        type value;

        [[nodiscard]] operator type() && {
            if constexpr (std::is_lvalue_reference_v<type>) {
                return value;
            } else {
                return std::move(value);
            }
        }

        template <typename U> requires (std::convertible_to<type, U>)
        [[nodiscard]] operator U() && {
            if constexpr (std::is_lvalue_reference_v<type>) {
                return value;
            } else {
                return std::move(value);
            }
        }
    };

    template <StaticStr Name, typename T>
    struct KeywordArg {
        using type = T;
        using opt = OptionalArg<KeywordArg>;
        static constexpr StaticStr name = Name;
        static constexpr ArgKind kind = ArgKind::KW;

        type value;

        [[nodiscard]] operator type() && {
            if constexpr (std::is_lvalue_reference_v<type>) {
                return value;
            } else {
                return std::move(value);
            }
        }

        template <typename U> requires (std::convertible_to<type, U>)
        [[nodiscard]] operator U() && {
            if constexpr (std::is_lvalue_reference_v<type>) {
                return value;
            } else {
                return std::move(value);
            }
        }
    };

    template <StaticStr Name, typename T>
    struct VarArgs {
        using type = std::conditional_t<
            std::is_rvalue_reference_v<T>,
            std::remove_reference_t<T>,
            std::conditional_t<
                std::is_lvalue_reference_v<T>,
                std::reference_wrapper<T>,
                T
            >
        >;
        static constexpr StaticStr name = Name;
        static constexpr ArgKind kind = ArgKind::POS | ArgKind::VARIADIC;

        std::vector<type> value;

        [[nodiscard]] operator std::vector<type>() && {
            return std::move(value);
        }
    };

    template <StaticStr Name, typename T>
    struct VarKwargs {
        using type = std::conditional_t<
            std::is_rvalue_reference_v<T>,
            std::remove_reference_t<T>,
            std::conditional_t<
                std::is_lvalue_reference_v<T>,
                std::reference_wrapper<T>,
                T
            >
        >;
        static constexpr StaticStr name = Name;
        static constexpr ArgKind kind = ArgKind::KW | ArgKind::VARIADIC;

        std::unordered_map<std::string, T> value;

        [[nodiscard]] operator std::unordered_map<std::string, T>() && {
            return std::move(value);
        }
    };

    /* A keyword parameter pack obtained by double-dereferencing a Python object. */
    template <mapping_like T>
    struct KwargPack {
        using key_type = T::key_type;
        using mapped_type = T::mapped_type;
        using type = mapped_type;
        static constexpr StaticStr name = "";
        static constexpr ArgKind kind = ArgKind::KW | ArgKind::VARIADIC;

        T value;

    private:

        static constexpr bool can_iterate =
            impl::yields_pairs_with<T, key_type, mapped_type> ||
            impl::has_items<T> ||
            (impl::has_keys<T> && impl::has_values<T>) ||
            (impl::yields<T, key_type> && impl::lookup_yields<T, mapped_type, key_type>) ||
            (impl::has_keys<T> && impl::lookup_yields<T, mapped_type, key_type>);

        auto transform() const {
            if constexpr (impl::yields_pairs_with<T, key_type, mapped_type>) {
                return value;

            } else if constexpr (impl::has_items<T>) {
                return value.items();

            } else if constexpr (impl::has_keys<T> && impl::has_values<T>) {
                return std::ranges::views::zip(value.keys(), value.values());

            } else if constexpr (
                impl::yields<T, key_type> && impl::lookup_yields<T, mapped_type, key_type>
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

        template <typename U = T> requires (can_iterate)
        auto begin() const { return std::ranges::begin(transform()); }
        template <typename U = T> requires (can_iterate)
        auto cbegin() const { return begin(); }
        template <typename U = T> requires (can_iterate)
        auto end() const { return std::ranges::end(transform()); }
        template <typename U = T> requires (can_iterate)
        auto cend() const { return end(); }
    };

    /* A positional parameter pack obtained by dereferencing a Python object. */
    template <iterable T>
    struct ArgPack {
        using type = iter_type<T>;
        static constexpr StaticStr name = "";
        static constexpr ArgKind kind = ArgKind::POS | ArgKind::VARIADIC;

        T value;

        auto begin() const { return std::ranges::begin(value); }
        auto cbegin() const { return begin(); }
        auto end() const { return std::ranges::end(value); }
        auto cend() const { return end(); }

        template <typename U = T> requires (impl::mapping_like<U>)
        auto operator*() const {
            return KwargPack<T>{std::forward<T>(value)};
        }
    };

}


/* A compile-time argument annotation that represents a bound positional or keyword
argument to a py::Function.  Uses aggregate initialization to extend the lifetime of
temporaries. */
template <StaticStr Name, typename T>
struct Arg {
    static_assert(Name != "", "Argument must have a name.");

    using type = T;
    using pos = impl::PositionalArg<Name, T>;
    using args = impl::VarArgs<Name, T>;
    using kw = impl::KeywordArg<Name, T>;
    using kwargs = impl::VarKwargs<Name, T>;
    using opt = impl::OptionalArg<Arg>;

    static constexpr StaticStr name = Name;
    static constexpr impl::ArgKind kind = impl::ArgKind::POS | impl::ArgKind::KW;

    type value;

    /* Argument rvalues are normally generated whenever a function is called.  Making
    them convertible to the underlying type means they can be used to call external
    C++ functions that are not aware of Python argument annotations. */
    [[nodiscard]] operator type() && {
        if constexpr (std::is_lvalue_reference_v<type>) {
            return value;
        } else {
            return std::move(value);
        }
    }

    /* Conversions to other types are also allowed, as long as the underlying type
    supports it. */
    template <typename U> requires (std::convertible_to<type, U>)
    [[nodiscard]] operator U() && {
        if constexpr (std::is_lvalue_reference_v<type>) {
            return value;
        } else {
            return std::move(value);
        }
    }
};


namespace impl {

    /* A singleton argument factory that allows arguments to be constructed via
    familiar assignment syntax, which extends the lifetime of temporaries. */
    template <StaticStr Name>
    struct ArgFactory {
        template <typename T>
        constexpr Arg<Name, T> operator=(T&& value) const {
            return {std::forward<T>(value)};
        }
    };

    /* Default seed for FNV-1a hash function. */
    constexpr size_t fnv1a_hash_seed = [] {
        if constexpr (sizeof(size_t) > 4) {
            return 14695981039346656037ULL;
        } else {
            return 2166136261u;
        }
    }();

    /* Default prime for FNV-1a hash function. */
    constexpr size_t fnv1a_hash_prime = [] {
        if constexpr (sizeof(size_t) > 4) {
            return 1099511628211ULL;
        } else {
            return 16777619u;
        }
    }();

    /* In the vast majority of cases, adjusting the seed is all that's needed to get a
    good FNV-1a hash, but just in case, we also provide the next 9 primes in case the
    default value cannot be used. */
    constexpr std::array<size_t, 10> fnv1a_fallback_primes = [] -> std::array<size_t, 10> {
        if constexpr (sizeof(size_t) > 4) {
            return {
                fnv1a_hash_prime,
                1099511628221ULL,
                1099511628227ULL,
                1099511628323ULL,
                1099511628329ULL,
                1099511628331ULL,
                1099511628359ULL,
                1099511628401ULL,
                1099511628403ULL,
                1099511628427ULL,
            };
        } else {
            return {
                fnv1a_hash_prime,
                16777633u,
                16777639u,
                16777643u,
                16777669u,
                16777679u,
                16777681u,
                16777699u,
                16777711u,
                16777721,
            };
        }
    }();

    /* A deterministic FNV-1a string hashing function that gives the same results at
    both compile time and run time. */
    constexpr size_t fnv1a(
        const char* str,
        size_t seed = fnv1a_hash_seed,
        size_t prime = fnv1a_hash_prime
    ) noexcept {
        while (*str) {
            seed ^= static_cast<size_t>(*str);
            seed *= prime;
            ++str;
        }
        return seed;
    }

    /* Round a number up to the next power of two unless it is one already. */
    template <std::unsigned_integral T>
    T next_power_of_two(T n) {
        constexpr size_t bits = sizeof(T) * 8;
        --n;
        for (size_t i = 1; i < bits; i <<= 1) {
            n |= (n >> i);
        }
        return ++n;
    }

    /* A simple, trivially-destructible representation of a single parameter in a
    function signature or call site. */
    struct Param {
        std::string_view name;
        PyObject* type;
        ArgKind kind;

        constexpr bool posonly() const noexcept { return kind.posonly(); }
        constexpr bool pos() const noexcept { return kind.pos(); }
        constexpr bool args() const noexcept { return kind.args(); }
        constexpr bool kwonly() const noexcept { return kind.kwonly(); }
        constexpr bool kw() const noexcept { return kind.kw(); }
        constexpr bool kwargs() const noexcept { return kind.kwargs(); }
        constexpr bool opt() const noexcept { return kind.opt(); }
        constexpr bool variadic() const noexcept { return kind.variadic(); }

        /* Compute a hash of this parameter's name, type, and kind, using the given
        FNV-1a hash seed and prime. */
        size_t hash(size_t seed, size_t prime) const noexcept {
            return hash_combine(
                fnv1a(name.data(), seed, prime),
                reinterpret_cast<size_t>(type),
                static_cast<size_t>(kind)
            );
        }

        /* Parse a C++ string that represents an argument name, throwing an error if it
        is invalid. */
        static std::string_view get_name(std::string_view str) {
            std::string_view sub = str.substr(
                str.starts_with("*") +
                str.starts_with("**")
            );
            if (sub.empty()) {
                throw TypeError("argument name cannot be empty");
            } else if (std::isdigit(sub.front())) {
                throw TypeError(
                    "argument name cannot start with a number: '" +
                    std::string(sub) + "'"
                );
            }
            for (const char c : sub) {
                if (std::isalnum(c) || c == '_') {
                    continue;
                }
                throw TypeError(
                    "argument name must only contain alphanumerics and underscores: '" +
                    std::string(sub) + "'"
                );
            }
            return str;
        }

        /* Parse a Python string that represents an argument name, throwing an error if
        it is invalid. */
        static std::string_view get_name(PyObject* str) {
            Py_ssize_t len;
            const char* data = PyUnicode_AsUTF8AndSize(str, &len);
            if (data == nullptr) {
                Exception::from_python();
            }
            return get_name({data, static_cast<size_t>(len)});
        }

    };

    /* A read-only container of `Param` objects that also holds a combined hash
    suitable for cache optimization when searching a function's overload trie.  The
    underlying container type is flexible, and will generally be either a `std::array`
    (if the number of arguments is known ahead of time) or a `std::vector` (if they
    must be dynamic), but any container that supports read-only iteration, item access,
    and `size()` queries is supported. */
    template <yields<const Param&> T>
        requires (has_size<T> && lookup_yields<T, const Param&, size_t>)
    struct Params {
        T value;
        size_t hash = 0;

        const Param& operator[](size_t i) const noexcept { return value[i]; }
        size_t size() const noexcept { return std::ranges::size(value); }
        bool empty() const noexcept { return std::ranges::empty(value); }
        auto begin() const noexcept { return std::ranges::begin(value); }
        auto cbegin() const noexcept { return std::ranges::cbegin(value); }
        auto end() const noexcept { return std::ranges::end(value); }
        auto cend() const noexcept { return std::ranges::cend(value); }
    };

    /* Inspect an annotated Python function and extract its inline type hints so that
    they can be translated into corresponding parameter lists. */
    struct Inspect {
    private:

        static Object import_typing() {
            PyObject* typing = PyImport_ImportModule("typing");
            if (typing == nullptr) {
                Exception::from_python();
            }
            return reinterpret_steal<Object>(typing);
        }

        static Object import_types() {
            PyObject* types = PyImport_ImportModule("types");
            if (types == nullptr) {
                Exception::from_python();
            }
            return reinterpret_steal<Object>(types);
        }

        Object get_signature() const {
            // signature = inspect.signature(func)
            // hints = typing.get_type_hints(func)
            // signature = signature.replace(
            //      return_annotation=hints.get("return", inspect.Parameter.empty),
            //      parameters=[
            //         p if p.annotation is inspect.Parameter.empty else
            //         p.replace(annotation=hints[p.name])
            //         for p in signature.parameters.values()
            //     ]
            // )
            Object signature = reinterpret_steal<Object>(PyObject_CallOneArg(
                ptr(getattr<"signature">(inspect)),
                ptr(func)
            ));
            if (signature.is(nullptr)) {
                Exception::from_python();
            }
            PyObject* get_type_hints_args[] = {ptr(func), Py_True};
            Object get_type_hints_kwnames = reinterpret_steal<Object>(
                PyTuple_Pack(1, TemplateString<"include_extras">::ptr)
            );
            Object hints = reinterpret_steal<Object>(PyObject_Vectorcall(
                ptr(getattr<"get_type_hints">(typing)),
                get_type_hints_args,
                1,
                ptr(get_type_hints_kwnames)
            ));
            if (hints.is(nullptr)) {
                Exception::from_python();
            }
            Object empty = getattr<"empty">(getattr<"Parameter">(inspect));
            Object parameters = reinterpret_steal<Object>(PyObject_CallMethodNoArgs(
                ptr(getattr<"parameters">(signature)),
                TemplateString<"values">::ptr
            ));
            Object new_params = reinterpret_steal<Object>(
                PyList_New(len(parameters))
            );
            Py_ssize_t idx = 0;
            for (Object param : parameters) {
                Object annotation = getattr<"annotation">(param);
                if (!annotation.is(empty)) {
                    annotation = reinterpret_steal<Object>(PyDict_GetItemWithError(
                        ptr(hints),
                        ptr(getattr<"name">(param))
                    ));
                    if (annotation.is(nullptr)) {
                        if (PyErr_Occurred()) {
                            Exception::from_python();
                        } else {
                            throw KeyError(
                                "no type hint for parameter: " + repr(param)
                            );
                        }
                    }
                    PyObject* replace_args[] = {ptr(annotation)};
                    Object replace_kwnames = reinterpret_steal<Object>(PyTuple_Pack(
                        1,
                        TemplateString<"annotation">::ptr
                    ));
                    if (replace_kwnames.is(nullptr)) {
                        Exception::from_python();
                    }
                    param = reinterpret_steal<Object>(PyObject_Vectorcall(
                        ptr(getattr<"replace">(param)),
                        replace_args,
                        0,
                        ptr(replace_kwnames)
                    ));
                    if (param.is(nullptr)) {
                        Exception::from_python();
                    }
                }
                // steals a reference
                PyList_SET_ITEM(ptr(new_params), idx++, release(param));
            }
            Object return_annotation = reinterpret_steal<Object>(PyDict_GetItem(
                ptr(hints),
                TemplateString<"return">::ptr
            ));
            if (return_annotation.is(nullptr)) {
                return_annotation = empty;
            }
            PyObject* replace_args[] = {
                ptr(return_annotation),
                ptr(new_params)
            };
            Object replace_kwnames = reinterpret_steal<Object>(PyTuple_Pack(
                2,
                TemplateString<"return_annotation">::ptr,
                TemplateString<"parameters">::ptr
            ));
            if (replace_kwnames.is(nullptr)) {
                Exception::from_python();
            }
            signature = reinterpret_steal<Object>(PyObject_Vectorcall(
                ptr(getattr<"replace">(signature)),
                replace_args,
                0,
                ptr(replace_kwnames)
            ));
            if (signature.is(nullptr)) {
                Exception::from_python();
            }
            return signature;
        }

    public:
        Object func;
        Object signature;
        size_t seed;
        size_t prime;

        explicit Inspect(
            PyObject* func,
            size_t seed = fnv1a_hash_seed,
            size_t prime = fnv1a_hash_prime
        ) : func(reinterpret_borrow<Object>(func)),
            signature(get_signature()),
            seed(seed),
            prime(prime)
        {}

        Inspect(const Inspect& other) = delete;
        Inspect(Inspect&& other) = delete;
        Inspect& operator=(const Inspect& other) = delete;
        Inspect& operator=(Inspect&& other) noexcept = delete;

        /* A callback function to use when parsing inline type hints within a Python
        function declaration. */
        struct Callback {
            using Func = std::function<bool(Object, std::vector<PyObject*>& result)>;
            std::string id;
            Func callback;
        };

        /* Initiate a search of the callback map in order to parse a Python-style type
        hint.  The search stops at the first callback that returns true, otherwise the
        hint is interpreted as either a single type if it is a Python class, or a
        generic `object` type otherwise. */
        static void parse(Object hint, std::vector<PyObject*>& out) {
            for (const Callback& cb : callbacks) {
                if (cb.callback(hint, out)) {
                    return;
                }
            }

            // Annotated types are unwrapped and reprocessed if not handled by a callback
            Object typing = import_typing();
            Object origin = reinterpret_steal<Object>(PyObject_CallOneArg(
                ptr(getattr<"get_origin">(typing)),
                ptr(hint)
            ));
            if (origin.is(nullptr)) {
                Exception::from_python();
            } else if (origin.is(getattr<"Annotated">(typing))) {
                Object args = reinterpret_steal<Object>(PyObject_CallOneArg(
                    ptr(getattr<"get_args">(typing)),
                    ptr(hint)
                ));
                if (args.is(nullptr)) {
                    Exception::from_python();
                }
                parse(reinterpret_borrow<Object>(
                    PyTuple_GET_ITEM(ptr(args), 0)
                ), out);
                return;
            }

            // unrecognized hints are assumed to implement `issubclass()`
            out.push_back(ptr(hint));
        }

        /* In order to provide custom handlers for Python type hints, each annotation
        will be passed through a series of callbacks that convert it into a flat list
        of Python types, which will be used to generate the final overload keys.

        Each callback is tested in order and expected to return true if it can handle
        the hint, in which case the search terminates and the final state of the `out`
        vector will be pushed into the set of possible overload keys.  If no callback
        can handle a given hint, then it is interpreted as a single type if it is a
        Python class, or as a generic `object` type otherwise, which is equivalent to
        Python's `typing.Any`.  Some type hints, such as `Union` and `Optional`, will
        recursively search the callback map in order to split the hint into its
        constituent types, which will be registered as unique overloads.

        Note that `inspect.get_type_hints(include_extras=True)` is used to extract the
        type hints from the function signature, meaning that stringized annotations and
        forward references will be normalized before any callbacks are invoked.  The
        `include_extras` flag is used to ensure that `typing.Annotated` hints are
        preserved, so that they can be interpreted by the callback map if necessary.
        The default behavior in this case is to simply extract the underlying type,
        but custom callbacks can be added to interpret these annotations as needed.

        For performance reasons, the types that are added to the `out` vector are
        always expected to be BORROWED references, and do not own the underlying
        type objects.  This allows the overload keys to be trivially destructible,
        which avoids an extra loop in their destructors.  Since an overload key is
        created every time a function is called, this is significant. */
        inline static std::vector<Callback> callbacks {
            /// NOTE: Callbacks are linearly searched, so more common constructs should
            /// be generally placed at the front of the list for performance reasons.
            {
                /// TODO: handling GenericAlias types is going to be fairly complicated, 
                /// and will require interactions with the global type map, and thus a
                /// forward declaration here.
                "types.GenericAlias",
                [](Object hint, std::vector<PyObject*>& out) -> bool {
                    Object types = import_types();
                    if (isinstance(hint, getattr<"GenericAlias">(types))) {
                        Object typing = import_typing();
                        Object origin = reinterpret_steal<Object>(PyObject_CallOneArg(
                            ptr(getattr<"get_origin">(typing)),
                            ptr(hint)
                        ));
                        /// TODO: search in type map or fall back to Object
                        Object args = reinterpret_steal<Object>(PyObject_CallOneArg(
                            ptr(getattr<"get_args">(typing)),
                            ptr(hint)
                        ));
                        /// TODO: parametrize the bertrand type with the same args.  If
                        /// this causes a template error, then fall back to its default
                        /// specialization (i.e. list[Object]).
                        throw NotImplementedError(
                            "generic type subscription is not yet implemented"
                        );
                        return true;
                    }
                    return false;
                }
            },
            {
                "types.UnionType",
                [](Object hint, std::vector<PyObject*>& out) -> bool {
                    Object types = import_types();
                    if (isinstance(hint, getattr<"UnionType">(types))) {
                        Object args = reinterpret_steal<Object>(PyObject_CallOneArg(
                            ptr(getattr<"get_args">(types)),
                            ptr(hint)
                        ));
                        Py_ssize_t len = PyTuple_GET_SIZE(ptr(args));
                        for (Py_ssize_t i = 0; i < len; ++i) {
                            parse(reinterpret_borrow<Object>(
                                PyTuple_GET_ITEM(ptr(args), i)
                            ), out);
                        }
                        return true;
                    }
                    return false;
                }
            },
            {
                /// NOTE: when `typing.get_origin()` is called on a `typing.Optional`,
                /// it returns `typing.Union`, meaning that this handler will also
                /// implicitly cover `Optional` annotations for free.
                "typing.Union",
                [](Object hint, std::vector<PyObject*>& out) -> bool {
                    Object typing = import_typing();
                    Object origin = reinterpret_steal<Object>(PyObject_CallOneArg(
                        ptr(getattr<"get_origin">(typing)),
                        ptr(hint)
                    ));
                    if (origin.is(nullptr)) {
                        Exception::from_python();
                    } else if (origin.is(getattr<"Union">(typing))) {
                        Object args = reinterpret_steal<Object>(PyObject_CallOneArg(
                            ptr(getattr<"get_args">(typing)),
                            ptr(hint)
                        ));
                        Py_ssize_t len = PyTuple_GET_SIZE(ptr(args));
                        for (Py_ssize_t i = 0; i < len; ++i) {
                            parse(reinterpret_borrow<Object>(
                                PyTuple_GET_ITEM(ptr(args), i)
                            ), out);
                        }
                        return true;
                    }
                    return false;
                }
            },
            {
                "typing.Any",
                [](Object hint, std::vector<PyObject*>& out) -> bool {
                    Object typing = import_typing();
                    Object origin = reinterpret_steal<Object>(PyObject_CallOneArg(
                        ptr(getattr<"get_origin">(typing)),
                        ptr(hint)
                    ));
                    if (origin.is(nullptr)) {
                        Exception::from_python();
                    } else if (origin.is(getattr<"Any">(typing))) {
                        PyObject* type = reinterpret_cast<PyObject*>(&PyBaseObject_Type);
                        bool contains = false;
                        for (PyObject* t : out) {
                            if (t == type) {
                                contains = true;
                            }
                        }
                        out.push_back(type);
                        return true;
                    }
                    return false;
                }
            },
            {
                "typing.TypeAliasType",
                [](Object hint, std::vector<PyObject*>& out) -> bool {
                    Object typing = import_typing();
                    if (isinstance(hint, getattr<"TypeAliasType">(typing))) {
                        parse(getattr<"__value__">(hint), out);
                        return true;
                    }
                    return false;
                }
            },
            {
                "typing.Literal",
                [](Object hint, std::vector<PyObject*>& out) -> bool {
                    Object typing = import_typing();
                    Object origin = reinterpret_steal<Object>(PyObject_CallOneArg(
                        ptr(getattr<"get_origin">(typing)),
                        ptr(hint)
                    ));
                    if (origin.is(nullptr)) {
                        Exception::from_python();
                    } else if (origin.is(getattr<"Literal">(typing))) {
                        Object args = reinterpret_steal<Object>(PyObject_CallOneArg(
                            ptr(getattr<"get_args">(typing)),
                            ptr(hint)
                        ));
                        if (args.is(nullptr)) {
                            Exception::from_python();
                        }
                        Py_ssize_t len = PyTuple_GET_SIZE(ptr(args));
                        for (Py_ssize_t i = 0; i < len; ++i) {
                            PyObject* type = reinterpret_cast<PyObject*>(
                                Py_TYPE(PyTuple_GET_ITEM(ptr(args), i))
                            );
                            bool contains = false;
                            for (PyObject* t : out) {
                                if (t == type) {
                                    contains = true;
                                }
                            }
                            if (!contains) {
                                out.push_back(type);
                            }
                        }
                        return true;
                    }
                    return false;
                }
            },
            {
                "typing.LiteralString",
                [](Object hint, std::vector<PyObject*>& out) -> bool {
                    Object typing = import_typing();
                    if (hint.is(getattr<"LiteralString">(typing))) {
                        PyObject* type = reinterpret_cast<PyObject*>(&PyUnicode_Type);
                        bool contains = false;
                        for (PyObject* t : out) {
                            if (t == type) {
                                contains = true;
                            }
                        }
                        if (!contains) {
                            out.push_back(type);
                        }
                        return true;
                    }
                    return false;
                }
            },
            {
                "typing.AnyStr",
                [](Object hint, std::vector<PyObject*>& out) -> bool {
                    Object typing = import_typing();
                    if (hint.is(getattr<"AnyStr">(typing))) {
                        PyObject* unicode = reinterpret_cast<PyObject*>(&PyUnicode_Type);
                        PyObject* bytes = reinterpret_cast<PyObject*>(&PyBytes_Type);
                        bool contains_unicode = false;
                        bool contains_bytes = false;
                        for (PyObject* t : out) {
                            if (t == unicode) {
                                contains_unicode = true;
                            } else if (t == bytes) {
                                contains_bytes = true;
                            }
                        }
                        if (!contains_unicode) {
                            out.push_back(unicode);
                        }
                        if (!contains_bytes) {
                            out.push_back(bytes);
                        }
                        return true;
                    }
                    return false;
                }
            },
            {
                "typing.NoReturn",
                [](Object hint, std::vector<PyObject*>& out) -> bool {
                    Object typing = import_typing();
                    if (
                        hint.is(getattr<"NoReturn">(typing)) ||
                        hint.is(getattr<"Never">(typing))
                    ) {
                        /// NOTE: this handler models NoReturn/Never by not pushing a
                        /// type to the `out` vector, giving an empty return type.
                        return true;
                    }
                    return false;
                }
            },
            {
                "typing.TypeGuard",
                [](Object hint, std::vector<PyObject*>& out) -> bool {
                    Object typing = import_typing();
                    Object origin = reinterpret_steal<Object>(PyObject_CallOneArg(
                        ptr(getattr<"get_origin">(typing)),
                        ptr(hint)
                    ));
                    if (origin.is(nullptr)) {
                        Exception::from_python();
                    } else if (origin.is(getattr<"TypeGuard">(typing))) {
                        PyObject* type = reinterpret_cast<PyObject*>(&PyBool_Type);
                        bool contains = false;
                        for (PyObject* t : out) {
                            if (t == type) {
                                contains = true;
                            }
                        }
                        if (!contains) {
                            out.push_back(type);
                        }
                        return true;
                    }
                    return false;
                }
            }
        };

        /* Get the possible return types of the function, using the same callback
        handlers as the parameters.  Note that functions with `typing.NoReturn` or
        `typing.Never` annotations can return an empty vector. */
        std::vector<PyObject*> returns() const {
            Object return_annotation = getattr<"return_annotation">(signature);
            std::vector<PyObject*> keys;
            parse(return_annotation, keys);
            return keys;
        }

        auto begin() const {
            if (overload_keys.empty()) {
                get_overloads();
            }
            return overload_keys.cbegin();
        }
        auto cbegin() const { return begin(); }
        auto end() const { return overload_keys.cend(); }
        auto cend() const { return end(); }

    private:
        Object inspect = [] {
            PyObject* inspect = PyImport_Import(TemplateString<"inspect">::ptr);
            if (inspect == nullptr) {
                Exception::from_python();
            }
            return reinterpret_steal<Object>(inspect);
        }();
        Object typing = [] {
            PyObject* inspect = PyImport_Import(TemplateString<"typing">::ptr);
            if (inspect == nullptr) {
                Exception::from_python();
            }
            return reinterpret_steal<Object>(inspect);
        }();

        using Params = impl::Params<std::vector<Param>>;
        mutable std::vector<Params> overload_keys;

        /* Iterate over the Python signature and invoke the matching callbacks */
        void get_overloads() const {
            Object Parameter = getattr<"Parameter">(inspect);
            Object empty = getattr<"empty">(Parameter);
            Object POSITIONAL_ONLY = getattr<"POSITIONAL_ONLY">(Parameter);
            Object POSITIONAL_OR_KEYWORD = getattr<"POSITIONAL_OR_KEYWORD">(Parameter);
            Object VAR_POSITIONAL = getattr<"VAR_POSITIONAL">(Parameter);
            Object KEYWORD_ONLY = getattr<"KEYWORD_ONLY">(Parameter);
            Object VAR_KEYWORD = getattr<"VAR_KEYWORD">(Parameter);

            Object parameters = getattr<"parameters">(signature);
            overload_keys.push_back({std::vector<Param>{}, 0});
            overload_keys.back().value.reserve(len(parameters));
            for (Object param : parameters) {
                // determine the name and category of each `inspect.Parameter` object
                std::string_view name = Param::get_name(
                    ptr(getattr<"name">(param)
                ));
                ArgKind category;
                Object kind = getattr<"kind">(param);
                if (kind.is(POSITIONAL_ONLY)) {
                    category = getattr<"default">(param).is(empty) ?
                        ArgKind::POS :
                        ArgKind::POS | ArgKind::OPT;
                } else if (kind.is(POSITIONAL_OR_KEYWORD)) {
                    category = getattr<"default">(param).is(empty) ?
                        ArgKind::POS | ArgKind::KW :
                        ArgKind::POS | ArgKind::KW | ArgKind::OPT;
                } else if (kind.is(KEYWORD_ONLY)) {
                    category = getattr<"default">(param).is(empty) ?
                        ArgKind::KW :
                        ArgKind::KW | ArgKind::OPT;
                } else if (kind.is(VAR_POSITIONAL)) {
                    category = ArgKind::POS | ArgKind::VARIADIC;
                } else if (kind.is(VAR_KEYWORD)) {
                    category = ArgKind::KW | ArgKind::VARIADIC;
                } else {
                    throw TypeError("unrecognized parameter kind: " + repr(kind));
                }

                // parse the annotation for each `inspect.Parameter` object
                std::vector<PyObject*> types;
                parse(param, types);

                // if there is more than one type in the output vector, then the
                // existing keys must be duplicated to maintain uniqueness
                overload_keys.reserve(types.size() * overload_keys.size());
                for (size_t i = 1; i < types.size(); ++i) {
                    for (size_t j = 0; j < overload_keys.size(); ++j) {
                        auto& key = overload_keys[j];
                        overload_keys.push_back(key);
                        overload_keys.back().value.reserve(key.value.capacity());
                    }
                }

                // append the types to the overload keys and update their hashes such
                // that each gives a unique path through a function's overload trie
                for (size_t i = 0; i < types.size(); ++i) {
                    PyObject* type = types[i];
                    for (size_t j = 0; j < overload_keys.size(); ++j) {
                        Params& key = overload_keys[i * overload_keys.size() + j];
                        key.value.push_back({
                            name,
                            type,
                            category
                        });
                        key.hash = hash_combine(
                            key.hash,
                            fnv1a(name.data(), seed, prime),
                            reinterpret_cast<size_t>(type)
                        );
                    }
                }
            }
        }

    };

    template <typename T>
    static constexpr bool _is_arg = false;
    template <typename T>
    static constexpr bool _is_arg<OptionalArg<T>> = true;
    template <StaticStr Name, typename T>
    static constexpr bool _is_arg<PositionalArg<Name, T>> = true;
    template <StaticStr Name, typename T>
    static constexpr bool _is_arg<Arg<Name, T>> = true;
    template <StaticStr Name, typename T>
    static constexpr bool _is_arg<KeywordArg<Name, T>> = true;
    template <StaticStr Name, typename T>
    static constexpr bool _is_arg<VarArgs<Name, T>> = true;
    template <StaticStr Name, typename T>
    static constexpr bool _is_arg<VarKwargs<Name, T>> = true;
    template <typename T>
    static constexpr bool _is_arg<ArgPack<T>> = true;
    template <typename T>
    static constexpr bool _is_arg<KwargPack<T>> = true;
    template <typename T>
    concept is_arg = _is_arg<std::remove_cvref_t<T>>;

    /* Inspect a C++ argument at compile time.  Normalizes unannotated types to
    positional-only arguments to maintain C++ style. */
    template <typename T>
    struct ArgTraits {
        using type                                  = T;
        using no_opt                                = T;
        static constexpr StaticStr name             = "";
        static constexpr ArgKind kind               = ArgKind::POS;
        static constexpr bool posonly() noexcept    { return kind.posonly(); }
        static constexpr bool pos() noexcept        { return kind.pos(); }
        static constexpr bool args() noexcept       { return kind.args(); }
        static constexpr bool kwonly() noexcept     { return kind.kwonly(); }
        static constexpr bool kw() noexcept         { return kind.kw(); }
        static constexpr bool kwargs() noexcept     { return kind.kwargs(); }
        static constexpr bool opt() noexcept        { return kind.opt(); }
        static constexpr bool variadic() noexcept   { return kind.variadic(); }
    };

    /* Inspect a C++ argument at compile time.  Forwards to the annotated type's
    interface where possible. */
    template <is_arg T>
    struct ArgTraits<T> {
    private:

        template <typename U>
        struct _no_opt { using type = U; };
        template <typename U>
        struct _no_opt<OptionalArg<U>> { using type = U; };

    public:
        using type                                  = std::remove_cvref_t<T>::type;
        using no_opt                                = _no_opt<std::remove_cvref_t<T>>::type;
        static constexpr StaticStr name             = std::remove_cvref_t<T>::name;
        static constexpr ArgKind kind               = std::remove_cvref_t<T>::kind;
        static constexpr bool posonly() noexcept    { return kind.posonly(); }
        static constexpr bool pos() noexcept        { return kind.pos(); }
        static constexpr bool args() noexcept       { return kind.args(); }
        static constexpr bool kwonly() noexcept     { return kind.kwonly(); }
        static constexpr bool kw() noexcept         { return kind.kw(); }
        static constexpr bool kwargs() noexcept     { return kind.kwargs(); }
        static constexpr bool opt() noexcept        { return kind.opt(); }
        static constexpr bool variadic() noexcept   { return kind.variadic(); }
    };

    /* Inspect an annotated C++ parameter list at compile time and extract metadata
    that allows a corresponding function to be called with Python-style arguments from
    C++. */
    template <typename... Args>
    struct Arguments : BertrandTag {
    private:

        template <size_t I, typename... Ts>
        static consteval size_t _n_posonly() { return 0; }
        template <size_t I, typename T, typename... Ts>
        static consteval size_t _n_posonly() {
            return _n_posonly<I + ArgTraits<T>::posonly(), Ts...>();
        }

        template <size_t I, typename... Ts>
        static consteval size_t _n_pos() { return 0; }
        template <size_t I, typename T, typename... Ts>
        static consteval size_t _n_pos() {
            return _n_pos<I + ArgTraits<T>::pos(), Ts...>();
        }

        template <size_t I, typename... Ts>
        static consteval size_t _n_kw() { return 0; }
        template <size_t I, typename T, typename... Ts>
        static consteval size_t _n_kw() {
            return _n_kw<I + ArgTraits<T>::kw(), Ts...>();
        }

        template <size_t I, typename... Ts>
        static consteval size_t _n_kwonly() { return 0; }
        template <size_t I, typename T, typename... Ts>
        static consteval size_t _n_kwonly() {
            return _n_kwonly<I + ArgTraits<T>::kwonly(), Ts...>();
        }

        template <size_t I, typename... Ts>
        static consteval size_t _n_opt() { return 0; }
        template <size_t I, typename T, typename... Ts>
        static consteval size_t _n_opt() {
            return _n_opt<I + ArgTraits<T>::opt(), Ts...>();
        }

        template <size_t I, typename... Ts>
        static consteval size_t _n_opt_posonly() { return 0; }
        template <size_t I, typename T, typename... Ts>
        static consteval size_t _n_opt_posonly() {
            return _n_opt_posonly<
                I + (ArgTraits<T>::posonly() && ArgTraits<T>::opt()),
                Ts...
            >();
        }

        template <size_t I, typename... Ts>
        static consteval size_t _n_opt_pos() { return 0; }
        template <size_t I, typename T, typename... Ts>
        static consteval size_t _n_opt_pos() {
            return _n_opt_pos<
                I + (ArgTraits<T>::pos() && ArgTraits<T>::opt()),
                Ts...
            >();
        }

        template <size_t I, typename... Ts>
        static consteval size_t _n_opt_kw() { return 0; }
        template <size_t I, typename T, typename... Ts>
        static consteval size_t _n_opt_kw() {
            return _n_opt_kw<
                I + (ArgTraits<T>::kw() && ArgTraits<T>::opt()),
                Ts...
            >();
        }

        template <size_t I, typename... Ts>
        static consteval size_t _n_opt_kwonly() { return 0; }
        template <size_t I, typename T, typename... Ts>
        static consteval size_t _n_opt_kwonly() {
            return _n_opt_kwonly<
                I + (ArgTraits<T>::kwonly() && ArgTraits<T>::opt()),
                Ts...
            >();
        }

        template <StaticStr Name, typename... Ts>
        static consteval size_t _idx() { return 0; }
        template <StaticStr Name, typename T, typename... Ts>
        static consteval size_t _idx() {
            return (ArgTraits<T>::name == Name) ? 0 : 1 + _idx<Name, Ts...>();
        }

        template <typename... Ts>
        static consteval size_t _args_idx() { return 0; }
        template <typename T, typename... Ts>
        static consteval size_t _args_idx() {
            return ArgTraits<T>::args() ? 0 : 1 + _args_idx<Ts...>();
        }

        template <typename... Ts>
        static consteval size_t _kw_idx() { return 0; }
        template <typename T, typename... Ts>
        static consteval size_t _kw_idx() {
            return ArgTraits<T>::kw() ? 0 : 1 + _kw_idx<Ts...>();
        }

        template <typename... Ts>
        static consteval size_t _kwonly_idx() { return 0; }
        template <typename T, typename... Ts>
        static consteval size_t _kwonly_idx() {
            return ArgTraits<T>::kwonly() ? 0 : 1 + _kwonly_idx<Ts...>();
        }

        template <typename... Ts>
        static consteval size_t _kwargs_idx() { return 0; }
        template <typename T, typename... Ts>
        static consteval size_t _kwargs_idx() {
            return ArgTraits<T>::kwargs() ? 0 : 1 + _kwargs_idx<Ts...>();
        }

        template <typename... Ts>
        static consteval size_t _opt_idx() { return 0; }
        template <typename T, typename... Ts>
        static consteval size_t _opt_idx() {
            return ArgTraits<T>::opt() ? 0 : 1 + _opt_idx<Ts...>();
        }

        template <typename... Ts>
        static consteval size_t _opt_posonly_idx() { return 0; }
        template <typename T, typename... Ts>
        static consteval size_t _opt_posonly_idx() {
            return ArgTraits<T>::posonly() && ArgTraits<T>::opt() ?
                0 : 1 + _opt_posonly_idx<Ts...>();
        }

        template <typename... Ts>
        static consteval size_t _opt_pos_idx() { return 0; }
        template <typename T, typename... Ts>
        static consteval size_t _opt_pos_idx() {
            return ArgTraits<T>::pos() && ArgTraits<T>::opt() ?
                0 : 1 + _opt_pos_idx<Ts...>();
        }

        template <typename... Ts>
        static consteval size_t _opt_kw_idx() { return 0; }
        template <typename T, typename... Ts>
        static consteval size_t _opt_kw_idx() {
            return ArgTraits<T>::kw() && ArgTraits<T>::opt() ?
                0 : 1 + _opt_kw_idx<Ts...>();
        }

        template <typename... Ts>
        static consteval size_t _opt_kwonly_idx() { return 0; }
        template <typename T, typename... Ts>
        static consteval size_t _opt_kwonly_idx() {
            return ArgTraits<T>::kwonly() && ArgTraits<T>::opt() ?
                0 : 1 + _opt_kwonly_idx<Ts...>();
        }

        template <size_t I, typename... Ts>
        static consteval bool _proper_argument_order() { return true; }
        template <size_t I, typename T, typename... Ts>
        static consteval bool _proper_argument_order() {
            if constexpr (
                (ArgTraits<T>::posonly() && (I > kw_idx || I > args_idx || I > kwargs_idx)) ||
                (ArgTraits<T>::pos() && (I > args_idx || I > kwonly_idx || I > kwargs_idx)) ||
                (ArgTraits<T>::args() && (I > kwonly_idx || I > kwargs_idx)) ||
                (ArgTraits<T>::kwonly() && (I > kwargs_idx))
            ) {
                return false;
            }
            return _proper_argument_order<I + 1, Ts...>();
        }

        template <size_t I, typename... Ts>
        static consteval bool _no_duplicate_arguments() { return true; }
        template <size_t I, typename T, typename... Ts>
        static consteval bool _no_duplicate_arguments() {
            if constexpr (
                (T::name != "" && I != idx<ArgTraits<T>::name>) ||
                (ArgTraits<T>::args() && I != args_idx) ||
                (ArgTraits<T>::kwargs() && I != kwargs_idx)
            ) {
                return false;
            }
            return _no_duplicate_arguments<I + 1, Ts...>();
        }

        template <size_t I, typename... Ts>
        static consteval bool _no_required_after_default() { return true; }
        template <size_t I, typename T, typename... Ts>
        static consteval bool _no_required_after_default() {
            if constexpr (ArgTraits<T>::pos() && !ArgTraits<T>::opt() && I > opt_idx) {
                return false;
            } else {
                return _no_required_after_default<I + 1, Ts...>();
            }
        }

        template <size_t I, typename... Ts>
        static constexpr bool _compatible() {
            return
                I == n ||
                (I == args_idx && args_idx == n - 1) ||
                (I == kwargs_idx && kwargs_idx == n - 1);
        };
        template <size_t I, typename T, typename... Ts>
        static constexpr bool _compatible() {
            if constexpr (ArgTraits<at<I>>::posonly()) {
                return
                    ArgTraits<T>::posonly() &&
                    !(ArgTraits<at<I>>::opt() && !ArgTraits<T>::opt()) &&
                    (ArgTraits<at<I>>::name == ArgTraits<T>::name) &&
                    issubclass<
                        typename ArgTraits<T>::type,
                        typename ArgTraits<at<I>>::type
                    >() &&
                    _compatible<I + 1, Ts...>();

            } else if constexpr (ArgTraits<at<I>>::pos()) {
                return
                    (ArgTraits<T>::pos() && ArgTraits<T>::kw()) &&
                    !(ArgTraits<at<I>>::opt() && !ArgTraits<T>::opt()) &&
                    (ArgTraits<at<I>>::name == ArgTraits<T>::name) &&
                    issubclass<
                        typename ArgTraits<T>::type,
                        typename ArgTraits<at<I>>::type
                    >() &&
                    _compatible<I + 1, Ts...>();

            } else if constexpr (ArgTraits<at<I>>::kw()) {
                return
                    (ArgTraits<T>::kw() && ArgTraits<T>::pos()) &&
                    !(ArgTraits<at<I>>::opt() && !ArgTraits<T>::opt()) &&
                    (ArgTraits<at<I>>::name == ArgTraits<T>::name) &&
                    issubclass<
                        typename ArgTraits<T>::type,
                        typename ArgTraits<at<I>>::type
                    >() &&
                    _compatible<I + 1, Ts...>();

            } else if constexpr (ArgTraits<at<I>>::args()) {
                if constexpr (ArgTraits<T>::pos() || ArgTraits<T>::args()) {
                    if constexpr (!issubclass<
                        typename ArgTraits<T>::type,
                        typename ArgTraits<at<I>>::type
                    >()) {
                        return false;
                    }
                    return _compatible<I, Ts...>();
                }
                return _compatible<I + 1, Ts...>();

            } else if constexpr (ArgTraits<at<I>>::kwargs()) {
                if constexpr (ArgTraits<T>::kw() || ArgTraits<T>::kwargs()) {
                    if constexpr (!issubclass<
                        typename ArgTraits<T>::type,
                        typename ArgTraits<at<I>>::type
                    >()) {
                        return false;
                    }
                    return _compatible<I, Ts...>();
                }
                return _compatible<I + 1, Ts...>();

            } else {
                static_assert(false, "unrecognized parameter type");
                return false;
            }
        }

        template <size_t I>
        static consteval uint64_t _required() {
            if constexpr (
                ArgTraits<unpack_type<I, Args...>>::opt() ||
                ArgTraits<unpack_type<I, Args...>>::variadic()
            ) {
                return 0ULL;
            } else {
                return 1ULL << I;
            }
        }

    public:
        static constexpr size_t n                   = sizeof...(Args);
        static constexpr size_t n_posonly           = _n_posonly<0, Args...>();
        static constexpr size_t n_pos               = _n_pos<0, Args...>();
        static constexpr size_t n_kw                = _n_kw<0, Args...>();
        static constexpr size_t n_kwonly            = _n_kwonly<0, Args...>();
        static constexpr size_t n_opt               = _n_opt<0, Args...>();
        static constexpr size_t n_opt_posonly       = _n_opt_posonly<0, Args...>();
        static constexpr size_t n_opt_pos           = _n_opt_pos<0, Args...>();
        static constexpr size_t n_opt_kw            = _n_opt_kw<0, Args...>();
        static constexpr size_t n_opt_kwonly        = _n_opt_kwonly<0, Args...>();

        template <StaticStr Name>
        static constexpr bool has                   = _idx<Name, Args...>() != n;
        static constexpr bool has_posonly           = n_posonly > 0;
        static constexpr bool has_pos               = n_pos > 0;
        static constexpr bool has_kw                = n_kw > 0;
        static constexpr bool has_kwonly            = n_kwonly > 0;
        static constexpr bool has_opt               = n_opt > 0;
        static constexpr bool has_opt_posonly       = n_opt_posonly > 0;
        static constexpr bool has_opt_pos           = n_opt_pos > 0;
        static constexpr bool has_opt_kw            = n_opt_kw > 0;
        static constexpr bool has_opt_kwonly        = n_opt_kwonly > 0;
        static constexpr bool has_args              = _args_idx<Args...>() != n;
        static constexpr bool has_kwargs            = _kwargs_idx<Args...>() != n;

        template <StaticStr Name> requires (has<Name>)
        static constexpr size_t idx                 = _idx<Name, Args...>();
        static constexpr size_t kw_idx              = _kw_idx<Args...>();
        static constexpr size_t kwonly_idx          = _kwonly_idx<Args...>();
        static constexpr size_t opt_idx             = _opt_idx<Args...>();
        static constexpr size_t opt_posonly_idx     = _opt_posonly_idx<Args...>();
        static constexpr size_t opt_pos_idx         = _opt_pos_idx<Args...>();
        static constexpr size_t opt_kw_idx          = _opt_kw_idx<Args...>();
        static constexpr size_t opt_kwonly_idx      = _opt_kwonly_idx<Args...>();
        static constexpr size_t args_idx            = _args_idx<Args...>();
        static constexpr size_t kwargs_idx          = _kwargs_idx<Args...>();

        static constexpr bool args_are_convertible_to_python =
            (std::convertible_to<typename ArgTraits<Args>::type, Object> && ...);
        static constexpr bool proper_argument_order =
            _proper_argument_order<0, Args...>();
        static constexpr bool no_duplicate_arguments =
            _no_duplicate_arguments<0, Args...>();
        static constexpr bool no_required_after_default =
            _no_required_after_default<0, Args...>();

        /* A template constraint that evaluates true if another signature represents a
        viable overload of a function with this signature. */
        template <typename... Ts>
        static constexpr bool compatible = _compatible<0, Ts...>(); 

        template <size_t I> requires (I < n)
        using at = unpack_type<I, Args...>;

        /* A single entry in a callback table, storing the argument name, a one-hot
        encoded bitmask specifying this argument's position, a function that can be
        used to validate the argument, and a lazy function that can be used to retrieve
        its corresponding Python type. */
        struct Callback {
            std::string_view name;
            uint64_t mask = 0;
            bool(*func)(PyObject*) = nullptr;
            PyObject*(*type)() = nullptr;
            explicit operator bool() const noexcept { return func != nullptr; }
            bool operator()(PyObject* type) const { return func(type); }
        };

        /* A bitmask with a 1 in the position of all of the required arguments in the
        parameter list.
        
        Each callback stores a one-hot encoded mask that is joined into a single
        bitmask as each argument is processed.  The resulting mask can then be compared
        to this constant to determine if all required arguments have been provided.  If
        that comparison evaluates to false, then further bitwise inspection can be done
        to determine exactly which arguments were missing, as well as their names.

        Note that this mask effectively limits the number of arguments that a function
        can accept to 64, which is a reasonable limit for most functions.  The
        performance benefits justify the limitation, and if you need more than 64
        arguments, you should probably be using a different design pattern. */
        static constexpr uint64_t required = [] {
            return []<size_t... Is>(std::index_sequence<Is...>) {
                return (_required<Is>() | ...);
            }(std::make_index_sequence<n>{});
        }();

    private:
        static constexpr Callback null_callback;

        /* Populate the positional argument table with an appropriate callback for
        each argument in the parameter list. */
        template <size_t I>
        static consteval void populate_positional_table(std::array<Callback, n>& table) {
            table[I] = {
                ArgTraits<at<I>>::name,
                ArgTraits<at<I>>::variadic() ? 0ULL : 1ULL << I,
                [](PyObject* type) -> bool {
                    using T = typename ArgTraits<at<I>>::type;
                    if constexpr (has_type<T>) {
                        int rc = PyObject_IsSubclass(
                            type,
                            ptr(Type<T>())
                        );
                        if (rc < 0) {
                            Exception::from_python();
                        }
                        return rc;
                    } else {
                        throw TypeError(
                            "C++ type has no Python equivalent: " +
                            demangle(typeid(T).name())
                        );
                    }
                },
                []() -> PyObject* {
                    using T = typename ArgTraits<at<I>>::type;
                    if constexpr (has_type<T>) {
                        return ptr(Type<T>());
                    } else {
                        throw TypeError(
                            "C++ type has no Python equivalent: " +
                            demangle(typeid(T).name())
                        );
                    }
                }
            };
        }

        /* An array of positional arguments to callbacks and bitmasks that can be used
        to validate the argument dynamically at runtime. */
        static constexpr std::array<Callback, n> positional_table = [] {
            std::array<Callback, n> table;
            []<size_t... Is>(std::index_sequence<Is...>, std::array<Callback, n>& table) {
                (populate_positional_table<Is>(table), ...);
            }(std::make_index_sequence<n>{}, table);
            return table;
        }();

        /* Get the size of the perfect hash table required to efficiently validate
        keyword arguments as a power of 2. */
        static consteval size_t keyword_table_size() {
            return next_power_of_two(2 * n_kw);
        }

        /* Take the modulus with respect to the keyword table by exploiting the power
        of 2 table size. */
        static constexpr size_t keyword_table_index(size_t hash) {
            constexpr size_t mask = keyword_table_size() - 1;
            return hash & mask;
        }

        /* Given a precomputed keyword index into the hash table, check to see if any
        subsequent arguments in the parameter list hash to the same index. */
        template <size_t I>
        static consteval bool collides_recursive(size_t idx, size_t seed, size_t prime) {
            if constexpr (I < sizeof...(Args)) {
                if constexpr (ArgTraits<at<I>>::kw) {
                    size_t hash = fnv1a(
                        ArgTraits<at<I>>::name,
                        seed,
                        prime
                    );
                    return
                        (keyword_table_index(hash) == idx) ||
                        collides_recursive<I + 1>(idx, seed, prime);
                } else {
                    return collides_recursive<I + 1>(idx, seed, prime);
                }
            } else {
                return false;
            }
        };

        /* Check to see if the candidate seed and prime produce any collisions for the
        target argument at index I. */
        template <size_t I>
        static consteval bool collides(size_t seed, size_t prime) {
            if constexpr (I < sizeof...(Args)) {
                if constexpr (ArgTraits<at<I>>::kw) {
                    size_t hash = fnv1a(
                        ArgTraits<at<I>>::name,
                        seed,
                        prime
                    );
                    return collides_recursive<I + 1>(
                        keyword_table_index(hash),
                        seed,
                        prime
                    ) || collides<I + 1>(seed, prime);
                } else {
                    return collides<I + 1>(seed, prime);
                }
            } else {
                return false;
            }
        }

        /* Find an FNV-1a seed and prime that produces perfect hashes with respect to
        the keyword table size. */
        static consteval std::pair<size_t, size_t> hash_components() {
            constexpr size_t recursion_limit = fnv1a_hash_seed + 100'000;
            size_t seed = fnv1a_hash_seed;
            size_t prime = fnv1a_hash_prime;
            size_t i = 0;
            while (collides<0>(seed, prime)) {
                if (++seed > recursion_limit) {
                    if (++i == 10) {
                        std::cerr << "error: unable to find a perfect hash seed "
                                  << "after 10^6 iterations.  Consider increasing the "
                                  << "recursion limit or reviewing the keyword "
                                  << "argument names for potential issues.\n";
                        std::exit(1);
                    }
                    seed = fnv1a_hash_seed;
                    prime = fnv1a_fallback_primes[i];
                }
            }
            return {seed, prime};
        }

        /* Populate the keyword table with an appropriate callback for each keyword
        argument in the parameter list. */
        template <size_t I>
        static consteval void populate_keyword_table(
            std::array<Callback, keyword_table_size()>& table,
            size_t seed,
            size_t prime
        ) {
            if constexpr (ArgTraits<at<I>>::kw()) {
                constexpr size_t i = keyword_table_index(
                    hash(ArgTraits<at<I>>::name)
                );
                table[i] = {
                    ArgTraits<at<I>>::name,
                    ArgTraits<at<I>>::variadic() ? 0ULL : 1ULL << I,
                    [](PyObject* type) -> bool {
                        using T = typename ArgTraits<at<I>>::type;
                        if constexpr (has_type<T>) {
                            int rc = PyObject_IsSubclass(
                                type,
                                ptr(Type<T>())
                            );
                            if (rc < 0) {
                                Exception::from_python();
                            }
                            return rc;
                        } else {
                            throw TypeError(
                                "C++ type has no Python equivalent: " +
                                demangle(typeid(T).name())
                            );
                        }
                    },
                    []() -> PyObject* {
                        using T = typename ArgTraits<at<I>>::type;
                        if constexpr (has_type<T>) {
                            return ptr(Type<T>());
                        } else {
                            throw TypeError(
                                "C++ type has no Python equivalent: " +
                                demangle(typeid(T).name())
                            );
                        }
                    }
                };
            }
        }

        /* The keyword table itself.  Each entry holds the expected keyword name for
        validation as well as a callback that can be used to validate its type
        dynamically at runtime. */
        static constexpr std::array<Callback, keyword_table_size()> keyword_table = [] {
            std::array<Callback, keyword_table_size()> table;
            []<size_t... Is>(
                std::index_sequence<Is...>,
                std::array<Callback, keyword_table_size()>& table,
                size_t seed,
                size_t prime
            ) {
                (populate_keyword_table<Is>(table, seed, prime), ...);
            }(
                std::make_index_sequence<n>{},
                table,
                hash_components().first,
                hash_components().second
            );
            return table;
        }();

        template <size_t I>
        static Param _parameters(size_t& hash) {
            constexpr Callback& callback = positional_table[I];
            PyObject* type = callback.type();
            hash = hash_combine(
                hash,
                fnv1a(
                    ArgTraits<at<I>>::name,
                    hash_components().first,
                    hash_components().second
                ),
                reinterpret_cast<size_t>(type)
            );
            return {ArgTraits<at<I>>::name, type, ArgTraits<at<I>>::kind};
        }

        /* After invoking a function with variadic positional arguments, the argument
        iterators must be fully consumed, otherwise there are additional positional
        arguments that were not consumed. */
        template <std::input_iterator Iter, std::sentinel_for<Iter> End>
        static void assert_args_are_exhausted(Iter& iter, const End& end) {
            if (iter != end) {
                std::string message =
                    "too many arguments in positional parameter pack: ['" +
                    repr(*iter);
                while (++iter != end) {
                    message += "', '" + repr(*iter);
                }
                message += "']";
                throw TypeError(message);
            }
        }

        /* Before invoking a function with variadic keyword arguments, those arguments
        must be scanned to ensure each of them are recognized and do not interfere with
        other keyword arguments given in the source signature. */
        template <typename Outer, typename Inner, typename Map>
        static void assert_kwargs_are_recognized(const Map& kwargs) {
            []<size_t... Is>(std::index_sequence<Is...>, const Map& kwargs) {
                std::vector<std::string> extra;
                for (const auto& [key, value] : kwargs) {
                    if (!callback(key)) {
                        extra.push_back(key);
                    }
                }
                if (!extra.empty()) {
                    auto iter = extra.begin();
                    auto end = extra.end();
                    std::string message =
                        "unexpected keyword arguments: ['" + repr(*iter);
                    while (++iter != end) {
                        message += "', '" + repr(*iter);
                    }
                    message += "']";
                    throw TypeError(message);
                }
            }(std::make_index_sequence<Outer::n>{}, kwargs);
        }

    public:

        /* A seed for an FNV-1a hash algorithm that was found to perfectly hash the
        keyword argument names from the enclosing parameter list. */
        static constexpr size_t seed = hash_components().first;

        /* A prime for an FNV-1a hash algorithm that was found to perfectly hash the
        keyword argument names from the enclosing parameter list. */
        static constexpr size_t prime = hash_components().second;

        /* Hash a byte string according to the FNV-1a algorithm using the seed and
        prime that were found at compile time to perfectly hash the keyword
        arguments. */
        static constexpr size_t hash(const char* str) noexcept {
            return fnv1a(str, seed, prime);
        }
        static constexpr size_t hash(std::string_view str) noexcept {
            return fnv1a(str.data(), seed, prime);
        }
        static constexpr size_t hash(const std::string& str) noexcept {
            return fnv1a(str.data(), seed, prime);
        }

        /* Return a new function object that captures a Python function and provides a
        forwarding C++ interface. */
        template <typename Return, typename R>
        static std::function<Return(Args...)> capture_self(const Object& self) {
            return [self](Args... args) -> Return {
                PyObject* result = Bind<Args...>{}(
                    ptr(self),
                    std::forward<Args>(args)...
                );
                if constexpr (std::is_void_v<Return>) {
                    Py_DECREF(result);
                } else {
                    return reinterpret_steal<R>(result);
                }
            };
        }

        /* Look up a positional argument, returning a callback object that can be used
        to efficiently validate it.  If the index does not correspond to a recognized
        positional argument, a null callback will be returned that evaluates to false
        under boolean logic.  If the parameter list accepts variadic positional
        arguments, then the variadic argument's callback will be returned instead. */
        static constexpr Callback& callback(size_t i) noexcept {
            if constexpr (has_args) {
                constexpr Callback& args_callback = positional_table[args_idx];
                return i < args_idx ? positional_table[i] : args_callback;
            } else if constexpr (has_kwonly) {
                return i < kwonly_idx ? positional_table[i] : null_callback;
            } else {
                return i < kwargs_idx ? positional_table[i] : null_callback;
            }
        }

        /* Look up a keyword argument, returning a callback object that can be used to
        efficiently validate it.  If the argument name is not recognized, a null
        callback will be returned that evaluates to false under boolean logic.  If the
        parameter list accepts variadic keyword arguments, then the variadic argument's
        callback will be returned instead. */
        static constexpr Callback& callback(std::string_view name) noexcept {
            const Callback& callback = keyword_table[
                keyword_table_index(hash(name.data()))
            ];
            if (callback.name == name) {
                return callback;
            } else {
                if constexpr (has_kwargs) {
                    constexpr Callback& kwargs_callback = keyword_table[kwargs_idx];
                    return kwargs_callback;
                } else {
                    return null_callback;
                }
            }
        }

        /* Produce an overload key that matches the enclosing parameter list. */
        static Params<std::array<Param, n>> parameters() {
            size_t hash = 0;
            return {
                []<size_t... Is>(std::index_sequence<Is...>, size_t& hash) {
                    return std::array<Param, n>{_parameters<Is>(hash)...};
                }(std::make_index_sequence<n>{}, hash),
                hash
            };
        }

        /* A tuple holding the default value for every argument in the enclosing
        parameter list that is marked as optional. */
        struct Defaults {
        private:
            using Outer = Arguments;

            /* The type of a single value in the defaults tuple.  The templated index
            is used to correlate the default value with its corresponding argument in
            the enclosing signature. */
            template <size_t I>
            struct Value  {
                using type = ArgTraits<Outer::at<I>>::type;
                static constexpr StaticStr name = ArgTraits<Outer::at<I>>::name;
                static constexpr size_t index = I;
                std::remove_reference_t<type> value;

                type get(this auto& self) {
                    if constexpr (std::is_rvalue_reference_v<type>) {
                        return std::remove_cvref_t<type>(self.value);
                    } else {
                        return self.value;
                    }
                }
            };

            /* Build a sub-signature holding only the arguments marked as optional from
            the enclosing signature.  This will be a specialization of the enclosing
            class, which is used to bind arguments to this class's constructor using
            the same semantics as the function's call operator. */
            template <typename Sig, typename... Ts>
            struct _Inner { using type = Sig; };
            template <typename... Sig, typename T, typename... Ts>
            struct _Inner<Arguments<Sig...>, T, Ts...> {
                template <typename U>
                struct sub_signature {
                    using type = _Inner<Arguments<Sig...>, Ts...>::type;
                };
                template <typename U> requires (ArgTraits<U>::opt())
                struct sub_signature<U> {
                    using type =_Inner<
                        Arguments<Sig..., typename ArgTraits<U>::no_opt>,
                        Ts...
                    >::type;
                };
                using type = sub_signature<T>::type;
            };
            using Inner = _Inner<Arguments<>, Args...>::type;

            /* Build a std::tuple of Value<I> instances to hold the default values
            themselves. */
            template <size_t I, typename Tuple, typename... Ts>
            struct _Tuple { using type = Tuple; };
            template <size_t I, typename... Part, typename T, typename... Ts>
            struct _Tuple<I, std::tuple<Part...>, T, Ts...> {
                template <typename U>
                struct tuple {
                    using type = _Tuple<I + 1, std::tuple<Part...>, Ts...>::type;
                };
                template <typename U> requires (ArgTraits<U>::opt())
                struct tuple<U> {
                    using type = _Tuple<I + 1, std::tuple<Part..., Value<I>>, Ts...>::type;
                };
                using type = tuple<T>::type;
            };
            using Tuple = _Tuple<0, std::tuple<>, Args...>::type;

            template <size_t I, typename T>
            static constexpr size_t _find = 0;
            template <size_t I, typename T, typename... Ts>
            static constexpr size_t _find<I, std::tuple<T, Ts...>> =
                (I == T::index) ? 0 : 1 + _find<I, std::tuple<Ts...>>;

        public:
            static constexpr size_t n               = Inner::n;
            static constexpr size_t n_posonly       = Inner::n_posonly;
            static constexpr size_t n_pos           = Inner::n_pos;
            static constexpr size_t n_kw            = Inner::n_kw;
            static constexpr size_t n_kwonly        = Inner::n_kwonly;

            template <StaticStr Name>
            static constexpr bool has               = Inner::template has<Name>;
            static constexpr bool has_posonly       = Inner::has_posonly;
            static constexpr bool has_pos           = Inner::has_pos;
            static constexpr bool has_kw            = Inner::has_kw;
            static constexpr bool has_kwonly        = Inner::has_kwonly;

            template <StaticStr Name> requires (has<Name>)
            static constexpr size_t idx             = Inner::template idx<Name>;
            static constexpr size_t kw_idx          = Inner::kw_idx;
            static constexpr size_t kwonly_idx      = Inner::kwonly_idx;

            template <size_t I> requires (I < n)
            using at = Inner::template at<I>;

            /* Bind an argument list to the default values tuple using the
            sub-signature's normal Bind<> machinery. */
            template <typename... Values>
            using Bind = Inner::template Bind<Values...>;

            /* Given an index into the enclosing signature, find the corresponding index
            in the defaults tuple if that index corresponds to a default value. */
            template <size_t I> requires (ArgTraits<typename Outer::at<I>>::opt())
            static constexpr size_t find = _find<I, Tuple>;

        private:

            template <size_t I, typename... Values>
            static constexpr decltype(auto) unpack(Values&&... values) {
                using observed = Arguments<Values...>;
                using T = Inner::template at<I>;
                constexpr StaticStr name = ArgTraits<T>::name;

                if constexpr (ArgTraits<T>::kwonly()) {
                    constexpr size_t idx = observed::template idx<name>;
                    return impl::unpack_arg<idx>(std::forward<Values>(values)...).value;

                } else if constexpr (ArgTraits<T>::kw()) {
                    if constexpr (I < observed::kw_idx) {
                        return impl::unpack_arg<I>(std::forward<Values>(values)...);
                    } else {
                        constexpr size_t idx = observed::template idx<name>;
                        return impl::unpack_arg<idx>(std::forward<Values>(values)...).value;
                    }

                } else {
                    return impl::unpack_arg<I>(std::forward<Values>(values)...);
                }
            }

            template <
                size_t I,
                typename... Values,
                std::input_iterator Iter,
                std::sentinel_for<Iter> End
            >
            static constexpr decltype(auto) unpack(
                size_t args_size,
                Iter& iter,
                const End& end,
                Values&&... values
            ) {
                using observed = Arguments<Values...>;
                using T = Inner::template at<I>;
                constexpr StaticStr name = ArgTraits<T>::name;

                if constexpr (ArgTraits<T>::kwonly()) {
                    constexpr size_t idx = observed::template idx<name>;
                    return impl::unpack_arg<idx>(std::forward<Values>(values)...).value;

                } else if constexpr (ArgTraits<T>::kw()) {
                    if constexpr (I < observed::kw_idx) {
                        return impl::unpack_arg<I>(std::forward<Values>(values)...);
                    } else {
                        if (iter != end) {
                            if constexpr (observed::template has<name>) {
                                throw TypeError(
                                    "conflicting values for parameter '" + name +
                                    "' at index " + std::to_string(I)
                                );
                            } else {
                                decltype(auto) result = *iter;
                                ++iter;
                                return result;
                            }

                        } else {
                            if constexpr (observed::template has<name>) {
                                constexpr size_t idx = observed::template idx<name>;
                                return impl::unpack_arg<idx>(
                                    std::forward<Values>(values)...
                                ).value;
                            } else {
                                throw TypeError(
                                    "no match for parameter '" + name +
                                    "' at index " + std::to_string(I)
                                );
                            }
                        }
                    }

                } else {
                    if constexpr (I < observed::kw_idx) {
                        return impl::unpack_arg<I>(std::forward<Values>(values)...);
                    } else {
                        if (iter != end) {
                            decltype(auto) result = *iter;
                            ++iter;
                            return result;
                        } else {
                            throw TypeError(
                                "no match for positional-only parmater at index " +
                                std::to_string(I)
                            );
                        }
                    }
                }
            }

            template <
                size_t I,
                typename... Values,
                typename Map
            >
            static constexpr decltype(auto) unpack(
                const Map& map,
                Values&&... values
            ) {
                using observed = Arguments<Values...>;
                using T = Inner::template at<I>;
                constexpr StaticStr name = ArgTraits<T>::name;

                if constexpr (ArgTraits<T>::kwonly()) {
                    auto item = map.find(name);
                    if constexpr (observed::template has<name>) {
                        if (item != map.end()) {
                            throw TypeError(
                                "conflicting values for parameter '" + name +
                                "' at index " + std::to_string(I)
                            );
                        }
                        constexpr size_t idx = observed::template idx<name>;
                        return impl::unpack_arg<idx>(
                            std::forward<Values>(values)...
                        ).value;
                    } else {
                        if (item != map.end()) {
                            return item->second;
                        } else {
                            throw TypeError(
                                "no match for parameter '" + name +
                                "' at index " + std::to_string(I)
                            );
                        }
                    }

                } else if constexpr (ArgTraits<T>::kw()) {
                    auto item = map.find(name);
                    if constexpr (I < observed::kw_idx) {
                        if (item != map.end()) {
                            throw TypeError(
                                "conflicting values for parameter '" + name +
                                "' at index " + std::to_string(I)
                            );
                        }
                        return impl::unpack_arg<I>(std::forward<Values>(values)...);
                    } else if constexpr (observed::template has<name>) {
                        if (item != map.end()) {
                            throw TypeError(
                                "conflicting values for parameter '" + name +
                                "' at index " + std::to_string(I)
                            );
                        }
                        constexpr size_t idx = observed::template idx<name>;
                        return impl::unpack_arg<idx>(
                            std::forward<Values>(values)...
                        ).value;
                    } else {
                        if (item != map.end()) {
                            return item->second;
                        } else {
                            throw TypeError(
                                "no match for parameter '" + name +
                                "' at index " + std::to_string(I)
                            );
                        }
                    }

                } else {
                    return impl::unpack_arg<I>(std::forward<Values>(values)...);
                }
            }

            template <
                size_t I,
                typename... Values,
                std::input_iterator Iter,
                std::sentinel_for<Iter> End,
                typename Map
            >
            static constexpr decltype(auto) unpack(
                size_t args_size,
                Iter& iter,
                const End& end,
                const Map& map,
                Values&&... values
            ) {
                using observed = Arguments<Values...>;
                using T = Inner::template at<I>;
                constexpr StaticStr name = ArgTraits<T>::name;

                if constexpr (ArgTraits<T>::kwonly()) {
                    return unpack<I>(
                        map,
                        std::forward<Values>(values)...
                    );

                } else if constexpr (ArgTraits<T>::kw()) {
                    auto item = map.find(name);
                    if constexpr (I < observed::kw_idx) {
                        if (item != map.end()) {
                            throw TypeError(
                                "conflicting values for parameter '" + name +
                                "' at index " + std::to_string(I)
                            );
                        }
                        return impl::unpack_arg<I>(std::forward<Values>(values)...);
                    } else {
                        if (iter != end) {
                            if constexpr (observed::template has<name>) {
                                throw TypeError(
                                    "conflicting values for parameter '" + name +
                                    "' at index " + std::to_string(I)
                                );
                            } else {
                                if (item != map.end()) {
                                    throw TypeError(
                                        "conflicting values for parameter '" + name +
                                        "' at index " + std::to_string(I)
                                    );
                                } else {
                                    decltype(auto) result = *iter;
                                    ++iter;
                                    return result;
                                }
                            }
                        } else {
                            if constexpr (observed::template has<name>) {
                                if (item != map.end()) {
                                    throw TypeError(
                                        "conflicting values for parameter '" + name +
                                        "' at index " + std::to_string(I)
                                    );
                                } else {
                                    constexpr size_t idx = observed::template idx<name>;
                                    return impl::unpack_arg<idx>(
                                        std::forward<Values>(values)...
                                    ).value;
                                }
                            } else {
                                if (item != map.end()) {
                                    return item->second;
                                } else {
                                    throw TypeError(
                                        "no match for parameter '" + name +
                                        "' at index " + std::to_string(I)
                                    );
                                }
                            }
                        }
                    }

                } else {
                    return unpack<I>(
                        args_size,
                        iter,
                        end,
                        std::forward<Values>(values)...
                    );
                }
            }

            template <size_t... Is, typename... Values>
            static constexpr Tuple build(
                std::index_sequence<Is...>,
                Values&&... values
            ) {
                using observed = Arguments<Values...>;

                if constexpr (observed::has_args && observed::has_kwargs) {
                    const auto& kwargs = impl::unpack_arg<observed::kwargs_idx>(
                        std::forward<Values>(values)...
                    );
                    assert_kwargs_are_recognized<Inner, observed>(kwargs);
                    const auto& args = impl::unpack_arg<observed::args_idx>(
                        std::forward<Values>(values)...
                    );
                    auto iter = std::ranges::begin(args);
                    auto end = std::ranges::end(args);
                    Tuple result = {{
                        unpack<Is>(
                            std::ranges::size(args),
                            iter,
                            end,
                            kwargs,
                            std::forward<Values>(values)...
                        )
                    }...};
                    assert_args_are_exhausted(iter, end);
                    return result;

                } else if constexpr (observed::has_args) {
                    const auto& args = impl::unpack_arg<observed::args_idx>(
                        std::forward<Values>(values)...
                    );
                    auto iter = std::ranges::begin(args);
                    auto end = std::ranges::end(args);
                    Tuple result = {{
                        unpack<Is>(
                            std::ranges::size(args),
                            iter,
                            end,
                            std::forward<Values>(values)...
                        )
                    }...};
                    assert_args_are_exhausted(iter, end);
                    return result;

                } else if constexpr (observed::has_kwargs) {
                    const auto& kwargs = impl::unpack_arg<observed::kwargs_idx>(
                        std::forward<Values>(values)...
                    );
                    assert_kwargs_are_recognized<Inner, observed>(kwargs);
                    return {{unpack<Is>(kwargs, std::forward<Values>(values)...)}...};

                } else {
                    return {{unpack<Is>(std::forward<Values>(values)...)}...};
                }
            }

        public:
            Tuple values;

            /* The default values' constructor takes Python-style arguments just like
            the call operator, and is only enabled if the call signature is well-formed
            and all optional arguments have been accounted for. */
            template <typename... Values> requires (Bind<Values...>::enable)
            constexpr Defaults(Values&&... values) : values(build(
                std::make_index_sequence<Inner::n>{},
                std::forward<Values>(values)...
            )) {}
            constexpr Defaults(const Defaults& other) = default;
            constexpr Defaults(Defaults&& other) = default;

            /* Get the default value at index I of the tuple.  Use find<> to correlate
            an index from the enclosing signature if needed. */
            template <size_t I> requires (I < n)
            decltype(auto) get() {
                return std::get<I>(values).get();
            }

            /* Get the default value at index I of the tuple.  Use find<> to correlate
            an index from the enclosing signature if needed. */
            template <size_t I> requires (I < n)
            decltype(auto) get() const {
                return std::get<I>(values).get();
            }

            /* Get the default value associated with the named argument, if it is
            marked as optional. */
            template <StaticStr Name> requires (has<Name>)
            decltype(auto) get() {
                return std::get<idx<Name>>(values).get();
            }

            /* Get the default value associated with the named argument, if it is
            marked as optional. */
            template <StaticStr Name> requires (has<Name>)
            decltype(auto) get() const {
                return std::get<idx<Name>>(values).get();
            }

        };

        /* A Trie-based data structure that describes a collection of dynamic overloads
        for a `py::Function` object, which will be dispatched to when called from
        either Python or C++. */
        struct Overloads {
            struct Edge;
            struct Metadata;
            struct Edges;
            struct Node;

        private:

            static bool typecheck(PyObject* lhs, PyObject* rhs) {
                int rc = PyObject_IsSubclass(
                    lhs,
                    rhs
                );
                if (rc < 0) {
                    Exception::from_python();
                }
                return rc;
            }

        public:

            /* A single link between two nodes in the trie, which describes how to
            traverse from one to the other.  Multiple edges may share the same target
            node, and a unique edge will be created for each parameter in a key when it
            is inserted, such that the original key can be unambiguously identified
            from a simple search of the trie structure. */
            struct Edge {
                size_t hash;
                uint64_t mask;
                std::string name;
                Object type;
                ArgKind kind;
                std::shared_ptr<Node> node;
            };

            /* An encoded representation of a function that has been inserted into the
            overload trie, which includes the function itself, a hash of the key that
            it was inserted under, a bitmask of the required arguments that must be
            satisfied to invoke the function, and a canonical path of edges starting
            from the root node that leads to the terminal function.

            These are stored in an associative set rather than a hash set in order to
            ensure address stability over the lifetime of the corresponding nodes, so
            that they don't have to manage any memory themselves. */
            struct Metadata {
                size_t hash;
                uint64_t required;
                Object func;
                std::vector<Edge> path;
                friend bool operator<(const Metadata& lhs, const Metadata& rhs) {
                    return lhs.hash < rhs.hash;
                }
                friend bool operator<(const Metadata& lhs, size_t rhs) {
                    return lhs.hash < rhs;
                }
                friend bool operator<(size_t lhs, const Metadata& rhs) {
                    return lhs < rhs.hash;
                }
            };

            /* A collection of edges linking nodes within the trie.  The edges are
            topologically sorted by their expected type, with subclasses coming before
            their parent classes, and then by their kind, with required arguments
            coming before optional arguments, which come before variadic arguments.
            The stored edges are non-owning references to the contents of the
            `Metadata::path` sequence, which is guaranteed to have a stable address. */
            struct Edges {
            private:

                /* The topologically-sorted type map needs to store an additional
                mapping layer to account for overlapping edges and ordering based on
                kind.  By allowing transparent comparisons, we can support direct
                lookups by hash without breaking proper order. */
                struct Table {
                    struct Ptr {
                        Edge* edge;
                        Ptr(const Edge* edge = nullptr) : edge(edge) {}
                        operator const Edge*() const { return edge; }
                        const Edge& operator*() const { return *edge; }
                        const Edge* operator->() const { return edge; }
                        friend bool operator<(const Ptr& lhs, const Ptr& rhs) {
                            return
                                lhs.edge->kind < rhs.edge->kind ||
                                lhs.edge->hash < rhs.edge->hash;
                        }
                        friend bool operator<(const Ptr& lhs, size_t rhs) {
                            return lhs.edge->hash < rhs;
                        }
                        friend bool operator<(size_t lhs, const Ptr& rhs) {
                            return lhs < rhs.edge->hash;
                        }
                    };
                    std::shared_ptr<Node> node;
                    using Set = std::set<const Ptr, std::less<>>;
                    Set set;
                };

                struct TopoSort {
                    static bool operator()(PyObject* lhs, PyObject* rhs) {
                        return typecheck(lhs, rhs) || lhs < rhs;
                    }
                };

                using Map = std::map<PyObject*, Table, TopoSort>;

                /* A range adaptor that only yields edges matching a particular key,
                identified by its hash. */
                struct HashView {
                    const Edges& self;
                    PyObject* type;
                    size_t hash;

                    struct Sentinel;

                    struct Iterator {
                        using iterator_category = std::input_iterator_tag;
                        using difference_type = std::ptrdiff_t;
                        using value_type = const Edge*;
                        using pointer = value_type*;
                        using reference = value_type&;

                        Map::iterator it;
                        Map::iterator end;
                        PyObject* type;
                        size_t hash;
                        const Edge* curr;

                        Iterator(
                            Map::iterator&& it,
                            Map::iterator&& end,
                            PyObject* type,
                            size_t hash
                        ) : it(std::move(it)), end(std::move(end)), type(type),
                            hash(hash), curr(nullptr)
                        {
                            while (this->it != this->end) {
                                if (typecheck(type, this->it->first)) {
                                    auto lookup = this->it->second.set.find(hash);
                                    if (lookup != this->it->second.set.end()) {
                                        curr = *lookup;
                                        break;
                                    }
                                }
                                ++it;
                            }
                        }

                        Iterator& operator++() {
                            ++it;
                            while (it != end) {
                                if (typecheck(type, it->first)) {
                                    auto lookup = it->second.set.find(hash);
                                    if (lookup != it->second.set.end()) {
                                        curr = *lookup;
                                        break;
                                    }
                                }
                                ++it;
                            }
                            return *this;
                        }

                        const Edge* operator*() const {
                            return curr;
                        }

                        bool operator==(const Sentinel& sentinel) const {
                            return it == end;
                        }

                        bool operator!=(const Sentinel& sentinel) const {
                            return it != end;
                        }

                    };

                    struct Sentinel {
                        bool operator==(const Iterator& iter) const {
                            return iter.it == iter.end;
                        }
                        bool operator!=(const Iterator& iter) const {
                            return iter.it != iter.end;
                        }
                    };

                    Iterator begin() const {
                        return Iterator(
                            self.map.begin(),
                            self.map.end(),
                            type,
                            hash
                        );
                    }

                    Sentinel end() const {
                        return {};
                    }

                };

                /* A range adaptor that yields edges in order, regardless of key. */
                struct OrderedView {
                    const Edges& self;
                    PyObject* type;

                    struct Sentinel;

                    struct Iterator {
                        using iterator_category = std::input_iterator_tag;
                        using difference_type = std::ptrdiff_t;
                        using value_type = const Edge*;
                        using pointer = value_type*;
                        using reference = value_type&;

                        Map::iterator it;
                        Map::iterator end;
                        Table::Set::iterator edge_it;
                        Table::Set::iterator edge_end;
                        PyObject* type;

                        Iterator(
                            Map::iterator&& it,
                            Map::iterator&& end,
                            PyObject* type
                        ) : it(std::move(it)), end(std::move(end)), type(type)
                        {
                            while (this->it != this->end) {
                                if (typecheck(type, this->it->first)) {
                                    edge_it = this->it->second.set.begin();
                                    edge_end = this->it->second.set.end();
                                    break;
                                }
                                ++it;
                            }
                        }

                        Iterator& operator++() {
                            ++edge_it;
                            if (edge_it == edge_end) {
                                ++it;
                                while (it != end) {
                                    if (typecheck(type, it->first)) {
                                        edge_it = it->second.set.begin();
                                        edge_end = it->second.set.end();
                                        break;
                                    }
                                    ++it;
                                }
                            }
                            return *this;
                        }

                        const Edge* operator*() const {
                            return *edge_it;
                        }

                        bool operator==(const Sentinel& sentinel) const {
                            return it == end;
                        }

                        bool operator!=(const Sentinel& sentinel) const {
                            return it != end;
                        }

                    };

                    struct Sentinel {
                        bool operator==(const Iterator& iter) const {
                            return iter.it == iter.end;
                        }
                        bool operator!=(const Iterator& iter) const {
                            return iter.it != iter.end;
                        }
                    };

                    Iterator begin() const {
                        return Iterator(
                            self.map.begin(),
                            self.map.end(),
                            type
                        );
                    }

                    Sentinel end() const {
                        return {};
                    }

                };

            public:
                Map map;

                /* Insert an edge into this map and initialize its node pointer.
                Returns true if the insertion resulted in the creation of a new node,
                or false if the edge references an existing node. */
                [[maybe_unused]] bool insert(Edge& edge) {
                    auto [outer, inserted] = map.try_emplace(
                        edge->type,
                        Table{}
                    );
                    auto [inner, success] = outer->second.set.insert(&edge);
                    if (!success) {
                        if (inserted) {
                            map.erase(outer);
                        }
                        throw TypeError(
                            "overload trie already contains an edge for type: " +
                            repr(edge.type)
                        );
                    }
                    if (inserted) {
                        outer->second.node = std::make_shared<Node>();
                    }
                    edge.node = outer->second.node;
                    return inserted;
                }

                /* Insert an edge into this map using an explicit node pointer.
                Returns true if the insertion created a new table in the map, or false
                if it was already present.  Does NOT initialize the edge's node
                pointer, and a false return value does NOT guarantee that the existing
                table references the same node. */
                [[maybe_unused]] bool insert(Edge& edge, std::shared_ptr<Node> node) {
                    auto [outer, inserted] = map.try_emplace(
                        edge->type,
                        Table{node}
                    );
                    auto [inner, success] = outer->second.set.insert(&edge);
                    if (!success) {
                        if (inserted) {
                            map.erase(outer);
                        }
                        throw TypeError(
                            "overload trie already contains an edge for type: " +
                            repr(edge.type)
                        );
                    }
                    return inserted;
                }

                /* Remove any outgoing edges from the map that match the given hash. */
                void remove(size_t hash) noexcept {
                    std::vector<PyObject*> dead;
                    for (auto& [type, table] : map) {
                        table.set.erase(hash);
                        if (table.set.empty()) {
                            dead.push_back(type);
                        }
                    }
                    for (PyObject* type : dead) {
                        map.erase(type);
                    }
                }

                /* Return a range adaptor that iterates over the topologically-sorted
                types and yields individual edges for those that match against an
                observed type.  If multiple edges exist for a given type, then the
                range will yield them in order based on kind, with required arguments
                coming before optional, which come before variadic.  There is no
                guarantee that the edges come from a single key, just that they match
                the observed type. */
                OrderedView match(PyObject* type) const {
                    return {*this, type};
                }

                /* Return a range adaptor that iterates over the topologically-sorted
                types, and yields individual edges for those that match against an
                observed type and originate from the specified key, identified by its
                unique hash.  Rather than matching all possible edges, this view will
                essentially trace out the specified key, only checking edges that are
                contained within it. */
                HashView match(PyObject* type, size_t hash) const {
                    return {*this, type, hash};
                }

            };

            /* A single node in the overload trie, which holds the topologically-sorted
            edge maps necessary for traversal, insertion, and deletion of candidate
            functions, as well as a (possibly null) terminal function to call if this
            node is the last in a given argument list. */
            struct Node {
                PyObject* func = nullptr;

                /* A sorted map of outgoing edges for positional arguments that can be
                given immediately after this node. */
                Edges positional;

                /* A map of keyword argument names to sorted maps of outgoing edges for
                the arguments that can follow this node.  A special empty string will
                be used to represent variadic keyword arguments, which can match any
                unrecognized names. */
                std::unordered_map<std::string_view, Edges> keyword;

                /* Recursively search for a matching function in this node's sub-trie.
                Returns a borrowed reference to a terminal function in the case of a
                match, or null if no match is found, which causes the algorithm to
                backtrack one level and continue searching. */
                template <typename Container>
                [[nodiscard]] PyObject* search(
                    const Params<Container>& key,
                    size_t idx,
                    uint64_t& mask,
                    size_t& hash
                ) const {
                    if (idx >= key.size()) {
                        return func;
                    }
                    const Param& param = key[idx];

                    /// NOTE: if the index is zero, then the hash is ambiguous, so we
                    /// need to test all edges in order to find a matching key.
                    /// Otherwise, we already know which key we're tracing, so we can
                    /// restrict our search to exact matches.  This maintains
                    /// consistency in the final bitmasks, since each recursive call
                    /// will only search along a single path after the first edge has
                    /// been identified, but it requires us to duplicate some logic
                    /// here, since the required view types are not interchangeable.

                    // positional arguments have empty names
                    if (param.name.empty()) {
                        if (idx) {
                            for (const Edge* edge : positional.match(param.type, hash)) {
                                size_t i = idx + 1;
                                // variadic positional arguments will test all
                                // remaining positional args against the expected type
                                // and only recur if they all match
                                if constexpr (Arguments::has_args) {
                                    if (edge->kind.variadic()) {
                                        const Param* curr;
                                        while (
                                            i < key.size() &&
                                            (curr = &key[i])->pos() &&
                                            typecheck(
                                                curr->type,
                                                ptr(edge->type)
                                            )
                                        ) {
                                            ++i;
                                        }
                                        if (i < key.size() && curr->pos()) {
                                            continue;  // failed comparison
                                        }
                                    }
                                }
                                uint64_t temp_mask = mask | edge->mask;
                                size_t temp_hash = edge->hash;
                                PyObject* result = edge->node->search(
                                    key,
                                    i,
                                    temp_mask,
                                    temp_hash
                                );
                                if (result) {
                                    mask = temp_mask;
                                    hash = temp_hash;
                                    return result;
                                }
                            }
                        } else {
                            for (const Edge* edge : positional.match(param.type)) {
                                size_t i = idx + 1;
                                if constexpr (Arguments::has_args) {
                                    if (edge->kind.variadic()) {
                                        const Param* curr;
                                        while (
                                            i < key.size() &&
                                            (curr = &key[i])->pos() &&
                                            typecheck(
                                                curr->type,
                                                ptr(edge->type)
                                            )
                                        ) {
                                            ++i;
                                        }
                                        if (i < key.size() && curr->pos()) {
                                            continue;
                                        }
                                    }
                                }
                                uint64_t temp_mask = mask | edge->mask;
                                size_t temp_hash = edge->hash;
                                PyObject* result = edge->node->search(
                                    key,
                                    i,
                                    temp_mask,
                                    temp_hash
                                );
                                if (result) {
                                    mask = temp_mask;
                                    hash = temp_hash;
                                    return result;
                                }
                            }
                        }

                    // keyword argument names must be looked up in the keyword map.  If
                    // the keyword name is not recognized, check for a variadic keyword
                    // argument under an empty string, and continue with that.
                    } else {
                        auto it = keyword.find(param.name);
                        if (
                            it != keyword.end() ||
                            (it = keyword.find("")) != keyword.end()
                        ) {
                            if (idx) {
                                for (const Edge* edge : it->second.match(param.type, hash)) {
                                    uint64_t temp_mask = mask | edge->mask;
                                    size_t temp_hash = edge->hash;
                                    PyObject* result = edge->node->search(
                                        key,
                                        idx + 1,
                                        temp_mask,
                                        temp_hash
                                    );
                                    if (result) {
                                        // Keyword arguments can be given in any order,
                                        // so the return value may not reflect the
                                        // deepest node.  To consistently find the
                                        // terminal node, we compare the mask of the
                                        // current node to the mask of the edge we're
                                        // following, and substitute if it is greater.
                                        if (mask > edge->mask) {
                                            result = func;
                                        }
                                        mask = temp_mask;
                                        hash = temp_hash;
                                        return result;
                                    }
                                }
                            } else {
                                for (const Edge* edge : it->second.match(param.type)) {
                                    uint64_t temp_mask = mask | edge->mask;
                                    size_t temp_hash = edge->hash;
                                    PyObject* result = edge->node->search(
                                        key,
                                        idx + 1,
                                        temp_mask,
                                        temp_hash
                                    );
                                    if (result) {
                                        if (mask > edge->mask) {
                                            result = func;
                                        }
                                        mask = temp_mask;
                                        hash = temp_hash;
                                        return result;
                                    }
                                }
                            }
                        }
                    }

                    // return nullptr to backtrack
                    return nullptr;
                }

                /* Remove all outgoing edges that match a particular hash. */
                void remove(size_t hash) {
                    positional.remove(hash);

                    std::vector<std::string_view> dead;
                    for (auto& [name, edges] : keyword) {
                        edges.remove(hash);
                        if (edges.map.empty()) {
                            dead.push_back(name);
                        }
                    }
                    for (std::string_view name : dead) {
                        keyword.erase(name);
                    }
                }

                /* Check to see if this node has any outgoing edges. */
                bool empty() const {
                    return positional.map.empty() && keyword.empty();
                }

            };

            std::shared_ptr<Node> root;
            std::set<const Metadata, std::less<>> data;
            mutable std::unordered_map<size_t, PyObject*> cache;

            /* Search the overload trie for a matching signature.  This will
            recursively backtrack until a matching node is found or the trie is
            exhausted, returning nullptr on a failed search.  The results will be
            cached for subsequent invocations.  An error will be thrown if the key does
            not fully satisfy the enclosing parameter list.  Note that variadic
            parameter packs must be expanded prior to calling this function.

            The Python-level call operator for `py::Function<>` will immediately
            delegate to this function after constructing a key from the input
            arguments, so it will be called every time a C++ function is invoked from
            Python.  If it returns null, then the fallback implementation will be
            used instead.

            Returns a borrowed reference to the terminal function if a match is
            found within the trie, or null otherwise. */
            template <typename Container>
            [[nodiscard]] PyObject* search(const Params<Container>& key) const {
                // check the cache first
                auto it = cache.find(key.hash);
                if (it != cache.end()) {
                    return it->second;
                }

                // ensure the key minimally satisfies the enclosing parameter list
                assert_valid_args(key);

                // search the trie for a matching node
                if (root) {
                    uint64_t mask = 0;
                    size_t hash;
                    PyObject* result = root->search(key, 0, mask, hash);
                    if (result) {
                        // hash is only initialized if the key has at least one param
                        if (key.empty()) {
                            cache[key.hash] = result;  // may be null
                            return result;
                        }
                        const Metadata& metadata = *(data.find(hash));
                        if ((mask & metadata.required) == metadata.required) {
                            cache[key.hash] = result;  // may be null
                            return result;
                        }
                    }
                }
                cache[key.hash] = nullptr;
                return nullptr;
            }

            /* Search the overload trie for a matching signature, suppressing errors
            caused by the signature not satisfying the enclosing parameter list.  This
            is equivalent to calling `search()` in a try/catch, but without any error
            handling overhead.  Errors are converted into a null optionals, separate
            from the null status of the wrapped pointer, which retains the same
            semantics as `search()`.

            This is used by the Python-level `__getitem__` and `__contains__` operators
            for `py::Function<>` instances, which converts a null optional result into
            `None` on the Python side.  Otherwise, it will return the function that
            would be invoked if the function were to be called with the given
            arguments, which may be a self-reference if the fallback implementation is
            selected.

            Returns a borrowed reference to the terminal function if a match is
            found. */
            template <typename Container>
            [[nodiscard]] std::optional<PyObject*> get(
                const Params<Container>& key
            ) const {
                auto it = cache.find(key.hash);
                if (it != cache.end()) {
                    return it->second;
                }

                uint64_t mask = 0;
                for (size_t i = 0, n = key.size(); i < n; ++i) {
                    const Param& param = key[i];
                    if (param.name.empty()) {
                        const Callback& callback = Arguments::callback(i);
                        if (!callback || !callback(param.type)) {
                            return std::nullopt;
                        }
                        mask |= callback.mask;
                    } else {
                        const Callback& callback = Arguments::callback(param.name);
                        if (
                            !callback ||
                            (mask & callback.mask) ||
                            !callback(param.type)
                        ) {
                            return std::nullopt;
                        }
                        mask |= callback.mask;
                    }
                }
                if ((mask & required) != required) {
                    return std::nullopt;
                }

                if (root) {
                    mask = 0;
                    size_t hash;
                    PyObject* result = root->search(key, 0, mask, hash);
                    if (result) {
                        if (key.empty()) {
                            cache[key.hash] = result;
                            return result;
                        }
                        const Metadata& metadata = *(data.find(hash));
                        if ((mask & metadata.required) == metadata.required) {
                            cache[key.hash] = result;
                            return result;
                        }
                    }
                }
                cache[key.hash] = nullptr;
                return nullptr;
            }

            /* Insert a function into the overload trie, throwing a TypeError if it
            does not conform to the enclosing parameter list or if it conflicts with
            another node in the trie. */
            template <typename Container>
            void insert(const Params<Container>& key, const Object& func) {
                // assert the key minimally satisfies the enclosing parameter list
                []<size_t... Is>(std::index_sequence<Is...>, const Params<Container>& key) {
                    size_t idx = 0;
                    (assert_viable_overload<Is>(key, idx), ...);
                }(std::make_index_sequence<Arguments::n>{}, key);

                // construct the root node if it doesn't already exist
                if (root == nullptr) {
                    root = std::make_shared<Node>();
                }

                // if the key is empty, then the root node is the terminal node
                if (key.empty()) {
                    if (root->func) {
                        throw TypeError("overload already exists");
                    }
                    root->func = ptr(func);
                    data.emplace(key.hash, 0, func, {});
                    cache.clear();
                    return;
                }

                // insert an edge linking each parameter in the key
                std::vector<Edge> path;
                path.reserve(key.size());
                Node* curr = root.get();
                int first_keyword = -1;
                int last_required = 0;
                uint64_t required = 0;
                for (int i = 0, end = key.size(); i < end; ++i) {
                    try {
                        const Param& param = key[i];
                        path.push_back({
                            .hash = key.hash,
                            .mask = 1ULL << i,
                            .name = param.name,
                            .type = reinterpret_borrow<Object>(param.type),
                            .kind = param.kind,
                            .node = nullptr
                        });
                        if (param.posonly()) {
                            curr->positional.insert(path.back());
                            if (!param.opt()) {
                                ++first_keyword;
                                last_required = i;
                                required |= 1ULL << i;
                            }
                        } else if (param.pos()) {
                            curr->positional.insert(path.back());
                            auto [it, _] = curr->keyword.try_emplace(param.name, Edges{});
                            it->second.insert(path.back(), path.back().node);
                            if (!param.opt()) {
                                last_required = i;
                                required |= 1ULL << i;
                            }
                        } else if (param.kw()) {
                            auto [it, _] = curr->keyword.try_emplace(param.name, Edges{});
                            it->second.insert(path.back());
                            if (!param.opt()) {
                                last_required = i;
                                required |= 1ULL << i;
                            }
                        } else if (param.args()) {
                            curr->positional.insert(path.back());
                        } else if (param.kwargs()) {
                            auto [it, _] = curr->keyword.try_emplace("", Edges{});
                            it->second.insert(path.back());
                        } else {
                            throw ValueError("invalid argument kind");
                        }
                        curr = path.back().node.get();

                    } catch (...) {
                        curr = root.get();
                        for (int j = 0; j < i; ++j) {
                            const Edge& edge = path[j];
                            curr->remove(edge.hash);
                            curr = edge.node.get();
                        }
                        if (root->empty()) {
                            root.reset();
                        }
                        throw;
                    }
                }

                // backfill the terminal functions and full keyword maps for each node
                try {
                    std::string_view name;
                    int start = key.size() - 1;
                    for (int i = start; i > first_keyword; --i) {
                        Edge& edge = path[i];
                        if (i >= last_required) {
                            if (edge.node->func) {
                                throw TypeError("overload already exists");
                            }
                            edge.node->func = ptr(func);
                        }
                        for (int j = first_keyword; j < key.size(); ++j) {
                            Edge& kw = path[j];
                            if (
                                kw.posonly() ||
                                kw.args() ||
                                kw.name == edge.name ||  // incoming edge
                                (i < start && kw.name == name)  // outgoing edge
                            ) {
                                continue;
                            }
                            auto& [it, _] = edge.node->keyword.try_emplace(
                                kw.name,
                                Edges{}
                            );
                            it->second.insert(kw, kw.node);
                        }
                        name = edge.name;
                    }

                    // extend backfill to the root node
                    if (!required) {
                        if (root->func) {
                            throw TypeError("overload already exists");
                        }
                        root->func = ptr(func);
                    }
                    bool extend_keywords = true;
                    for (Edge& edge : path) {
                        if (!edge.posonly()) {
                            break;
                        } else if (!edge.opt()) {
                            extend_keywords = false;
                            break;
                        }
                    }
                    if (extend_keywords) {
                        for (int j = first_keyword; j < key.size(); ++j) {
                            Edge& kw = path[j];
                            if (kw.posonly() || kw.args()) {
                                continue;
                            }
                            auto& [it, _] = root->keyword.try_emplace(
                                kw.name,
                                Edges{}
                            );
                            it->second.insert(kw, kw.node);
                        }
                    }

                } catch (...) {
                    Node* curr = root.get();
                    for (int i = 0, end = key.size(); i < end; ++i) {
                        const Edge& edge = path[i];
                        curr->remove(edge.hash);
                        if (i >= last_required) {
                            edge.node->func = nullptr;
                        }
                        curr = edge.node.get();
                    }
                    if (root->empty()) {
                        root.reset();
                    }
                    throw;
                }

                // track the function and required arguments for the inserted key
                data.emplace(key.hash, required, func, std::move(path));
                cache.clear();
            }

            /* Remove a node from the overload trie and prune any dead-ends that lead
            to it.  Returns the function that was removed, or nullopt if no matching
            function was found. */
            template <typename Container>
            [[maybe_unused]] std::optional<Object> remove(const Params<Container>& key) {
                // assert the key minimally satisfies the enclosing parameter list
                assert_valid_args(key);

                // search the trie for a matching node
                if (root) {
                    uint64_t mask = 0;
                    size_t hash;
                    Object result = reinterpret_borrow<Object>(
                        root->search(key, 0, mask, hash)
                    );
                    if (!result.is(nullptr)) {
                        // hash is only initialized if the key has at least one param
                        if (key.empty()) {
                            for (const Metadata& data : this->data) {
                                if (data.func == ptr(result)) {
                                    hash = data.hash;
                                    break;
                                }
                            }
                        }
                        const Metadata& metadata = *(data.find(hash));
                        if ((mask & metadata.required) == metadata.required) {
                            Node* curr = root.get();
                            for (const Edge& edge : metadata.path) {
                                curr->remove(edge.hash);
                                if (edge.node->func == ptr(metadata.func)) {
                                    edge.node->func = nullptr;
                                }
                                curr = edge.node.get();
                            }
                            if (root->func == ptr(metadata.func)) {
                                root->func = nullptr;
                            }
                            data.erase(hash);
                            if (data.empty()) {
                                root.reset();
                            }
                            return result;
                        }
                    }
                }
                return std::nullopt;
            }

            /* Clear the overload trie, removing all tracked functions. */
            void clear() {
                cache.clear();
                root.reset();
                data.clear();
            }

            /* Manually reset the function's overload cache, forcing overload paths to
            be recalculated on subsequent calls. */
            void flush() {
                cache.clear();
            }

        private:

            template <typename Container>
            static void assert_valid_args(const Params<Container>& key) {
                uint64_t mask = 0;
                for (size_t i = 0, n = key.size(); i < n; ++i) {
                    const Param& param = key[i];
                    if (param.name.empty()) {
                        const Callback& callback = Arguments::callback(i);
                        if (!callback) {
                            throw TypeError(
                                "received unexpected positional argument at index " +
                                std::to_string(i)
                            );
                        }
                        if (!callback(param.type)) {
                            throw TypeError(
                                "expected positional argument at index " +
                                std::to_string(i) + " to be a subclass of '" +
                                repr(reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(callback.type())
                                )) + "', not: '" + repr(reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(param.type)
                                )) + "'"
                            );
                        }
                        mask |= callback.mask;
                    } else if (param.kw()) {
                        const Callback& callback = Arguments::callback(param.name);
                        if (!callback) {
                            throw TypeError(
                                "received unexpected keyword argument: '" +
                                std::string(param.name) + "'"
                            );
                        }
                        if (mask & callback.mask) {
                            throw TypeError(
                                "received multiple values for argument '" +
                                std::string(param.name) + "'"
                            );
                        }
                        if (!callback(param.type)) {
                            throw TypeError(
                                "expected argument '" + std::string(param.name) +
                                "' to be a subclass of '" +
                                repr(reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(callback.type())
                                )) + "', not: '" + repr(reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(param.type)
                                )) + "'"
                            );
                        }
                        mask |= callback.mask;
                    }
                }
                if ((mask & Arguments::required) != Arguments::required) {
                    uint64_t missing = Arguments::required & ~(mask & Arguments::required);
                    std::string msg = "missing required arguments: [";
                    size_t i = 0;
                    while (i < n) {
                        if (missing & (1ULL << i)) {
                            const Callback& param = positional_table[i];
                            if (param.name.empty()) {
                                msg += "<parameter " + std::to_string(i) + ">";
                            } else {
                                msg += "'" + std::string(param.name) + "'";
                            }
                            ++i;
                            break;
                        }
                        ++i;
                    }
                    while (i < n) {
                        if (missing & (1ULL << i)) {
                            const Callback& param = positional_table[i];
                            if (param.name.empty()) {
                                msg += ", <parameter " + std::to_string(i) + ">";
                            } else {
                                msg += ", '" + std::string(param.name) + "'";
                            }
                        }
                        ++i;
                    }
                    msg += "]";
                    throw TypeError(msg);
                }
            }

            template <size_t I, typename Container>
            static void assert_viable_overload(
                const Params<Container>& key,
                size_t& idx
            ) {
                using T = __object__<std::remove_cvref_t<typename ArgTraits<at<I>>::type>>;

                constexpr auto description = [](const Param& param) {
                    if (param.kwonly()) {
                        return "keyword-only";
                    } else if (param.kw()) {
                        return "positional-or-keyword";
                    } else if (param.pos()) {
                        return "positional";
                    } else if (param.args()) {
                        return "variadic positional";
                    } else if (param.kwargs()) {
                        return "variadic keyword";
                    } else {
                        return "<unknown>";
                    }
                };

                if constexpr (ArgTraits<at<I>>::posonly()) {
                    if (idx >= key.size()) {
                        if (ArgTraits<at<I>>::name.empty()) {
                            throw TypeError(
                                "missing positional-only argument at index " +
                                std::to_string(idx)
                            );
                        } else {
                            throw TypeError(
                                "missing positional-only argument '" +
                                ArgTraits<at<I>>::name + "' at index " +
                                std::to_string(idx)
                            );
                        }
                    }
                    const Param& param = key[idx];
                    if (!param.posonly()) {
                        if (ArgTraits<at<I>>::name.empty()) {
                            throw TypeError(
                                "expected positional-only argument at index " +
                                std::to_string(idx) + ", not " + description(param)
                            );
                        } else {
                            throw TypeError(
                                "expected argument '" + ArgTraits<at<I>>::name +
                                "' at index " + std::to_string(idx) +
                                " to be positional-only, not " + description(param)
                            );
                        }
                    }
                    if (
                        !ArgTraits<at<I>>::name.empty() &&
                        param.name != ArgTraits<at<I>>::name
                    ) {
                        throw TypeError(
                            "expected argument '" + ArgTraits<at<I>>::name +
                            "' at index " + std::to_string(idx) + ", not '" +
                            std::string(param.name) + "'"
                        );
                    }
                    if (!ArgTraits<at<I>>::opt() && param.opt()) {
                        if (ArgTraits<at<I>>::name.empty()) {
                            throw TypeError(
                                "required positional-only argument at index " +
                                std::to_string(idx) + " must not have a default "
                                "value"
                            );
                        } else {
                            throw TypeError(
                                "required positional-only argument '" +
                                ArgTraits<at<I>>::name + "' at index " +
                                std::to_string(idx) + " must not have a default "
                                "value"
                            );
                        }
                    }
                    if (!issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param.type)
                    ))) {
                        if (ArgTraits<at<I>>::name.empty()) {
                            throw TypeError(
                                "expected positional-only argument at index " +
                                std::to_string(idx) + " to be a subclass of '" +
                                repr(reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(ptr(Type<T>()))
                                )) + "', not: '" + repr(reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(param.type)
                                )) + "'"
                            );
                        } else {
                            throw TypeError(
                                "expected positional-only argument '" +
                                ArgTraits<at<I>>::name + "' at index " +
                                std::to_string(idx) + " to be a subclass of '" +
                                repr(reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(ptr(Type<T>()))
                                )) + "', not: '" + repr(reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(param.type)
                                )) + "'"
                            );
                        }
                    }
                    ++idx;

                } else if constexpr (ArgTraits<at<I>>::pos()) {
                    if (idx >= key.size()) {
                        throw TypeError(
                            "missing positional-or-keyword argument '" +
                            ArgTraits<at<I>>::name + "' at index " +
                            std::to_string(idx)
                        );
                    }
                    const Param& param = key[idx];
                    if (!param.pos() || !param.kw()) {
                        throw TypeError(
                            "expected argument '" + ArgTraits<at<I>>::name +
                            "' at index " + std::to_string(idx) +
                            " to be positional-or-keyword, not " + description(param)
                        );
                    }
                    if (param.name != ArgTraits<at<I>>::name) {
                        throw TypeError(
                            "expected positional-or-keyword argument '" +
                            ArgTraits<at<I>>::name + "' at index " +
                            std::to_string(idx) + ", not '" +
                            std::string(param.name) + "'"
                        );
                    }
                    if (!ArgTraits<at<I>>::opt() && param.opt()) {
                        throw TypeError(
                            "required positional-or-keyword argument '" +
                            ArgTraits<at<I>>::name + "' at index " +
                            std::to_string(idx) + " must not have a default value"
                        );
                    }
                    if (!issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param.type)
                    ))) {
                        throw TypeError(
                            "expected positional-or-keyword argument '" +
                            ArgTraits<at<I>>::name + "' at index " +
                            std::to_string(idx) + " to be a subclass of '" +
                            repr(reinterpret_borrow<Object>(
                                reinterpret_cast<PyObject*>(ptr(Type<T>()))
                            )) + "', not: '" + repr(reinterpret_borrow<Object>(
                                reinterpret_cast<PyObject*>(param.type)
                            )) + "'"
                        );
                    }
                    ++idx;

                } else if constexpr (ArgTraits<at<I>>::kw()) {
                    if (idx >= key.size()) {
                        throw TypeError(
                            "missing keyword-only argument '" + ArgTraits<at<I>>::name +
                            "' at index " + std::to_string(idx)
                        );
                    }
                    const Param& param = key[idx];
                    if (!param.kwonly()) {
                        throw TypeError(
                            "expected argument '" + ArgTraits<at<I>>::name +
                            "' at index " + std::to_string(idx) +
                            " to be keyword-only, not " + description(param)
                        );
                    }
                    if (param.name != ArgTraits<at<I>>::name) {
                        throw TypeError(
                            "expected keyword-only argument '" + ArgTraits<at<I>>::name +
                            "' at index " + std::to_string(idx) + ", not '" +
                            std::string(param.name) + "'"
                        );
                    }
                    if (!ArgTraits<at<I>>::opt() && param.opt()) {
                        throw TypeError(
                            "required keyword-only argument '" + ArgTraits<at<I>>::name +
                            "' at index " + std::to_string(idx) + " must not have a "
                            "default value"
                        );
                    }
                    if (!issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param.type)
                    ))) {
                        throw TypeError(
                            "expected keyword-only argument '" + ArgTraits<at<I>>::name +
                            "' at index " + std::to_string(idx) +
                            " to be a subclass of '" + repr(reinterpret_borrow<Object>(
                                reinterpret_cast<PyObject*>(ptr(Type<T>()))
                            )) + "', not: '" + repr(reinterpret_borrow<Object>(
                                reinterpret_cast<PyObject*>(param.type)
                            )) + "'"
                        );
                    }
                    ++idx;

                } else if constexpr (ArgTraits<at<I>>::args()) {
                    while (idx < key.size()) {
                        const Param& param = key[idx];
                        if (!(param.pos() || param.args())) {
                            break;
                        }
                        if (!issubclass<T>(
                            reinterpret_borrow<Object>(
                                reinterpret_cast<PyObject*>(param.type)
                            )
                        )) {
                            if (param.name.empty()) {
                                throw TypeError(
                                    "expected variadic positional argument at index " +
                                    std::to_string(idx) + " to be a subclass of '" +
                                    repr(reinterpret_borrow<Object>(
                                        reinterpret_cast<PyObject*>(ptr(Type<T>()))
                                    )) + "', not: '" + repr(reinterpret_borrow<Object>(
                                        reinterpret_cast<PyObject*>(param.type)
                                    )) + "'"
                                );
                            } else {
                                throw TypeError(
                                    "expected variadic positional argument '" +
                                    std::string(param.name) + "' at index " +
                                    std::to_string(idx) + " to be a subclass of '" +
                                    repr(reinterpret_borrow<Object>(
                                        reinterpret_cast<PyObject*>(ptr(Type<T>()))
                                    )) + "', not: '" + repr(reinterpret_borrow<Object>(
                                        reinterpret_cast<PyObject*>(param.type)
                                    )) + "'"
                                );
                            }
                        }
                        ++idx;
                    }

                } else if constexpr (ArgTraits<at<I>>::kwargs()) {
                    while (idx < key.size()) {
                        const Param& param = key[idx];
                        if (!(param.kw() || param.kwargs())) {
                            break;
                        }
                        if (!issubclass<T>(
                            reinterpret_borrow<Object>(
                                reinterpret_cast<PyObject*>(param.type)
                            )
                        )) {
                            throw TypeError(
                                "expected variadic keyword argument '" +
                                std::string(param.name) + "' at index " +
                                std::to_string(idx) + " to be a subclass of '" +
                                repr(reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(ptr(Type<T>()))
                                )) + "', not: '" + repr(reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(param.type)
                                )) + "'"
                            );
                        }
                        ++idx;
                    }

                } else {
                    static_assert(false, "invalid argument kind");
                }
            }

        };

        /// TODO: add a method to Bind<> that produces a simplified Params list from a valid
        /// C++ parameter list.  There also needs to be a cousin for Python calls that
        /// does the same thing from a vectorcall argument array.  Both of the latter
        /// can apply the simplified logic, since it will be used in the call operator.
        /// -> This key() method will also return a std::array-based Params list,
        /// similar to the above.

        /* A helper that binds C++ arguments to the enclosing signature and performs
        the necessary translation to invoke a matching C++ or Python function. */
        template <typename... Values>
        struct Bind {
        private:
            using Outer = Arguments;
            using Inner = Arguments<Values...>;

            template <size_t I>
            using Target = Outer::template at<I>;
            template <size_t J>
            using Source = Inner::template at<J>;

            /* Upon encountering a variadic positional pack in the target signature,
            recursively traverse the remaining source positional arguments and ensure
            that each is convertible to the target type. */
            template <size_t J, typename T>
            static consteval bool consume_target_args() {
                if constexpr (J < Inner::kw_idx && J < Inner::kwargs_idx) {
                    return std::convertible_to<
                        typename ArgTraits<Source<J>>::type,
                        typename ArgTraits<T>::type
                    > && consume_target_args<J + 1, T>();
                } else {
                    return true;
                }
            }

            /* Upon encountering a variadic keyword pack in the target signature,
            recursively traverse the source keyword arguments and extract any that
            aren't present in the target signature, ensuring they are convertible to
            the target type. */
            template <size_t J, typename T>
            static consteval bool consume_target_kwargs() {
                if constexpr (J < Inner::n) {
                    return (
                        Outer::template has<ArgTraits<Source<J>>::name> ||
                        std::convertible_to<
                            typename ArgTraits<Source<J>>::type,
                            typename ArgTraits<T>::type
                        >
                    ) && consume_target_kwargs<J + 1, T>();
                } else {
                    return true;
                }
            }

            /* Upon encountering a variadic positional pack in the source signature,
            recursively traverse the target positional arguments and ensure that the source
            type is convertible to each target type. */
            template <size_t I, typename S>
            static consteval bool consume_source_args() {
                if constexpr (I < kwonly_idx && I < kwargs_idx) {
                    return std::convertible_to<
                        typename ArgTraits<S>::type,
                        typename ArgTraits<Target<I>>::type
                    > && consume_source_args<I + 1, S>();
                } else {
                    return true;
                }
            }

            /* Upon encountering a variadic keyword pack in the source signature,
            recursively traverse the target keyword arguments and extract any that aren't
            present in the source signature, ensuring that the source type is convertible
            to each target type. */
            template <size_t I, typename S>
            static consteval bool consume_source_kwargs() {
                if constexpr (I < Outer::n) {
                    return (
                        Inner::template has<ArgTraits<Target<I>>::name> ||
                        std::convertible_to<
                            typename ArgTraits<S>::type,
                            typename ArgTraits<Target<I>>::type
                        >
                    ) && consume_source_kwargs<I + 1, S>();
                } else {
                    return true;
                }
            }

            /* Recursively check whether the source arguments conform to Python calling
            conventions (i.e. no positional arguments after a keyword, no duplicate
            keywords, etc.), fully satisfy the target signature, and are convertible to
            the expected types, after accounting for parameter packs in both signatures.

            The actual algorithm is quite complex, especially since it is evaluated at
            compile time via template recursion and must validate both signatures at
            once.  Here's a rough outline of how it works:

                1.  Generate two indices, I and J, which are used to traverse over the
                    target and source signatures, respectively.
                2.  For each I, J, inspect the target argument at index I:
                    a.  If the target argument is positional-only, check that the
                        source argument is not a keyword, otherwise check that the
                        target argument is marked as optional.
                    b.  If the target argument is positional-or-keyword, check that the
                        source argument meets the criteria for (a), or that the source
                        signature contains a matching keyword argument.
                    c.  If the target argument is keyword-only, check that the source's
                        positional arguments have been exhausted, and that the source
                        signature contains a matching keyword argument or the target
                        argument is marked as optional.
                    d.  If the target argument is variadic positional, recur until all
                        source positional arguments have been exhausted, ensuring that
                        each is convertible to the target type.
                    e.  If the target argument is variadic keyword, recur until all
                        source keyword arguments have been exhausted, ensuring that
                        each is convertible to the target type.
                3.  Then inspect the source argument at index J:
                    a.  If the source argument is positional, check that it is
                        convertible to the target type.
                    b.  If the source argument is keyword, check that the target
                        signature contains a matching keyword argument, and that the
                        source argument is convertible to the target type.
                    c.  If the source argument is variadic positional, recur until all
                        target positional arguments have been exhausted, ensuring that
                        each is convertible from the source type.
                    d.  If the source argument is variadic keyword, recur until all
                        target keyword arguments have been exhausted, ensuring that
                        each is convertible from the source type.
                4.  Advance to the next argument pair and repeat until all arguments
                    have been checked.  If there are more target arguments than source
                    arguments, then the remaining target arguments must be optional or
                    variadic.  If there are more source arguments than target
                    arguments, then we return false, as the only way to satisfy the
                    target signature is for it to include variadic arguments, which
                    would have avoided this case.

            If all of these conditions are met, then the call operator is enabled and
            the arguments can be translated by the reordering operators listed below.
            Otherwise, the call operator is disabled and the arguments are rejected in
            a way that allows LSPs to provide informative feedback to the user. */
            template <size_t I, size_t J>
            static consteval bool enable_recursive() {
                // both lists are satisfied
                if constexpr (I >= Outer::n && J >= Inner::n) {
                    return true;

                // there are extra source arguments
                } else if constexpr (I >= Outer::n) {
                    return false;

                // there are extra target arguments
                } else if constexpr (J >= Inner::n) {
                    using T = Target<I>;
                    if constexpr (ArgTraits<T>::opt() || ArgTraits<T>::args()) {
                        return enable_recursive<I + 1, J>();
                    } else if constexpr (ArgTraits<T>::kwargs()) {
                        return consume_target_kwargs<Inner::kw_idx, T>();
                    }
                    return false;  // a required argument is missing

                // both lists have arguments remaining
                } else {
                    using T = Target<I>;
                    using S = Source<J>;

                    // ensure target arguments are present without conflict + expand
                    // parameter packs over source arguments
                    if constexpr (ArgTraits<T>::posonly()) {
                        if constexpr (
                            (
                                ArgTraits<T>::name != "" &&
                                Inner::template has<ArgTraits<T>::name>
                            ) || (
                                !ArgTraits<T>::opt() &&
                                ArgTraits<typename Inner::template at<J>>::kw()
                            )
                        ) {
                            return false;
                        }
                    } else if constexpr (ArgTraits<T>::pos()) {
                        if constexpr (
                            (
                                ArgTraits<typename Inner::template at<J>>::pos() &&
                                Inner::template has<ArgTraits<T>::name>
                            ) || (
                                ArgTraits<typename Inner::template at<J>>::kw() &&
                                !ArgTraits<T>::opt() &&
                                !Inner::template has<ArgTraits<T>::name>
                            )
                        ) {
                            return false;
                        }
                    } else if constexpr (ArgTraits<T>::kwonly()) {
                        if constexpr (
                            ArgTraits<typename Inner::template at<J>>::pos() || (
                                !ArgTraits<T>::opt() &&
                                !Inner::template has<ArgTraits<T>::name>
                            )
                        ) {
                            return false;
                        }
                    } else if constexpr (ArgTraits<T>::args()) {
                        return consume_target_args<J, T>() && enable_recursive<
                            I + 1,
                            Inner::has_kw ? Inner::kw_idx : Inner::kwargs_idx
                        >();  // skip ahead to source keywords
                    } else if constexpr (ArgTraits<T>::kwargs()) {
                        return consume_target_kwargs<
                            Inner::has_kw ? Inner::kw_idx : Inner::kwargs_idx,
                            T
                        >();  // end of expression
                    } else {
                        return false;  // not reachable
                    }

                    // ensure source arguments match targets & expand parameter packs
                    if constexpr (ArgTraits<S>::posonly()) {
                        if constexpr (!std::convertible_to<
                            typename ArgTraits<S>::type,
                            typename ArgTraits<T>::type
                        >) {
                            return false;
                        }
                    } else if constexpr (ArgTraits<S>::kw()) {
                        if constexpr (Outer::template has<ArgTraits<S>::name>) {
                            using T2 = Target<Outer::template idx<ArgTraits<S>::name>>;
                            if constexpr (!std::convertible_to<
                                typename ArgTraits<S>::type,
                                typename ArgTraits<T2>::type
                            >) {
                                return false;
                            }
                        } else if constexpr (Outer::has_kwargs) {
                            if constexpr (!std::convertible_to<
                                typename ArgTraits<S>::type,
                                typename ArgTraits<Target<Outer::kwargs_idx>>::type
                            >) {
                                return false;
                            }
                        } else {
                            return false;
                        }
                    } else if constexpr (ArgTraits<S>::args()) {
                        return consume_source_args<I, S>() && enable_recursive<
                            Outer::has_kwonly ? Outer::kwonly_idx : Outer::kwargs_idx,
                            J + 1
                        >();  // skip to target keywords
                    } else if constexpr (ArgTraits<S>::kwargs()) {
                        return consume_source_kwargs<I, S>();  // end of expression
                    } else {
                        return false;  // not reachable
                    }

                    // advance to next argument pair
                    return enable_recursive<I + 1, J + 1>();
                }
            }

            /// TODO: no idea if build_kwargs() is correct or necessary

            template <size_t I, typename T>
            static constexpr void build_kwargs(
                std::unordered_map<std::string, T>& map,
                Values&&... args
            ) {
                using Arg = Source<Inner::kw_idx + I>;
                if constexpr (!Outer::template has<ArgTraits<Arg>::name>) {
                    map.emplace(
                        ArgTraits<Arg>::name,
                        impl::unpack_arg<Inner::kw_index + I>(
                            std::forward<Source>(args)...
                        )
                    );
                }
            }

            /* The cpp_to_cpp() method is used to convert an index sequence over the
             * enclosing signature into the corresponding values pulled from either the
             * call site or the function's defaults.  It is complicated by the presence
             * of variadic parameter packs in both the target signature and the call
             * arguments, which have to be handled as a cross product of possible
             * combinations.
             *
             * Unless a variadic parameter pack is given at the call site, all of these
             * are resolved entirely at compile time by reordering the arguments using
             * template recursion.  However, because the size of a variadic parameter
             * pack cannot be determined at compile time, calls that use these will have
             * to extract values at runtime, and may therefore raise an error if a
             * corresponding value does not exist in the parameter pack, or if there are
             * extras that are not included in the target signature.
             */

            /// TODO: this side of things might need modifications to be able to handle
            /// the `self` parameter.

            template <size_t I>
            static constexpr ArgTraits<Target<I>>::type cpp_to_cpp(
                const Defaults& defaults,
                Values&&... values
            ) {
                using T = Target<I>;
                using type = ArgTraits<T>::type;
                constexpr StaticStr name = ArgTraits<T>::name;

                if constexpr (ArgTraits<T>::kwonly()) {
                    if constexpr (Inner::template has<name>) {
                        constexpr size_t idx = Inner::template idx<name>;
                        return impl::unpack_arg<idx>(std::forward<Values>(values)...);
                    } else {
                        return defaults.template get<I>();
                    }

                } else if constexpr (ArgTraits<T>::kw()) {
                    if constexpr (I < Inner::kw_idx) {
                        return impl::unpack_arg<I>(std::forward<Values>(values)...);
                    } else if constexpr (Inner::template has<name>) {
                        constexpr size_t idx = Inner::template idx<name>;
                        return impl::unpack_arg<idx>(std::forward<Values>(values)...);
                    } else {
                        return defaults.template get<I>();
                    }

                } else if constexpr (ArgTraits<T>::args()) {
                    using Pack = std::vector<type>;
                    Pack vec;
                    if constexpr (I < Inner::kw_idx) {
                        constexpr size_t diff = Inner::kw_idx - I;
                        vec.reserve(diff);
                        []<size_t... Js>(
                            std::index_sequence<Js...>,
                            Pack& vec,
                            Values&&... args
                        ) {
                            (vec.push_back(
                                impl::unpack_arg<I + Js>(std::forward<Values>(args)...)
                            ), ...);
                        }(
                            std::make_index_sequence<diff>{},
                            vec,
                            std::forward<Values>(values)...
                        );
                    }
                    return vec;

                } else if constexpr (ArgTraits<T>::kwargs()) {
                    using Pack = std::unordered_map<std::string, type>;
                    Pack pack;
                    []<size_t... Js>(
                        std::index_sequence<Js...>,
                        Pack& pack,
                        Values&&... args
                    ) {
                        (build_kwargs<Js>(pack, std::forward<Values>(args)...), ...);
                    }(
                        std::make_index_sequence<Inner::n - Inner::kw_idx>{},
                        pack,
                        std::forward<Values>(values)...
                    );
                    return pack;

                } else {
                    if constexpr (I < Inner::kw_idx) {
                        return impl::unpack_arg<I>(std::forward<Values>(values)...);
                    } else {
                        return defaults.template get<I>();
                    }
                }
            }

            template <size_t I, std::input_iterator Iter, std::sentinel_for<Iter> End>
            static constexpr ArgTraits<Target<I>>::type cpp_to_cpp(
                const Defaults& defaults,
                size_t args_size,
                Iter& iter,
                const End& end,
                Values&&... values
            ) {
                using T = Target<I>;
                using type = ArgTraits<T>::type;
                constexpr StaticStr name = ArgTraits<T>::name;

                if constexpr (ArgTraits<T>::kwonly()) {
                    return cpp_to_cpp<I>(defaults, std::forward<Values>(values)...);

                } else if constexpr (ArgTraits<T>::kw()) {
                    if constexpr (I < Inner::kw_idx) {
                        return impl::unpack_arg<I>(std::forward<Values>(values)...);
                    } else {
                        if (iter != end) {
                            if constexpr (Inner::template has<name>) {
                                throw TypeError(
                                    "conflicting values for parameter '" + name +
                                    "' at index " + std::to_string(I)
                                );
                            } else {
                                decltype(auto) result = *iter;
                                ++iter;
                                return result;
                            }

                        } else {
                            if constexpr (Inner::template has<name>) {
                                constexpr size_t idx = Inner::template idx<name>;
                                return impl::unpack_arg<idx>(std::forward<Values>(values)...);
                            } else {
                                if constexpr (ArgTraits<T>::opt()) {
                                    return defaults.template get<I>();
                                } else {
                                    throw TypeError(
                                        "no match for parameter '" + name +
                                        "' at index " + std::to_string(I)
                                    );
                                }
                            }
                        }
                    }

                } else if constexpr (ArgTraits<T>::args()) {
                    using Pack = std::vector<type>;  /// TODO: can't store references
                    Pack vec;
                    if constexpr (I < Inner::args_idx) {
                        constexpr size_t diff = Inner::args_idx - I;
                        vec.reserve(diff + args_size);
                        []<size_t... Js>(
                            std::index_sequence<Js...>,
                            Pack& vec,
                            Values&&... args
                        ) {
                            (vec.push_back(
                                impl::unpack_arg<I + Js>(std::forward<Values>(args)...)
                            ), ...);
                        }(
                            std::make_index_sequence<diff>{},
                            vec,
                            std::forward<Values>(values)...
                        );
                        vec.insert(vec.end(), iter, end);
                    }
                    return vec;

                } else if constexpr (ArgTraits<T>::kwargs()) {
                    return cpp_to_cpp<I>(defaults, std::forward<Values>(values)...);

                } else {
                    if constexpr (I < Inner::kw_idx) {
                        return impl::unpack_arg<I>(std::forward<Values>(values)...);
                    } else {
                        if (iter != end) {
                            decltype(auto) result = *iter;
                            ++iter;
                            return result;
                        } else {
                            if constexpr (ArgTraits<T>::opt()) {
                                return defaults.template get<I>();
                            } else {
                                throw TypeError(
                                    "no match for positional-only parameter at index " +
                                    std::to_string(I)
                                );
                            }
                        }
                    }
                }
            }

            template <size_t I, typename Mapping>
            static constexpr ArgTraits<Target<I>>::type cpp_to_cpp(
                const Defaults& defaults,
                const Mapping& map,
                Values&&... values
            ) {
                using T = Target<I>;
                using type = ArgTraits<T>::type;
                constexpr StaticStr name = ArgTraits<T>::name;

                if constexpr (ArgTraits<T>::kwonly()) {
                    auto item = map.find(name);
                    if constexpr (Inner::template has<name>) {
                        if (item != map.end()) {
                            throw TypeError(
                                "conflicting value for parameter '" + name +
                                "' at index " + std::to_string(I)
                            );
                        }
                        constexpr size_t idx = Inner::template idx<name>;
                        return impl::unpack_arg<idx>(std::forward<Values>(values)...);
                    } else {
                        if (item != map.end()) {
                            return item->second;
                        } else {
                            if constexpr (ArgTraits<T>::opt()) {
                                return defaults.template get<I>();
                            } else {
                                throw TypeError(
                                    "no match for parameter '" + name +
                                    "' at index " + std::to_string(I)
                                );
                            }
                        }
                    }

                } else if constexpr (ArgTraits<T>::kw()) {
                    auto item = map.find(name);
                    if constexpr (I < Inner::kw_idx) {
                        if (item != map.end()) {
                            throw TypeError(
                                "conflicting value for parameter '" + name +
                                "' at index " + std::to_string(I)
                            );
                        }
                        return impl::unpack_arg<I>(std::forward<Values>(values)...);
                    } else if constexpr (Inner::template has<name>) {
                        if (item != map.end()) {
                            throw TypeError(
                                "conflicting value for parameter '" + name +
                                "' at index " + std::to_string(I)
                            );
                        }
                        constexpr size_t idx = Inner::template idx<name>;
                        return impl::unpack_arg<idx>(std::forward<Values>(values)...);
                    } else {
                        if (item != map.end()) {
                            return item->second;
                        } else {
                            if constexpr (ArgTraits<T>::opt()) {
                                return defaults.template get<I>();
                            } else {
                                throw TypeError(
                                    "no match for parameter '" + name +
                                    "' at index " + std::to_string(I)
                                );
                            }
                        }
                    }

                } else if constexpr (ArgTraits<T>::args()) {
                    return cpp_to_cpp<I>(defaults, std::forward<Values>(values)...);

                } else if constexpr (ArgTraits<T>::kwargs()) {
                    using Pack = std::unordered_map<std::string, type>;
                    Pack pack;
                    []<size_t... Js>(
                        std::index_sequence<Js...>,
                        Pack& pack,
                        Values&&... values
                    ) {
                        (build_kwargs<Js>(pack, std::forward<Values>(values)...), ...);
                    }(
                        std::make_index_sequence<Inner::n - Inner::kw_idx>{},
                        pack,
                        std::forward<Values>(values)...
                    );
                    for (const auto& [key, value] : map) {
                        if (pack.contains(key)) {
                            throw TypeError(
                                "duplicate value for parameter '" + key + "'"
                            );
                        }
                        pack[key] = value;
                    }
                    return pack;

                } else {
                    return cpp_to_cpp<I>(defaults, std::forward<Values>(values)...);
                }
            }

            template <
                size_t I,
                std::input_iterator Iter,
                std::sentinel_for<Iter> End,
                typename Mapping
            >
            static constexpr ArgTraits<Target<I>>::type cpp_to_cpp(
                const Defaults& defaults,
                size_t args_size,
                Iter& iter,
                const End& end,
                const Mapping& map,
                Values&&... values
            ) {
                using T = Target<I>;
                using type = ArgTraits<T>::type;
                constexpr StaticStr name = ArgTraits<T>::name;

                if constexpr (ArgTraits<T>::kwonly()) {
                    return cpp_to_cpp<I>(
                        defaults,
                        map,
                        std::forward<Values>(values)...
                    );

                } else if constexpr (ArgTraits<T>::kw()) {
                    auto item = map.find(name);
                    if constexpr (I < Inner::kw_idx) {
                        if (item != map.end()) {
                            throw TypeError(
                                "conflicting values for parameter '" + name +
                                "' at index " + std::to_string(I)
                            );
                        }
                        return impl::unpack_arg<I>(std::forward<Values>(values)...);
                    } else {
                        if (iter != end) {
                            if constexpr (Inner::template has<name>) {
                                throw TypeError(
                                    "conflicting values for parameter '" + name +
                                    "' at index " + std::to_string(I)
                                );
                            } else {
                                if (item != map.end()) {
                                    throw TypeError(
                                        "conflicting values for parameter '" + name +
                                        "' at index " + std::to_string(I)
                                    );
                                } else {
                                    decltype(auto) result = *iter;
                                    ++iter;
                                    return result;
                                }
                            }
                        } else {
                            if constexpr (Inner::template has<name>) {
                                if (item != map.end()) {
                                    throw TypeError(
                                        "conflicting values for parameter '" + name +
                                        "' at index " + std::to_string(I)
                                    );
                                } else {
                                    constexpr size_t idx = Inner::template idx<name>;
                                    return impl::unpack_arg<idx>(
                                        std::forward<Values>(values)...
                                    ).value;
                                }
                            } else {
                                if (item != map.end()) {
                                    return item->second;
                                } else {
                                    if constexpr (ArgTraits<T>::opt()) {
                                        return defaults.template get<I>();
                                    } else {
                                        throw TypeError(
                                            "no match for parameter '" + name +
                                            "' at index " + std::to_string(I)
                                        );
                                    }
                                }
                            }
                        }
                    }

                } else if constexpr (ArgTraits<T>::args()) {
                    return cpp_to_cpp<I>(
                        defaults,
                        args_size,
                        iter,
                        end,
                        std::forward<Values>(values)...
                    );

                } else if constexpr (ArgTraits<T>::kwargs()) {
                    return cpp_to_cpp<I>(
                        defaults,
                        map,
                        std::forward<Values>(values)...
                    );

                } else {
                    return cpp_to_cpp<I>(
                        defaults,
                        args_size,
                        iter,
                        end,
                        std::forward<Values>(values)...
                    );
                }
            }

            /* The py_to_cpp() method is used to convert an index sequence over the
             * enclosing signature into the corresponding values pulled from either an
             * array of Python vectorcall arguments or the function's defaults.  This
             * requires us to parse an array of PyObject* pointers with a binary layout
             * that looks something like this:
             *
             *                          ( kwnames tuple )
             *      -------------------------------------
             *      | x | p | p | p |...| k | k | k |...|
             *      -------------------------------------
             *            ^             ^
             *            |             nargs ends here
             *            *args starts here
             *
             * Where 'x' is an optional first element that can be temporarily written to
             * in order to efficiently forward the `self` argument for bound methods,
             * etc.  The presence of this argument is determined by the
             * PY_VECTORCALL_ARGUMENTS_OFFSET flag, which is encoded in nargs.  You can
             * check for its presence by bitwise AND-ing against nargs, and the true
             * number of arguments must be extracted using `PyVectorcall_NARGS(nargs)`
             * to account for this.
             *
             * If PY_VECTORCALL_ARGUMENTS_OFFSET is set and 'x' is written to, then it must
             * always be reset to its original value before the function returns.  This
             * allows for nested forwarding/scoping using the same argument list, with no
             * extra allocations.
             */

            template <size_t I>
            static ArgTraits<Target<I>>::type py_to_cpp(
                const Defaults& defaults,
                PyObject* const* args,
                size_t nargsf,
                PyObject* kwnames,
                size_t kwcount
            ) {
                using T = Target<I>;
                using type = ArgTraits<T>::type;
                constexpr StaticStr name = ArgTraits<T>::name;
                bool has_self = nargsf & PY_VECTORCALL_ARGUMENTS_OFFSET;
                size_t nargs = PyVectorcall_NARGS(nargsf);

                if constexpr (ArgTraits<T>::kwonly()) {
                    if (kwnames != nullptr) {
                        for (size_t i = 0; i < kwcount; ++i) {
                            const char* kwname = PyUnicode_AsUTF8(
                                PyTuple_GET_ITEM(kwnames, i)
                            );
                            if (kwname == nullptr) {
                                Exception::from_python();
                            } else if (std::strcmp(kwname, name) == 0) {
                                return reinterpret_borrow<Object>(args[i]);
                            }
                        }
                    }
                    if constexpr (ArgTraits<T>::opt()) {
                        return defaults.template get<I>();
                    } else {
                        throw TypeError(
                            "missing required keyword-only argument '" + name + "'"
                        );
                    }

                } else if constexpr (ArgTraits<T>::kw()) {
                    if (I < nargs) {
                        return reinterpret_borrow<Object>(args[I]);
                    } else if (kwnames != nullptr) {
                        for (size_t i = 0; i < kwcount; ++i) {
                            const char* kwname = PyUnicode_AsUTF8(
                                PyTuple_GET_ITEM(kwnames, i)
                            );
                            if (kwname == nullptr) {
                                Exception::from_python();
                            } else if (std::strcmp(kwname, name) == 0) {
                                return reinterpret_borrow<Object>(args[nargs + i]);
                            }
                        }
                    }
                    if constexpr (ArgTraits<T>::opt()) {
                        return defaults.template get<I>();
                    } else {
                        throw TypeError(
                            "missing required argument '" + name + "' at index " +
                            std::to_string(I)
                        );
                    }

                } else if constexpr (ArgTraits<T>::args()) {
                    std::vector<type> vec;
                    for (size_t i = I; i < nargs; ++i) {
                        vec.push_back(reinterpret_borrow<Object>(args[i]));
                    }
                    return vec;

                } else if constexpr (ArgTraits<T>::kwargs()) {
                    std::unordered_map<std::string, type> map;
                    if (kwnames != nullptr) {
                        auto sequence = std::make_index_sequence<Outer::n_kw>{};
                        for (size_t i = 0; i < kwcount; ++i) {
                            Py_ssize_t length;
                            const char* kwname = PyUnicode_AsUTF8AndSize(
                                PyTuple_GET_ITEM(kwnames, i),
                                &length
                            );
                            if (kwname == nullptr) {
                                Exception::from_python();
                            } else if (!Outer::callback(kwname)) {
                                map.emplace(
                                    std::string(kwname, length),
                                    reinterpret_borrow<Object>(args[nargs + i])
                                );
                            }
                        }
                    }
                    return map;

                } else {
                    if (I < nargs) {
                        return reinterpret_borrow<Object>(args[I]);
                    }
                    if constexpr (ArgTraits<T>::opt()) {
                        return defaults.template get<I>();
                    } else {
                        throw TypeError(
                            "missing required positional-only argument at index " +
                            std::to_string(I)
                        );
                    }
                }
            }

            /* The cpp_to_py() method is used to allocate a Python vectorcall argument
             * array and populate it according to a C++ argument list.  This is
             * essentially the inverse of the py_to_cpp() method, and requires us to
             * allocate an array with the same binary layout as described above.  That
             * array can then be used to efficiently call a Python function using the
             * vectorcall protocol, which is the fastest possible way to call a Python
             * function from C++.
             */

            template <size_t J>
            static void cpp_to_py(
                PyObject* kwnames,
                PyObject** args,
                Values&&... values
            ) {
                using S = Source<J>;
                using type = ArgTraits<S>::type;
                constexpr StaticStr name = ArgTraits<S>::name;

                if constexpr (ArgTraits<S>::kw()) {
                    try {
                        PyTuple_SET_ITEM(
                            kwnames,
                            J - Inner::kw_idx,
                            Py_NewRef(TemplateString<name>::ptr)
                        );
                        args[J + 1] = release(as_object(
                            impl::unpack_arg<J>(std::forward<Values>(values)...)
                        ));
                    } catch (...) {
                        for (size_t i = 1; i <= J; ++i) {
                            Py_XDECREF(args[i]);
                        }
                    }

                } else if constexpr (ArgTraits<S>::args()) {
                    size_t curr = J + 1;
                    try {
                        const auto& var_args = impl::unpack_arg<J>(
                            std::forward<Values>(values)...
                        );
                        for (const auto& value : var_args) {
                            args[curr] = release(as_object(value));
                            ++curr;
                        }
                    } catch (...) {
                        for (size_t i = 1; i < curr; ++i) {
                            Py_XDECREF(args[i]);
                        }
                    }

                } else if constexpr (ArgTraits<S>::kwargs()) {
                    size_t curr = J + 1;
                    try {
                        const auto& var_kwargs = impl::unpack_arg<J>(
                            std::forward<Values>(values)...
                        );
                        for (const auto& [key, value] : var_kwargs) {
                            PyObject* name = PyUnicode_FromStringAndSize(
                                key.data(),
                                key.size()
                            );
                            if (name == nullptr) {
                                Exception::from_python();
                            }
                            PyTuple_SET_ITEM(kwnames, curr - Inner::kw_idx, name);
                            args[curr] = release(as_object(value));
                            ++curr;
                        }
                    } catch (...) {
                        for (size_t i = 1; i < curr; ++i) {
                            Py_XDECREF(args[i]);
                        }
                    }

                } else {
                    try {
                        args[J + 1] = release(as_object(
                            impl::unpack_arg<J>(std::forward<Values>(values)...)
                        ));
                    } catch (...) {
                        for (size_t i = 1; i <= J; ++i) {
                            Py_XDECREF(args[i]);
                        }
                    }
                }
            }

        public:
            static constexpr size_t n               = sizeof...(Values);
            static constexpr size_t n_pos           = Inner::n_pos;
            static constexpr size_t n_kw            = Inner::n_kw;

            template <StaticStr Name>
            static constexpr bool has               = Inner::template has<Name>;
            static constexpr bool has_pos           = Inner::has_pos;
            static constexpr bool has_args          = Inner::has_args;
            static constexpr bool has_kw            = Inner::has_kw;
            static constexpr bool has_kwargs        = Inner::has_kwargs;

            template <StaticStr Name> requires (has<Name>)
            static constexpr size_t idx             = Inner::template idx<Name>;
            static constexpr size_t args_idx        = Inner::args_idx;
            static constexpr size_t kw_idx          = Inner::kw_idx;
            static constexpr size_t kwargs_idx      = Inner::kwargs_idx;

            template <size_t I> requires (I < n)
            using at = Inner::template at<I>;

            /* Call operator is only enabled if source arguments are well-formed and
            match the target signature. */
            static constexpr bool enable =
                Inner::proper_argument_order &&
                Inner::no_duplicate_arguments &&
                enable_recursive<0, 0>();

            /// TODO: both of these call operators need to be updated to handle the
            /// `self` parameter.

            /* Invoke a C++ function from C++ using Python-style arguments. */
            template <typename Func>
                requires (enable && std::is_invocable_v<Func, Args...>)
            static auto operator()(
                const Defaults& defaults,
                Func&& func,
                Values&&... values
            ) -> std::invoke_result_t<Func, Args...> {
                using Return = std::invoke_result_t<Func, Args...>;
                return []<size_t... Is>(
                    std::index_sequence<Is...>,
                    const Defaults& defaults,
                    Func&& func,
                    Values&&... values
                ) {
                    if constexpr (Inner::has_args && Inner::has_kwargs) {
                        const auto& kwargs = impl::unpack_arg<Inner::kwargs_idx>(
                            std::forward<Values>(values)...
                        );
                        if constexpr (!Outer::has_kwargs) {
                            assert_kwargs_are_recognized<Outer, Inner>(kwargs);
                        }
                        const auto& args = impl::unpack_arg<Inner::args_idx>(
                            std::forward<Values>(values)...
                        );
                        auto iter = std::ranges::begin(args);
                        auto end = std::ranges::end(args);
                        if constexpr (std::is_void_v<Return>) {
                            func({
                                cpp_to_cpp<Is>(
                                    defaults,
                                    std::ranges::size(args),
                                    iter,
                                    end,
                                    kwargs,
                                    std::forward<Values>(values)...
                                )
                            }...);
                            if constexpr (!Outer::has_args) {
                                assert_args_are_exhausted(iter, end);
                            }
                        } else {
                            decltype(auto) result = func({
                                cpp_to_cpp<Is>(
                                    defaults,
                                    std::ranges::size(args),
                                    iter,
                                    end,
                                    kwargs,
                                    std::forward<Values>(values)...
                                )
                            }...);
                            if constexpr (!Outer::has_args) {
                                assert_args_are_exhausted(iter, end);
                            }
                            return result;
                        }

                    // variadic positional arguments are passed as an iterator range, which
                    // must be exhausted after the function call completes
                    } else if constexpr (Inner::has_args) {
                        const auto& args = impl::unpack_arg<Inner::args_idx>(
                            std::forward<Values>(values)...
                        );
                        auto iter = std::ranges::begin(args);
                        auto end = std::ranges::end(args);
                        if constexpr (std::is_void_v<Return>) {
                            func({
                                cpp_to_cpp<Is>(
                                    defaults,
                                    std::ranges::size(args),
                                    iter,
                                    end,
                                    std::forward<Values>(values)...
                                )
                            }...);
                            if constexpr (!Outer::has_args) {
                                assert_args_are_exhausted(iter, end);
                            }
                        } else {
                            decltype(auto) result = func({
                                cpp_to_cpp<Is>(
                                    defaults,
                                    std::ranges::size(args),
                                    iter,
                                    end,
                                    std::forward<Source>(values)...
                                )
                            }...);
                            if constexpr (!Outer::has_args) {
                                assert_args_are_exhausted(iter, end);
                            }
                            return result;
                        }

                    // variadic keyword arguments are passed as a dictionary, which must be
                    // validated up front to ensure all keys are recognized
                    } else if constexpr (Inner::has_kwargs) {
                        const auto& kwargs = impl::unpack_arg<Inner::kwargs_idx>(
                            std::forward<Values>(values)...
                        );
                        if constexpr (!Outer::has_kwargs) {
                            assert_kwargs_are_recognized<Outer, Inner>(kwargs);
                        }
                        return func({
                            cpp_to_cpp<Is>(
                                defaults,
                                kwargs,
                                std::forward<Values>(values)...
                            )
                        }...);

                    // interpose the two if there are both positional and keyword argument packs
                    } else {
                        return func({
                            cpp_to_cpp<Is>(
                                defaults,
                                std::forward<Source>(values)...
                            )
                        }...);
                    }
                }(
                    std::make_index_sequence<Outer::n>{},
                    defaults,
                    std::forward<Func>(func),
                    std::forward<Source>(values)...
                );
            }

            /// TODO: the inclusion of the `self` parameter changes the logic around
            /// calling with no args, one arg, etc.  Perhaps the best thing to do is
            /// to just always use the vectorcall protocol.

            /* Invoke a Python function from C++ using Python-style arguments.  This
            will always return a new reference to a raw Python object, or throw a
            runtime error if the arguments are malformed in some way. */
            template <typename = void> requires (enable)
            static PyObject* operator()(
                PyObject* func,
                Values&&... values
            ) {
                return []<size_t... Is>(
                    std::index_sequence<Is...>,
                    PyObject* func,
                    Values&&... values
                ) {
                    PyObject* result;

                    // if there are no arguments, we can use the no-args protocol
                    if constexpr (Inner::n == 0) {
                        result = PyObject_CallNoArgs(func);

                    // if there are no variadic arguments, we can stack allocate the argument
                    // array with a fixed size
                    } else if constexpr (!Inner::has_args && !Inner::has_kwargs) {
                        // if there is only one argument, we can use the one-arg protocol
                        if constexpr (Inner::n == 1) {
                            if constexpr (Inner::has_kw) {
                                result = PyObject_CallOneArg(
                                    func,
                                    ptr(as_object(
                                        impl::unpack_arg<0>(
                                            std::forward<Values>(values)...
                                        ).value
                                    ))
                                );
                            } else {
                                result = PyObject_CallOneArg(
                                    func,
                                    ptr(as_object(
                                        impl::unpack_arg<0>(
                                            std::forward<Values>(values)...
                                        )
                                    ))
                                );
                            }

                        // if there is more than one argument, we construct a vectorcall
                        // argument array
                        } else {
                            PyObject* array[Inner::n + 1];
                            array[0] = nullptr;
                            PyObject* kwnames;
                            if constexpr (Inner::has_kw) {
                                kwnames = PyTuple_New(Inner::n_kw);
                            } else {
                                kwnames = nullptr;
                            }
                            (
                                cpp_to_python<Is>(
                                    kwnames,
                                    array,  // 1-indexed
                                    std::forward<Values>(values)...
                                ),
                                ...
                            );
                            Py_ssize_t npos = Inner::n - Inner::n_kw;
                            result = PyObject_Vectorcall(
                                func,
                                array,
                                npos | PY_VECTORCALL_ARGUMENTS_OFFSET,
                                kwnames
                            );
                            for (size_t i = 1; i <= Inner::n; ++i) {
                                Py_XDECREF(array[i]);  // release all argument references
                            }
                        }

                    // otherwise, we have to heap-allocate the array with a variable size
                    } else if constexpr (Inner::has_args && !Inner::has_kwargs) {
                        const auto& args = impl::unpack_arg<Inner::args_idx>(
                            std::forward<Values>(values)...
                        );
                        size_t nargs = Inner::n - 1 + std::ranges::size(args);
                        PyObject** array = new PyObject*[nargs + 1];
                        array[0] = nullptr;
                        PyObject* kwnames;
                        if constexpr (Inner::has_kw) {
                            kwnames = PyTuple_New(Inner::n_kw);
                        } else {
                            kwnames = nullptr;
                        }
                        (
                            cpp_to_python<Is>(
                                kwnames,
                                array,
                                std::forward<Values>(values)...
                            ),
                            ...
                        );
                        Py_ssize_t npos = Inner::n - 1 - Inner::n_kw + std::ranges::size(args);
                        result = PyObject_Vectorcall(
                            func,
                            array,
                            npos | PY_VECTORCALL_ARGUMENTS_OFFSET,
                            nullptr
                        );
                        for (size_t i = 1; i <= Inner::n; ++i) {
                            Py_XDECREF(array[i]);
                        }
                        delete[] array;

                    // The following specializations handle the cross product of
                    // positional/keyword parameter packs which differ only in initialization
                    } else if constexpr (!Inner::has_args && Inner::has_kwargs) {
                        const auto& kwargs = impl::unpack_arg<Inner::kwargs_idx>(
                            std::forward<Values>(values)...
                        );
                        size_t nargs = Inner::n - 1 + std::ranges::size(kwargs);
                        PyObject** array = new PyObject*[nargs + 1];
                        array[0] = nullptr;
                        PyObject* kwnames = PyTuple_New(Inner::n_kw + std::ranges::size(kwargs));
                        (
                            cpp_to_python<Is>(
                                kwnames,
                                array,
                                std::forward<Values>(values)...
                            ),
                            ...
                        );
                        Py_ssize_t npos = Inner::n - 1 - Inner::n_kw;
                        result = PyObject_Vectorcall(
                            func,
                            array,
                            npos | PY_VECTORCALL_ARGUMENTS_OFFSET,
                            kwnames
                        );
                        for (size_t i = 1; i <= Inner::n; ++i) {
                            Py_XDECREF(array[i]);
                        }
                        delete[] array;

                    } else {
                        const auto& args = impl::unpack_arg<Inner::args_idx>(
                            std::forward<Values>(values)...
                        );
                        const auto& kwargs = impl::unpack_arg<Inner::kwargs_idx>(
                            std::forward<Values>(values)...
                        );
                        size_t nargs = Inner::n - 2 + std::ranges::size(args) + std::ranges::size(kwargs);
                        PyObject** array = new PyObject*[nargs + 1];
                        array[0] = nullptr;
                        PyObject* kwnames = PyTuple_New(Inner::n_kw + std::ranges::size(kwargs));
                        (
                            cpp_to_python<Is>(
                                kwnames,
                                array,
                                std::forward<Values>(values)...
                            ),
                            ...
                        );
                        size_t npos = Inner::n - 2 - Inner::n_kw + std::ranges::size(args);
                        result = PyObject_Vectorcall(
                            func,
                            array,
                            npos | PY_VECTORCALL_ARGUMENTS_OFFSET,
                            kwnames
                        );
                        for (size_t i = 1; i <= Inner::n; ++i) {
                            Py_XDECREF(array[i]);
                        }
                        delete[] array;
                    }

                    // A null return value indicates an error
                    if (result == nullptr) {
                        Exception::from_python();
                    }
                    return result;  // will be None if the function returns void
                }(
                    std::make_index_sequence<Outer::n>{},
                    func,
                    std::forward<Values>(values)...
                );
            }

        };

    };

    /* Convert a non-member function pointer into a member function pointer of the
    given, cvref-qualified type.  Passing void as the enclosing class will return the
    non-member function pointer as-is. */
    template <typename Func, typename Self>
    struct as_member_func {
        static constexpr bool enable = false;
    };
    template <typename R, typename... A, typename Self>
        requires (std::is_void_v<std::remove_cvref_t<Self>>)
    struct as_member_func<R(*)(A...), Self> {
        static constexpr bool enable = true;
        using type = R(*)(A...);
    };
    template <typename R, typename... A, typename Self>
        requires (std::is_void_v<std::remove_cvref_t<Self>>)
    struct as_member_func<R(*)(A...) noexcept, Self> {
        static constexpr bool enable = true;
        using type = R(*)(A...) noexcept;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...), Self> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...);
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...) noexcept, Self> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) noexcept;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...), Self&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) &;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...) noexcept, Self&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) & noexcept;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...), Self&&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) &&;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...) noexcept, Self&&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) && noexcept;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...), const Self> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) const;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...) noexcept, const Self> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) const noexcept;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...), const Self&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) const &;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...) noexcept, const Self&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) const & noexcept;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...), const Self&&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) const &&;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...) noexcept, const Self&&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) const && noexcept;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...), volatile Self> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) volatile;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...) noexcept, volatile Self> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) volatile noexcept;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...), volatile Self&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) volatile &;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...) noexcept, volatile Self&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) volatile & noexcept;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...), volatile Self&&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) volatile &&;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...) noexcept, volatile Self&&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) volatile && noexcept;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...), const volatile Self> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) const volatile;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...) noexcept, const volatile Self> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) const volatile noexcept;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...), const volatile Self&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) const volatile &;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...) noexcept, const volatile Self&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) const volatile & noexcept;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...), const volatile Self&&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) const volatile &&;
    };
    template <typename R, typename... A, typename Self>
    struct as_member_func<R(*)(A...) noexcept, const volatile Self&&> {
        static constexpr bool enable = true;
        using type = R(std::remove_cvref_t<Self>::*)(A...) const volatile && noexcept;
    };

    /* Introspect the proper signature for a py::Function instance from a generic
    function pointer. */
    template <typename T>
    struct Signature : BertrandTag {
        using type = T;
        static constexpr bool enable = false;
    };
    template <typename R, typename... A>
    struct Signature<R(*)(A...)> : Arguments<A...> {
        using type = R(*)(A...);
        static constexpr bool enable = true;
        static constexpr bool has_self = false;
        static constexpr bool return_is_convertible_to_python =
            std::convertible_to<R, Object>;
        template <typename T>
        static constexpr bool can_make_member = as_member_func<R(*)(A...), T>::enable;
        using Return = R;
        using Self = void;
        using to_ptr = Signature;
        using to_value = Signature<R(A...)>;
        template <typename R2>
        using with_return = Signature<R2(*)(A...)>;
        template <typename C> requires (can_make_member<C>)
        using with_self = Signature<typename as_member_func<R(*)(A...), C>::type>;
        template <typename... A2>
        using with_args = Signature<R(*)(A2...)>;
        template <typename R2, typename... A2>
        static constexpr bool compatible =
            std::convertible_to<R2, R> &&
            (Arguments<A...>::template compatible<A2> && ...);
        template <typename Func>
        static constexpr bool invocable = std::is_invocable_r_v<Func, R, A...>;
    };
    template <typename R, typename... A>
    struct Signature<R(*)(A...) noexcept> : Signature<R(*)(A...)> {};
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...)> : Arguments<C&, A...> {
        using type = R(C::*)(A...);
        static constexpr bool enable = true;
        static constexpr bool has_self = true;
        static constexpr bool return_is_convertible_to_python =
            std::convertible_to<R, Object>;
        template <typename T>
        static constexpr bool can_make_member = as_member_func<R(*)(A...), T>::enable;
        using Return = R;
        using Self = C&;
        using to_ptr = Signature<R(*)(Self, A...)>;
        using to_value = Signature<R(Self, A...)>;
        template <typename R2>
        using with_return = Signature<R2(C::*)(A...)>;
        template <typename C2> requires (can_make_member<C2>)
        using with_self = Signature<typename as_member_func<R(*)(A...), C2>::type>;
        template <typename... A2>
        using with_args = Signature<R(C::*)(A2...)>;
        template <typename R2, typename... A2>
        static constexpr bool compatible =
            std::convertible_to<R2, R> &&
            (Arguments<Self, A...>::template compatible<A2> && ...);
        template <typename Func>
        static constexpr bool invocable = std::is_invocable_r_v<Func, R, Self, A...>;
    };
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) noexcept> : Signature<R(C::*)(A...)> {};
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) &> : Signature<R(C::*)(A...)> {};
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) & noexcept> : Signature<R(C::*)(A...)> {};
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) const> : Arguments<const C&, A...> {
        using type = R(C::*)(A...) const;
        static constexpr bool enable = true;
        static constexpr bool has_self = true;
        static constexpr bool return_is_convertible_to_python =
            std::convertible_to<R, Object>;
        template <typename T>
        static constexpr bool can_make_member = as_member_func<R(*)(A...), T>::enable;
        using Return = R;
        using Self = const C&;
        using to_ptr = Signature<R(*)(Self, A...)>;
        using to_value = Signature<R(Self, A...)>;
        template <typename R2>
        using with_return = Signature<R2(C::*)(A...) const>;
        template <typename C2> requires (can_make_member<C2>)
        using with_self = Signature<typename as_member_func<R(*)(A...), C2>::type>;
        template <typename... A2>
        using with_args = Signature<R(C::*)(A2...) const>;
        template <typename R2, typename... A2>
        static constexpr bool compatible =
            std::convertible_to<R2, R> &&
            (Arguments<Self, A...>::template compatible<A2> && ...);
        template <typename Func>
        static constexpr bool invocable = std::is_invocable_r_v<Func, R, Self, A...>;
    };
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) const noexcept> : Signature<R(C::*)(A...) const> {};
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) const &> : Signature<R(C::*)(A...) const> {};
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) const & noexcept> : Signature<R(C::*)(A...) const> {};
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) volatile> : Arguments<volatile C&, A...> {
        using type = R(C::*)(A...) volatile;
        static constexpr bool enable = true;
        static constexpr bool has_self = true;
        static constexpr bool return_is_convertible_to_python =
            std::convertible_to<R, Object>;
        template <typename T>
        static constexpr bool can_make_member = as_member_func<R(*)(A...), T>::enable;
        using Return = R;
        using Self = volatile C&;
        using to_ptr = Signature<R(*)(Self, A...)>;
        using to_value = Signature<R(Self, A...)>;
        template <typename R2>
        using with_return = Signature<R2(C::*)(A...) volatile>;
        template <typename C2> requires (can_make_member<C2>)
        using with_self = Signature<typename as_member_func<R(*)(A...), C2>::type>;
        template <typename... A2>
        using with_args = Signature<R(C::*)(A2...) volatile>;
        template <typename R2, typename... A2>
        static constexpr bool compatible =
            std::convertible_to<R2, R> &&
            (Arguments<Self, A...>::template compatible<A2> && ...);
        template <typename Func>
        static constexpr bool invocable = std::is_invocable_r_v<Func, R, Self, A...>;
    };
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) volatile noexcept> : Signature<R(C::*)(A...) volatile> {};
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) volatile &> : Signature<R(C::*)(A...) volatile> {};
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) volatile & noexcept> : Signature<R(C::*)(A...) volatile> {};
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) const volatile> : Arguments<const volatile C&, A...> {
        using type = R(C::*)(A...) const volatile;
        static constexpr bool enable = true;
        static constexpr bool has_self = true;
        static constexpr bool return_is_convertible_to_python =
            std::convertible_to<R, Object>;
        template <typename T>
        static constexpr bool can_make_member = as_member_func<R(*)(A...), T>::enable;
        using Return = R;
        using Self = const volatile C&;
        using to_ptr = Signature<R(*)(Self, A...)>;
        using to_value = Signature<R(Self, A...)>;
        template <typename R2>
        using with_return = Signature<R2(C::*)(A...) const volatile>;
        template <typename C2> requires (can_make_member<C2>)
        using with_self = Signature<typename as_member_func<R(*)(A...), C2>::type>;
        template <typename... A2>
        using with_args = Signature<R(C::*)(A2...) const volatile>;
        template <typename R2, typename... A2>
        static constexpr bool compatible =
            std::convertible_to<R2, R> &&
            (Arguments<Self, A...>::template compatible<A2> && ...);
        template <typename Func>
        static constexpr bool invocable = std::is_invocable_r_v<Func, R, Self, A...>;
    };
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) const volatile noexcept> : Signature<R(C::*)(A...) const volatile> {};
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) const volatile &> : Signature<R(C::*)(A...) const volatile> {};
    template <typename R, typename C, typename... A>
    struct Signature<R(C::*)(A...) const volatile & noexcept> : Signature<R(C::*)(A...) const volatile> {}; 

    template <typename T>
    concept function_pointer_like = Signature<T>::enable;
    template <typename T>
    concept args_fit_within_bitset = Signature<T>::n <= 64;
    template <typename T>
    concept args_are_convertible_to_python = Signature<T>::args_are_convertible_to_python;
    template <typename T>
    concept return_is_convertible_to_python = Signature<T>::return_is_convertible_to_python;
    template <typename T>
    concept proper_argument_order = Signature<T>::proper_argument_order;
    template <typename T>
    concept no_duplicate_arguments = Signature<T>::no_duplicate_arguments;
    template <typename T>
    concept no_required_after_default = Signature<T>::no_required_after_default;

    /// TODO: Maybe the Python side of the function classes should be defined after
    /// types, so that everything can use the same `BertrandMeta` metaclass with a
    /// single, unified interface.  Every type would therefore support the same level
    /// of function overloading/custom behavior as everything else, and checks would
    /// be really easy to implement.  The intervening logic, however, will not be easy.
    /// -> That might ONLY be necessary for the __export__ function.  Otherwise, I
    /// should define as much as I possibly can here.  I just won't need anything
    /// involving metaclasses.

    /// TODO: functions might still need a custom metaclass in order to apply the
    /// correct key resolution, but that can be a subclass of BertrandMeta, or that
    /// can be somehow inserted using some kind of function pointer, or something.

    /* A special metaclass for all functions, which enables class-level subscription
    to navigate the C++ template hierarchy in a manner similar to `typing.Callable`. */
    struct FunctionMeta : PyTypeObject {
        static constexpr StaticStr __doc__ =
R"doc(A common metaclass for all `py::Function` types, which allows class-level
subscription of the C++ template hierarchy.

Notes
-----
This metaclass is not part of the public interface, but is necessary to form
`typing.Callable`-like annotations for C++ functions, which require special syntax to
properly encode full type information.)doc";

        static PyTypeObject __type__;

        Object templates = reinterpret_steal<Object>(nullptr);

        /// TODO: this getitem operator must account for the leading return type +
        /// member class, using `cls::type` or just `type` syntax.

        /* Subscript a function type to navigate its template hierarchy. */
        static PyObject* __getitem__(FunctionMeta* cls, PyObject* specifier) {
            if (PyTuple_Check(specifier)) {
                Py_INCREF(specifier);
            } else {
                specifier = PyTuple_Pack(1, specifier);
                if (specifier == nullptr) {
                    return nullptr;
                }
            }
            Object key = reinterpret_steal<Object>(specifier);
            try {
                Params<std::vector<Param>> params = resolve(
                    key,
                    fnv1a_hash_seed,
                    fnv1a_hash_prime
                );
                Object hash = reinterpret_steal<Object>(PyLong_FromSize_t(
                    params.hash
                ));
                if (hash.is(nullptr)) {
                    return nullptr;
                }
                Object value = reinterpret_steal<Object>(PyDict_GetItemWithError(
                    ptr(cls->templates),
                    ptr(hash)
                ));
                if (value.is(nullptr)) {
                    if (PyErr_Occurred()) {
                        return nullptr;
                    }
                    std::string message =
                        "class template has not been instantiated: Function[";
                    Object repr = reinterpret_steal<Object>(
                        PyObject_Repr(ptr(key))
                    );
                    if (repr.is(nullptr)) {
                        return nullptr;
                    }
                    Py_ssize_t len;
                    const char* data = PyUnicode_AsUTF8AndSize(
                        ptr(repr),
                        &len
                    );
                    if (data == nullptr) {
                        return nullptr;
                    }
                    // strip leading/trailing ()
                    message += std::string(data + 1, len - 2) + "]";
                    PyErr_SetString(PyExc_TypeError, message.c_str());
                    return nullptr;
                }
                return release(value);
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        /* `len(Function)` gives the total number of template instantiations that can
        be reached by subscripting this type. */
        static Py_ssize_t __len__(FunctionMeta* cls) {
            return cls->templates.is(nullptr) ?
                0 : PyDict_Size(ptr(cls->templates));
        }

        /* `iter(Function)` yields template instantiations of this type in the order
        in which they were registered. */
        static PyObject* __iter__(FunctionMeta* cls) {
            if (cls->templates.is(nullptr)) {
                PyErr_SetNone(PyExc_StopIteration);
                return nullptr;
            }
            Object values = reinterpret_steal<Object>(PyObject_CallMethodNoArgs(
                ptr(cls->templates),
                TemplateString<"values">::ptr
            ));
            if (values.is(nullptr)) {
                return nullptr;
            }
            return PyObject_GetIter(ptr(values));
        }

        /// TODO: maybe the `in` operator should always check for convertibility to the
        /// given type, rather than strict subclass relationships, or template
        /// instantiations.

        /* `x in Function` returns true if `x` is a type or instance of a type that can
        be converted to the given `Function` specialization. */
        static int __contains__(FunctionMeta* cls, PyObject* item) {
            PyTypeObject* type = PyType_Check(item) ?
                reinterpret_cast<PyTypeObject*>(item) :
                Py_TYPE(item);
    
            /// TODO: convertibility will be rather complex to implement.  This
            /// basically constrains the construction of the overall `bertrand`
            /// module, since this will necessarily require global configuration.
            /// -> probably involves looking up the type in the global map, and then
            /// doing an issubclass check on the type object.

            return PyType_IsSubtype(type, reinterpret_cast<PyTypeObject*>(cls));
        }

        /* repr(Function) demangles the C++ type name in diagnostics. */
        static PyObject* __repr__(FunctionMeta* cls) {
            std::string name = "<class '" + demangle(cls->tp_name) + "'>";
            return PyUnicode_FromStringAndSize(name.c_str(), name.size());
        }

    private:

        /// TODO: I can insert the seed and prime as metaclass members, so there's no
        /// need to pass them here, and we can use the same consistent hash.

        /* Parse a Python-style parameter list into a C++ template signature.  An
        error will be raised if the parameter list is malformed, applying the same
        restrictions as a standard Python function definition. */
        static Params<std::vector<Param>> resolve(
            const Object& specifier,
            size_t seed,
            size_t prime
        ) {
            int size = PyList_GET_SIZE(ptr(specifier));
            std::vector<Param> key;
            key.reserve(size);
            std::unordered_set<std::string_view> names;

            int posonly_idx = -1;
            int kwonly_idx = -1;
            int args_idx = -1;
            int kwargs_idx = -1;
            int default_idx = -1;
            for (int i = 0; i < size; ++i) {
                PyObject* item = PyList_GET_ITEM(ptr(specifier), i);

                // raw strings represent either a '/' or '*' delimiter, or an argument
                // name with a dynamic type, following Python syntax
                if (PyUnicode_Check(item)) {
                    Py_ssize_t len;
                    const char* data = PyUnicode_AsUTF8AndSize(
                        item,
                        &len
                    );
                    if (data == nullptr) {
                        Exception::from_python();
                    }
                    std::string_view name {data, static_cast<size_t>(len)};

                    // positional-only delimiter
                    if (name == "/") {
                        if (i == 0) {
                            throw TypeError(
                                "at least one argument must precede positional-only "
                                "argument delimiter '/'"
                            );
                        } else if (posonly_idx != -1) {
                            throw TypeError(
                                "positional-only argument delimiter '/' appears at "
                                "multiple indices: " + std::to_string(posonly_idx) +
                                " and " + std::to_string(i)
                            );
                        } else if (args_idx != -1 && i > args_idx) {
                            throw TypeError(
                                "positional-only argument delimiter '/' cannot follow "
                                "variadic positional arguments"
                            );
                        } else if (kwonly_idx != -1 && i > kwonly_idx) {
                            throw TypeError(
                                "positional-only argument delimiter '/' cannot follow "
                                "keyword-only argument delimiter '*': " +
                                repr(specifier)
                            );
                        } else if (kwargs_idx != -1 && i > kwargs_idx) {
                            throw TypeError(
                                "positional-only argument delimiter '/' cannot follow "
                                "variadic keyword arguments"
                            );
                        }
                        posonly_idx = i;

                    // keyword-only delimiter
                    } else if (name == "*") {
                        if (kwonly_idx != -1) {
                            throw TypeError(
                                "keyword-only argument delimiter '*' appears at "
                                "multiple indices: " + std::to_string(kwonly_idx) +
                                " and " + std::to_string(i)
                            );
                        } else if (kwargs_idx != -1 && i > kwargs_idx) {
                            throw TypeError(
                                "keyword-only argument delimiter '*' cannot follow "
                                "variadic keyword arguments"
                            );
                        }
                        kwonly_idx = i;

                    // named argument
                    } else {
                        name = Param::get_name(name);

                        // variadic keyword arguments
                        if (name.starts_with("**")) {
                            if (kwargs_idx != -1) {
                                throw TypeError(
                                    "variadic keyword arguments appear at multiple "
                                    "indices: " + std::to_string(kwargs_idx) +
                                    " and " + std::to_string(i)
                                );
                            }
                            name.remove_prefix(2);
                            if (names.contains(name)) {
                                throw TypeError(
                                    "duplicate argument name: '" + std::string(name) +
                                    "'"
                                );
                            }
                            kwargs_idx = i;
                            names.insert(name);
                            key.push_back(
                                {
                                    name,
                                    item,
                                    ArgKind::KW | ArgKind::VARIADIC
                                }
                            );

                        // variadic positional arguments
                        } else if (name.starts_with("*")) {
                            if (args_idx != -1) {
                                throw TypeError(
                                    "variadic positional arguments appear at multiple "
                                    "indices: " + std::to_string(args_idx) +
                                    " and " + std::to_string(i)
                                );
                            } else if (kwonly_idx != -1 && i > kwonly_idx) {
                                throw TypeError(
                                    "variadic positional arguments cannot follow "
                                    "keyword-only argument delimiter '*': " +
                                    repr(specifier)
                                );
                            } else if (kwargs_idx != -1 && i > kwargs_idx) {
                                throw TypeError(
                                    "variadic positional arguments cannot follow "
                                    "variadic keyword arguments"
                                );
                            }
                            name.remove_prefix(1);
                            if (names.contains(name)) {
                                throw TypeError(
                                    "duplicate argument name: '" + std::string(name) +
                                    "'"
                                );
                            }
                            args_idx = i;
                            names.insert(name);
                            key.push_back(
                                {
                                    name,
                                    item,
                                    ArgKind::POS | ArgKind::VARIADIC
                                }
                            );

                        } else if (names.contains(name)) {
                            throw TypeError(
                                "duplicate argument name: '" + std::string(name) + "'"
                            );

                        // keyword-only argument
                        } else if (i > kwonly_idx) {
                            if (kwargs_idx != -1 && i > kwargs_idx) {
                                throw TypeError(
                                    "keyword-only argument cannot follow variadic "
                                    "keyword arguments: " + repr(
                                        reinterpret_borrow<Object>(item)
                                    )
                                );
                            }
                            names.insert(name);
                            key.push_back(
                                {
                                    name,
                                    item,
                                    ArgKind::KW
                                }
                            );

                        // positional-or-keyword argument
                        } else {
                            /// NOTE: because we're forward iterating over the
                            /// tuple, we can't distinguish positional-only
                            /// arguments from positional-or-keyword arguments at
                            /// this point.  We have to do that in a separate loop,
                            /// while we're building the hash
                            if (kwargs_idx != -1 && i > kwargs_idx) {
                                throw TypeError(
                                    "positional-or-keyword argument cannot follow "
                                    "variadic keyword arguments: " + repr(
                                        reinterpret_borrow<Object>(item)
                                    )
                                );
                            }
                            if (default_idx != -1 && i > default_idx) {
                                throw TypeError(
                                    "required positional-or-keyword argument cannot "
                                    "follow argument with a default value: " + repr(
                                        reinterpret_borrow<Object>(item)
                                    )
                                );
                            }
                            names.insert(name);
                            key.push_back(
                                {
                                    name,
                                    item,
                                    ArgKind::POS | ArgKind::KW
                                }
                            );
                        }
                    }

                // slices are used to annotate arguments with type constraints and/or
                // default values
                } else if (PySlice_Check(item)) {
                    PySliceObject* slice = reinterpret_cast<PySliceObject*>(item);

                    // a string first element signifies a named argument
                    if (PyUnicode_Check(slice->start)) {
                        std::string_view name = Param::get_name(slice->start);

                        // variadic keyword arguments
                        if (name.starts_with("**")) {
                            if (kwargs_idx != -1) {
                                throw TypeError(
                                    "variadic keyword arguments appear at multiple "
                                    "indices: " + std::to_string(kwargs_idx) +
                                    " and " + std::to_string(i)
                                );
                            } else if (slice->step != Py_None) {
                                throw TypeError(
                                    "variadic keyword argument cannot have default "
                                    "values: " + repr(specifier)
                                );
                            }
                            name.remove_prefix(2);
                            if (names.contains(name)) {
                                throw TypeError(
                                    "duplicate argument name: '" + std::string(name) +
                                    "'"
                                );
                            }
                            kwargs_idx = i;
                            names.insert(name);
                            key.push_back(
                                {
                                    name,
                                    slice->start,
                                    ArgKind::KW | ArgKind::VARIADIC
                                }
                            );

                        // variadic positional arguments
                        } else if (name.starts_with("*")) {
                            if (args_idx != -1) {
                                throw TypeError(
                                    "variadic positional arguments appear at multiple "
                                    "indices: " + std::to_string(args_idx) +
                                    " and " + std::to_string(i)
                                );
                            } else if (kwonly_idx != -1 && i > kwonly_idx) {
                                throw TypeError(
                                    "variadic positional arguments cannot follow "
                                    "keyword-only argument delimiter '*': " +
                                    repr(specifier)
                                );
                            } else if (kwargs_idx != -1 && i > kwargs_idx) {
                                throw TypeError(
                                    "variadic positional arguments cannot follow "
                                    "variadic keyword arguments"
                                );
                            } else if (slice->step != Py_None) {
                                throw TypeError(
                                    "variadic positional argument cannot have default "
                                    "values: " + repr(specifier)
                                );
                            }
                            name.remove_prefix(1);
                            if (names.contains(name)) {
                                throw TypeError(
                                    "duplicate argument name: '" + std::string(name) +
                                    "'"
                                );
                            }
                            args_idx = i;
                            names.insert(name);
                            key.push_back(
                                {
                                    name,
                                    slice->start,
                                    ArgKind::POS | ArgKind::VARIADIC
                                }
                            );

                        } else if (names.contains(name)) {
                            throw TypeError(
                                "duplicate argument name: '" + std::string(name) + "'"
                            );

                        // keyword-only argument
                        } else if (i > kwonly_idx) {
                            if (kwargs_idx != -1 && i > kwargs_idx) {
                                throw TypeError(
                                    "keyword-only argument cannot follow variadic "
                                    "keyword arguments: " + repr(specifier)
                                );
                            }
                            if (slice->step == Py_Ellipsis) {
                                key.push_back(
                                    {
                                        name,
                                        slice->start,
                                        ArgKind::KW | ArgKind::OPT
                                    }
                                );
                            } else if (slice->step != Py_None) {
                                throw TypeError(
                                    "keyword-only argument with a default value must "
                                    "have an ellipsis as the third element of the "
                                    "slice: " + repr(specifier)
                                );
                            } else {
                                key.push_back(
                                    {
                                        name,
                                        slice->start,
                                        ArgKind::KW
                                    }
                                );
                            }
                            names.insert(name);

                        // positional-or-keyword argument
                        } else {
                            if (kwargs_idx != -1 && i > kwargs_idx) {
                                throw TypeError(
                                    "positional-or-keyword argument cannot follow "
                                    "variadic keyword arguments: " + repr(specifier)
                                );
                            } else if (default_idx != -1 && i > default_idx) {
                                throw TypeError(
                                    "required positional-or-keyword argument cannot "
                                    "follow argument with a default value: " + repr(
                                        reinterpret_borrow<Object>(item)
                                    )
                                );
                            } else if (slice->step == Py_Ellipsis) {
                                key.push_back(
                                    {
                                        name,
                                        slice->start,
                                        ArgKind::POS | ArgKind::KW | ArgKind::OPT
                                    }
                                );
                            } else if (slice->step != Py_None) {
                                throw TypeError(
                                    "positional-or-keyword argument with a default "
                                    "value must have an ellipsis as the third element "
                                    "of the slice: " + repr(specifier)
                                );
                            } else {
                                key.push_back(
                                    {
                                        name,
                                        slice->start,
                                        ArgKind::POS | ArgKind::KW
                                    }
                                );

                            }
                            names.insert(name);
                        }

                    // otherwise, the argument must list an ellipsis as the sole second
                    // item, signifying an anonymous, positional-only, optional argument
                    } else {
                        if (i > posonly_idx) {
                            throw TypeError(
                                "positional-only argument cannot follow `/` "
                                "delimiter: " + repr(specifier)
                            );
                        } else if (i > kwonly_idx) {
                            throw TypeError(
                                "positional-only argument cannot follow '*' "
                                "delimiter: " + repr(specifier)
                            );
                        } else if (i > args_idx) {
                            throw TypeError(
                                "positional-only argument cannot follow variadic "
                                "positional arguments " + repr(specifier)
                            );
                        } else if (slice->stop != Py_Ellipsis) {
                            throw TypeError(
                                "positional-only argument with a default value must "
                                "have an ellipsis as the second element of the "
                                "slice: " + repr(specifier)
                            );
                        } else if (slice->step != Py_None) {
                            throw TypeError(
                                "positional-only argument with a default value must "
                                "not have a third element: " + repr(specifier)
                            );
                        }
                        key.push_back(
                            {
                                "",
                                slice->start,
                                ArgKind::POS | ArgKind::OPT
                            }
                        );
                    }

                // all other objects are passed along as-is and interpreted as
                // positional-only arguments.  Generally, these will be type objects,
                // but they can technically be anything that implements `issubclass()`.
                } else {
                    if (i > posonly_idx) {
                        throw TypeError(
                            "positional-only argument cannot follow `/` delimiter: " +
                            repr(specifier)
                        );
                    } else if (i > kwonly_idx) {
                        throw TypeError(
                            "positional-only argument cannot follow '*' delimiter: " +
                            repr(specifier)
                        );
                    } else if (i > args_idx) {
                        throw TypeError(
                            "positional-only argument cannot follow variadic "
                            "positional arguments " + repr(specifier)
                        );
                    } else if (i > default_idx) {
                        throw TypeError(
                            "parameter without a default value cannot follow a "
                            "parameter with a default value: " + repr(specifier)
                        );
                    }
                    key.push_back({"", item, ArgKind::POS});
                }
            }

            size_t hash = 0;
            for (int i = 0, end = key.size(); i < end; ++i) {
                Param& param = key[i];
                if (i < posonly_idx) {
                    param.kind = param.kind & ~ArgKind::KW;
                }
                hash = hash_combine(hash, param.hash(seed, prime));
            }
            return {std::move(key), hash};
        }

    };

    /// TODO: I need a static metaclass that serves as the template interface for
    /// all function types, and which implements __class_getitem__ to allow for
    /// similar class subscription syntax as for Python callables:
    ///
    /// Function[Foo::None, bool, "x": int, "/", "y", "*args": float, "*", "z": str: ..., "**kwargs": type]
    /// 
    /// This describes a bound method of Foo which returns void and takes a bool and
    /// an int as positional parameters, the second of which is explicitly named "x"
    /// (although this is ignored in the final key as a normalization).  It then
    /// takes a positional-or-keyword argument named "y" with any type, followed by
    /// an arbitrary number of positional arguments of type float.  Finally, it takes
    /// a keyword-only argument named "z" with a default value, and then
    /// an arbitrary number of keyword arguments pointing to type objects.

    /// TODO: one of these signatures is going to have to be generated for each
    /// function type, and must include at least a return type and parameter list,
    /// which may be Function[None, []] for a void function with no arguments.
    /// TODO: there has to also be some complex logic to make sure that type
    /// identifiers are compatible, possibly requiring an overload of isinstance()/
    /// issubclass() to ensure the same semantics are followed in both Python and C++.

    /* A structural type hint whose `isinstance()`/`issubclass()` operators only return
    true if ALL of the composite functions are present within a type's interface. */
    struct FuncIntersect : PyObject {
        Object lhs;
        Object rhs;

        explicit FuncIntersect(Object&& lhs, Object&& rhs) :
            lhs(std::move(lhs)),
            rhs(std::move(rhs))
        {}

        static void __dealloc__(FuncIntersect* self) {
            self->~FuncIntersect();
        }

        static PyObject* __instancecheck__(FuncIntersect* self, PyObject* instance) {
            int rc = PyObject_IsInstance(
                instance,
                ptr(self->lhs)
            );
            if (rc < 0) {
                return nullptr;
            } else if (!rc) {
                Py_RETURN_FALSE;
            }
            rc = PyObject_IsInstance(
                instance,
                ptr(self->rhs)
            );
            if (rc < 0) {
                return nullptr;
            } else if (!rc) {
                Py_RETURN_FALSE;
            }
            Py_RETURN_TRUE;
        }

        static PyObject* __subclasscheck__(FuncIntersect* self, PyObject* cls) {
            int rc = PyObject_IsSubclass(
                cls,
                ptr(self->lhs)
            );
            if (rc < 0) {
                return nullptr;
            } else if (!rc) {
                Py_RETURN_FALSE;
            }
            rc = PyObject_IsSubclass(
                cls,
                ptr(self->rhs)
            );
            if (rc < 0) {
                return nullptr;
            } else if (!rc) {
                Py_RETURN_FALSE;
            }
            Py_RETURN_TRUE;
        }

        static PyObject* __and__(PyObject* lhs, PyObject* rhs) {
            try {
                /// TODO: do some validation on the inputs, ensuring that they are both
                /// bertrand functions or structural intersection/union hints.

                FuncIntersect* hint = reinterpret_cast<FuncIntersect*>(
                    __type__.tp_alloc(&__type__, 0)
                );
                if (hint == nullptr) {
                    return nullptr;
                }
                try {
                    new (hint) FuncIntersect(
                        reinterpret_borrow<Object>(lhs),
                        reinterpret_borrow<Object>(rhs)
                    );
                } catch (...) {
                    Py_DECREF(hint);
                    throw;
                }
                return hint;
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        static PyObject* __repr__(FuncIntersect* self) {
            try {
                std::string str =
                    "(" + repr(self->lhs) + " & " + repr(self->rhs) + ")";
                return PyUnicode_FromStringAndSize(str.c_str(), str.size());
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        static PyNumberMethods number;

        inline static PyMethodDef methods[] = {
            {
                "__instancecheck__",
                reinterpret_cast<PyCFunction>(&__instancecheck__),
                METH_O,
                nullptr
            },
            {
                "__subclasscheck__",
                reinterpret_cast<PyCFunction>(&__subclasscheck__),
                METH_O,
                nullptr
            },
            {nullptr}
        };

        static PyTypeObject __type__;

    };

    /* A structural type hint whose `isinstance()`/`issubclass()` operators only return
    true if ANY of the composite functions are present within a type's interface. */
    struct FuncUnion : PyObject {
        Object lhs;
        Object rhs;

        explicit FuncUnion(Object&& lhs, Object&& rhs) :
            lhs(std::move(lhs)),
            rhs(std::move(rhs))
        {}

        static void __dealloc__(FuncUnion* self) {
            self->~FuncUnion();
        }

        static PyObject* __instancecheck__(FuncUnion* self, PyObject* instance) {
            int rc = PyObject_IsInstance(
                instance,
                ptr(self->lhs)
            );
            if (rc < 0) {
                return nullptr;
            } else if (rc) {
                Py_RETURN_TRUE;
            }
            rc = PyObject_IsInstance(
                instance,
                ptr(self->rhs)
            );
            if (rc < 0) {
                return nullptr;
            } else if (rc) {
                Py_RETURN_TRUE;
            }
            Py_RETURN_FALSE;
        }

        static PyObject* __subclasscheck__(FuncUnion* self, PyObject* cls) {
            int rc = PyObject_IsSubclass(
                cls,
                ptr(self->lhs)
            );
            if (rc < 0) {
                return nullptr;
            } else if (rc) {
                Py_RETURN_TRUE;
            }
            rc = PyObject_IsSubclass(
                cls,
                ptr(self->rhs)
            );
            if (rc < 0) {
                return nullptr;
            } else if (rc) {
                Py_RETURN_TRUE;
            }
            Py_RETURN_FALSE;
        }

        static PyObject* __or__(PyObject* lhs, PyObject* rhs) {
            try {
                /// TODO: do some validation on the inputs, ensuring that they are both
                /// bertrand functions or structural intersection/union hints.

                FuncUnion* hint = reinterpret_cast<FuncUnion*>(
                    __type__.tp_alloc(&__type__, 0)
                );
                if (hint == nullptr) {
                    return nullptr;
                }
                try {
                    new (hint) FuncUnion(
                        reinterpret_borrow<Object>(lhs),
                        reinterpret_borrow<Object>(rhs)
                    );
                } catch (...) {
                    Py_DECREF(hint);
                    throw;
                }
                return hint;
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        static PyObject* __repr__(FuncUnion* self) {
            try {
                std::string str =
                    "(" + repr(self->lhs) + " | " + repr(self->rhs) + ")";
                return PyUnicode_FromStringAndSize(str.c_str(), str.size());
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        static PyNumberMethods number;

        inline static PyMethodDef methods[] = {
            {
                "__instancecheck__",
                reinterpret_cast<PyCFunction>(&__instancecheck__),
                METH_O,
                nullptr
            },
            {
                "__subclasscheck__",
                reinterpret_cast<PyCFunction>(&__subclasscheck__),
                METH_O,
                nullptr
            },
            {nullptr}
        };

        static PyTypeObject __type__;

    };

    PyNumberMethods FuncIntersect::number = {
        .nb_and = reinterpret_cast<binaryfunc>(&FuncIntersect::__and__),
        .nb_or = reinterpret_cast<binaryfunc>(&FuncUnion::__or__),
    };

    PyNumberMethods FuncUnion::number = {
        .nb_and = reinterpret_cast<binaryfunc>(&FuncIntersect::__and__),
        .nb_or = reinterpret_cast<binaryfunc>(&FuncUnion::__or__),
    };

    PyTypeObject FuncIntersect::__type__ = {
        .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = typeid(FuncIntersect).name(),
        .tp_basicsize = sizeof(FuncIntersect),
        .tp_itemsize = 0,
        .tp_dealloc = reinterpret_cast<destructor>(&FuncIntersect::__dealloc__),
        .tp_repr = reinterpret_cast<reprfunc>(&FuncIntersect::__repr__),
        .tp_as_number = &FuncIntersect::number,
        .tp_flags = Py_TPFLAGS_DEFAULT,
        .tp_methods = FuncIntersect::methods,
    };

    PyTypeObject FuncUnion::__type__ = {
        .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = typeid(FuncUnion).name(),
        .tp_basicsize = sizeof(FuncUnion),
        .tp_itemsize = 0,
        .tp_dealloc = reinterpret_cast<destructor>(&FuncUnion::__dealloc__),
        .tp_repr = reinterpret_cast<reprfunc>(&FuncUnion::__repr__),
        .tp_as_number = &FuncUnion::number,
        .tp_flags = Py_TPFLAGS_DEFAULT,
        .tp_methods = FuncUnion::methods,
    };


    /// TODO: instance methods have no descriptor wrapper, so all of the logic
    /// of the underlying function will be available when the unbound method
    /// descriptor is interacted with.  Because of this, I should probably
    /// standardize that across all descriptors, so that they behave semantically
    /// at all times.

    /// TODO: the underlying factory function is just the normal Python-side
    /// constructor, which, for member functions, must accept a self argument
    /// during construction.

    /// TODO: These may actually be able to be lifted out of the class entirely,
    /// and then just forwarded to generically via the Python interface.  That
    /// would significantly reduce the number of types that are created, and
    /// potentially allow me to expose them as singular `@bertrand.classmethod`/
    /// etc decorators, which could potentially allow them to be used analogously
    /// to the Python versions, except that they would convert the underlying
    /// function into a C++ function beforehand by calling the bertrand module,
    /// which enables things like dynamic overloads, etc.

    /// TODO: these will require __export__ scripts that ready these types globally
    /// for the `bertrand` module, and possibly expose them to Python at the same
    /// time.

    struct ClassMethod : PyObject {
        static PyTypeObject __type__;

        vectorcallfunc call = reinterpret_cast<vectorcallfunc>(&__call__);
        PyObject* __wrapped__;
        PyObject* cls;
        PyObject* target;

        /// TODO: this will have to look up the correct target type from the
        /// template map, which necessitates a forward declaration here, not
        /// elsewhere.
        explicit ClassMethod(PyObject* func, PyObject* cls) :
            __wrapped__(func),
            cls(cls),
            target(release(get_member_function_type(func, cls)))
        {
            Py_INCREF(__wrapped__);
            Py_INCREF(cls);
        }

        ~ClassMethod() noexcept {
            Py_DECREF(__wrapped__);
            Py_DECREF(cls);
            Py_DECREF(target);
        }

        static void __dealloc__(ClassMethod* self) {
            self->~ClassMethod();
        }

        static PyObject* __call__(
            ClassMethod* self,
            PyObject* const* args,
            size_t nargsf,
            PyObject* kwnames
        ) {
            try {
                size_t nargs = PyVectorcall_NARGS(nargsf);
                if (nargsf & PY_VECTORCALL_ARGUMENTS_OFFSET) {
                    PyObject** arr = const_cast<PyObject**>(args) - 1;
                    PyObject* temp = arr[0];
                    arr[0] = self->cls;
                    PyObject* result = PyObject_Vectorcall(
                        self->__wrapped__,
                        arr,
                        nargs + 1,
                        kwnames
                    );
                    arr[0] = temp;
                    return result;
                }

                size_t n = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0);
                PyObject** arr = new PyObject*[n + 1];
                arr[0] = self->cls;
                for (size_t i = 0; i < n; ++i) {
                    arr[i + 1] = args[i];
                }
                PyObject* result = PyObject_Vectorcall(
                    self->__wrapped__,
                    arr,
                    n + 1,
                    kwnames
                );
                delete[] arr;
                return result;

            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        static PyObject* __get__(ClassMethod* self, PyObject* obj, PyObject* type) {
            PyObject* const args[] = {
                nullptr,
                self->__wrapped__,
                reinterpret_cast<PyObject*>(Py_TYPE(obj))
            };
            return PyObject_Vectorcall(
                self->target,
                args + 1,
                2 | PY_VECTORCALL_ARGUMENTS_OFFSET,
                nullptr
            );
        }

        static Py_ssize_t __len__(ClassMethod* self) noexcept {
            /// TODO: this should filter overloads with the given prefix
        }

        static PyObject* __getitem__(
            ClassMethod* self,
            PyObject* specifier
        ) noexcept {
            /// TODO: same filtering
        }

        static PyObject* __delitem__(
            ClassMethod* self,
            PyObject* specifier,
            PyObject* value
        ) noexcept {
            /// TODO: same filtering
        }

        static int __contains__(ClassMethod* self, PyObject* func) noexcept {
            /// TODO: same filtering
        }

        static PyObject* __iter__(ClassMethod* self) noexcept {
            /// TODO: same filtering
        }

        static PyObject* __signature__(ClassMethod* self, void*) noexcept {
            /// TODO: filter the first argument?
        }

        /// TODO: these descriptors should also be callable, in which case it would
        /// just forward to the underlying function without binding.

        /// TODO: __getattr__/__setattr__/__delattr__ forwarding to the underlying
        /// type, as well as probably indexing, membership tests, etc, but not
        /// structural type checks, &/|, further descriptors, etc.
        /// -> These need access to cls in order to filter out the correct
        /// overloads when subscripting, iterating, membership testing, etc.

        /// TODO: when it comes to overload navigation/subscription/etc, I'll
        /// have the same prefixing problem as for bound methods, where I need
        /// to filter the trie in some way to get the correct overloads.

        static PyObject* __repr__(ClassMethod* self) {
            try {
                std::string str =
                    "classmethod(" + repr(reinterpret_borrow<Object>(
                        self->__wrapped__
                    )) + ")";
                return PyUnicode_FromStringAndSize(str.c_str(), str.size());
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

    private:

        /// TODO: this must be defined after the metaclass + template framework
        /// is figured out.
        static Object get_member_function_type(
            PyObject* func,
            PyObject* cls
        ) noexcept;

    };

    PyTypeObject ClassMethod::__type__ = {
        .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = typeid(ClassMethod).name(),
        .tp_basicsize = sizeof(ClassMethod),
        .tp_itemsize = 0,
        .tp_dealloc = reinterpret_cast<destructor>(&ClassMethod::__dealloc__),
        .tp_vectorcall_offset = offsetof(ClassMethod, call),
        .tp_repr = reinterpret_cast<reprfunc>(&ClassMethod::__repr__),
        .tp_flags =
            Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION |
            Py_TPFLAGS_HAVE_VECTORCALL,
        .tp_doc = PyDoc_STR(
R"doc(A descriptor that binds a C++ function as a class method of a Python class.

Notes
-----
This descriptor can only be instantiated by applying the `@classmethod`
decorator of a bertrand function to a Python type.

Note that each template instantiation exposes a unique descriptor type, which
mirrors C++ semantics and enables structural typing via `isinstance()` and
`issubclass()`.)doc"
        ),
        .tp_descr_get = reinterpret_cast<descrgetfunc>(&ClassMethod::__get__),
    };

    struct StaticMethod : PyObject {
        static PyTypeObject __type__;

        vectorcallfunc call = reinterpret_cast<vectorcallfunc>(&__call__);
        PyObject* __wrapped__;

        explicit StaticMethod(PyObject* __wrapped__) noexcept :
            __wrapped__(__wrapped__)
        {
            Py_INCREF(__wrapped__);
        }

        ~StaticMethod() noexcept {
            Py_DECREF(__wrapped__);
        }

        static void __dealloc__(StaticMethod* self) noexcept {
            self->~StaticMethod();
        }

        static PyObject* __getattr__(StaticMethod* self, PyObject* attr) noexcept {
            if (PyObject* result = PyObject_GenericGetAttr(
                self,
                attr
            )) {
                return result;

            } else if (PyErr_ExceptionMatches(PyExc_AttributeError)) {
                PyErr_Clear();
                return PyObject_GetAttr(
                    reinterpret_cast<PyObject*>(self->__wrapped__),
                    attr
                );

            } else {
                return nullptr;
            }
        }

        static int __setattr__(
            StaticMethod* self,
            PyObject* attr,
            PyObject* value
        ) noexcept {
            /// TODO: does this require a more refined approach similar to
            /// __getattr__?
            return PyObject_SetAttr(
                self->__wrapped__,
                attr,
                value
            );
        }

        static PyObject* __call__(
            StaticMethod* self,
            PyObject* const* args,
            size_t nargsf,
            PyObject* kwnames
        ) {
            return PyObject_Vectorcall(
                self->__wrapped__,
                args,
                nargsf,
                kwnames
            );
        }

        static PyObject* __get__(
            StaticMethod* self,
            PyObject* obj,
            PyObject* type
        ) noexcept {
            return Py_NewRef(self->__wrapped__);
        }

        static Py_ssize_t __len__(StaticMethod* self) noexcept {
            return PyObject_Length(self->__wrapped__);
        }

        static PyObject* __getitem__(
            StaticMethod* self,
            PyObject* specifier
        ) noexcept {
            return PyObject_GetItem(self->__wrapped__, specifier);
        }

        static int __delitem__(
            StaticMethod* self,
            PyObject* specifier,
            PyObject* value
        ) noexcept {
            return PyObject_SetItem(
                self->__wrapped__,
                specifier,
                value
            );
        }

        static int __contains__(StaticMethod* self, PyObject* func) noexcept {
            return PySequence_Contains(self->__wrapped__, func);
        }

        static PyObject* __iter__(StaticMethod* self) noexcept {
            return PyObject_GetIter(self->__wrapped__);
        }

        static PyObject* __instancecheck__(
            StaticMethod* self,
            PyObject* instance
        ) noexcept {
            int rc = PyObject_IsInstance(instance, self->__wrapped__);
            if (rc < 0) {
                return nullptr;
            }
            return PyBool_FromLong(rc);
        }

        static PyObject* __subclasscheck__(
            StaticMethod* self,
            PyObject* cls
        ) noexcept {
            int rc = PyObject_IsSubclass(cls, self->__wrapped__);
            if (rc < 0) {
                return nullptr;
            }
            return PyBool_FromLong(rc);
        }

        static PyObject* __repr__(StaticMethod* self) noexcept {
            try {
                std::string str =
                    "staticmethod(" + repr(reinterpret_borrow<Object>(
                        self->__wrapped__
                    )) + ")";
                return PyUnicode_FromStringAndSize(str.c_str(), str.size());
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

    private:

        inline static PySequenceMethods sequence = {
            .sq_length = reinterpret_cast<lenfunc>(&StaticMethod::__len__),
            .sq_contains = reinterpret_cast<objobjproc>(
                &StaticMethod::__contains__
            )
        };

        inline static PyMappingMethods mapping = {
            .mp_length = reinterpret_cast<lenfunc>(&StaticMethod::__len__),
            .mp_subscript = reinterpret_cast<binaryfunc>(&StaticMethod::__getitem__),
            .mp_ass_subscript = reinterpret_cast<objobjargproc>(
                &StaticMethod::__delitem__
            )
        };

        inline static PyMethodDef methods[] = {
            {
                "__instancecheck__",
                reinterpret_cast<PyCFunction>(__instancecheck__),
                METH_O,
                nullptr
            },
            {
                "__subclasscheck__",
                reinterpret_cast<PyCFunction>(__subclasscheck__),
                METH_O,
                nullptr
            },
            {nullptr}
        };

        static PyMemberDef members[];

    };

    PyMemberDef StaticMethod::members[] = {
        {
            "__wrapped__",
            Py_T_OBJECT_EX,
            offsetof(StaticMethod, __wrapped__),
            Py_READONLY,
            nullptr
        },
        {nullptr}
    };

    PyTypeObject StaticMethod::__type__ = {
        .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = typeid(StaticMethod).name(),
        .tp_basicsize = sizeof(StaticMethod),
        .tp_itemsize = 0,
        .tp_dealloc = reinterpret_cast<destructor>(&StaticMethod::__dealloc__),
        .tp_vectorcall_offset = offsetof(StaticMethod, call),
        .tp_repr = reinterpret_cast<reprfunc>(&StaticMethod::__repr__),
        .tp_as_sequence = &StaticMethod::sequence,
        .tp_as_mapping = &StaticMethod::mapping,
        .tp_call = PyVectorcall_Call,
        .tp_getattro = reinterpret_cast<getattrofunc>(&StaticMethod::__getattr__),
        .tp_setattro = reinterpret_cast<setattrofunc>(&StaticMethod::__setattr__),
        .tp_flags =
            Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION |
            Py_TPFLAGS_HAVE_VECTORCALL,
        .tp_doc = PyDoc_STR(
R"doc(A descriptor that binds a C++ function into a static method of a Python
class.

Notes
-----
This descriptor can only be instantiated by applying the `@staticmethod`
decorator of a bertrand function to a Python type.

Note that each template instantiation exposes a unique descriptor type, which
mirrors C++ semantics and enables structural typing via `isinstance()` and
`issubclass()`.)doc"
        ),
        .tp_iter = reinterpret_cast<getiterfunc>(&StaticMethod::__iter__),
        .tp_methods = StaticMethod::methods,
        .tp_members = StaticMethod::members,
        .tp_descr_get = reinterpret_cast<descrgetfunc>(&StaticMethod::__get__),
    };

    struct Property : PyObject {
        static PyTypeObject __type__;

        PyObject* cls;
        PyObject* fget;
        PyObject* fset;
        PyObject* fdel;

        /// TODO: Properties should convert the setter/deleter into C++ functions
        /// supporting overloads, just like the getter?  I don't even really know
        /// how that would work.

        explicit Property(
            PyObject* cls,
            PyObject* fget,
            PyObject* fset,
            PyObject* fdel
        ) noexcept : cls(cls), fget(fget), fset(fset), fdel(fdel)
        {
            Py_INCREF(cls);
            Py_INCREF(fget);
            Py_XINCREF(fset);
            Py_XINCREF(fdel);
        }

        ~Property() noexcept {
            Py_DECREF(cls);
            Py_DECREF(fget);
            Py_XDECREF(fset);
            Py_XDECREF(fdel);
        }

        static void __dealloc__(Property* self) noexcept {
            self->~Property();
        }

        static PyObject* __get__(
            Property* self,
            PyObject* obj,
            PyObject* type
        ) noexcept {
            return PyObject_CallOneArg(self->fget, obj);
        }

        static PyObject* __set__(
            Property* self,
            PyObject* obj,
            PyObject* value
        ) noexcept {
            if (value) {
                if (self->fset == nullptr) {
                    PyObject* name = PyObject_GetAttr(
                        self->fget,
                        TemplateString<"__name__">::ptr
                    );
                    if (name == nullptr) {
                        return nullptr;
                    }
                    PyErr_Format(
                        PyExc_AttributeError,
                        "property '%U' of %R object has no setter",
                        name,
                        self->cls
                    );
                    Py_DECREF(name);
                    return nullptr;
                }
                PyObject* const args[] = {obj, value};
                return PyObject_Vectorcall(
                    self->fset,
                    args,
                    2,
                    nullptr
                );
            }

            if (self->fdel == nullptr) {
                PyObject* name = PyObject_GetAttr(
                    self->fget,
                    TemplateString<"__name__">::ptr
                );
                if (name == nullptr) {
                    return nullptr;
                }
                PyErr_Format(
                    PyExc_AttributeError,
                    "property '%U' of %R object has no deleter",
                    name,
                    self->cls
                );
                Py_DECREF(name);
                return nullptr;
            }
            return PyObject_CallOneArg(self->fdel, obj);
        }

        /// TODO: @setter/@deleter decorators?  

        static PyObject* __repr__(Property* self) noexcept {
            try {
                std::string str =
                    "property(" + repr(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(self->fget))
                    ) + ")";
                return PyUnicode_FromStringAndSize(str.c_str(), str.size());
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

    private:

        /// TODO: these may need to be properties, so that assigning to a
        /// property's setter/deleter will convert the object into a C++ function

        /// TODO: properties must expose a __wrapped__ member that points to the
        /// getter.

        static PyMemberDef members[];

    };

    PyMemberDef Property::members[] = {
        {
            "fget",
            Py_T_OBJECT_EX,
            offsetof(Property, fget),
            Py_READONLY,
            nullptr
        },
        {
            "fset",
            Py_T_OBJECT_EX,
            offsetof(Property, fset),
            0,
            nullptr
        },
        {
            "fdel",
            Py_T_OBJECT_EX,
            offsetof(Property, fdel),
            0,
            nullptr
        },
        {nullptr}
    };

    PyTypeObject Property::__type__ = {
        .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = typeid(Property).name(),
        .tp_basicsize = sizeof(Property),
        .tp_itemsize = 0,
        .tp_dealloc = reinterpret_cast<destructor>(&Property::__dealloc__),
        .tp_repr = reinterpret_cast<reprfunc>(&Property::__repr__),
        .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
        .tp_doc = PyDoc_STR(
R"doc(A descriptor that binds a C++ function as a property getter of a Python
class.

Notes
-----
This descriptor can only be instantiated by applying the `@property` decorator
of a bertrand function to a Python type.

Note that each template instantiation exposes a unique descriptor type, which
mirrors C++ semantics and enables structural typing via `isinstance()` and
`issubclass()`.)doc"
        ),
        .tp_members = Property::members,
        .tp_descr_get = reinterpret_cast<descrgetfunc>(&Property::__get__),
        .tp_descr_set = reinterpret_cast<descrsetfunc>(&Property::__set__),
    };

}


template <typename F = Object(*)(
    Arg<"args", const Object&>::args,
    Arg<"kwargs", const Object&>::kwargs
)>
    requires (
        impl::function_pointer_like<F> &&
        impl::args_fit_within_bitset<F> &&
        impl::args_are_convertible_to_python<F> &&
        impl::return_is_convertible_to_python<F> &&
        impl::proper_argument_order<F> &&
        impl::no_duplicate_arguments<F> &&
        impl::no_required_after_default<F>
    )
struct Function;


template <typename F>
struct Interface<Function<F>> : impl::FunctionTag {

    /* The normalized function pointer type for this specialization. */
    using Signature = impl::Signature<F>::type;

    /* The type of the function's `self` argument, or void if it is not a member
    function. */
    using Self = impl::Signature<F>::Self;

    /* A tuple holding the function's default values, which are inferred from the input
    signature. */
    using Defaults = impl::Signature<F>::Defaults;

    /* A trie-based data structure describing dynamic overloads for a function
    object. */
    using Overloads = impl::Signature<F>::Overloads;

    /* The function's return type. */
    using Return = impl::Signature<F>::Return;

    /* Instantiate a new function type with the same arguments, but a different return
    type. */
    template <typename R> requires (std::convertible_to<R, Object>)
    using with_return =
        Function<typename impl::Signature<F>::template with_return<R>::type>;

    /* Instantiate a new function type with the same return type and arguments, but
    bound to a particular type. */
    template <typename C>
        requires (
            std::convertible_to<C, Object> &&
            impl::Signature<F>::template can_make_member<C>
        )
    using with_self =
        Function<typename impl::Signature<F>::template with_self<C>::type>;

    /* Instantiate a new function type with the same return type, but different
    arguments. */
    template <typename... A>
        requires (
            sizeof...(A) <= (64 - impl::Signature<F>::has_self) &&
            impl::Arguments<A...>::args_are_convertible_to_python &&
            impl::Arguments<A...>::proper_argument_order &&
            impl::Arguments<A...>::no_duplicate_arguments &&
            impl::Arguments<A...>::no_required_after_default
        )
    using with_args =
        Function<typename impl::Signature<F>::template with_args<A...>::type>;

    /* Check whether a target function can be registered as a valid overload of this
    function type.  Such a function must minimally account for all the arguments in
    this function signature (which may be bound to subclasses), and list a return
    type that can be converted to this function's return type.  If the function accepts
    variadic positional or keyword arguments, then overloads may include any number of
    additional parameters in their stead, as long as all of those parameters are
    convertible to the variadic type. */
    template <typename Func>
    static constexpr bool compatible = false;

    template <typename Func>
        requires (impl::Signature<std::remove_cvref_t<Func>>::enable)
    static constexpr bool compatible<Func> =
        []<size_t... Is>(std::index_sequence<Is...>) {
            return impl::Signature<F>::template compatible<
                typename impl::Signature<std::remove_cvref_t<Func>>::Return,
                typename impl::Signature<std::remove_cvref_t<Func>>::template at<Is>...
            >;
        }(std::make_index_sequence<impl::Signature<std::remove_cvref_t<Func>>::n>{});

    template <typename Func>
        requires (
            !impl::Signature<std::remove_cvref_t<Func>>::enable &&
            impl::inherits<Func, impl::FunctionTag>
        )
    static constexpr bool compatible<Func> = compatible<
        typename std::remove_reference_t<Func>::Signature
    >;

    template <typename Func>
        requires (
            !impl::Signature<Func>::enable &&
            !impl::inherits<Func, impl::FunctionTag> &&
            impl::has_call_operator<Func>
        )
    static constexpr bool compatible<Func> = 
        impl::Signature<decltype(&std::remove_reference_t<Func>::operator())>::enable &&
        compatible<
            typename impl::Signature<decltype(&std::remove_reference_t<Func>::operator())>::
            template with_self<void>::type
        >;

    /* Check whether this function type can be used to invoke an external C++ function.
    This is identical to a `std::is_invocable_r_v<Func, ...>` check against this
    function's return and argument types.  Note that member functions expect a `self`
    parameter to be listed first, following Python style. */
    template <typename Func>
    static constexpr bool invocable = impl::Signature<F>::template invocable<Func>;

    /* Check whether the function can be called with the given arguments, after
    accounting for optional/variadic/keyword arguments, etc. */
    template <typename... Args>
    static constexpr bool bind = impl::Signature<F>::template Bind<Args...>::enable;

    /* The total number of arguments that the function accepts, not counting `self`. */
    static constexpr size_t n = impl::Signature<F>::n;

    /* The total number of positional-only arguments that the function accepts. */
    static constexpr size_t n_posonly = impl::Signature<F>::n_posonly;

    /* The total number of positional arguments that the function accepts, counting
    both positional-or-keyword and positional-only arguments, but not keyword-only,
    variadic positional or keyword arguments, or `self`. */
    static constexpr size_t n_pos = impl::Signature<F>::n_pos;

    /* The total number of keyword arguments that the function accepts, counting
    both positional-or-keyword and keyword-only arguments, but not positional-only or
    variadic positional or keyword arguments, or `self`. */
    static constexpr size_t n_kw = impl::Signature<F>::n_kw;

    /* The total number of keyword-only arguments that the function accepts. */
    static constexpr size_t n_kwonly = impl::Signature<F>::n_kwonly;

    /* The total number of optional arguments that are present in the function
    signature, including both positional and keyword arguments. */
    static constexpr size_t n_opt = impl::Signature<F>::n_opt;

    /* The total number of optional positional-only arguments that the function
    accepts. */
    static constexpr size_t n_opt_posonly = impl::Signature<F>::n_opt_posonly;

    /* The total number of optional positional arguments that the function accepts,
    counting both positional-only and positional-or-keyword arguments, but not
    keyword-only or variadic positional or keyword arguments, or `self`. */
    static constexpr size_t n_opt_pos = impl::Signature<F>::n_opt_pos;

    /* The total number of optional keyword arguments that the function accepts,
    counting both keyword-only and positional-or-keyword arguments, but not
    positional-only or variadic positional or keyword arguments, or `self`. */
    static constexpr size_t n_opt_kw = impl::Signature<F>::n_opt_kw;

    /* The total number of optional keyword-only arguments that the function
    accepts. */
    static constexpr size_t n_opt_kwonly = impl::Signature<F>::n_opt_kwonly;

    /* Check if the named argument is present in the function signature. */
    template <StaticStr Name>
    static constexpr bool has = impl::Signature<F>::template has<Name>;

    /* Check if the function accepts any positional-only arguments. */
    static constexpr bool has_posonly = impl::Signature<F>::has_posonly;

    /* Check if the function accepts any positional arguments, counting both
    positional-or-keyword and positional-only arguments, but not keyword-only,
    variadic positional or keyword arguments, or `self`. */
    static constexpr bool has_pos = impl::Signature<F>::has_pos;

    /* Check if the function accepts any keyword arguments, counting both
    positional-or-keyword and keyword-only arguments, but not positional-only or
    variadic positional or keyword arguments, or `self`. */
    static constexpr bool has_kw = impl::Signature<F>::has_kw;

    /* Check if the function accepts any keyword-only arguments. */
    static constexpr bool has_kwonly = impl::Signature<F>::has_kwonly;

    /* Check if the function accepts at least one optional argument. */
    static constexpr bool has_opt = impl::Signature<F>::has_opt;

    /* Check if the function accepts at least one optional positional-only argument. */
    static constexpr bool has_opt_posonly = impl::Signature<F>::has_opt_posonly;

    /* Check if the function accepts at least one optional positional argument.  This
    will match either positional-or-keyword or positional-only arguments. */
    static constexpr bool has_opt_pos = impl::Signature<F>::has_opt_pos;

    /* Check if the function accepts at least one optional keyword argument.  This will
    match either positional-or-keyword or keyword-only arguments. */
    static constexpr bool has_opt_kw = impl::Signature<F>::has_opt_kw;

    /* Check if the function accepts at least one optional keyword-only argument. */
    static constexpr bool has_opt_kwonly = impl::Signature<F>::has_opt_kwonly;

    /* Check if the function has a `self` parameter, indicating that it can be called
    as a member function. */
    static constexpr bool has_self = impl::Signature<F>::has_self;

    /* Check if the function accepts variadic positional arguments. */
    static constexpr bool has_args = impl::Signature<F>::has_args;

    /* Check if the function accepts variadic keyword arguments. */
    static constexpr bool has_kwargs = impl::Signature<F>::has_kwargs;

    /* Find the index of the named argument, if it is present. */
    template <StaticStr Name> requires (has<Name>)
    static constexpr size_t idx = impl::Signature<F>::template idx<Name>;

    /* Find the index of the first keyword argument that appears in the function
    signature.  This will match either a positional-or-keyword argument or a
    keyword-only argument.  If no such argument is present, this will return `n`. */
    static constexpr size_t kw_idx = impl::Signature<F>::kw_index;

    /* Find the index of the first keyword-only argument that appears in the function
    signature.  If no such argument is present, this will return `n`. */
    static constexpr size_t kwonly_idx = impl::Signature<F>::kw_only_index;

    /* Find the index of the first optional argument in the function signature.  If no
    such argument is present, this will return `n`. */
    static constexpr size_t opt_idx = impl::Signature<F>::opt_index;

    /* Find the index of the first optional positional-only argument in the function
    signature.  If no such argument is present, this will return `n`. */
    static constexpr size_t opt_posonly_idx = impl::Signature<F>::opt_posonly_index;

    /* Find the index of the first optional positional argument in the function
    signature.  This will match either a positional-or-keyword argument or a
    positional-only argument.  If no such argument is present, this will return `n`. */
    static constexpr size_t opt_pos_idx = impl::Signature<F>::opt_pos_index;

    /* Find the index of the first optional keyword argument in the function signature.
    This will match either a positional-or-keyword argument or a keyword-only argument.
    If no such argument is present, this will return `n`. */
    static constexpr size_t opt_kw_idx = impl::Signature<F>::opt_kw_index;

    /* Find the index of the first optional keyword-only argument in the function
    signature.  If no such argument is present, this will return `n`. */
    static constexpr size_t opt_kwonly_idx = impl::Signature<F>::opt_kwonly_index;

    /* Find the index of the variadic positional arguments in the function signature,
    if they are present.  If no such argument is present, this will return `n`. */
    static constexpr size_t args_idx = impl::Signature<F>::args_index;

    /* Find the index of the variadic keyword arguments in the function signature, if
    they are present.  If no such argument is present, this will return `n`. */
    static constexpr size_t kwargs_idx = impl::Signature<F>::kwargs_index;

    /* Get the (annotated) type of the argument at index I of the function's
    signature. */
    template <size_t I> requires (I < n)
    using at = impl::Signature<F>::template at<I>;

    /* A bitmask of all the required arguments needed to call this function.  This is
    used during argument validation to quickly determine if the parameter list is
    satisfied when keyword are provided out of order, etc. */
    static constexpr uint64_t required = impl::Signature<F>::required;

    /* An FNV-1a seed that was found to perfectly hash the function's keyword argument
    names. */
    static constexpr size_t seed = impl::Signature<F>::seed;

    /* The FNV-1a prime number that was found to perfectly hash the function's keyword
    argument names. */
    static constexpr size_t prime = impl::Signature<F>::prime;

    /* Hash a string according to the seed and prime that were found at compile time to
    perfectly hash this function's keyword arguments. */
    [[nodiscard]] static constexpr size_t hash(const char* str) noexcept {
        return impl::Signature<F>::hash(str);
    }
    [[nodiscard]] static constexpr size_t hash(std::string_view str) noexcept {
        return impl::Signature<F>::hash(str);
    }
    [[nodiscard]] static constexpr size_t hash(const std::string& str) noexcept {
        return impl::Signature<F>::hash(str);
    }

    /* Call an external C++ function that matches the target signature using
    Python-style arguments.  The default values (if any) must be provided as an
    initializer list immediately before the function to be invoked, or constructed
    elsewhere and passed by reference.  This helper has no overhead over a traditional
    C++ function call, disregarding any logic necessary to construct default values. */
    template <typename Func, typename... Args> requires (invocable<Func> && bind<Args...>)
    static Return call(const Defaults& defaults, Func&& func, Args&&... args) {
        return typename impl::Signature<F>::template Bind<Args...>{}(
            defaults,
            std::forward<Func>(func),
            std::forward<Args>(args)...
        );
    }

    /* Call an external Python function using Python-style arguments.  The optional
    `R` template parameter specifies an interim return type, which is used to interpret
    the result of the raw CPython call.  The interim result will then be implicitly
    converted to the function's final return type.  The default value is `Object`,
    which incurs an additional dynamic type check on conversion.  If the exact return
    type is known in advance, then setting `R` equal to that type will avoid any extra
    checks or conversions at the expense of safety if that type is incorrect.  */
    template <impl::inherits<Object> R = Object, typename... Args>
        requires (!std::is_reference_v<R> && bind<Args...>)
    static Return call(PyObject* func, Args&&... args) {
        PyObject* result = typename impl::Signature<F>::template Bind<Args...>{}(
            func,
            std::forward<Args>(args)...
        );
        if constexpr (std::is_void_v<Return>) {
            Py_DECREF(result);
        } else {
            return reinterpret_steal<R>(result);
        }
    }

    /* Register an overload for this function from C++. */
    template <typename Self, typename Func>
        requires (
            !std::is_const_v<std::remove_reference_t<Self>> &&
            compatible<Func>
        )
    void overload(this Self&& self, const Function<Func>& func);

    /* Attach the function as a bound method of a Python type. */
    template <typename T>
    void method(this const auto& self, Type<T>& type);

    template <typename T>
    void classmethod(this const auto& self, Type<T>& type);

    template <typename T>
    void staticmethod(this const auto& self, Type<T>& type);

    template <typename T>
    void property(
        this const auto& self,
        Type<T>& type,
        /* setter */,
        /* deleter */
    );

    /// TODO: when getting and setting these properties, do I need to use Attr
    /// proxies for consistency?

    __declspec(property(get=_get_name, put=_set_name)) std::string __name__;
    [[nodiscard]] std::string _get_name(this const auto& self);
    void _set_name(this auto& self, const std::string& name);

    __declspec(property(get=_get_doc, put=_set_doc)) std::string __doc__;
    [[nodiscard]] std::string _get_doc(this const auto& self);
    void _set_doc(this auto& self, const std::string& doc);

    /// TODO: __defaults__ should return a std::tuple of default values, as they are
    /// given in the signature.

    __declspec(property(get=_get_defaults, put=_set_defaults))
        std::optional<Tuple<Object>> __defaults__;
    [[nodiscard]] std::optional<Tuple<Object>> _get_defaults(this const auto& self);
    void _set_defaults(this auto& self, const Tuple<Object>& defaults);

    /// TODO: This should return a std::tuple of Python type annotations for each
    /// argument.

    __declspec(property(get=_get_annotations, put=_set_annotations))
        std::optional<Dict<Str, Object>> __annotations__;
    [[nodiscard]] std::optional<Dict<Str, Object>> _get_annotations(this const auto& self);
    void _set_annotations(this auto& self, const Dict<Str, Object>& annotations);

    /// TODO: __signature__, which returns a proper Python `inspect.Signature` object.

};
template <typename F>
struct Interface<Type<Function<F>>> {

    /* The normalized function pointer type for this specialization. */
    using Signature = Interface<Function<F>>::Signature;

    /* The type of the function's `self` argument, or void if it is not a member
    function. */
    using Self = Interface<Function<F>>::Self;

    /* A tuple holding the function's default values, which are inferred from the input
    signature and stored as a `std::tuple`. */
    using Defaults = Interface<Function<F>>::Defaults;

    /* A trie-based data structure describing dynamic overloads for a function
    object. */
    using Overloads = Interface<Function<F>>::Overloads;

    /* The function's return type. */
    using Return = Interface<Function<F>>::Return;

    /* Instantiate a new function type with the same arguments, but a different return
    type. */
    template <typename R> requires (std::convertible_to<R, Object>)
    using with_return = Interface<Function<F>>::template with_return<R>;

    /* Instantiate a new function type with the same return type and arguments, but
    bound to a particular type. */
    template <typename C>
        requires (
            std::convertible_to<C, Object> &&
            impl::Signature<F>::template can_make_member<C>
        )
    using with_self = Interface<Function<F>>::template with_self<C>;

    /* Instantiate a new function type with the same return type, but different
    arguments. */
    template <typename... A>
        requires (
            sizeof...(A) <= (64 - impl::Signature<F>::has_self) &&
            impl::Arguments<A...>::args_are_convertible_to_python &&
            impl::Arguments<A...>::proper_argument_order &&
            impl::Arguments<A...>::no_duplicate_arguments &&
            impl::Arguments<A...>::no_required_after_default
        )
    using with_args = Interface<Function<F>>::template with_args<A...>;

    /* Check whether a target function can be registered as a valid overload of this
    function type.  Such a function must minimally account for all the arguments in
    this function signature (which may be bound to subclasses), and list a return
    type that can be converted to this function's return type.  If the function accepts
    variadic positional or keyword arguments, then overloads may include any number of
    additional parameters in their stead, as long as all of those parameters are
    convertible to the variadic type. */
    template <typename Func>
    static constexpr bool compatible = Interface<Function<F>>::template compatible<Func>;

    /* Check whether this function type can be used to invoke an external C++ function.
    This is identical to a `std::is_invocable_r_v<Func, ...>` check against this
    function's return and argument types.  Note that member functions expect a `self`
    parameter to be listed first, following Python style. */
    template <typename Func>
    static constexpr bool invocable = Interface<Function<F>>::template invocable<Func>;

    /* Check whether the function can be called with the given arguments, after
    accounting for optional/variadic/keyword arguments, etc. */
    template <typename... Args>
    static constexpr bool bind = Interface<Function<F>>::template bind<Args...>;

    /* The total number of arguments that the function accepts, not counting `self`. */
    static constexpr size_t n = Interface<Function<F>>::n;

    /* The total number of positional-only arguments that the function accepts. */
    static constexpr size_t n_posonly = Interface<Function<F>>::n_posonly;

    /* The total number of positional arguments that the function accepts, counting
    both positional-or-keyword and positional-only arguments, but not keyword-only,
    variadic positional or keyword arguments, or `self`. */
    static constexpr size_t n_pos = Interface<Function<F>>::n_pos;

    /* The total number of keyword arguments that the function accepts, counting
    both positional-or-keyword and keyword-only arguments, but not positional-only or
    variadic positional or keyword arguments, or `self`. */
    static constexpr size_t n_kw = Interface<Function<F>>::n_kw;

    /* The total number of keyword-only arguments that the function accepts. */
    static constexpr size_t n_kwonly = Interface<Function<F>>::n_kwonly;

    /* The total number of optional arguments that are present in the function
    signature, including both positional and keyword arguments. */
    static constexpr size_t n_opt = Interface<Function<F>>::n_opt;

    /* The total number of optional positional-only arguments that the function
    accepts. */
    static constexpr size_t n_opt_posonly = Interface<Function<F>>::n_opt_posonly;

    /* The total number of optional positional arguments that the function accepts,
    counting both positional-only and positional-or-keyword arguments, but not
    keyword-only or variadic positional or keyword arguments, or `self`. */
    static constexpr size_t n_opt_pos = Interface<Function<F>>::n_opt_pos;

    /* The total number of optional keyword arguments that the function accepts,
    counting both keyword-only and positional-or-keyword arguments, but not
    positional-only or variadic positional or keyword arguments, or `self`. */
    static constexpr size_t n_opt_kw = Interface<Function<F>>::n_opt_kw;

    /* The total number of optional keyword-only arguments that the function
    accepts. */
    static constexpr size_t n_opt_kwonly = Interface<Function<F>>::n_opt_kwonly;

    /* Check if the named argument is present in the function signature. */
    template <StaticStr Name>
    static constexpr bool has = Interface<Function<F>>::template has<Name>;

    /* Check if the function accepts any positional-only arguments. */
    static constexpr bool has_posonly = Interface<Function<F>>::has_posonly;

    /* Check if the function accepts any positional arguments, counting both
    positional-or-keyword and positional-only arguments, but not keyword-only,
    variadic positional or keyword arguments, or `self`. */
    static constexpr bool has_pos = Interface<Function<F>>::has_pos;

    /* Check if the function accepts any keyword arguments, counting both
    positional-or-keyword and keyword-only arguments, but not positional-only or
    variadic positional or keyword arguments, or `self`. */
    static constexpr bool has_kw = Interface<Function<F>>::has_kw;

    /* Check if the function accepts any keyword-only arguments. */
    static constexpr bool has_kwonly = Interface<Function<F>>::has_kwonly;

    /* Check if the function accepts at least one optional argument. */
    static constexpr bool has_opt = Interface<Function<F>>::has_opt;

    /* Check if the function accepts at least one optional positional-only argument. */
    static constexpr bool has_opt_posonly = Interface<Function<F>>::has_opt_posonly;

    /* Check if the function accepts at least one optional positional argument.  This
    will match either positional-or-keyword or positional-only arguments. */
    static constexpr bool has_opt_pos = Interface<Function<F>>::has_opt_pos;

    /* Check if the function accepts at least one optional keyword argument.  This will
    match either positional-or-keyword or keyword-only arguments. */
    static constexpr bool has_opt_kw = Interface<Function<F>>::has_opt_kw;

    /* Check if the function accepts at least one optional keyword-only argument. */
    static constexpr bool has_opt_kwonly = Interface<Function<F>>::has_opt_kwonly;

    /* Check if the function has a `self` parameter, indicating that it can be called
    as a member function. */
    static constexpr bool has_self = Interface<Function<F>>::has_self;

    /* Check if the function accepts variadic positional arguments. */
    static constexpr bool has_args = Interface<Function<F>>::has_args;

    /* Check if the function accepts variadic keyword arguments. */
    static constexpr bool has_kwargs = Interface<Function<F>>::has_kwargs;

    /* Find the index of the named argument, if it is present. */
    template <StaticStr Name> requires (has<Name>)
    static constexpr size_t idx = Interface<Function<F>>::template idx<Name>;

    /* Find the index of the first keyword argument that appears in the function
    signature.  This will match either a positional-or-keyword argument or a
    keyword-only argument.  If no such argument is present, this will return `n`. */
    static constexpr size_t kw_idx = Interface<Function<F>>::kw_index;

    /* Find the index of the first keyword-only argument that appears in the function
    signature.  If no such argument is present, this will return `n`. */
    static constexpr size_t kwonly_idx = Interface<Function<F>>::kw_only_index;

    /* Find the index of the first optional argument in the function signature.  If no
    such argument is present, this will return `n`. */
    static constexpr size_t opt_idx = Interface<Function<F>>::opt_index;

    /* Find the index of the first optional positional-only argument in the function
    signature.  If no such argument is present, this will return `n`. */
    static constexpr size_t opt_posonly_idx = Interface<Function<F>>::opt_posonly_index;

    /* Find the index of the first optional positional argument in the function
    signature.  This will match either a positional-or-keyword argument or a
    positional-only argument.  If no such argument is present, this will return `n`. */
    static constexpr size_t opt_pos_idx = Interface<Function<F>>::opt_pos_index;

    /* Find the index of the first optional keyword argument in the function signature.
    This will match either a positional-or-keyword argument or a keyword-only argument.
    If no such argument is present, this will return `n`. */
    static constexpr size_t opt_kw_idx = Interface<Function<F>>::opt_kw_index;

    /* Find the index of the first optional keyword-only argument in the function
    signature.  If no such argument is present, this will return `n`. */
    static constexpr size_t opt_kwonly_idx = Interface<Function<F>>::opt_kwonly_index;

    /* Find the index of the variadic positional arguments in the function signature,
    if they are present.  If no such argument is present, this will return `n`. */
    static constexpr size_t args_idx = Interface<Function<F>>::args_index;

    /* Find the index of the variadic keyword arguments in the function signature, if
    they are present.  If no such argument is present, this will return `n`. */
    static constexpr size_t kwargs_idx = Interface<Function<F>>::kwargs_index;

    /* Get the (possibly annotated) type of the argument at index I of the function's
    signature. */
    template <size_t I> requires (I < n)
    using at = Interface<Function<F>>::template at<I>;

    /* A bitmask of all the required arguments needed to call this function.  This is
    used during argument validation to quickly determine if the parameter list is
    satisfied when keyword are provided out of order, etc. */
    static constexpr uint64_t required = Interface<Function<F>>::required;

    /* An FNV-1a seed that was found to perfectly hash the function's keyword argument
    names. */
    static constexpr size_t seed = Interface<Function<F>>::seed;

    /* The FNV-1a prime number that was found to perfectly hash the function's keyword
    argument names. */
    static constexpr size_t prime = Interface<Function<F>>::prime;

    /* Hash a string according to the seed and prime that were found at compile time to
    perfectly hash this function's keyword arguments. */
    [[nodiscard]] static constexpr size_t hash(const char* str) noexcept {
        return impl::Signature<F>::hash(str);
    }
    [[nodiscard]] static constexpr size_t hash(std::string_view str) noexcept {
        return impl::Signature<F>::hash(str);
    }
    [[nodiscard]] static constexpr size_t hash(const std::string& str) noexcept {
        return impl::Signature<F>::hash(str);
    }

    /* Call an external C++ function that matches the target signature using
    Python-style arguments.  The default values (if any) must be provided as an
    initializer list immediately before the function to be invoked, or constructed
    elsewhere and passed by reference.  This helper has no overhead over a traditional
    C++ function call, disregarding any logic necessary to construct default values. */
    template <typename Func, typename... Args> requires (invocable<Func> && bind<Args...>)
    static Return call(const Defaults& defaults, Func&& func, Args&&... args) {
        return Interface<Function<F>>::call(
            defaults,
            std::forward<Func>(func),
            std::forward<Args>(args)...
        );
    }

    /* Call an external Python function using Python-style arguments.  The optional
    `R` template parameter specifies an interim return type, which is used to interpret
    the result of the raw CPython call.  The interim result will then be implicitly
    converted to the function's final return type.  The default value is `Object`,
    which incurs an additional dynamic type check on conversion.  If the exact return
    type is known in advance, then setting `R` equal to that type will avoid any extra
    checks or conversions at the expense of safety if that type is incorrect.  */
    template <impl::inherits<Object> R = Object, typename... Args>
        requires (!std::is_reference_v<R> && bind<Args...>)
    static Return call(PyObject* func, Args&&... args) {
        return Interface<Function<F>>::template call<R>(
            func,
            std::forward<Args>(args)...
        );
    }

    /* Register an overload for this function. */
    template <impl::inherits<Interface<Function<F>>> Self, typename Func>
        requires (!std::is_const_v<std::remove_reference_t<Self>> && compatible<Func>)
    void overload(Self&& self, const Function<Func>& func) {
        std::forward<Self>(self).overload(func);
    }

    /* Attach the function as a bound method of a Python type. */
    template <impl::inherits<Interface<Function<F>>> Self, typename T>
    void method(const Self& self, Type<T>& type) {
        std::forward<Self>(self).method(type);
    }

    template <impl::inherits<Interface<Function<F>>> Self, typename T>
    void classmethod(const Self& self, Type<T>& type) {
        std::forward<Self>(self).classmethod(type);
    }

    template <impl::inherits<Interface<Function<F>>> Self, typename T>
    void staticmethod(const Self& self, Type<T>& type) {
        std::forward<Self>(self).staticmethod(type);
    }

    template <impl::inherits<Interface<Function<F>>> Self, typename T>
    void property(const Self& self, Type<T>& type, /* setter */, /* deleter */) {
        std::forward<Self>(self).property(type);
    }

    template <impl::inherits<Interface> Self>
    [[nodiscard]] static std::string __name__(const Self& self) {
        return self.__name__;
    }

    template <impl::inherits<Interface> Self>
    [[nodiscard]] static std::string __doc__(const Self& self) {
        return self.__doc__;
    }

    template <impl::inherits<Interface> Self>
    [[nodiscard]] static std::optional<Tuple<Object>> __defaults__(const Self& self);

    template <impl::inherits<Interface> Self>
    [[nodiscard]] static std::optional<Dict<Str, Object>> __annotations__(const Self& self);

};


/* A universal function wrapper that can represent either a Python function exposed to
C++, or a C++ function exposed to Python with equivalent semantics.  Supports keyword,
optional, and variadic arguments through the `py::Arg` annotation.

Notes
-----
When constructed with a C++ function, this class will create a Python object that
encapsulates the function and allows it to be called from Python.  The Python wrapper
has a unique type for each template signature, which allows Bertrand to enforce strong
type safety and provide accurate error messages if a signature mismatch is detected.
It also allows Bertrand to directly unpack the underlying function from the Python
object, bypassing the Python interpreter and demoting the call to pure C++ where
possible.  If the function accepts `py::Arg` annotations in its signature, then these
will be extracted using template metaprogramming and observed when the function is
called in either language.

When constructed with a Python function, this class will store the function directly
and allow it to be called from C++ with the same semantics as the Python interpreter.
The `inspect` module is used to extract parameter names, categories, and default
values, as well as type annotations if they are present, all of which will be checked
against the expected signature and result in errors if they do not match.  `py::Arg`
annotations can be used to provide keyword, optional, and variadic arguments according
to the templated signature, and the function will be called directly using the
vectorcall protocol, which is the most efficient way to call a Python function from
C++.  

Container unpacking via the `*` and `**` operators is also supported, although it must
be explicitly enabled for C++ containers by overriding the dereference operator (which
is done automatically for iterable Python objects), and is limited in some respects
compared to Python:

    1.  The unpacked container must be the last argument in its respective category
        (positional or keyword), and there can only be at most one of each at the call
        site.  These are not reflected in ordinary Python, but are necessary to ensure
        that compile-time argument matching is unambiguous.
    2.  The container's value type must be convertible to each of the argument types
        that follow it in the function signature, or else a compile error will be
        raised.
    3.  If double unpacking is performed, then the container must yield key-value pairs
        where the key is implicitly convertible to a string, and the value is
        convertible to the corresponding argument type.  If this is not the case, a
        compile error will be raised.
    4.  If the container does not contain enough elements to satisfy the remaining
        arguments, or it contains too many, a runtime error will be raised when the
        function is called.  Since it is impossible to know the size of the container
        at compile time, this cannot be done statically.

Examples
--------
Consider the following function:

    int subtract(int x, int y) {
        return x - y;
    }

We can directly wrap this as a `py::Function` if we want, which does not alter the
calling convention or signature in any way:

    py::Function func("subtract", "a simple example function", subtract);
    func(1, 2);  // returns -1

If this function is exported to Python, its call signature will remain unchanged,
meaning that both arguments must be supplied as positional-only arguments, and no
default values will be considered.

    >>> func(1, 2)  # ok, returns -1
    >>> func(1)  # error: missing required positional argument
    >>> func(1, y = 2)  # error: unexpected keyword argument

We can add parameter names and default values by annotating the C++ function (or a
wrapper around it) with `py::Arg` tags.  For instance:

    py::Function func(
        "subtract",
        "a simple example function",
        [](py::Arg<"x", int> x, py::Arg<"y", int>::opt y) {
            return subtract(x.value, y.value);
        },
        py::arg<"y"> = 2
    );

Note that the annotations store their values in an explicit `value` member, which uses
aggregate initialization to extend the lifetime of temporaries.  The annotations can
thus store references with the same semantics as an ordinary function call, as if the
annotations were not present.  For instance, this:

    py::Function func(
        "subtract",
        "a simple example function",
        [](py::Arg<"x", const int&> x, py::Arg<"y", const int&>::opt y) {
            return subtract(x.value, y.value);
        },
        py::arg<"y"> = 2
    );

is equivalent to the previous example in every way, but with the added benefit that the
`x` and `y` arguments will not be copied unnecessarily according to C++ value
semantics.

With this in place, we can now do the following:

    func(1);
    func(1, 2);
    func(1, py::arg<"y"> = 2);

    // or, equivalently:
    static constexpr auto x = py::arg<"x">;
    static constexpr auto y = py::arg<"y">;
    func(x = 1);
    func(x = 1, y = 2);
    func(y = 2, x = 1);  // keyword arguments can have arbitrary order

All of which will return the same result as before.  The function can also be passed to
Python and called similarly:

    >>> func(1)
    >>> func(1, 2)
    >>> func(1, y = 2)
    >>> func(x = 1)
    >>> func(x = 1, y = 2)
    >>> func(y = 2, x = 1)

What's more, all of the logic necessary to handle these cases is resolved statically at
compile time, meaning that there is no runtime cost for using these annotations, and no
additional code is generated for the function itself.  When it is called from C++, all
we have to do is inspect the provided arguments and match them against the underlying
signature, generating a compile time index sequence that can be used to reorder the
arguments and insert default values where needed.  In fact, each of the above
invocations will be transformed into the same underlying function call, with virtually
the same performance characteristics as raw C++ (disregarding any extra indirection
caused by the `std::function` wrapper).

Additionally, since all arguments are evaluated purely at compile time, we can enforce
strong type safety guarantees on the function signature and disallow invalid calls
using template constraints.  This means that proper call syntax is automatically
enforced throughout the codebase, in a way that allows static analyzers to give proper
syntax highlighting and LSP support. */
template <typename F>
    requires (
        impl::function_pointer_like<F> &&
        impl::args_fit_within_bitset<F> &&
        impl::args_are_convertible_to_python<F> &&
        impl::return_is_convertible_to_python<F> &&
        impl::proper_argument_order<F> &&
        impl::no_duplicate_arguments<F> &&
        impl::no_required_after_default<F>
    )
struct Function : Object, Interface<Function<F>> {
private:

    /* Non-member function type. */
    template <typename Sig>
    struct PyFunction : def<PyFunction<Sig>, Function>, PyObject {
        static constexpr StaticStr __doc__ =
R"doc(A Python wrapper around a C++ function.

Notes
-----
This type is not directly instantiable from Python.  Instead, it can only be
accessed through the `bertrand.Function` template interface, which can be
navigated by subscripting the interface according to a possible function
signature.

Examples
--------
>>> from bertrand import Function
>>> Function[::None, int, str]
<class 'py::Function<void(*)(py::Int, py::Str)>'>
>>> Function[::int, "x": int, "y": int]
<class 'py::Function<py::Int(*)(py::Arg<"x", py::Int>, py::Arg<"y", py::Int>)>'>
>>> Function[str::str, str, "maxsplit": int: ...]
<class 'py::Function<py::Str(py::Str::*)(py::Str, py::Arg<"maxsplit", py::Int>::opt)>'>

Each of these accessors will resolve to a unique Python type that wraps a
specific C++ function signature.  If no existing template instantiation could
be found for a given signature, then a new instantiation will be JIT-compiled
on the fly and cached for future use (TODO).)doc";

        static PyTypeObject __type__;

        /// TODO: store the template key on the metaclass, so that it doesn't require
        /// any memory, and can be used to efficiently bind the function to a type
        /// in the descriptor constructors.  That will turn it into a tuple allocation,
        /// dictionary lookup, and then a Python constructor call, which is about
        /// as good as it gets in terms of performance.  class methods will be only
        /// slightly slower than instance methods as a result.

        vectorcallfunc call = &__call__;
        PyObject* pyfunc = nullptr;
        PyObject* name = nullptr;
        PyObject* docstring = nullptr;
        std::function<typename Sig::to_value::type> func;
        Sig::Defaults defaults;
        Sig::Overloads overloads;

        explicit PyFunction(
            const std::string& name,
            const std::string& docstring,
            std::function<typename Sig::to_value::type>&& func,
            Sig::Defaults&& defaults
        ) : func(std::move(func)), defaults(std::move(defaults))
        {
            this->name = PyUnicode_FromStringAndSize(name.c_str(), name.size());
            if (this->name == nullptr) {
                Exception::from_python();
            }
            this->docstring = PyUnicode_FromStringAndSize(
                docstring.c_str(),
                docstring.size()
            );
            if (this->docstring == nullptr) {
                Py_DECREF(this->name);
                Exception::from_python();
            }
        }

        /// TODO: this constructor would be called from the narrowing __cast__ operator
        /// when a Python function is passed to C++.  It should extract all the
        /// necessary information from the Python function object, by way of the
        /// inspect module.  It should also validate that the signature exactly matches
        /// the expected signature, and raise a TypeError if it does not.
        explicit PyFunction(const Object& pyfunc) :
            pyfunc(ptr(pyfunc)),
            func([pyfunc]() {
                /// TODO: this will require some complicated metaprogramming to
                /// replicate the expected signature
            }),
            defaults([](const Object& pyfunc) {
                /// TODO: yet more metaprogramming
                /// -> This will have to use Inspect() to extract the signature and
                /// find default values.  That will require updates to the Inspect
                /// struct
            }(pyfunc))
        {
            this->name = PyObject_GetAttr(
                name,
                impl::TemplateString<"__name__">::ptr
            );
            if (this->name == nullptr) {
                Exception::from_python();
            }
            this->docstring = PyObject_GetAttr(
                docstring,
                impl::TemplateString<"__doc__">::ptr
            );
            if (this->docstring == nullptr) {
                Py_DECREF(this->name);
                Exception::from_python();
            }
            Py_INCREF(this->pyfunc);
        }

        ~PyFunction() noexcept {
            Py_XDECREF(pyfunc);
            Py_XDECREF(name);
            Py_XDECREF(docstring);
        }

        /// TODO: __init__/__new__.  Possibly just __new__, which may apply a type
        /// check.  That method could replace the second constructor, although the
        /// Python-level constructors would need to forward to a C++ constructor at
        /// some point.

        static void __dealloc__(PyFunction* self) noexcept {
            self->~PyFunction();
        }

        /// TODO: since functions use static types, I might be able to define the
        /// export script here as well, without needing forward references.  Although,
        /// it will require some finesse to handle template interfaces, because
        /// that's where the actual type is stored, rather than on the module itself.
        /// TODO: also, some function types may not be exported to module scope, if
        /// they are defined within an enclosing class, etc.
        /// TODO: __export__ must also ready the descriptor and identifier types with
        /// a static guard.

        template <StaticStr ModName>
        static Type<Function> __export__(Module<ModName> bindings);

        /* Importing a function type just borrows a reference to the static type
        object corresponding to this signature. */
        static Type<Function> __import__() {
            return reinterpret_borrow<Type<Function>>(
                reinterpret_cast<PyObject*>(&__type__)
            );
        }

        /* Register an overload from Python.  Accepts only a single argument, which
        must be a function or other callable object that can be passed to the
        `inspect.signature()` factory function.  That includes user-defined types with
        overloaded call operators, as long as the operator is properly annotated
        according to Python style, or the object provides a `__signature__` property
        that returns a valid `inspect.Signature` object.  This method can be used as a
        decorator from Python. */
        static PyObject* overload(PyFunction* self, PyObject* func) noexcept {
            try {
                Object obj = reinterpret_borrow<Object>(func);
                impl::Inspect signature = {func, Sig::seed, Sig::prime};
                for (PyObject* rtype : signature.returns()) {
                    Object type = reinterpret_borrow<Object>(rtype);
                    if (!issubclass<typename Sig::Return>(type)) {
                        std::string message =
                            "overload return type '" + repr(type) + "' is not a "
                            "subclass of " + repr(Type<typename Sig::Return>());
                        PyErr_SetString(PyExc_TypeError, message.c_str());
                        return nullptr;
                    }
                }
                auto it = signature.begin();
                auto end = signature.end();
                try {
                    while (it != end) {
                        self->overloads.insert(*it, obj);
                        ++it;
                    }
                } catch (...) {
                    auto it2 = signature.begin();
                    while (it2 != it) {
                        self->overloads.remove(*it2);
                        ++it2;
                    }
                    throw;
                }
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        /* Manually clear the function's overload trie from Python. */
        static PyObject* clear(PyFunction* self) noexcept {
            try {
                self->overloads.clear();
                Py_RETURN_NONE;
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        /* Manually clear the function's overload cache from Python. */
        static PyObject* flush(PyFunction* self) noexcept {
            try {
                self->overloads.flush();
                Py_RETURN_NONE;
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        /* Attach a function to a type as an instance method descriptor.  Accepts the
        type to attach to, which can be provided by calling this method as a decorator
        from Python. */
        static PyObject* method(PyFunction* self, PyObject* cls) noexcept {
            try {
                if (!PyType_Check(cls)) {
                    PyErr_Format(
                        PyExc_TypeError,
                        "expected a type object, not: %R",
                        cls
                    );
                    return nullptr;
                }
                if (PyObject_HasAttr(cls, self->name)) {
                    PyErr_Format(
                        PyExc_TypeError,
                        "attribute '%U' already exists on type '%R'",
                        self->name,
                        cls
                    );
                    return nullptr;
                }
                if (PyObject_SetAttr(cls, self->name, self)) {
                    return nullptr;
                }
                return Py_NewRef(cls);
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        /* Attach a function to a type as a class method descriptor.  Accepts the type
        to attach to, which can be provided by calling this method as a decorator from
        Python. */
        static PyObject* classmethod(PyFunction* self, PyObject* cls) noexcept {
            if (PyObject_HasAttr(cls, self->name)) {
                PyErr_Format(
                    PyExc_AttributeError,
                    "attribute '%U' already exists on type '%R'",
                    self->name,
                    cls
                );
                return nullptr;
            }
            using impl::ClassMethod;
            ClassMethod* descr = reinterpret_cast<ClassMethod*>(
                ClassMethod::__type__.tp_alloc(&ClassMethod::__type__, 0)
            );
            if (descr == nullptr) {
                return nullptr;
            }
            try {
                new (descr) ClassMethod(self, cls);
            } catch (...) {
                Py_DECREF(descr);
                Exception::to_python();
                return nullptr;
            }
            int rc = PyObject_SetAttr(cls, self->name, descr);
            Py_DECREF(descr);
            if (rc) {
                return nullptr;
            }
            return Py_NewRef(cls);
        }

        /* Attach a function to a type as a static method descriptor.  Accepts the type
        to attach to, which can be provided by calling this method as a decorator from
        Python. */
        static PyObject* staticmethod(PyFunction* self, PyObject* cls) noexcept {
            if (PyObject_HasAttr(cls, self->name)) {
                PyErr_Format(
                    PyExc_AttributeError,
                    "attribute '%U' already exists on type '%R'",
                    self->name,
                    cls
                );
                return nullptr;
            }
            using impl::StaticMethod;
            StaticMethod* descr = reinterpret_cast<StaticMethod*>(
                StaticMethod::__type__.tp_alloc(&StaticMethod::__type__, 0)
            );
            if (descr == nullptr) {
                return nullptr;
            }
            try {
                new (descr) StaticMethod(self);
            } catch (...) {
                Py_DECREF(descr);
                Exception::to_python();
                return nullptr;
            }
            int rc = PyObject_SetAttr(cls, self->name, descr);
            Py_DECREF(descr);
            if (rc) {
                return nullptr;
            }
            return Py_NewRef(cls);
        }

        /* Attach a function to a type as a getset descriptor.  Accepts a type object
        to attach to, which can be provided by calling this method as a decorator from
        Python, as well as two keyword-only arguments for an optional setter and
        deleter.  The same getter/setter fields are available from the descriptor
        itself via traditional Python `@Type.property.setter` and
        `@Type.property.deleter` decorators. */
        static PyObject* property(
            PyFunction* self,
            PyObject* const* args,
            size_t nargsf,
            PyObject* kwnames
        ) noexcept {
            size_t nargs = PyVectorcall_NARGS(nargsf);
            PyObject* cls;
            if (nargs == 0) {
                PyErr_Format(
                    PyExc_TypeError,
                    "%U.property() requires a type object as the sole "
                    "positional argument",
                    self->name
                );
                return nullptr;
            } else if (nargs == 1) {
                cls = args[0];
            } else {
                PyErr_Format(
                    PyExc_TypeError,
                    "%U.property() takes exactly one positional argument",
                    self->name
                );
                return nullptr;
            }

            PyObject* fset = nullptr;
            PyObject* fdel = nullptr;
            if (kwnames) {
                Py_ssize_t kwcount = PyTuple_GET_SIZE(kwnames);
                if (kwcount > 2) {
                    PyErr_SetString(
                        PyExc_TypeError,
                        "property() takes at most 2 keyword arguments"
                    );
                    return nullptr;
                } else if (kwcount > 1) {
                    PyObject* key = PyTuple_GET_ITEM(kwnames, 0);
                    int rc = PyObject_RichCompareBool(
                        key,
                        impl::TemplateString<"setter">::ptr,
                        Py_EQ
                    );
                    if (rc < 0) {
                        return nullptr;
                    } else if (rc) {
                        fset = args[1];
                    } else {
                        rc = PyObject_RichCompareBool(
                            key,
                            impl::TemplateString<"deleter">::ptr,
                            Py_EQ
                        );
                        if (rc < 0) {
                            return nullptr;
                        } else if (rc) {
                            fdel = args[1];
                        } else {
                            PyErr_Format(
                                PyExc_TypeError,
                                "unexpected keyword argument '%U'",
                                key
                            );
                            return nullptr;
                        }
                    }
                    key = PyTuple_GET_ITEM(kwnames, 1);
                    rc = PyObject_RichCompareBool(
                        key,
                        impl::TemplateString<"deleter">::ptr,
                        Py_EQ
                    );
                    if (rc < 0) {
                        return nullptr;
                    } else if (rc) {
                        fdel = args[2];
                    } else {
                        rc = PyObject_RichCompareBool(
                            key,
                            impl::TemplateString<"setter">::ptr,
                            Py_EQ
                        );
                        if (rc < 0) {
                            return nullptr;
                        } else if (rc) {
                            fset = args[2];
                        } else {
                            PyErr_Format(
                                PyExc_TypeError,
                                "unexpected keyword argument '%U'",
                                key
                            );
                            return nullptr;
                        }
                    }
                } else if (kwcount > 0) {
                    PyObject* key = PyTuple_GET_ITEM(kwnames, 0);
                    int rc = PyObject_RichCompareBool(
                        key,
                        impl::TemplateString<"setter">::ptr,
                        Py_EQ
                    );
                    if (rc < 0) {
                        return nullptr;
                    } else if (rc) {
                        fset = args[1];
                    } else {
                        rc = PyObject_RichCompareBool(
                            key,
                            impl::TemplateString<"deleter">::ptr,
                            Py_EQ
                        );
                        if (rc < 0) {
                            return nullptr;
                        } else if (rc) {
                            fdel = args[1];
                        } else {
                            PyErr_Format(
                                PyExc_TypeError,
                                "unexpected keyword argument '%U'",
                                key
                            );
                            return nullptr;
                        }
                    }
                }
            }

            if (PyObject_HasAttr(cls, self->name)) {
                PyErr_Format(
                    PyExc_AttributeError,
                    "attribute '%U' already exists on type '%R'",
                    self->name,
                    cls
                );
                return nullptr;
            }
            using Property = impl::Property;
            Property* descr = reinterpret_cast<Property*>(
                Property::__type__.tp_alloc(&Property::__type__, 0)
            );
            if (descr == nullptr) {
                return nullptr;
            }
            try {
                new (descr) Property(cls, self, fset, fdel);
            } catch (...) {
                Py_DECREF(descr);
                Exception::to_python();
                return nullptr;
            }
            int rc = PyObject_SetAttr(cls, self->name, descr);
            Py_DECREF(descr);
            if (rc) {
                return nullptr;
            }
            return Py_NewRef(cls);
        }

        /* Call the function from Python. */
        static PyObject* __call__(
            PyFunction* self,
            PyObject* const* args,
            size_t nargsf,
            PyObject* kwnames
        ) noexcept {
            try {
                // check for overloads and forward if one is found
                if (!self->overloads.data.empty()) {
                    PyObject* overload = self->overloads.search(
                        call_key(args, nargsf, kwnames)
                    );
                    if (overload) {
                        return PyObject_Vectorcall(
                            overload,
                            args,
                            nargsf,
                            kwnames
                        );
                    }
                }

                // if this function wraps a captured Python function, then we can
                // immediately forward to it as an optimization
                if (!self->pyfunc.is(nullptr)) {
                    return PyObject_Vectorcall(
                        ptr(self->pyfunc),
                        args,
                        nargsf,
                        kwnames
                    );
                }

                /// TODO: fix the last bugs in the call operator.

                // otherwise, we fall back to the base C++ implementation, which
                // requires us to translate the Python arguments according to the
                // template signature
                return []<size_t... Is>(
                    std::index_sequence<Is...>,
                    PyFunction* self,
                    PyObject* const* args,
                    Py_ssize_t nargsf,
                    PyObject* kwnames
                ) {
                    size_t nargs = PyVectorcall_NARGS(nargsf);
                    if constexpr (!Sig::has_args) {
                        constexpr size_t expected = Sig::n - Sig::n_kwonly;
                        if (nargs > expected) {
                            throw TypeError(
                                "expected at most " + std::to_string(expected) +
                                " positional arguments, but received " +
                                std::to_string(nargs)
                            );
                        }
                    }
                    size_t kwcount = kwnames ? PyTuple_GET_SIZE(kwnames) : 0;
                    if constexpr (!Sig::has_kwargs) {
                        for (size_t i = 0; i < kwcount; ++i) {
                            Py_ssize_t len;
                            const char* name = PyUnicode_AsUTF8AndSize(
                                PyTuple_GET_ITEM(kwnames, i),
                                &len
                            );
                            if (name == nullptr) {
                                Exception::from_python();
                            } else if (!Sig::callback(std::string_view(name, len))) {
                                throw TypeError(
                                    "unexpected keyword argument '" +
                                    std::string(name, len) + "'"
                                );
                            }
                        }
                    }
                    if constexpr (std::is_void_v<typename Sig::Return>) {
                        self->func(
                            {impl::Arguments<Target...>::template from_python<Is>(
                                self->defaults,
                                args,
                                nargs,
                                kwnames,
                                kwcount
                            )}...
                        );
                        Py_RETURN_NONE;
                    } else {
                        return release(as_object(
                            self->func(
                                {impl::Arguments<Target...>::template from_python<Is>(
                                    self->defaults,
                                    args,
                                    nargs,
                                    kwnames,
                                    kwcount
                                )}...
                            )
                        ));
                    }
                }(
                    std::make_index_sequence<Sig::n>{},
                    self,
                    args,
                    nargsf,
                    kwnames
                );
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        /// TODO: these forward declarations can potentially be avoided if I take the
        /// function's type and then subscript it using PyObject_GetItem, which keeps
        /// everything localized as much as possible.  It does require some adjustment
        /// in the metaclass type so that subscripting an instantiation would forward
        /// to the parent template interface.  Basically, this will just be a
        /// condition in the metaclass's subscript operator that checks if this class's
        /// template instantiations are null, but the template interface is not, and
        /// forwards to the interface if that is the case.

        /* Implement the descriptor protocol to generate member functions. */
        static PyObject* __get__(
            PyFunction* self,
            PyObject* obj,
            PyObject* type
        ) noexcept;

        /* `len(function)` will get the number of overloads that are currently being
        tracked. */
        static Py_ssize_t __len__(PyFunction* self) noexcept {
            return self->overloads.data.size();
        }

        /* Index the function to resolve a specific overload, as if the function were
        being called normally.  Returns `None` if no overload can be found, indicating
        that the function will be called with its base implementation. */
        static PyObject* __getitem__(PyFunction* self, PyObject* specifier) noexcept {
            if (PyTuple_Check(specifier)) {
                Py_INCREF(specifier);
            } else {
                specifier = PyTuple_Pack(1, specifier);
                if (specifier == nullptr) {
                    return nullptr;
                }
            }
            try {
                Object key = reinterpret_steal<Object>(specifier);
                std::optional<PyObject*> func = self->overloads.get(
                    subscript_key(key)
                );
                if (func.has_value()) {
                    return Py_NewRef(func.value() ? func.value() : self);
                }
                Py_RETURN_NONE;
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        /* Delete a matching overload, removing it from the overload trie. */
        static int __delitem__(
            PyFunction* self,
            PyObject* specifier,
            PyObject* value
        ) noexcept {
            if (value) {
                PyErr_SetString(
                    PyExc_TypeError,
                    "functions do not support item assignment: use "
                    "`@func.overload` to register an overload instead"
                );
                return -1;
            }
            if (PyTuple_Check(specifier)) {
                Py_INCREF(specifier);
            } else {
                specifier = PyTuple_Pack(1, specifier);
                if (specifier == nullptr) {
                    return -1;
                }
            }
            try {
                Object key = reinterpret_steal<Object>(specifier);
                std::optional<Object> func = self->overloads.remove(
                    subscript_key(key)
                );
                return 0;
            } catch (...) {
                Exception::to_python();
                return -1;
            }
        }

        /* Check whether a given function is contained within the overload trie. */
        static int __contains__(PyFunction* self, PyObject* func) noexcept {
            try {
                for (const auto& data : self->overloads.data) {
                    if (ptr(data.func) == func) {
                        return 1;
                    }
                }
                return 0;
            } catch (...) {
                Exception::to_python();
                return -1;
            }
        }

        /* Iterate over all overloads in the same order in which they were
        registered. */
        static PyObject* __iter__(PyFunction* self) noexcept {
            /// TODO: use the iterator types from access.h
        }

        /* Check whether an object implements this function via the descriptor
        protocol. */
        static PyObject* __instancecheck__(
            PyFunction* self,
            PyObject* instance
        ) noexcept {
            return __subclasscheck__(
                self,
                reinterpret_cast<PyObject*>(Py_TYPE(instance))
            );
        }

        /* Check whether a type implements this function via the descriptor
        protocol. */
        static PyObject* __subclasscheck__(
            PyFunction* self,
            PyObject* cls
        ) noexcept {
            constexpr auto check_intersection = [](
                this auto& check_intersection,
                PyFunction* self,
                PyObject* cls
            ) -> bool {
                if (PyType_IsSubtype(
                    reinterpret_cast<PyTypeObject*>(cls),
                    &impl::FuncIntersect::__type__
                )) {
                    return check_intersection(
                        self,
                        ptr(reinterpret_cast<impl::FuncIntersect*>(cls)->lhs)
                    ) || check_intersection(
                        self,
                        ptr(reinterpret_cast<impl::FuncIntersect*>(cls)->rhs)
                    );
                }
                return reinterpret_cast<PyObject*>(self) == cls;
            };

            try {
                /// NOTE: structural interesection types are considered subclasses of
                /// their component functions, as are exact function matches.
                if (check_intersection(self, cls)) {
                    Py_RETURN_TRUE;
                }

                /// otherwise, we check for an equivalent descriptor on the input class
                if (PyObject_HasAttr(cls, self->name)) {
                    Object attr = reinterpret_steal<Object>(
                        PyObject_GetAttr(cls, self->name)
                    );
                    if (attr.is(nullptr)) {
                        return nullptr;
                    } else if (attr.is(self) || (
                        hasattr<"__wrapped__">(attr) &&
                        getattr<"__wrapped__">(attr).is(self)
                    )) {
                        Py_RETURN_TRUE;
                    }
                }
                Py_RETURN_FALSE;
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        /* Supplying a __signature__ attribute allows C++ functions to be introspected
        via the `inspect` module, just like their pure-Python equivalents. */
        static PyObject* __signature__(PyFunction* self, void*) noexcept {
            try {
                Object inspect = reinterpret_steal<Object>(PyImport_Import(
                    impl::TemplateString<"inspect">::ptr
                ));
                if (inspect.is(nullptr)) {
                    return nullptr;
                }
                Object Signature = getattr<"Signature">(inspect);
                Object Parameter = getattr<"Parameter">(inspect);

                // build the parameter annotations
                Object tuple = PyTuple_New(Sig::n);
                if (tuple.is(nullptr)) {
                    return nullptr;
                }
                []<size_t... Is>(
                    std::index_sequence<Is...>,
                    PyObject* tuple,
                    PyFunction* self,
                    const Object& Parameter
                ) {
                    (PyTuple_SET_ITEM(  // steals a reference
                        tuple,
                        Is,
                        release(build_parameter<Is>(self, Parameter))
                    ), ...);
                }(
                    std::make_index_sequence<Sig::n>{},
                    ptr(tuple),
                    self,
                    Parameter
                );

                // get the return annotation
                Type<typename Sig::Return> return_type;

                // create the signature object
                PyObject* args[2] = {ptr(tuple), ptr(return_type)};
                Object kwnames = reinterpret_steal<Object>(
                    PyTuple_Pack(1, impl::TemplateString<"return_annotation">::ptr)
                );
                return PyObject_Vectorcall(
                    ptr(Signature),
                    args,
                    1,
                    ptr(kwnames)
                );
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        /* Default `repr()` demangles the function name + signature. */
        static PyObject* __repr__(PyFunction* self) noexcept {
            try {
                constexpr std::string demangled =
                    impl::demangle(typeid(Function<F>).name());
                std::string str = "<" + demangled + " at " +
                    std::to_string(reinterpret_cast<size_t>(self)) + ">";
                return PyUnicode_FromStringAndSize(str.c_str(), str.size());
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

    private:

        static impl::Params<std::vector<impl::Param>> call_key(
            PyObject* const* args,
            Py_ssize_t nargsf,
            PyObject* kwnames
        ) {
            size_t hash = 0;
            Py_ssize_t nargs = PyVectorcall_NARGS(nargsf);
            Py_ssize_t kwcount = kwnames ? PyTuple_GET_SIZE(kwnames) : 0;
            std::vector<impl::Param> key;
            key.reserve(nargs + kwcount);

            for (Py_ssize_t i = 0; i < nargs; ++i) {
                PyObject* arg = args[i];
                key.push_back({
                    "",
                    reinterpret_cast<PyObject*>(Py_TYPE(arg)),
                    impl::ArgKind::POS
                });
                hash = impl::hash_combine(
                    hash,
                    key.back().hash(Sig::seed, Sig::prime)
                );
            }

            for (Py_ssize_t i = 0; i < kwcount; ++i) {
                PyObject* name = PyTuple_GET_ITEM(kwnames, i);
                key.push_back({
                    impl::Param::get_name(name),
                    reinterpret_cast<PyObject*>(Py_TYPE(args[nargs + i])),
                    impl::ArgKind::KW
                });
                hash = impl::hash_combine(
                    hash,
                    key.back().hash(Sig::seed, Sig::prime)
                );
            }
            return {std::move(key), hash};
        }

        static impl::Params<std::vector<impl::Param>> subscript_key(
            const Object& specifier
        ) {
            size_t hash = 0;
            Py_ssize_t size = PyTuple_GET_SIZE(ptr(specifier));
            std::vector<impl::Param> key;
            key.reserve(size);

            std::unordered_set<std::string_view> names;
            Py_ssize_t kw_idx = std::numeric_limits<Py_ssize_t>::max();
            for (Py_ssize_t i = 0; i < size; ++i) {
                PyObject* item = PyTuple_GET_ITEM(ptr(specifier), i);

                // slices represent keyword arguments
                if (PySlice_Check(item)) {
                    PySliceObject* slice = reinterpret_cast<PySliceObject*>(item);
                    if (!PyUnicode_Check(slice->start)) {
                        throw TypeError(
                            "expected a keyword argument name as first "
                            "element of slice, not " + repr(
                                reinterpret_borrow<Object>(slice->start)
                            )
                        );
                    }
                    std::string_view name = impl::Param::get_name(slice->start);
                    if (names.contains(name)) {
                        throw TypeError(
                            "duplicate keyword argument: " + std::string(name)
                        );
                    } else if (slice->step != Py_None) {
                        throw TypeError(
                            "keyword argument cannot have a third slice element: " +
                            repr(reinterpret_borrow<Object>(slice->step))
                        );
                    }
                    key.push_back({
                        name,
                        PyType_Check(slice->stop) ?
                            slice->stop :
                            reinterpret_cast<PyObject*>(Py_TYPE(slice->stop)),
                        impl::ArgKind::KW
                    });
                    hash = impl::hash_combine(
                        hash,
                        key.back().hash(Sig::seed, Sig::prime)
                    );
                    kw_idx = i;
                    names.insert(name);

                // all other objects are positional arguments
                } else {
                    if (i > kw_idx) {
                        throw TypeError(
                            "positional argument follows keyword argument"
                        );
                    }
                    key.push_back({
                        "",
                        PyType_Check(item) ?
                            item :
                            reinterpret_cast<PyObject*>(Py_TYPE(item)),
                        impl::ArgKind::POS
                    });
                    hash = impl::hash_combine(
                        hash,
                        key.back().hash(Sig::seed, Sig::prime)
                    );
                }
            }

            return {std::move(key), hash};
        }

        template <size_t I>
        static Object build_parameter(PyFunction* self, const Object& Parameter) {
            using T = Sig::template at<I>;
            using Traits = impl::ArgTraits<T>;

            Object name = reinterpret_steal<Object>(
                PyUnicode_FromStringAndSize(
                    Traits::name,
                    Traits::name.size()
                )
            );
            if (name.is(nullptr)) {
                Exception::from_python();
            }

            Object kind;
            if constexpr (Traits::kwonly()) {
                kind = getattr<"KEYWORD_ONLY">(Parameter);
            } else if constexpr (Traits::kw()) {
                kind = getattr<"POSITIONAL_OR_KEYWORD">(Parameter);
            } else if constexpr (Traits::pos()) {
                kind = getattr<"POSITIONAL_ONLY">(Parameter);
            } else if constexpr (Traits::args()) {
                kind = getattr<"VAR_POSITIONAL">(Parameter);
            } else if constexpr (Traits::kwargs()) {
                kind = getattr<"VAR_KEYWORD">(Parameter);
            } else {
                throw TypeError("unrecognized argument kind");
            }

            Object default_value = self->defaults.template get<I>();
            Type<typename Traits::type> annotation;

            PyObject* args[4] = {
                ptr(name),
                ptr(kind),
                ptr(default_value),
                ptr(annotation),
            };
            Object kwnames = reinterpret_steal<Object>(
                PyTuple_Pack(4,
                    impl::TemplateString<"name">::ptr,
                    impl::TemplateString<"kind">::ptr,
                    impl::TemplateString<"default">::ptr,
                    impl::TemplateString<"annotation">::ptr
                )
            );
            Object result = reinterpret_steal<Object>(PyObject_Vectorcall(
                ptr(Parameter),
                args,
                0,
                ptr(kwnames)
            ));
            if (result.is(nullptr)) {
                Exception::from_python();
            }
            return result;
        }

        inline static PyNumberMethods number = {
            .nb_and = reinterpret_cast<binaryfunc>(&impl::FuncIntersect::__and__),
            .nb_or = reinterpret_cast<binaryfunc>(&impl::FuncUnion::__or__),
        };

        inline static PyMethodDef methods[] = {
            {
                "overload",
                reinterpret_cast<PyCFunction>(&overload),
                METH_O,
                PyDoc_STR(
R"doc()doc"
                )
            },
            {
                "clear",
                reinterpret_cast<PyCFunction>(&clear),
                METH_NOARGS,
                PyDoc_STR(
R"doc()doc"
                )
            },
            {
                "flush",
                reinterpret_cast<PyCFunction>(&flush),
                METH_NOARGS,
                PyDoc_STR(
R"doc()doc"
                )
            },
            {
                "method",
                reinterpret_cast<PyCFunction>(&method),
                METH_O,
                PyDoc_STR(
R"doc()doc"
                )
            },
            {
                "classmethod",
                reinterpret_cast<PyCFunction>(&classmethod),
                METH_O,
                PyDoc_STR(
R"doc()doc"
                )
            },
            {
                "staticmethod",
                reinterpret_cast<PyCFunction>(&staticmethod),
                METH_O,
                PyDoc_STR(
R"doc()doc"
                )
            },
            {
                "property",
                reinterpret_cast<PyCFunction>(&property),
                METH_FASTCALL | METH_KEYWORDS,
                PyDoc_STR(
R"doc()doc"
                )
            },
            {nullptr}
        };

        inline static PyGetSetDef getset[] = {
            {
                "__signature__",
                reinterpret_cast<getter>(&__signature__),
                nullptr,
                PyDoc_STR(
R"doc(A property that produces an accurate `inspect.Signature` object when a
C++ function is introspected from Python.

Notes
-----
Providing this descriptor allows the `inspect` module to be used on C++
functions as if they were implemented in Python itself, reflecting the signature
of their underlying `py::Function` representation.)doc"
                ),
                nullptr
            },
            {nullptr}
        };

    };

    /* Bound member function type.  Must be constructed with a corresponding `self`
    parameter, which will be inserted as the first argument to a call according to
    Python style. */
    template <typename Sig> requires (Sig::has_self)
    struct PyFunction<Sig> : def<PyFunction<Sig>, Function>, PyObject {
        static constexpr StaticStr __doc__ =
R"doc(A bound member function descriptor.

Notes
-----
This type is equivalent to Python's internal `types.MethodType`, which
describes the return value of a method descriptor when accessed from an
instance of an enclosing class.  The only difference is that this type is
implemented in C++, and thus has a unique instantiation for each signature.

Additionally, it must be noted that instances of this type must be constructed
with an appropriate `self` parameter, which is inserted as the first argument
to the underlying C++/Python function when called, according to Python style.
As such, it is not possible for an instance of this type to represent an
unbound function object; those are always represented as a non-member function
type instead.  By templating `py::Function<...>` on a member function pointer,
you are directly indicating the presence of the bound `self` parameter, in a
way that encodes this information into the type systems of both languages
simultaneously.

In essence, all this type does is hold a reference to both an equivalent
non-member function, as well as a reference to the `self` object that the
function is bound to.  All operations will be simply forwarded to the
underlying non-member function, including overloads, introspection, and so on,
but with the `self` argument already accounted for.

Examples
--------
TODO
.)doc";

        static PyTypeObject __type__;

        vectorcallfunc call = &__call__;
        PyObject* __wrapped__;
        PyObject* __self__;

        explicit PyFunction(PyObject* __wrapped__, PyObject* __self__) :
            __wrapped__(__wrapped__), __self__(__self__)
        {
            Py_INCREF(__wrapped__);
            Py_INCREF(__self__);
        }

        ~PyFunction() noexcept {
            Py_XDECREF(__wrapped__);
            Py_XDECREF(__self__);
        }

        /// TODO: I'll need a Python-level __init__/__new__ method that
        /// constructs a new instance of this type, which will be called
        /// when the descriptor is accessed.

        template <StaticStr ModName>
        static Type<Function> __export__(Module<ModName> bindings) {

        }

        /* Importing a function type just borrows a reference to the static type
        object corresponding to this signature. */
        static Type<Function> __import__() {
            return reinterpret_borrow<Type<Function>>(
                reinterpret_cast<PyObject*>(&__type__)
            );
        }

        static PyObject* __call__(
            PyFunction* self,
            PyObject* const* args,
            size_t nargsf,
            PyObject* kwnames
        ) noexcept {
            try {
                /// NOTE: Python includes an optimization of the vectorcall protocol
                /// for bound functions that can temporarily forward the correct `self`
                /// argument without reallocating the underlying array, which we can
                /// take advantage of if possible.
                size_t nargs = PyVectorcall_NARGS(nargsf);
                if (nargsf & PY_VECTORCALL_ARGUMENTS_OFFSET) {
                    PyObject** arr = const_cast<PyObject**>(args) - 1;
                    PyObject* temp = arr[0];
                    arr[0] = self->__self__;
                    PyObject* result = PyObject_Vectorcall(
                        self->__wrapped__,
                        arr,
                        nargs + 1,
                        kwnames
                    );
                    arr[0] = temp;
                    return result;
                }

                /// otherwise, we have to heap allocate a new array and copy the arguments
                size_t n = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0);
                PyObject** arr = new PyObject*[n + 1];
                arr[0] = self->__self__;
                for (size_t i = 0; i < n; ++i) {
                    arr[i + 1] = args[i];
                }
                PyObject* result = PyObject_Vectorcall(
                    self->__wrapped__,
                    arr,
                    nargs + 1,
                    kwnames
                );
                delete[] arr;
                return result;

            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

        static Py_ssize_t __len__(PyFunction* self) noexcept {
            /// TODO: this will have to only count the subset of the overload trie
            /// that matches the bound type as the first argument.
        }

        /* Subscripting a bound method will forward to the unbound method, prepending
        the key with the `self` argument. */
        static PyObject* __getitem__(
            PyFunction* self,
            PyObject* specifier
        ) noexcept {
            if (PyTuple_Check(specifier)) {
                Py_ssize_t len = PyTuple_GET_SIZE(specifier);
                PyObject* tuple = PyTuple_New(len + 1);
                if (tuple == nullptr) {
                    return nullptr;
                }
                PyTuple_SET_ITEM(tuple, 0, Py_NewRef(self->__self__));
                for (Py_ssize_t i = 0; i < len; ++i) {
                    PyTuple_SET_ITEM(
                        tuple,
                        i + 1,
                        Py_NewRef(PyTuple_GET_ITEM(specifier, i))
                    );
                }
                specifier = tuple;
            } else {
                specifier = PyTuple_Pack(2, self->__self__, specifier);
                if (specifier == nullptr) {
                    return nullptr;
                }
            }
            PyObject* result = PyObject_GetItem(self->__wrapped__, specifier);
            Py_DECREF(specifier);
            return result;
        }

        /* Deleting an overload from a bound method will forward the deletion to the
        unbound method, prepending the key with the `self` argument. */
        static int __delitem__(
            PyFunction* self,
            PyObject* specifier,
            PyObject* value
        ) noexcept {
            if (value) {
                PyErr_SetString(
                    PyExc_TypeError,
                    "functions do not support item assignment: use "
                    "`@func.overload` to register an overload instead"
                );
                return -1;
            }
            if (PyTuple_Check(specifier)) {
                Py_ssize_t len = PyTuple_GET_SIZE(specifier);
                PyObject* tuple = PyTuple_New(len + 1);
                if (tuple == nullptr) {
                    return -1;
                }
                PyTuple_SET_ITEM(tuple, 0, Py_NewRef(self->__self__));
                for (Py_ssize_t i = 0; i < len; ++i) {
                    PyTuple_SET_ITEM(
                        tuple,
                        i + 1,
                        Py_NewRef(PyTuple_GET_ITEM(specifier, i))
                    );
                }
                specifier = tuple;
            } else {
                specifier = PyTuple_Pack(2, self->__self__, specifier);
                if (specifier == nullptr) {
                    return -1;
                }
            }
            int result = PyObject_DelItem(self->__wrapped__, specifier);
            Py_DECREF(specifier);
            return result;
        }

        static int __contains__(PyFunction* self, PyObject* func) noexcept {
            /// TODO: once again, only count the subset of the overload trie that
            /// matches the bound type as the first argument.
        }

        static PyObject* __iter__(PyFunction* self) noexcept {
            /// TODO: use the iterator types from access.h
        }

        static PyObject* __signature__(PyFunction* self, void*) noexcept {
            /// TODO: same as underlying function, but strips the `self` parameter.
        }

        /* Default `repr()` reflects Python conventions for bound methods. */
        static PyObject* __repr__(PyFunction* self) noexcept {
            try {
                std::string str =
                    "<bound method " +
                    impl::demangle(Py_TYPE(self->__self__)->tp_name) + ".";
                Py_ssize_t len;
                const char* name = PyUnicode_AsUTF8AndSize(
                    self->__wrapped__->name,
                    &len
                );
                if (name == nullptr) {
                    return nullptr;
                }
                str += std::string(name, len) + " of ";
                str += repr(reinterpret_borrow<Object>(self->__self__)) + ">";
                return PyUnicode_FromStringAndSize(str.c_str(), str.size());
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

    private:

        inline static PyNumberMethods number = {
            .nb_and = reinterpret_cast<binaryfunc>(&impl::FuncIntersect::__and__),
            .nb_or = reinterpret_cast<binaryfunc>(&impl::FuncUnion::__or__),
        };

        inline static PyMethodDef methods[] = {
            {nullptr}
        };

        inline static PyGetSetDef getset[] = {
            {
                "__signature__",
                reinterpret_cast<getter>(&__signature__),
                nullptr,
                PyDoc_STR(
R"doc(A property that produces an accurate `inspect.Signature` object when a
C++ function is introspected from Python.

Notes
-----
Providing this descriptor allows the `inspect` module to be used on C++
functions as if they were implemented in Python itself, reflecting the signature
of their underlying `py::Function` representation.)doc"
                ),
                nullptr
            },
            {nullptr}
        };

    };

public:
    using __python__ = PyFunction<impl::Signature<F>>;

    Function(PyObject* p, borrowed_t t) : Object(p, t) {}
    Function(PyObject* p, stolen_t t) : Object(p, t) {}

    template <typename... A> requires (implicit_ctor<Function>::template enable<A...>)
    Function(A&&... args) : Object(
        implicit_ctor<Function>{},
        std::forward<A>(args)...
    ) {}

    template <typename... A> requires (explicit_ctor<Function>::template enable<A...>)
    explicit Function(A&&... args) : Object(
        explicit_ctor<Function>{},
        std::forward<A>(args)...
    ) {}

};


/// TODO: I would also need some way to disambiguate static functions from member
/// functions when doing CTAD.  This is probably accomplished by providing an extra
/// argument to the constructor which holds the `self` value, and is implicitly
/// convertible to the function's first parameter type.  In that case, the CTAD
/// guide would always deduce to a member function over a static function.  If the
/// extra argument is given and is not convertible to the first parameter type, then
/// we issue a compile error, and if the extra argument is not given at all, then we
/// interpret it as a static function.


/// TODO: so providing the extra `self` argument would be a way to convert a static
/// function pointer into a member function pointer.  The only problem is what happens
/// if the first argument type is `std::string`?  Perhaps you need to pass the self
/// parameter as an initializer list.  Alternatively, the initializer list could be
/// necessary for the function name/docstring, which would prevent conflicts with
/// the `self` wrapper.

/// -> What if you provide self as an initializer list, together with the function
/// itself?


/*
    auto func = py::Function(
        "subtract",
        "a simple example function",
        {
            foo,
            [](py::Arg<"x", const Foo&> x, py::Arg<"y", int>::opt y) {
                return x.value - y.value;
            }
        },
        py::arg<"y"> = 2
    );
*/


/* CTAD guides for construction from function pointer types. */
template <typename Func, typename... Defaults>
    requires (
        impl::Signature<Func>::enable &&
        impl::Signature<Func>::Defaults::template Bind<Defaults...>::enable
    )
Function(Func, Defaults&&...)
    -> Function<typename impl::Signature<Func>::type>;
template <typename Func, typename... Defaults>
    requires (
        impl::Signature<Func>::enable &&
        impl::Signature<Func>::Defaults::template Bind<Defaults...>::enable
    )
Function(std::string, Func, Defaults&&...)
    -> Function<typename impl::Signature<Func>::type>;
template <typename Func, typename... Defaults>
    requires (
        impl::Signature<Func>::enable &&
        impl::Signature<Func>::Defaults::template Bind<Defaults...>::enable
    )
Function(std::string, std::string, Func, Defaults&&...)
    -> Function<typename impl::Signature<Func>::type>;


/// TODO: if you pass in a static function + a self parameter, then CTAD will deduce to
/// a member function of the appropriate type.


/// TODO: modify these to account for conversion from static functions to member functions
template <typename Self, typename Func, typename... Defaults>
    requires (
        impl::Signature<Func>::enable &&
        (impl::Signature<Func>::has_self ?
            std::same_as<Self, typename impl::Signature<Func>::Self> :
            impl::Signature<Func>::n > 0 && std::convertible_to<
                Self,
                typename impl::ArgTraits<typename impl::Signature<Func>::template at<0>>::type
            >
        ) &&
        impl::Signature<Func>::Defaults::template Bind<Defaults...>::enable
    )
Function(std::pair<Self&&, Func>, Defaults&&...)
    -> Function<typename impl::Signature<Func>::type>;
template <typename Self, typename Func, typename... Defaults>
    requires (
        impl::Signature<Func>::enable &&
        std::same_as<Self, typename impl::Signature<Func>::Self> &&
        impl::Signature<Func>::Defaults::template Bind<Defaults...>::enable
    )
Function(std::string, std::pair<Self&&, Func>, Defaults&&...)
    -> Function<typename impl::Signature<Func>::type>;
template <typename Self, typename Func, typename... Defaults>
    requires (
        impl::Signature<Func>::enable &&
        std::same_as<Self, typename impl::Signature<Func>::Self> &&
        impl::Signature<Func>::Defaults::template Bind<Defaults...>::enable
    )
Function(std::string, std::string, std::pair<Self&&, Func>, Defaults&&...)
    -> Function<typename impl::Signature<Func>::type>;


/* CTAD guides for construction from function objects implementing a call operator. */
template <typename Func, typename... Defaults>
    requires (
        !impl::Signature<Func>::enable &&
        !impl::inherits<Func, impl::FunctionTag> &&
        impl::has_call_operator<Func> &&
        impl::Signature<decltype(&Func::operator())>::enable &&
        impl::Signature<decltype(&Func::operator())>::Defaults::
            template Bind<Defaults...>::enable
    )
Function(Func, Defaults&&...)
    -> Function<typename impl::Signature<decltype(&Func::operator())>::type>;
template <typename Func, typename... Defaults>
    requires (
        !impl::Signature<Func>::enable &&
        !impl::inherits<Func, impl::FunctionTag> &&
        impl::has_call_operator<Func> &&
        impl::Signature<decltype(&Func::operator())>::enable &&
        impl::Signature<decltype(&Func::operator())>::Defaults::
            template Bind<Defaults...>::enable
    )
Function(std::string, Func, Defaults&&...)
    -> Function<typename impl::Signature<decltype(&Func::operator())>::type>;
template <typename Func, typename... Defaults>
    requires (
        !impl::Signature<Func>::enable &&
        !impl::inherits<Func, impl::FunctionTag> &&
        impl::has_call_operator<Func> &&
        impl::Signature<decltype(&Func::operator())>::enable &&
        impl::Signature<decltype(&Func::operator())>::Defaults::
            template Bind<Defaults...>::enable
    )
Function(std::string, std::string, Func, Defaults&&...)
    -> Function<typename impl::Signature<decltype(&Func::operator())>::type>;







template <typename Return, typename... Target, typename Func, typename... Values>
    requires (
        !impl::python_like<Func> &&
        std::is_invocable_r_v<Return, Func, Target...> &&
        Function<Return(Target...)>::Defaults::template enable<Values...>
    )
struct __init__<Function<Return(Target...)>, Func, Values...> {
    using type = Function<Return(Target...)>;
    static type operator()(Func&& func, Values&&... defaults) {
        return reinterpret_steal<type>(py::Type<type>::__python__::__create__(
            "",
            "",
            std::function(std::forward<Func>(func)),
            typename type::Defaults(std::forward<Values>(defaults)...)
        ));
    }
};


template <
    std::convertible_to<std::string> Name,
    typename Return,
    typename... Target,
    typename Func,
    typename... Values
>
    requires (
        !impl::python_like<Func> &&
        std::is_invocable_r_v<Return, Func, Target...> &&
        Function<Return(Target...)>::Defaults::template enable<Values...>
    )
struct __init__<Function<Return(Target...)>, Name, Func, Values...> {
    using type = Function<Return(Target...)>;
    static type operator()(Name&& name, Func&& func, Values&&... defaults) {
        return reinterpret_steal<type>(py::Type<type>::__python__::__create__(
            std::forward(name),
            "",
            std::function(std::forward<Func>(func)),
            typename type::Defaults(std::forward<Values>(defaults)...)
        ));
    }
};


template <
    std::convertible_to<std::string> Name,
    std::convertible_to<std::string> Doc,
    typename Return,
    typename... Target,
    typename Func,
    typename... Values
>
    requires (
        !impl::python_like<Func> &&
        std::is_invocable_r_v<Return, Func, Target...> &&
        Function<Return(Target...)>::Defaults::template enable<Values...>
    )
struct __init__<Function<Return(Target...)>, Name, Doc, Func, Values...> {
    using type = Function<Return(Target...)>;
    static type operator()(Name&& name, Doc&& doc, Func&& func, Values&&... defaults) {
        return reinterpret_steal<type>(py::Type<type>::__python__::__create__(
            std::forward(name),
            std::forward<Doc>(doc),
            std::function(std::forward<Func>(func)),
            typename type::Defaults(std::forward<Values>(defaults)...)
        ));
    }
};





/// TODO: class methods can be indicated by a member method of Type<T>.  That
/// would allow this mechanism to scale arbitrarily.


// TODO: constructor should fail if the function type is a subclass of my root
// function type, but not a subclass of this specific function type.  This
// indicates a type mismatch in the function signature, which is a violation of
// static type safety.  I can then print a helpful error message with the demangled
// function types which can show their differences.
// -> This can be implemented in the actual call operator itself, but it would be
// better to implement it on the constructor side.  Perhaps including it in the
// isinstance()/issubclass() checks would be sufficient, since those are used
// during implicit conversion anyways.


/// TODO: all of these should be moved to their respective methods:
/// -   assert_matches() is needed in isinstance() + issubclass() to ensure
///     strict type safety.  Maybe also in the constructor, which can be
///     avoided using CTAD.
/// -   assert_satisfies() is needed in .overload()


struct TODO2 {


    template <size_t I, typename Container>
    static bool _matches(const Params<Container>& key) {
        using T = __object__<std::remove_cvref_t<typename ArgTraits<at<I>>::type>>::type;
        if (I < key.size()) {
            const Param& param = key[I];
            if constexpr (ArgTraits<at<I>>::kwonly()) {
                return (
                    (param.kwonly() & (param.opt() == ArgTraits<at<I>>::opt())) &&
                    (param.name == ArgTraits<at<I>>::name) &&
                    (reinterpret_cast<PyObject*>(param.type) == ptr(Type<T>()))
                );
            } else if constexpr (ArgTraits<at<I>>::kw()) {
                return (
                    (param.kw() & (param.opt() == ArgTraits<at<I>>::opt())) &&
                    (param.name == ArgTraits<at<I>>::name) &&
                    (reinterpret_cast<PyObject*>(param.type) == ptr(Type<T>()))
                );
            } else if constexpr (ArgTraits<at<I>>::pos()) {
                return (
                    (param.posonly() & (param.opt() == ArgTraits<at<I>>::opt())) &&
                    (param.name == ArgTraits<at<I>>::name) &&
                    (reinterpret_cast<PyObject*>(param.type) == ptr(Type<T>()))
                );
            } else if constexpr (ArgTraits<at<I>>::args()) {
                return (
                    param.args() &&
                    (param.name == ArgTraits<at<I>>::name) &&
                    (reinterpret_cast<PyObject*>(param.type) == ptr(Type<T>()))
                );
            } else if constexpr (ArgTraits<at<I>>::kwargs()) {
                return (
                    param.kwargs() &&
                    (param.name == ArgTraits<at<I>>::name) &&
                    (reinterpret_cast<PyObject*>(param.type) == ptr(Type<T>()))
                );
            } else {
                static_assert(false, "unrecognized parameter kind");
            }
        }
        return false;
    }

    template <size_t I, typename Container>
    static void _assert_matches(const Params<Container>& key) {
        using T = __object__<std::remove_cvref_t<typename ArgTraits<at<I>>::type>>::type;

        constexpr auto description = [](const Param& param) {
            if (param.kwonly()) {
                return "keyword-only";
            } else if (param.kw()) {
                return "positional-or-keyword";
            } else if (param.pos()) {
                return "positional";
            } else if (param.args()) {
                return "variadic positional";
            } else if (param.kwargs()) {
                return "variadic keyword";
            } else {
                return "<unknown>";
            }
        };

        if constexpr (ArgTraits<at<I>>::kwonly()) {
            if (I >= key.size()) {
                throw TypeError(
                    "missing keyword-only argument: '" + ArgTraits<at<I>>::name +
                    "' at index: " + std::to_string(I) 
                );
            }
            const Param& param = key[I];
            if (!param.kwonly()) {
                throw TypeError(
                    "expected argument '" + ArgTraits<at<I>>::name +
                    "' to be keyword-only, not " + description(param)
                );
            }
            if (param.name != ArgTraits<at<I>>::name) {
                throw TypeError(
                    "expected keyword-only argument '" + ArgTraits<at<I>>::name +
                    "' at index " + std::to_string(I) + ", not: '" +
                    std::string(param.name) + "'"
                );
            }
            if constexpr (ArgTraits<T>::opt()) {
                if (!param.opt()) {
                    throw TypeError(
                        "expected keyword-only argument '" + ArgTraits<at<I>>::name +
                        "' to have a default value"
                    );
                }
            } else {
                if (param.opt()) {
                    throw TypeError(
                        "expected keyword-only argument '" + ArgTraits<at<I>>::name +
                        "' to not have a default value"
                    );
                }
            }
            Type<T> expected;
            int rc = PyObject_IsSubclass(
                reinterpret_cast<PyObject*>(param.type),
                ptr(expected)
            );
            if (rc < 0) {
                Exception::from_python();
            } else if (!rc) {
                throw TypeError(
                    "expected keyword-only argument '" + ArgTraits<at<I>>::name +
                    "' to be a subclass of '" + repr(expected) + "', not: '" +
                    repr(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param.type)
                    )) + "'"
                );
            }

        } else if constexpr (ArgTraits<at<I>>::kw()) {
            if (I >= key.size()) {
                throw TypeError(
                    "missing positional-or-keyword argument: '" +
                    ArgTraits<at<I>>::name + "' at index: " + std::to_string(I)
                );
            }
            const Param& param = key[I];
            if (!param.kw()) {
                throw TypeError(
                    "expected argument '" + ArgTraits<at<I>>::name +
                    "' to be positional-or-keyword, not " + description(param)
                );
            }
            if (param.name != ArgTraits<at<I>>::name) {
                throw TypeError(
                    "expected positional-or-keyword argument '" +
                    ArgTraits<at<I>>::name + "' at index " + std::to_string(I) +
                    ", not: '" + std::string(param.name) + "'"
                );
            }
            if constexpr (ArgTraits<T>::opt()) {
                if (!param.opt()) {
                    throw TypeError(
                        "expected positional-or-keyword argument '" +
                        ArgTraits<at<I>>::name + "' to have a default value"
                    );
                }
            } else {
                if (param.opt()) {
                    throw TypeError(
                        "expected positional-or-keyword argument '" +
                        ArgTraits<at<I>>::name + "' to not have a default value"
                    );
                }
            }
            Type<T> expected;
            int rc = PyObject_IsSubclass(
                reinterpret_cast<PyObject*>(param.type),
                ptr(expected)
            );
            if (rc < 0) {
                Exception::from_python();
            } else if (!rc) {
                throw TypeError(
                    "expected positional-or-keyword argument '" +
                    ArgTraits<at<I>>::name + "' to be a subclass of '" +
                    repr(expected) + "', not: '" + repr(
                        reinterpret_borrow<Object>(
                            reinterpret_cast<PyObject*>(param.type)
                        )
                    ) + "'"
                );
            }

        } else if constexpr (ArgTraits<at<I>>::pos()) {
            if (I >= key.size()) {
                throw TypeError(
                    "missing positional-only argument: '" +
                    ArgTraits<at<I>>::name + "' at index: " + std::to_string(I)
                );
            }
            const Param& param = key[I];
            if (!param.posonly()) {
                throw TypeError(
                    "expected argument '" + ArgTraits<at<I>>::name +
                    "' to be positional-only, not " + description(param)
                );
            }
            if (param.name != ArgTraits<at<I>>::name) {
                throw TypeError(
                    "expected positional-only argument '" +
                    ArgTraits<at<I>>::name + "' at index " + std::to_string(I) +
                    ", not: '" + std::string(param.name) + "'"
                );
            }
            if constexpr (ArgTraits<T>::opt()) {
                if (!param.opt()) {
                    throw TypeError(
                        "expected positional-only argument '" +
                        ArgTraits<at<I>>::name + "' to have a default value"
                    );
                }
            } else {
                if (param.opt()) {
                    throw TypeError(
                        "expected positional-only argument '" +
                        ArgTraits<at<I>>::name + "' to not have a default value"
                    );
                }
            }
            Type<T> expected;
            int rc = PyObject_IsSubclass(
                reinterpret_cast<PyObject*>(param.type),
                ptr(expected)
            );
            if (rc < 0) {
                Exception::from_python();
            } else if (!rc) {
                throw TypeError(
                    "expected positional-only argument '" +
                    ArgTraits<at<I>>::name + "' to be a subclass of '" +
                    repr(expected) + "', not: '" + repr(
                        reinterpret_borrow<Object>(
                            reinterpret_cast<PyObject*>(param.type)
                        )
                    ) + "'"
                );
            }

        } else if constexpr (ArgTraits<at<I>>::args()) {
            if (I >= key.size()) {
                throw TypeError(
                    "missing variadic positional argument: '" +
                    ArgTraits<at<I>>::name + "' at index: " + std::to_string(I)
                );
            }
            const Param& param = key[I];
            if (!param.args()) {
                throw TypeError(
                    "expected argument '" + ArgTraits<at<I>>::name +
                    "' to be variadic positional, not " + description(param)
                );
            }
            if (param.name != ArgTraits<at<I>>::name) {
                throw TypeError(
                    "expected variadic positional argument '" +
                    ArgTraits<at<I>>::name + "' at index " + std::to_string(I) +
                    ", not: '" + std::string(param.name) + "'"
                );
            }
            Type<T> expected;
            int rc = PyObject_IsSubclass(
                reinterpret_cast<PyObject*>(param.type),
                ptr(expected)
            );
            if (rc < 0) {
                Exception::from_python();
            } else if (!rc) {
                throw TypeError(
                    "expected variadic positional argument '" +
                    ArgTraits<at<I>>::name + "' to be a subclass of '" +
                    repr(expected) + "', not: '" + repr(
                        reinterpret_borrow<Object>(
                            reinterpret_cast<PyObject*>(param.type)
                        )
                    ) + "'"
                );
            }

        } else if constexpr (ArgTraits<at<I>>::kwargs()) {
            if (I >= key.size()) {
                throw TypeError(
                    "missing variadic keyword argument: '" +
                    ArgTraits<at<I>>::name + "' at index: " + std::to_string(I)
                );
            }
            const Param& param = key[I];
            if (!param.kwargs()) {
                throw TypeError(
                    "expected argument '" + ArgTraits<at<I>>::name +
                    "' to be variadic keyword, not " + description(param)
                );
            }
            if (param.name != ArgTraits<at<I>>::name) {
                throw TypeError(
                    "expected variadic keyword argument '" +
                    ArgTraits<at<I>>::name + "' at index " + std::to_string(I) +
                    ", not: '" + std::string(param.name) + "'"
                );
            }
            Type<T> expected;
            int rc = PyObject_IsSubclass(
                reinterpret_cast<PyObject*>(param.type),
                ptr(expected)
            );
            if (rc < 0) {
                Exception::from_python();
            } else if (!rc) {
                throw TypeError(
                    "expected variadic keyword argument '" +
                    ArgTraits<at<I>>::name + "' to be a subclass of '" +
                    repr(expected) + "', not: '" + repr(
                        reinterpret_borrow<Object>(
                            reinterpret_cast<PyObject*>(param.type)
                        )
                    ) + "'"
                );
            }

        } else {
            static_assert(false, "unrecognized parameter type");
        }
    }

    template <size_t I, typename... Ts>
    static constexpr bool _satisfies() { return true; };
    template <size_t I, typename T, typename... Ts>
    static constexpr bool _satisfies() {
        if constexpr (ArgTraits<at<I>>::kwonly()) {
            return (
                (
                    ArgTraits<T>::kwonly() &
                    (~ArgTraits<at<I>>::opt() | ArgTraits<T>::opt())
                ) &&
                (ArgTraits<at<I>>::name == ArgTraits<T>::name) &&
                issubclass<
                    typename ArgTraits<T>::type,
                    typename ArgTraits<at<I>>::type
                >()
            ) && satisfies<I + 1, Ts...>;

        } else if constexpr (ArgTraits<at<I>>::kw()) {
            return (
                (
                    ArgTraits<T>::kw() &
                    (~ArgTraits<at<I>>::opt() | ArgTraits<T>::opt())
                ) &&
                (ArgTraits<at<I>>::name == ArgTraits<T>::name) &&
                issubclass<
                    typename ArgTraits<T>::type,
                    typename ArgTraits<at<I>>::type
                >()
            ) && satisfies<I + 1, Ts...>;

        } else if constexpr (ArgTraits<at<I>>::pos()) {
            return (
                (
                    ArgTraits<T>::pos() &
                    (~ArgTraits<at<I>>::opt() | ArgTraits<T>::opt())
                ) &&
                (ArgTraits<at<I>>::name == ArgTraits<T>::name) &&
                issubclass<
                    typename ArgTraits<T>::type,
                    typename ArgTraits<at<I>>::type
                >()
            ) && satisfies<I + 1, Ts...>;

        } else if constexpr (ArgTraits<at<I>>::args()) {
            if constexpr ((ArgTraits<T>::pos() || ArgTraits<T>::args())) {
                if constexpr (
                    !issubclass<ArgTraits<T>::type, ArgTraits<at<I>>::type>()
                ) {
                    return false;
                }
                return satisfies<I, Ts...>;
            }
            return satisfies<I + 1, Ts...>;

        } else if constexpr (ArgTraits<at<I>>::kwargs()) {
            if constexpr (ArgTraits<T>::kw()) {
                if constexpr (
                    !has<ArgTraits<T>::name> &&
                    !issubclass<ArgTraits<T>::type, ArgTraits<at<I>>::type>()
                ) {
                    return false;
                }
                return satisfies<I, Ts...>;
            } else if constexpr (ArgTraits<T>::kwargs()) {
                if constexpr (
                    !issubclass<ArgTraits<T>::type, ArgTraits<at<I>>::type>()
                ) {
                    return false;
                }
                return satisfies<I, Ts...>;
            }
            return satisfies<I + 1, Ts...>;

        } else {
            static_assert(false, "unrecognized parameter type");
        }

        return false;
    }

    template <size_t I, typename Container>
    static bool _satisfies(const Params<Container>& key, size_t& idx) {
        using T = __object__<std::remove_cvref_t<typename ArgTraits<at<I>>::type>>::type;

        /// NOTE: if the original argument in the enclosing signature is required,
        /// then the new argument cannot be optional.  Otherwise, it can be either
        /// required or optional.

        if constexpr (ArgTraits<at<I>>::kwonly()) {
            if (idx < key.size()) {
                const Param& param = key[idx];
                ++idx;
                return (
                    (param.kwonly() & (~ArgTraits<at<I>>::opt() | param.opt())) &&
                    (param.name == ArgTraits<at<I>>::name) &&
                    (issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param.type))
                    ))
                );
            }

        } else if constexpr (ArgTraits<at<I>>::kw()) {
            if (idx < key.size()) {
                const Param& param = key[idx];
                ++idx;
                return (
                    (param.kw() & (~ArgTraits<at<I>>::opt() | param.opt())) &&
                    (param.name == ArgTraits<at<I>>::name) &&
                    (issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param.type))
                    ))
                );
            }

        } else if constexpr (ArgTraits<at<I>>::pos()) {
            if (idx < key.size()) {
                const Param& param = key[idx];
                ++idx;
                return (
                    (param.pos() & (~ArgTraits<at<I>>::opt() | param.opt())) &&
                    (param.name == ArgTraits<at<I>>::name) &&
                    (issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param.type))
                    ))
                );
            }

        } else if constexpr (ArgTraits<at<I>>::args()) {
            if (idx < key.size()) {
                const Param* param = &key[idx];
                while (param->pos()) {
                    if (!issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param->type))
                    )) {
                        return false;
                    }
                    ++idx;
                    if (idx == key.size()) {
                        return true;
                    }
                    param = &key[idx];            
                }
                if (param->args()) {
                    if (!issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param->type))
                    )) {
                        return false;
                    }
                    ++idx;
                    return true;
                }
            }
            return true;

        } else if constexpr (ArgTraits<at<I>>::kwargs()) {
            if (idx < key.size()) {
                const Param* param = &key[idx];
                while (param->kw()) {
                    if (
                        /// TODO: check to see if the argument is present
                        // !callback(param->name) &&
                        !issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param->type))
                    )) {
                        return false;
                    }
                    ++idx;
                    if (idx == key.size()) {
                        return true;
                    }
                    param = &key[idx];
                }
                if (param->kwargs()) {
                    if (!issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param->type))
                    )) {
                        return false;
                    }
                    ++idx;
                    return true;
                }
            }
            return true;

        } else {
            static_assert(false, "unrecognized parameter type");
        }
        return false;
    }

    template <size_t I, typename Container>
    static void _assert_satisfies(const Params<Container>& key, size_t& idx) {
        using T = __object__<std::remove_cvref_t<typename ArgTraits<at<I>>::type>>::type;

        constexpr auto description = [](const Param& param) {
            if (param.kwonly()) {
                return "keyword-only";
            } else if (param.kw()) {
                return "positional-or-keyword";
            } else if (param.pos()) {
                return "positional";
            } else if (param.args()) {
                return "variadic positional";
            } else if (param.kwargs()) {
                return "variadic keyword";
            } else {
                return "<unknown>";
            }
        };

        if constexpr (ArgTraits<at<I>>::kwonly()) {
            if (idx >= key.size()) {
                throw TypeError(
                    "missing keyword-only argument: '" + ArgTraits<at<I>>::name +
                    "' at index: " + std::to_string(idx)
                );
            }
            const Param& param = key[idx];
            if (!param.kwonly()) {
                throw TypeError(
                    "expected argument '" + ArgTraits<at<I>>::name +
                    "' to be keyword-only, not " + description(param)
                );
            }
            if (param.name != ArgTraits<at<I>>::name) {
                throw TypeError(
                    "expected keyword-only argument '" + ArgTraits<at<I>>::name +
                    "' at index " + std::to_string(idx) + ", not: '" +
                    std::string(param.name) + "'"
                );
            }
            if (~ArgTraits<at<I>>::opt() & param.opt()) {
                throw TypeError(
                    "required keyword-only argument '" + ArgTraits<at<I>>::name +
                    "' must not have a default value"
                );
            }
            if (!issubclass<T>(reinterpret_borrow<Object>(
                reinterpret_cast<PyObject*>(param.type))
            )) {
                throw TypeError(
                    "expected keyword-only argument '" + ArgTraits<at<I>>::name +
                    "' to be a subclass of '" + repr(Type<T>()) + "', not: '" +
                    repr(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param.type)
                    )) + "'"
                );
            }
            ++idx;

        } else if constexpr (ArgTraits<at<I>>::kw()) {
            if (idx >= key.size()) {
                throw TypeError(
                    "missing positional-or-keyword argument: '" +
                    ArgTraits<at<I>>::name + "' at index: " + std::to_string(idx)
                );
            }
            const Param& param = key[idx];
            if (!param.kw()) {
                throw TypeError(
                    "expected argument '" + ArgTraits<at<I>>::name +
                    "' to be positional-or-keyword, not " + description(param)
                );
            }
            if (param.name != ArgTraits<at<I>>::name) {
                throw TypeError(
                    "expected positional-or-keyword argument '" +
                    ArgTraits<at<I>>::name + "' at index " + std::to_string(idx) +
                    ", not: '" + std::string(param.name) + "'"
                );
            }
            if (~ArgTraits<at<I>>::opt() & param.opt()) {
                throw TypeError(
                    "required positional-or-keyword argument '" +
                    ArgTraits<at<I>>::name + "' must not have a default value"
                );
            }
            if (!issubclass<T>(reinterpret_borrow<Object>(
                reinterpret_cast<PyObject*>(param.type))
            )) {
                throw TypeError(
                    "expected positional-or-keyword argument '" +
                    ArgTraits<at<I>>::name + "' to be a subclass of '" +
                    repr(Type<T>()) + "', not: '" + repr(
                        reinterpret_borrow<Object>(
                            reinterpret_cast<PyObject*>(param.type)
                        )
                    ) + "'"
                );
            }
            ++idx;

        } else if constexpr (ArgTraits<at<I>>::pos()) {
            if (idx >= key.size()) {
                throw TypeError(
                    "missing positional argument: '" + ArgTraits<at<I>>::name +
                    "' at index: " + std::to_string(idx)
                );
            }
            const Param& param = key[idx];
            if (!param.pos()) {
                throw TypeError(
                    "expected argument '" + ArgTraits<at<I>>::name +
                    "' to be positional, not " + description(param)
                );
            }
            if (param.name != ArgTraits<at<I>>::name) {
                throw TypeError(
                    "expected positional argument '" + ArgTraits<at<I>>::name +
                    "' at index " + std::to_string(idx) + ", not: '" +
                    std::string(param.name) + "'"
                );
            }
            if (~ArgTraits<at<I>>::opt() & param.opt()) {
                throw TypeError(
                    "required positional argument '" + ArgTraits<at<I>>::name +
                    "' must not have a default value"
                );
            }
            if (!issubclass<T>(reinterpret_borrow<Object>(
                reinterpret_cast<PyObject*>(param.type))
            )) {
                throw TypeError(
                    "expected positional argument '" + ArgTraits<at<I>>::name +
                    "' to be a subclass of '" + repr(Type<T>()) + "', not: '" +
                    repr(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param.type)
                    )) + "'"
                );
            }
            ++idx;

        } else if constexpr (ArgTraits<at<I>>::args()) {
            if (idx < key.size()) {
                const Param* param = &key[idx];
                while (param->pos() && idx < key.size()) {
                    if (!issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param->type))
                    )) {
                        throw TypeError(
                            "expected positional argument '" +
                            std::string(param->name) + "' to be a subclass of '" +
                            repr(Type<T>()) + "', not: '" + repr(
                                reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(param->type)
                                )
                            ) + "'"
                        );
                    }
                    ++idx;
                    if (idx == key.size()) {
                        return;
                    }
                    param = &key[idx];
                }
                if (param->args()) {
                    if (!issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param->type))
                    )) {
                        throw TypeError(
                            "expected variadic positional argument '" +
                            std::string(param->name) + "' to be a subclass of '" +
                            repr(Type<T>()) + "', not: '" + repr(
                                reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(param->type)
                                )
                            ) + "'"
                        );
                    }
                    ++idx;
                }
            }

        } else if constexpr (ArgTraits<at<I>>::kwargs()) {
            if (idx < key.size()) {
                const Param* param = &key[idx];
                while (param->kw() && idx < key.size()) {
                    if (!issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param->type))
                    )) {
                        throw TypeError(
                            "expected keyword argument '" +
                            std::string(param->name) + "' to be a subclass of '" +
                            repr(Type<T>()) + "', not: '" + repr(
                                reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(param->type)
                                )
                            ) + "'"
                        );
                    }
                    ++idx;
                    if (idx == key.size()) {
                        return;
                    }
                    param = &key[idx];
                }
                if (param->kwargs()) {
                    if (!issubclass<T>(reinterpret_borrow<Object>(
                        reinterpret_cast<PyObject*>(param->type))
                    )) {
                        throw TypeError(
                            "expected variadic keyword argument '" +
                            std::string(param->name) + "' to be a subclass of '" +
                            repr(Type<T>()) + "', not: '" + repr(
                                reinterpret_borrow<Object>(
                                    reinterpret_cast<PyObject*>(param->type)
                                )
                            ) + "'"
                        );
                    }
                    ++idx;
                }
            }

        } else {
            static_assert(false, "unrecognized parameter type");
        }
    }

    /* Check to see if a compile-time function signature exactly matches the
    enclosing parameter list. */
    template <typename... Params>
    static constexpr bool matches() {
        return (std::same_as<Params, Args> && ...);
    }

    /* Check to see if a dynamic function signature exactly matches the enclosing
    parameter list. */
    template <typename Container>
    static bool matches(const Params<Container>& key) {
        return []<size_t... Is>(
            std::index_sequence<Is...>,
            const Params<Container>& key
        ) {
            return key.size() == n && (_matches<Is>(key) && ...);
        }(std::make_index_sequence<n>{}, key);
    }

    /* Validate a dynamic function signature, raising an error if it does not
    exactly match the enclosing parameter list. */
    template <typename Container>
    static void assert_matches(const Params<Container>& key) {
        []<size_t... Is>(
            std::index_sequence<Is...>,
            const Params<Container>& key
        ) {
            if (key.size() != n) {
                throw TypeError(
                    "expected " + std::to_string(n) + " arguments, got " +
                    std::to_string(key.size())
                );
            }
            (_assert_matches<Is>(key), ...);
        }(std::make_index_sequence<n>{}, key);
    }

    /* Check to see if a compile-time function signature can be bound to the
    enclosing parameter list, meaning that it could be registered as a viable
    overload. */
    template <typename... Params>
    static constexpr bool satisfies() {
        return _satisfies<0, Params...>();
    }

    /* Check to see if a dynamic function signature can be bound to the enclosing
    parameter list, meaning that it could be registered as a viable overload. */
    template <typename Container>
    static bool satisfies(const Params<Container>& key) {
        return []<size_t... Is>(
            std::index_sequence<Is...>,
            const Params<Container>& key,
            size_t idx
        ) {
            return key.size() == n && (_satisfies<Is>(key, idx) && ...);
        }(std::make_index_sequence<n>{}, key, 0);
    }

    /* Validate a Python function signature, raising an error if it cannot be
    bound to the enclosing parameter list. */
    template <typename Container>
    static void assert_satisfies(const Params<Container>& key) {
        []<size_t... Is>(
            std::index_sequence<Is...>,
            const Params<Container>& key,
            size_t idx
        ) {
            if (key.size() != n) {
                throw TypeError(
                    "expected " + std::to_string(n) + " arguments, got " +
                    std::to_string(key.size())
                );
            }
            (_assert_satisfies<Is>(key, idx), ...);
        }(std::make_index_sequence<n>{}, key, 0);
    }

};




template <typename T, typename R, typename... A>
struct __isinstance__<T, Function<R(A...)>>                 : Returns<bool> {
    static constexpr bool operator()(const T& obj) {
        if (impl::cpp_like<T>) {
            return issubclass<T, Function<R(A...)>>();

        } else if constexpr (issubclass<T, Function<R(A...)>>()) {
            return ptr(obj) != nullptr;

        } else if constexpr (impl::is_object_exact<T>) {
            return ptr(obj) != nullptr && (
                PyFunction_Check(ptr(obj)) ||
                PyMethod_Check(ptr(obj)) ||
                PyCFunction_Check(ptr(obj))
            );
        } else {
            return false;
        }
    }
};


// TODO: if default specialization is given, type checks should be fully generic, right?
// issubclass<T, Function<>>() should check impl::is_callable_any<T>;

template <typename T, typename R, typename... A>
struct __issubclass__<T, Function<R(A...)>>                 : Returns<bool> {
    static constexpr bool operator()() {
        return std::is_invocable_r_v<R, T, A...>;
    }
    static constexpr bool operator()(const T&) {
        // TODO: this is going to have to be radically rethought.
        // Maybe I just forward to an issubclass() check against the type object?
        // In fact, this could maybe be standard operating procedure for all types.
        // 
        return PyType_IsSubtype(
            reinterpret_cast<PyTypeObject*>(ptr(Type<T>())),
            reinterpret_cast<PyTypeObject*>(ptr(Type<Function<R(A...)>>()))
        );
    }
};


/* Call the function with the given arguments.  If the wrapped function is of the
coupled Python type, then this will be translated into a raw C++ call, bypassing
Python entirely. */
template <impl::inherits<impl::FunctionTag> Self, typename... Args>
    requires (std::remove_reference_t<Self>::bind<Args...>)
struct __call__<Self, Args...> : Returns<typename std::remove_reference_t<Self>::Return> {
    using Func = std::remove_reference_t<Self>;
    static Func::Return operator()(Self&& self, Args&&... args) {
        if (!self->overloads.data.empty()) {
            /// TODO: generate an overload key from the C++ arguments
            /// -> This can be implemented in Arguments<...>::Bind<...>::key()
            PyObject* overload = self->overloads.search(/* overload key */);
            if (overload) {
                return Func::call(overload, std::forward<Args>(args)...);
            }
        }
        return Func::call(self->defaults, self->func, std::forward<Args>(args)...);
    }
};


/// TODO: __getitem__, __contains__, __iter__, __len__, __bool__


template <typename F>
template <typename Self, typename Func>
    requires (
        !std::is_const_v<std::remove_reference_t<Self>> &&
        compatible<Func>
    )
void Interface<Function<F>>::overload(this Self&& self, const Function<Func>& func) {
    /// TODO: C++ side of function overloading
}


template <typename F>
template <typename T>
void Interface<Function<F>>::method(this const auto& self, Type<T>& type) {
    /// TODO: C++ side of method binding
}


template <typename F>
template <typename T>
void Interface<Function<F>>::classmethod(this const auto& self, Type<T>& type) {
    /// TODO: C++ side of method binding
}


template <typename F>
template <typename T>
void Interface<Function<F>>::staticmethod(this const auto& self, Type<T>& type) {
    /// TODO: C++ side of method binding
}


template <typename F>
template <typename T>
void Interface<Function<F>>::property(
    this const auto& self,
    Type<T>& type,
    /* setter */,
    /* deleter */
) {
    /// TODO: C++ side of method binding
}


namespace impl {

    /* A convenience function that calls a named method of a Python object using
    C++-style arguments.  Avoids the overhead of creating a temporary Function object. */
    template <StaticStr Name, typename Self, typename... Args>
        requires (
            __getattr__<std::decay_t<Self>, Name>::enable &&
            std::derived_from<typename __getattr__<std::decay_t<Self>, Name>::type, FunctionTag> &&
            __getattr__<std::decay_t<Self>, Name>::type::template invocable<Args...>
        )
    decltype(auto) call_method(Self&& self, Args&&... args) {
        using Func = __getattr__<std::decay_t<Self>, Name>::type;
        Object meth = reinterpret_steal<Object>(PyObject_GetAttr(
            ptr(self),
            TemplateString<Name>::ptr
        ));
        if (meth.is(nullptr)) {
            Exception::from_python();
        }
        try {
            return Func::template invoke<typename Func::ReturnType>(
                meth,
                std::forward<Args>(args)...
            );
        } catch (...) {
            throw;
        }
    }

    /* A convenience function that calls a named method of a Python type object using
    C++-style arguments.  Avoids the overhead of creating a temporary Function object. */
    template <typename Self, StaticStr Name, typename... Args>
        requires (
            __getattr__<std::decay_t<Self>, Name>::enable &&
            std::derived_from<typename __getattr__<std::decay_t<Self>, Name>::type, FunctionTag> &&
            __getattr__<std::decay_t<Self>, Name>::type::template invocable<Args...>
        )
    decltype(auto) call_static(Args&&... args) {
        using Func = __getattr__<std::decay_t<Self>, Name>::type;
        Object meth = reinterpret_steal<Object>(PyObject_GetAttr(
            ptr(Self::type),
            TemplateString<Name>::ptr
        ));
        if (meth.is(nullptr)) {
            Exception::from_python();
        }
        try {
            return Func::template invoke<typename Func::ReturnType>(
                meth,
                std::forward<Args>(args)...
            );
        } catch (...) {
            throw;
        }
    }

    /// NOTE: the type returned by `std::mem_fn()` is implementation-defined, so we
    /// have to do some template magic to trick the compiler into deducing the correct
    /// type during template specializations.
    template <typename Sig>
    using std_mem_fn_type = respecialize<decltype(
        std::mem_fn(std::declval<void(Object::*)()>())
    ), Sig>;

};


#define NON_MEMBER_FUNC(IN, OUT) \
    template <typename R, typename... A> \
    struct __object__<IN> : Returns<Function<OUT>> {};

#define MEMBER_FUNC(IN, OUT) \
    template <typename R, typename C, typename... A> \
    struct __object__<IN> : Returns<Function<OUT>> {};

#define STD_MEM_FN(IN, OUT) \
    template <typename R, typename C, typename... A> \
    struct __object__<impl::std_mem_fn_type<IN>> : Returns<Function<OUT>> {};


NON_MEMBER_FUNC(R(A...), R(*)(A...))
NON_MEMBER_FUNC(R(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(&)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(&)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(&&)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(&&)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(*)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(*)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(*&)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(*&)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(*&&)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(*&&)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(*const)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(*const)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(*const&)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(*const&)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(*const&&)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(*const&&)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(*volatile)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(*volatile)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(*volatile&)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(*volatile&)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(*volatile&&)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(*volatile&&)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(*const volatile)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(*const volatile)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(*const volatile&)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(*const volatile&)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(R(*const volatile&&)(A...), R(*)(A...))
NON_MEMBER_FUNC(R(*const volatile&&)(A...) noexcept, R(*)(A...) noexcept)
NON_MEMBER_FUNC(std::function<R(A...)>, R(*)(A...))
NON_MEMBER_FUNC(std::function<R(A...)>&, R(*)(A...))
NON_MEMBER_FUNC(std::function<R(A...)>&&, R(*)(A...))
NON_MEMBER_FUNC(const std::function<R(A...)>, R(*)(A...))
NON_MEMBER_FUNC(const std::function<R(A...)>&, R(*)(A...))
NON_MEMBER_FUNC(const std::function<R(A...)>&&, R(*)(A...))
NON_MEMBER_FUNC(volatile std::function<R(A...)>, R(*)(A...))
NON_MEMBER_FUNC(volatile std::function<R(A...)>&, R(*)(A...))
NON_MEMBER_FUNC(volatile std::function<R(A...)>&&, R(*)(A...))
NON_MEMBER_FUNC(const volatile std::function<R(A...)>, R(*)(A...))
NON_MEMBER_FUNC(const volatile std::function<R(A...)>&, R(*)(A...))
NON_MEMBER_FUNC(const volatile std::function<R(A...)>&&, R(*)(A...))
MEMBER_FUNC(R(C::*)(A...), R(C::*)(A...))
MEMBER_FUNC(R(C::*)(A...) noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*)(A...) &, R(C::*)(A...))
MEMBER_FUNC(R(C::*)(A...) & noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*)(A...) const, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*)(A...) const noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*)(A...) const &, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*)(A...) const & noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*)(A...) volatile, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*)(A...) volatile &, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*)(A...) const volatile, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*)(A...) const volatile &, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*&)(A...), R(C::*)(A...))
MEMBER_FUNC(R(C::*&)(A...) noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*&)(A...) &, R(C::*)(A...))
MEMBER_FUNC(R(C::*&)(A...) & noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*&)(A...) const, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*&)(A...) const noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*&)(A...) const &, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*&)(A...) volatile, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*&)(A...) volatile &, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*&)(A...) const volatile, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*&)(A...) const volatile &, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*&&)(A...), R(C::*)(A...))
MEMBER_FUNC(R(C::*&&)(A...) noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*&&)(A...) &, R(C::*)(A...))
MEMBER_FUNC(R(C::*&&)(A...) & noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*&&)(A...) const, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*&&)(A...) const noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*&&)(A...) const &, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*&&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*&&)(A...) volatile, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*&&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*&&)(A...) volatile &, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*&&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*&&)(A...) const volatile, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*&&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*&&)(A...) const volatile &, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*&&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*const)(A...), R(C::*)(A...))
MEMBER_FUNC(R(C::*const)(A...) noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*const)(A...) &, R(C::*)(A...))
MEMBER_FUNC(R(C::*const)(A...) & noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*const)(A...) const, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*const)(A...) const noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*const)(A...) const &, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*const)(A...) const & noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*const)(A...) volatile, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*const)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*const)(A...) volatile &, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*const)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*const)(A...) const volatile, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*const)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*const)(A...) const volatile &, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*const)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*const&)(A...), R(C::*)(A...))
MEMBER_FUNC(R(C::*const&)(A...) noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*const&)(A...) &, R(C::*)(A...))
MEMBER_FUNC(R(C::*const&)(A...) & noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*const&)(A...) const, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*const&)(A...) const noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*const&)(A...) const &, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*const&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*const&)(A...) volatile, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*const&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*const&)(A...) volatile &, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*const&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*const&)(A...) const volatile, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*const&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*const&)(A...) const volatile &, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*const&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*const&&)(A...), R(C::*)(A...))
MEMBER_FUNC(R(C::*const&&)(A...) noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*const&&)(A...) &, R(C::*)(A...))
MEMBER_FUNC(R(C::*const&&)(A...) & noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*const&&)(A...) const, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*const&&)(A...) const noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*const&&)(A...) const &, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*const&&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*const&&)(A...) volatile, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*const&&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*const&&)(A...) volatile &, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*const&&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*const&&)(A...) const volatile, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*const&&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*const&&)(A...) const volatile &, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*const&&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*volatile)(A...), R(C::*)(A...))
MEMBER_FUNC(R(C::*volatile)(A...) noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*volatile)(A...) &, R(C::*)(A...))
MEMBER_FUNC(R(C::*volatile)(A...) & noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*volatile)(A...) const, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*volatile)(A...) const noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*volatile)(A...) const &, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*volatile)(A...) const & noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*volatile)(A...) volatile, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*volatile)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*volatile)(A...) volatile &, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*volatile)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*volatile)(A...) const volatile, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*volatile)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*volatile)(A...) const volatile &, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*volatile)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*volatile&)(A...), R(C::*)(A...))
MEMBER_FUNC(R(C::*volatile&)(A...) noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*volatile&)(A...) &, R(C::*)(A...))
MEMBER_FUNC(R(C::*volatile&)(A...) & noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*volatile&)(A...) const, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*volatile&)(A...) const noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*volatile&)(A...) const &, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*volatile&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*volatile&)(A...) volatile, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*volatile&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*volatile&)(A...) volatile &, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*volatile&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*volatile&)(A...) const volatile, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*volatile&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*volatile&)(A...) const volatile &, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*volatile&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*volatile&&)(A...), R(C::*)(A...))
MEMBER_FUNC(R(C::*volatile&&)(A...) noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*volatile&&)(A...) &, R(C::*)(A...))
MEMBER_FUNC(R(C::*volatile&&)(A...) & noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*volatile&&)(A...) const, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*volatile&&)(A...) const noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*volatile&&)(A...) const &, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*volatile&&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*volatile&&)(A...) volatile, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*volatile&&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*volatile&&)(A...) volatile &, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*volatile&&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*volatile&&)(A...) const volatile, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*volatile&&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*volatile&&)(A...) const volatile &, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*volatile&&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*const volatile)(A...), R(C::*)(A...))
MEMBER_FUNC(R(C::*const volatile)(A...) noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*const volatile)(A...) &, R(C::*)(A...))
MEMBER_FUNC(R(C::*const volatile)(A...) & noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*const volatile)(A...) const, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*const volatile)(A...) const noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*const volatile)(A...) const &, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*const volatile)(A...) const & noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*const volatile)(A...) volatile, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*const volatile)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*const volatile)(A...) volatile &, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*const volatile)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*const volatile)(A...) const volatile, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*const volatile)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*const volatile)(A...) const volatile &, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*const volatile)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*const volatile&)(A...), R(C::*)(A...))
MEMBER_FUNC(R(C::*const volatile&)(A...) noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*const volatile&)(A...) &, R(C::*)(A...))
MEMBER_FUNC(R(C::*const volatile&)(A...) & noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*const volatile&)(A...) const, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*const volatile&)(A...) const noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*const volatile&)(A...) const &, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*const volatile&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*const volatile&)(A...) volatile, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*const volatile&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*const volatile&)(A...) volatile &, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*const volatile&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*const volatile&)(A...) const volatile, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*const volatile&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*const volatile&)(A...) const volatile &, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*const volatile&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*const volatile&&)(A...), R(C::*)(A...))
MEMBER_FUNC(R(C::*const volatile&&)(A...) noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*const volatile&&)(A...) &, R(C::*)(A...))
MEMBER_FUNC(R(C::*const volatile&&)(A...) & noexcept, R(C::*)(A...) noexcept)
MEMBER_FUNC(R(C::*const volatile&&)(A...) const, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*const volatile&&)(A...) const noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*const volatile&&)(A...) const &, R(C::*)(A...) const)
MEMBER_FUNC(R(C::*const volatile&&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
MEMBER_FUNC(R(C::*const volatile&&)(A...) volatile, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*const volatile&&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*const volatile&&)(A...) volatile &, R(C::*)(A...) volatile)
MEMBER_FUNC(R(C::*const volatile&&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
MEMBER_FUNC(R(C::*const volatile&&)(A...) const volatile, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*const volatile&&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
MEMBER_FUNC(R(C::*const volatile&&)(A...) const volatile &, R(C::*)(A...) const volatile)
MEMBER_FUNC(R(C::*const volatile&&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*)(A...), R(C::*)(A...))
STD_MEM_FN(R(C::*)(A...) noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*)(A...) &, R(C::*)(A...))
STD_MEM_FN(R(C::*)(A...) & noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*)(A...) const, R(C::*)(A...) const)
STD_MEM_FN(R(C::*)(A...) const noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*)(A...) const &, R(C::*)(A...) const)
STD_MEM_FN(R(C::*)(A...) const & noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*)(A...) volatile, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*)(A...) volatile &, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*)(A...) const volatile, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*)(A...) const volatile &, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*&)(A...), R(C::*)(A...))
STD_MEM_FN(R(C::*&)(A...) noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*&)(A...) &, R(C::*)(A...))
STD_MEM_FN(R(C::*&)(A...) & noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*&)(A...) const, R(C::*)(A...) const)
STD_MEM_FN(R(C::*&)(A...) const noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*&)(A...) const &, R(C::*)(A...) const)
STD_MEM_FN(R(C::*&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*&)(A...) volatile, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*&)(A...) volatile &, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*&)(A...) const volatile, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*&)(A...) const volatile &, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*&&)(A...), R(C::*)(A...))
STD_MEM_FN(R(C::*&&)(A...) noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*&&)(A...) &, R(C::*)(A...))
STD_MEM_FN(R(C::*&&)(A...) & noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*&&)(A...) const, R(C::*)(A...) const)
STD_MEM_FN(R(C::*&&)(A...) const noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*&&)(A...) const &, R(C::*)(A...) const)
STD_MEM_FN(R(C::*&&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*&&)(A...) volatile, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*&&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*&&)(A...) volatile &, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*&&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*&&)(A...) const volatile, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*&&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*&&)(A...) const volatile &, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*&&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*const)(A...), R(C::*)(A...))
STD_MEM_FN(R(C::*const)(A...) noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*const)(A...) &, R(C::*)(A...))
STD_MEM_FN(R(C::*const)(A...) & noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*const)(A...) const, R(C::*)(A...) const)
STD_MEM_FN(R(C::*const)(A...) const noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*const)(A...) const &, R(C::*)(A...) const)
STD_MEM_FN(R(C::*const)(A...) const & noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*const)(A...) volatile, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*const)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*const)(A...) volatile &, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*const)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*const)(A...) const volatile, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*const)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*const)(A...) const volatile &, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*const)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*const&)(A...), R(C::*)(A...))
STD_MEM_FN(R(C::*const&)(A...) noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*const&)(A...) &, R(C::*)(A...))
STD_MEM_FN(R(C::*const&)(A...) & noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*const&)(A...) const, R(C::*)(A...) const)
STD_MEM_FN(R(C::*const&)(A...) const noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*const&)(A...) const &, R(C::*)(A...) const)
STD_MEM_FN(R(C::*const&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*const&)(A...) volatile, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*const&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*const&)(A...) volatile &, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*const&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*const&)(A...) const volatile, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*const&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*const&)(A...) const volatile &, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*const&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*const&&)(A...), R(C::*)(A...))
STD_MEM_FN(R(C::*const&&)(A...) noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*const&&)(A...) &, R(C::*)(A...))
STD_MEM_FN(R(C::*const&&)(A...) & noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*const&&)(A...) const, R(C::*)(A...) const)
STD_MEM_FN(R(C::*const&&)(A...) const noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*const&&)(A...) const &, R(C::*)(A...) const)
STD_MEM_FN(R(C::*const&&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*const&&)(A...) volatile, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*const&&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*const&&)(A...) volatile &, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*const&&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*const&&)(A...) const volatile, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*const&&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*const&&)(A...) const volatile &, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*const&&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*volatile)(A...), R(C::*)(A...))
STD_MEM_FN(R(C::*volatile)(A...) noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*volatile)(A...) &, R(C::*)(A...))
STD_MEM_FN(R(C::*volatile)(A...) & noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*volatile)(A...) const, R(C::*)(A...) const)
STD_MEM_FN(R(C::*volatile)(A...) const noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*volatile)(A...) const &, R(C::*)(A...) const)
STD_MEM_FN(R(C::*volatile)(A...) const & noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*volatile)(A...) volatile, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*volatile)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*volatile)(A...) volatile &, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*volatile)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*volatile)(A...) const volatile, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*volatile)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*volatile)(A...) const volatile &, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*volatile)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*volatile&)(A...), R(C::*)(A...))
STD_MEM_FN(R(C::*volatile&)(A...) noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*volatile&)(A...) &, R(C::*)(A...))
STD_MEM_FN(R(C::*volatile&)(A...) & noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*volatile&)(A...) const, R(C::*)(A...) const)
STD_MEM_FN(R(C::*volatile&)(A...) const noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*volatile&)(A...) const &, R(C::*)(A...) const)
STD_MEM_FN(R(C::*volatile&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*volatile&)(A...) volatile, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*volatile&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*volatile&)(A...) volatile &, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*volatile&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*volatile&)(A...) const volatile, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*volatile&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*volatile&)(A...) const volatile &, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*volatile&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*volatile&&)(A...), R(C::*)(A...))
STD_MEM_FN(R(C::*volatile&&)(A...) noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*volatile&&)(A...) &, R(C::*)(A...))
STD_MEM_FN(R(C::*volatile&&)(A...) & noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*volatile&&)(A...) const, R(C::*)(A...) const)
STD_MEM_FN(R(C::*volatile&&)(A...) const noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*volatile&&)(A...) const &, R(C::*)(A...) const)
STD_MEM_FN(R(C::*volatile&&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*volatile&&)(A...) volatile, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*volatile&&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*volatile&&)(A...) volatile &, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*volatile&&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*volatile&&)(A...) const volatile, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*volatile&&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*volatile&&)(A...) const volatile &, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*volatile&&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*const volatile)(A...), R(C::*)(A...))
STD_MEM_FN(R(C::*const volatile)(A...) noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*const volatile)(A...) &, R(C::*)(A...))
STD_MEM_FN(R(C::*const volatile)(A...) & noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*const volatile)(A...) const, R(C::*)(A...) const)
STD_MEM_FN(R(C::*const volatile)(A...) const noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*const volatile)(A...) const &, R(C::*)(A...) const)
STD_MEM_FN(R(C::*const volatile)(A...) const & noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*const volatile)(A...) volatile, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*const volatile)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*const volatile)(A...) volatile &, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*const volatile)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*const volatile)(A...) const volatile, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*const volatile)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*const volatile)(A...) const volatile &, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*const volatile)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*const volatile&)(A...), R(C::*)(A...))
STD_MEM_FN(R(C::*const volatile&)(A...) noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*const volatile&)(A...) &, R(C::*)(A...))
STD_MEM_FN(R(C::*const volatile&)(A...) & noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*const volatile&)(A...) const, R(C::*)(A...) const)
STD_MEM_FN(R(C::*const volatile&)(A...) const noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*const volatile&)(A...) const &, R(C::*)(A...) const)
STD_MEM_FN(R(C::*const volatile&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*const volatile&)(A...) volatile, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*const volatile&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*const volatile&)(A...) volatile &, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*const volatile&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*const volatile&)(A...) const volatile, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*const volatile&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*const volatile&)(A...) const volatile &, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*const volatile&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*const volatile&&)(A...), R(C::*)(A...))
STD_MEM_FN(R(C::*const volatile&&)(A...) noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*const volatile&&)(A...) &, R(C::*)(A...))
STD_MEM_FN(R(C::*const volatile&&)(A...) & noexcept, R(C::*)(A...) noexcept)
STD_MEM_FN(R(C::*const volatile&&)(A...) const, R(C::*)(A...) const)
STD_MEM_FN(R(C::*const volatile&&)(A...) const noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*const volatile&&)(A...) const &, R(C::*)(A...) const)
STD_MEM_FN(R(C::*const volatile&&)(A...) const & noexcept, R(C::*)(A...) const noexcept)
STD_MEM_FN(R(C::*const volatile&&)(A...) volatile, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*const volatile&&)(A...) volatile noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*const volatile&&)(A...) volatile &, R(C::*)(A...) volatile)
STD_MEM_FN(R(C::*const volatile&&)(A...) volatile & noexcept, R(C::*)(A...) volatile noexcept)
STD_MEM_FN(R(C::*const volatile&&)(A...) const volatile, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*const volatile&&)(A...) const volatile noexcept, R(C::*)(A...) const volatile noexcept)
STD_MEM_FN(R(C::*const volatile&&)(A...) const volatile &, R(C::*)(A...) const volatile)
STD_MEM_FN(R(C::*const volatile&&)(A...) const volatile & noexcept, R(C::*)(A...) const volatile noexcept)


#undef NON_MEMBER_FUNC
#undef MEMBER_FUNC
#undef STD_MEM_FN


/////////////////////////
////    OPERATORS    ////
/////////////////////////


/// TODO: list all the special rules of container unpacking here?


/* A compile-time factory for binding keyword arguments with Python syntax.  constexpr
instances of this class can be used to provide an even more Pythonic syntax:

    constexpr auto x = py::arg<"x">;
    my_func(x = 42);
*/
template <StaticStr name>
constexpr impl::ArgFactory<name> arg {};


/* Dereference operator is used to emulate Python container unpacking. */
template <impl::python_like Self> requires (impl::iterable<Self>)
[[nodiscard]] auto operator*(Self&& self) -> impl::ArgPack<Self> {
    return {std::forward<Self>(self)};
}


/// TODO: Object::operator()


}  // namespace py


#endif

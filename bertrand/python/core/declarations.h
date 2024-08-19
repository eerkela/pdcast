#ifndef BERTRAND_PYTHON_CORE_DECLARATIONS_H
#define BERTRAND_PYTHON_CORE_DECLARATIONS_H

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <chrono>
#include <complex>
#include <concepts>
#include <deque>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <ostream>
#include <ranges>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// required for demangling
#if defined(__GNUC__) || defined(__clang__)
    #include <cxxabi.h>
    #include <cstdlib>
#elif defined(_MSC_VER)
    #include <windows.h>
    #include <dbghelp.h>
    #pragma comment(lib, "dbghelp.lib")
#endif

#define Py_BUILD_CORE

#include <Python.h>
#include <frameobject.h>
#include <internal/pycore_frame.h>  // required to assign to frame->f_lineno
#include <internal/pycore_moduleobject.h>  // required to create module subclasses

#undef Py_BUILD_CORE

#include <cpptrace/cpptrace.hpp>

#include <bertrand/static_str.h>


namespace py {
using bertrand::StaticStr;


namespace impl {
    struct BertrandTag {};
    struct TypeTag;
    struct IterTag : BertrandTag {};
    struct ArgTag : BertrandTag {};
    struct FunctionTag : BertrandTag {};
    struct ModuleTag;
    struct TupleTag : BertrandTag {};
    struct ListTag : BertrandTag{};
    struct SetTag : BertrandTag {};
    struct FrozenSetTag : BertrandTag {};
    struct KeyTag : BertrandTag {};
    struct ValueTag : BertrandTag {};
    struct ItemTag : BertrandTag {};
    struct DictTag : BertrandTag {};
    struct MappingProxyTag : BertrandTag {};

    static std::string demangle(const char* name) {
        #if defined(__GNUC__) || defined(__clang__)
            int status = 0;
            std::unique_ptr<char, void(*)(void*)> res {
                abi::__cxa_demangle(
                    name,
                    nullptr,
                    nullptr,
                    &status
                ),
                std::free
            };
            return (status == 0) ? res.get() : name;
        #elif defined(_MSC_VER)
            char undecorated_name[1024];
            if (UnDecorateSymbolName(
                name,
                undecorated_name,
                sizeof(undecorated_name),
                UNDNAME_COMPLETE
            )) {
                return std::string(undecorated_name);
            } else {
                return name;
            }
        #else
            return name; // fallback: no demangling
        #endif
    }

    template <size_t I>
    static void unpack_arg() {
        static_assert(false, "index out of range for parameter pack");
    }

    template <size_t I, typename T, typename... Ts>
    static decltype(auto) unpack_arg(T&& curr, Ts&&... next) {
        if constexpr (I == 0) {
            return std::forward<T>(curr);
        } else {
            return unpack_arg<I - 1>(std::forward<Ts>(next)...);
        }
    }

    enum class Origin {
        PYTHON,
        CPP
    };

}


/* A static RAII guard that initializes the Python interpreter the first time a Python
object is created and finalizes it when the program exits. */
struct Interpreter : impl::BertrandTag {

    /* Ensure that the interpreter is active within the given context.  This is
    called internally whenever a Python object is created from pure C++ inputs, and is
    not called in any other context in order to avoid unnecessary overhead.  It must be
    implemented as a function in order to avoid C++'s static initialization order
    fiasco. */
    static const Interpreter& init() {
        static Interpreter instance{};
        return instance;
    }

    Interpreter(const Interpreter&) = delete;
    Interpreter(Interpreter&&) = delete;

private:

    Interpreter() {
        if (!Py_IsInitialized()) {
            Py_Initialize();
        }
    }

    ~Interpreter() {
        if (Py_IsInitialized()) {
            Py_Finalize();
        }
    }
};


/// NOTE: note that std::derived_from<Object> can't be used here because any type
/// that's incomplete will fail to compile.  As such, I have to use static assertions
/// within the classes instead, so that this whole Rube Goldberg machine doesn't fall
/// apart.


struct Handle;
struct Object;
struct Code;
struct Frame;
template <typename T = Object>
struct Type;
struct BertrandMeta;
template <typename Return>
struct Iterator;
template <StaticStr Name, typename T>
struct Arg;
template <typename>
struct Function;
template <StaticStr Name>
struct Module;
struct NoneType;
struct NotImplementedType;
struct EllipsisType;
struct Slice;
struct Bool;
struct Int;
struct Float;
struct Complex;
struct Str;
struct Bytes;
struct ByteArray;
struct Date;
struct Time;
struct Datetime;
struct Timedelta;
struct Timezone;
struct Range;
template <typename Val = Object>
struct List;
template <typename Val = Object>
struct Tuple;
template <typename Key = Object>
struct Set;
template <typename Key = Object>
struct FrozenSet;
template <typename Key = Object, typename Val = Object>
struct Dict;
template <typename Map>
struct KeyView;
template <typename Map>
struct ValueView;
template <typename Map>
struct ItemView;
template <typename Map>
struct MappingProxy;


/* Base class for disabled control structures. */
struct Disable : impl::BertrandTag {
    static constexpr bool enable = false;
};


/* Base class for enabled control structures.  Encodes the return type as a template
parameter. */
template <typename T>
struct Returns : impl::BertrandTag {
    static constexpr bool enable = true;
    using type = T;
};


template <typename T>
struct __as_object__                                        : Disable {};
template <typename Derived, typename Base>
struct __isinstance__                                       : Disable {};
template <typename Derived, typename Base>
struct __issubclass__                                       : Disable {};
template <typename Self, typename... Args>
struct __init__                                             : Disable {};
template <typename Self, typename... Args>
struct __explicit_init__                                    : Disable {};
template <typename From, typename To>
struct __cast__                                             : Disable {};
template <typename From, typename To>
struct __explicit_cast__                                    : Disable {};
template <typename Self, typename... Args>
struct __call__                                             : Disable {};
template <typename Self, StaticStr Name>
struct __getattr__                                          : Disable {};
template <typename Self, StaticStr Name, typename Value>
struct __setattr__                                          : Disable {};
template <typename Self, StaticStr Name>
struct __delattr__                                          : Disable {};
template <typename Self, typename... Key>
struct __getitem__                                          : Disable {};
template <typename Self, typename Value, typename... Key>
struct __setitem__                                          : Disable {};
template <typename Self, typename... Key>
struct __delitem__                                          : Disable {};
template <typename Self>
struct __len__                                              : Disable {};
template <typename Self>
struct __iter__                                             : Disable {};
template <typename Self>
struct __reversed__                                         : Disable {};
template <typename Self, typename Key>
struct __contains__                                         : Disable {};
template <typename Self>
struct __hash__                                             : Disable {};
template <typename Self>
struct __abs__                                              : Disable {};
template <typename Self>
struct __invert__                                           : Disable {};
template <typename Self>
struct __pos__                                              : Disable {};
template <typename Self>
struct __neg__                                              : Disable {};
template <typename Self>
struct __increment__                                        : Disable {};
template <typename Self>
struct __decrement__                                        : Disable {};
template <typename L, typename R>
struct __lt__                                               : Disable {};
template <typename L, typename R>
struct __le__                                               : Disable {};
template <typename L, typename R>
struct __eq__                                               : Disable {};
template <typename L, typename R>
struct __ne__                                               : Disable {};
template <typename L, typename R>
struct __ge__                                               : Disable {};
template <typename L, typename R>
struct __gt__                                               : Disable {};
template <typename L, typename R>
struct __add__                                              : Disable {};
template <typename L, typename R>
struct __iadd__                                             : Disable {};
template <typename L, typename R>
struct __sub__                                              : Disable {};
template <typename L, typename R>
struct __isub__                                             : Disable {};
template <typename L, typename R>
struct __mul__                                              : Disable {};
template <typename L, typename R>
struct __imul__                                             : Disable {};
template <typename L, typename R>
struct __truediv__                                          : Disable {};
template <typename L, typename R>
struct __itruediv__                                         : Disable {};
template <typename L, typename R>
struct __floordiv__                                         : Disable {};
template <typename L, typename R>
struct __ifloordiv__                                        : Disable {};
template <typename L, typename R>
struct __mod__                                              : Disable {};
template <typename L, typename R>
struct __imod__                                             : Disable {};
template <typename L, typename R>
struct __pow__                                              : Disable {};
template <typename L, typename R>
struct __ipow__                                             : Disable {};
template <typename L, typename R>
struct __lshift__                                           : Disable {};
template <typename L, typename R>
struct __ilshift__                                          : Disable {};
template <typename L, typename R>
struct __rshift__                                           : Disable {};
template <typename L, typename R>
struct __irshift__                                          : Disable {};
template <typename L, typename R>
struct __and__                                              : Disable {};
template <typename L, typename R>
struct __iand__                                             : Disable {};
template <typename L, typename R>
struct __or__                                               : Disable {};
template <typename L, typename R>
struct __ior__                                              : Disable {};
template <typename L, typename R>
struct __xor__                                              : Disable {};
template <typename L, typename R>
struct __ixor__                                             : Disable {};


/* A Python interface mixin which can be used to reflect multiple inheritance within
the Object hierarchy.

When mixed with an Object base class, this class allows its interface to be separated
from the underlying PyObject* pointer, meaning several interfaces can be mixed together
without affecting the object's binary layout.  Each interface can use
`reinterpret_cast<Object>(*this)` to access the PyObject* pointer, and can further cast
that pointer to the a specific C++ type if necessary to access fields at the C++ level.

This class must be specialized for all types that wish to support multiple inheritance.
Doing so is rather tricky due to the circular dependency between the Object and its
Interface, so here's a simple example to illustrate how it's done:

    // forward declarations for Object wrapper and its Type
    struct Wrapper;
    template <>
    struct Type<Wrapper>;

    template <>
    struct Interface<Wrapper> : Interface<Base1>, Interface<Base2>, ... {
        void foo();  // forward declarations for interface methods
        int bar() const;
        static std::string baz();
    };

    template <>
    struct Interface<Type<Wrapper>> : Interface<Type<Base1>>, Interface<Type<Base2>>, ... {
        static void foo(Wrapper& self);  // non-static methods gain a self parameter
        static int bar(const Wrapper& self);
        static std::string baz();  // static methods stay the same
    };

    // define the wrapper itself
    struct Wrapper : Object, Interface<Wrapper> {
        Wrapper(Handle h, borrowed_t t) : Object(h, t) {}
        Wrapper(Handle h, stolen_t t) : Object(h, t) {}

        template <typename... Args> requires (implicit_ctor<Wrapper>::enable<Args...>)
        Wrapper(Args&&... args) : Object(
            implicit_ctor<Wrapper>{},
            std::forward<Args>(args)...
        ) {}

        template <typename... Args> requires (explicit_ctor<Wrapper>::enable<Args...>)
        explicit Wrapper(Args&&... args) : Object(
            explicit_ctor<Wrapper>{},
            std::forward<Args>(args)...
        ) {}
    };

    // define the wrapper's Python type
    template <>
    struct Type<Wrapper> : Object, Interface<Type<Wrapper>>, impl::TypeTag {
        struct __python__ : TypeTag::def<__python__, Wrapper, SomeCppObj> {
            static Type __export__(Bindings bindings) {
                // export a C++ object's interface to Python.  The base classes
                // reflect in Python the interface inheritance we defined here.
                return bindings.template finalize<Base1, Base2, ...>();
            }
        };

        // Alternatively, if the Wrapper represents a pure Python class:
        struct __python__ : TypeTag::def<__python__, Wrapper> {
            static Type __import__() {
                // get a reference to the external Python class, perhaps by importing
                // a module and getting a reference to the class object
            }
        };

        Type(Handle h, borrowed_t t) : Object(h, t) {}
        Type(Handle h, stolen_t t) : Object(h, t) {}

        template <typename... Args> requires (implicit_ctor<Type>::enable<Args...>)
        Type(Args&&... args) : Object(
            implicit_ctor<Type>{},
            std::forward<Args>(args)...
        ) {}

        template <typename... Args> requires (explicit_ctor<Type>::enable<Args...>)
        explicit Type(Args&&... args) : Object(
            explicit_ctor<Type>{},
            std::forward<Args>(args)...
        ) {}

    };

    // specialize the necessary control structures
    template <>
    struct __getattr__<Wrapper, "foo"> : Returns<Function<void()>> {};
    template <>
    struct __getattr__<Wrapper, "bar"> : Returns<Function<int()>> {};
    template <>
    struct __getattr__<Wrapper, "baz"> : Returns<Function<std::string()>> {};
    template <>
    struct __getattr__<Type<Wrapper>, "foo"> : Returns<Function<void(Wrapper&)>> {};
    template <>
    struct __getattr__<Type<Wrapper>, "bar"> : Returns<Function<int(const Wrapper&)>> {};
    template <>
    struct __getattr__<Type<Wrapper>, "baz"> : Returns<Function<std::string()>> {};
    // ... for all supported C++ operators

    // implement the interface methods
    void Interface<Wrapper>::foo() {
        print("Hello, world!");
    }
    int Interface<Wrapper>::bar() const {
        return 42;
    }
    std::string Interface<Wrapper>::baz() {
        return "static methods work too!";
    }
    void Interface<Type<Wrapper>>::foo(Wrapper& self) {
        self.foo();
    }
    int Interface<Type<Wrapper>>::bar(const Wrapper& self) {
        return self.bar();
    }
    std::string Interface<Type<Wrapper>>::baz() {
        return Wrapper::baz();
    }

This pattern is fairly rigid, as the forward declarations are necessary to prevent
circular dependencies from causing compilation errors.  It also requires that the
same interface be defined for both the Object and its Type, as well as its Python
representation, so that they can be treated symmetrically across all languages. 
However, the upside is that once it has been set up, this block of code is fully
self-contained, ensures that both the Python and C++ interfaces are kept in sync, and
can represent complex inheritance hierarchies with ease.  By inheriting from
interfaces, the C++ Object types can directly mirror any Python class hierarchy, even
accounting for multiple inheritance.  In fact, with a few `using` declarations to
resolve conflicts, the Object and its Type can even model Python-style MRO, or expose
multiple overloads at the same time. */
template <typename T>
struct Interface;


namespace impl {

    /* Trigger implicit conversion operators and/or implicit constructors, but not
    explicit ones.  In contrast, static_cast<>() will trigger explicit constructors on
    the target type, which can give unexpected results and violate type safety. */
    template <typename U>
    decltype(auto) implicit_cast(U&& value) {
        return std::forward<U>(value);
    }

    /* A convenience class that stores a static Python string for use during attr
    lookups.  Using this class ensures that only one string is allocated per attribute
    name, even if that name is repeated across multiple contexts. */
    template <StaticStr name>
    struct TemplateString : BertrandTag {
        inline static PyObject* ptr = (Interpreter::init(), PyUnicode_FromStringAndSize(
            name,
            name.size()
        ));  // NOTE: string will be garbage collected at shutdown
    };

    template <typename T>
    using iter_type = decltype(*std::ranges::begin(std::declval<T>()));

    template <typename T>
    using reverse_iter_type = decltype(*std::ranges::rbegin(std::declval<T>()));

    template <typename T, typename Key>
    using lookup_type = decltype(std::declval<T>()[std::declval<Key>()]);

    template <typename T>
    constexpr bool is_generic_helper = false;
    template <template <typename...> typename T, typename... Ts>
    constexpr bool is_generic_helper<T<Ts...>> = true;
    template <typename T>
    concept is_generic = is_generic_helper<T>;

    template <typename T, typename = void>
    constexpr bool has_interface_helper = false;
    template <typename T>
    constexpr bool has_interface_helper<T, std::void_t<Interface<T>>> = true;
    template <typename T>
    concept has_interface = has_interface_helper<T>;

    template <typename T, typename = void>
    constexpr bool has_type_helper = false;
    template <typename T>
    constexpr bool has_type_helper<T, std::void_t<Type<T>>> = true;
    template <typename T>
    concept has_type = has_type_helper<T>;

    template <typename T, typename = void>
    constexpr bool is_type_helper = false;
    template <typename T>
    constexpr bool is_type_helper<T, std::void_t<typename T::__python__>> = true;
    template <typename T>
    concept is_type = std::derived_from<T, TypeTag> && is_type_helper<T>;

    template <typename T, typename = void>
    constexpr bool is_module_helper = false;
    template <typename T>
    constexpr bool is_module_helper<T, std::void_t<typename T::__python__>> = true;
    template <typename T>
    concept is_module = std::derived_from<T, ModuleTag> && is_module_helper<T>;

    template <typename T, StaticStr Name, typename... Args>
    concept attr_is_callable_with =
        __getattr__<T, Name>::enable &&
        std::derived_from<typename __getattr__<T, Name>::type, FunctionTag> &&
        __getattr__<T, Name>::type::template invocable<Args...>;

    template <typename From, typename To>
    concept has_conversion_operator = requires(From&& from) {
        from.operator To();
    };

    template <typename From, typename To>
    concept explicitly_convertible_to = requires(From from) {
        static_cast<To>(from);
    };

    template <typename T>
    concept iterable = requires(T t) {
        { std::ranges::begin(t) } -> std::input_or_output_iterator;
        { std::ranges::end(t) } -> std::input_or_output_iterator;
    };

    template <typename T, typename Value>
    concept yields = iterable<T> && std::convertible_to<iter_type<T>, Value>;

    template <typename T>
    concept reverse_iterable = requires(T t) {
        { std::ranges::rbegin(t) } -> std::input_or_output_iterator;
        { std::ranges::rend(t) } -> std::input_or_output_iterator;
    };

    template <typename T, typename Value>
    concept yields_reverse =
        reverse_iterable<T> && std::convertible_to<reverse_iter_type<T>, Value>;

    template <typename T>
    concept iterator_like = requires(T begin, T end) {
        { *begin } -> std::convertible_to<typename std::decay_t<T>::value_type>;
        { ++begin } -> std::same_as<std::remove_reference_t<T>&>;
        { begin++ } -> std::same_as<std::remove_reference_t<T>>;
        { begin == end } -> std::convertible_to<bool>;
        { begin != end } -> std::convertible_to<bool>;
    };

    template <typename T>
    static constexpr bool is_optional_helper = false;
    template <typename T>
    static constexpr bool is_optional_helper<std::optional<T>> = true;
    template <typename T>
    static constexpr bool is_optional = is_optional_helper<std::decay_t<T>>;

    template <typename T>
    concept has_size = requires(T t) {
        { std::size(t) } -> std::convertible_to<size_t>;
    };

    template <typename T>
    concept sequence_like = iterable<T> && has_size<T> && requires(T t) {
        { t[0] } -> std::convertible_to<iter_type<T>>;
    };

    template <typename T>
    concept mapping_like = requires(T t) {
        typename std::decay_t<T>::key_type;
        typename std::decay_t<T>::mapped_type;
        { t[std::declval<typename std::decay_t<T>::key_type>()] } ->
            std::convertible_to<typename std::decay_t<T>::mapped_type>;
    };

    /// TODO: update this for multidimensional subscripting
    template <typename T, typename... Key>
    concept supports_lookup =
        !std::is_pointer_v<T> &&
        !std::integral<std::decay_t<T>> &&
        requires(T t, Key... key) {
            { t[key...] };
        };

    /// TODO: update this for multidimensional subscripting
    template <typename T, typename Value, typename... Key>
    concept lookup_yields = supports_lookup<T, Key...> && requires(T t, Key... key) {
        { t[key...] } -> std::convertible_to<Value>;
    };

    /// TODO: update this for multidimensional subscripting
    template <typename T, typename Value, typename... Key>
    concept supports_item_assignment =
        !std::is_pointer_v<T> &&
        !std::integral<std::decay_t<T>> &&
        requires(T t, Key... key, Value value) {
            { t[key...] = value };
        };

    template <typename T>
    concept pair_like = std::tuple_size<T>::value == 2 && requires(T t) {
        { std::get<0>(t) };
        { std::get<1>(t) };
    };

    template <typename T, typename First, typename Second>
    concept pair_like_with = pair_like<T> && requires(T t) {
        { std::get<0>(t) } -> std::convertible_to<First>;
        { std::get<1>(t) } -> std::convertible_to<Second>;
    };

    template <typename T>
    concept yields_pairs = iterable<T> && pair_like<iter_type<T>>;

    template <typename T, typename First, typename Second>
    concept yields_pairs_with =
        iterable<T> && pair_like_with<iter_type<T>, First, Second>;

    template <typename T>
    concept hashable = requires(T t) {
        { std::hash<std::decay_t<T>>{}(t) } -> std::convertible_to<size_t>;
    };

    template <typename T>
    concept has_abs = requires(T t) {
        { std::abs(t) };
    };

    template <typename T>
    using abs_type = decltype(std::abs(std::declval<T>()));

    template <typename T, typename Return>
    concept abs_returns = requires(T t) {
        { std::abs(t) } -> std::convertible_to<Return>;
    };

    template <typename T>
    concept has_invert = requires(T t) {
        { ~t };
    };

    template <typename T>
    using invert_type = decltype(~std::declval<T>());

    template <typename T, typename Return>
    concept invert_returns = requires(T t) {
        { ~t } -> std::convertible_to<Return>;
    };

    template <typename T>
    concept has_pos = requires(T t) {
        { +t };
    };

    template <typename T>
    using pos_type = decltype(+std::declval<T>());

    template <typename T, typename Return>
    concept pos_returns = requires(T t) {
        { +t } -> std::convertible_to<Return>;
    };

    template <typename T>
    concept has_neg = requires(T t) {
        { -t };
    };

    template <typename T>
    using neg_type = decltype(-std::declval<T>());

    template <typename T, typename Return>
    concept neg_returns = requires(T t) {
        { -t } -> std::convertible_to<Return>;
    };

    template <typename T>
    concept has_preincrement = requires(T t) {
        { ++t } -> std::convertible_to<T&>;
    };

    template <typename T>
    using preincrement_type = decltype(++std::declval<T>());

    template <typename T>
    concept has_postincrement = requires(T t) {
        { t++ } -> std::convertible_to<T>;
    };

    template <typename T>
    using postincrement_type = decltype(std::declval<T>()++);

    template <typename T>
    concept has_predecrement = requires(T t) {
        { --t } -> std::convertible_to<T&>;
    };

    template <typename T>
    using predecrement_type = decltype(--std::declval<T>());

    template <typename T>
    concept has_postdecrement = requires(T t) {
        { t-- } -> std::convertible_to<T>;
    };

    template <typename T>
    using postdecrement_type = decltype(std::declval<T>()--);

    template <typename L, typename R>
    concept has_lt = requires(L l, R r) {
        { l < r };
    };

    template <typename L, typename R>
    using lt_type = decltype(std::declval<L>() < std::declval<R>());

    template <typename L, typename R, typename Return>
    concept lt_returns = requires(L l, R r) {
        { l < r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_le = requires(L l, R r) {
        { l <= r };
    };

    template <typename L, typename R>
    using le_type = decltype(std::declval<L>() <= std::declval<R>());

    template <typename L, typename R, typename Return>
    concept le_returns = requires(L l, R r) {
        { l <= r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_eq = requires(L l, R r) {
        { l == r };
    };

    template <typename L, typename R>
    using eq_type = decltype(std::declval<L>() == std::declval<R>());

    template <typename L, typename R, typename Return>
    concept eq_returns = requires(L l, R r) {
        { l == r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_ne = requires(L l, R r) {
        { l != r };
    };

    template <typename L, typename R>
    using ne_type = decltype(std::declval<L>() != std::declval<R>());

    template <typename L, typename R, typename Return>
    concept ne_returns = requires(L l, R r) {
        { l != r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_ge = requires(L l, R r) {
        { l >= r };
    };

    template <typename L, typename R>
    using ge_type = decltype(std::declval<L>() >= std::declval<R>());

    template <typename L, typename R, typename Return>
    concept ge_returns = requires(L l, R r) {
        { l >= r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_gt = requires(L l, R r) {
        { l > r };
    };

    template <typename L, typename R>
    using gt_type = decltype(std::declval<L>() > std::declval<R>());

    template <typename L, typename R, typename Return>
    concept gt_returns = requires(L l, R r) {
        { l > r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_add = requires(L l, R r) {
        { l + r };
    };

    template <typename L, typename R>
    using add_type = decltype(std::declval<L>() + std::declval<R>());

    template <typename L, typename R, typename Return>
    concept add_returns = requires(L l, R r) {
        { l + r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_iadd = requires(L& l, R r) {
        { l += r } -> std::convertible_to<L&>;
    };

    template <typename L, typename R>
    using iadd_type = decltype(std::declval<L&>() += std::declval<R>());

    template <typename L, typename R>
    concept has_sub = requires(L l, R r) {
        { l - r };
    };

    template <typename L, typename R>
    using sub_type = decltype(std::declval<L>() - std::declval<R>());

    template <typename L, typename R, typename Return>
    concept sub_returns = requires(L l, R r) {
        { l - r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_isub = requires(L& l, R r) {
        { l -= r } -> std::convertible_to<L&>;
    };

    template <typename L, typename R>
    using isub_type = decltype(std::declval<L&>() -= std::declval<R>());

    template <typename L, typename R>
    concept has_mul = requires(L l, R r) {
        { l * r };
    };

    template <typename L, typename R>
    using mul_type = decltype(std::declval<L>() * std::declval<R>());

    template <typename L, typename R, typename Return>
    concept mul_returns = requires(L l, R r) {
        { l * r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_imul = requires(L& l, R r) {
        { l *= r } -> std::convertible_to<L&>;
    };

    template <typename L, typename R>
    using imul_type = decltype(std::declval<L&>() *= std::declval<R>());

    template <typename L, typename R>
    concept has_truediv = requires(L l, R r) {
        { l / r };
    };

    template <typename L, typename R>
    using truediv_type = decltype(std::declval<L>() / std::declval<R>());

    template <typename L, typename R, typename Return>
    concept truediv_returns = requires(L l, R r) {
        { l / r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_itruediv = requires(L& l, R r) {
        { l /= r } -> std::convertible_to<L&>;
    };

    template <typename L, typename R>
    using itruediv_type = decltype(std::declval<L&>() /= std::declval<R>());

    template <typename L, typename R>
    concept has_mod = requires(L l, R r) {
        { l % r };
    };

    template <typename L, typename R>
    using mod_type = decltype(std::declval<L>() % std::declval<R>());

    template <typename L, typename R, typename Return>
    concept mod_returns = requires(L l, R r) {
        { l % r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_imod = requires(L& l, R r) {
        { l %= r } -> std::convertible_to<L&>;
    };

    template <typename L, typename R>
    using imod_type = decltype(std::declval<L&>() %= std::declval<R>());

    template <typename L, typename R>
    concept has_pow = requires(L l, R r) {
        { std::pow(l, r) };
    };

    template <typename L, typename R>
    using pow_type = decltype(std::pow(std::declval<L>(), std::declval<R>()));

    template <typename L, typename R, typename Return>
    concept pow_returns = requires(L l, R r) {
        { std::pow(l, r) } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_lshift = requires(L l, R r) {
        { l << r };
    };

    template <typename L, typename R>
    using lshift_type = decltype(std::declval<L>() << std::declval<R>());

    template <typename L, typename R, typename Return>
    concept lshift_returns = requires(L l, R r) {
        { l << r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_ilshift = requires(L& l, R r) {
        { l <<= r } -> std::convertible_to<L&>;
    };

    template <typename L, typename R>
    using ilshift_type = decltype(std::declval<L&>() <<= std::declval<R>());

    template <typename L, typename R>
    concept has_rshift = requires(L l, R r) {
        { l >> r };
    };

    template <typename L, typename R>
    using rshift_type = decltype(std::declval<L>() >> std::declval<R>());

    template <typename L, typename R, typename Return>
    concept rhsift_returns = requires(L l, R r) {
        { l >> r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_irshift = requires(L& l, R r) {
        { l >>= r } -> std::convertible_to<L&>;
    };

    template <typename L, typename R>
    using irshift_type = decltype(std::declval<L&>() >>= std::declval<R>());

    template <typename L, typename R>
    concept has_and = requires(L l, R r) {
        { l & r };
    };

    template <typename L, typename R>
    using and_type = decltype(std::declval<L>() & std::declval<R>());

    template <typename L, typename R, typename Return>
    concept and_returns = requires(L l, R r) {
        { l & r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_iand = requires(L& l, R r) {
        { l &= r } -> std::convertible_to<L&>;
    };

    template <typename L, typename R>
    using iand_type = decltype(std::declval<L&>() &= std::declval<R>());

    template <typename L, typename R>
    concept has_or = requires(L l, R r) {
        { l | r };
    };

    template <typename L, typename R>
    using or_type = decltype(std::declval<L>() | std::declval<R>());

    template <typename L, typename R, typename Return>
    concept or_returns = requires(L l, R r) {
        { l | r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_ior = requires(L& l, R r) {
        { l |= r } -> std::convertible_to<L&>;
    };

    template <typename L, typename R>
    using ior_type = decltype(std::declval<L&>() |= std::declval<R>());

    template <typename L, typename R>
    concept has_xor = requires(L l, R r) {
        { l ^ r };
    };

    template <typename L, typename R>
    using xor_type = decltype(std::declval<L>() ^ std::declval<R>());

    template <typename L, typename R, typename Return>
    concept xor_returns = requires(L l, R r) {
        { l ^ r } -> std::convertible_to<Return>;
    };

    template <typename L, typename R>
    concept has_ixor = requires(L& l, R r) {
        { l ^= r } -> std::convertible_to<L&>;
    };

    template <typename L, typename R>
    using ixor_type = decltype(std::declval<L&>() ^= std::declval<R>());

    template <typename T>
    concept has_concat = requires(const T& lhs, const T& rhs) {
        { lhs + rhs } -> std::convertible_to<T>;
    };

    template <typename T>
    concept has_inplace_concat = requires(T& lhs, const T& rhs) {
        { lhs += rhs } -> std::convertible_to<T&>;
    };

    template <typename T>
    concept has_repeat = requires(const T& lhs, size_t rhs) {
        { lhs * rhs } -> std::convertible_to<T>;
    };

    template <typename T>
    concept has_inplace_repeat = requires(T& lhs, size_t rhs) {
        { lhs *= rhs } -> std::convertible_to<T&>;
    };

    template <typename T>
    concept has_operator_bool = requires(T t) {
        { !t } -> std::convertible_to<bool>;
    };

    template <typename T>
    concept has_to_string = requires(T t) {
        { std::to_string(t) } -> std::convertible_to<std::string>;
    };

    template <typename T>
    concept has_stream_insertion = requires(std::ostream& os, T t) {
        { os << t } -> std::convertible_to<std::ostream&>;
    };

    template <typename T>
    concept has_call_operator = requires { &std::decay_t<T>::operator(); };

    template <typename T>
    concept is_callable_any = 
        std::is_function_v<std::remove_pointer_t<std::decay_t<T>>> ||
        std::is_member_function_pointer_v<std::decay_t<T>> ||
        has_call_operator<T>;

    template <typename T>
    concept string_literal = requires(T t) {
        { []<size_t N>(const char(&)[N]){}(t) };
    };

    template <typename T>
    concept complex_like = requires(T t) {
        { t.real() } -> std::convertible_to<double>;
        { t.imag() } -> std::convertible_to<double>;
    };

    template <typename T>
    concept has_empty = requires(T t) {
        { t.empty() } -> std::convertible_to<bool>;
    };

    template <typename T>
    concept has_reserve = requires(T t, size_t n) {
        { t.reserve(n) } -> std::same_as<void>;
    };

    template <typename T, typename Key>
    concept has_contains = requires(T t, Key key) {
        { t.contains(key) } -> std::convertible_to<bool>;
    };

    template <typename T>
    concept has_keys = requires(T t) {
        { t.keys() } -> iterable;
        { t.keys() } -> yields<typename std::decay_t<T>::key_type>;
    };

    template <typename T>
    concept has_values = requires(T t) {
        { t.values() } -> iterable;
        { t.values() } -> yields<typename std::decay_t<T>::mapped_type>;
    };

    template <typename T>
    concept has_items = requires(T t) {
        { t.items() } -> iterable;
        { t.items() } -> yields_pairs_with<
            typename std::decay_t<T>::key_type,
            typename std::decay_t<T>::mapped_type
        >;
    };

    template <typename T>
    concept bertrand_like = std::derived_from<std::decay_t<T>, BertrandTag>;

    template <typename T>
    concept python_like = std::derived_from<std::decay_t<T>, Handle>;

    template <typename... Ts>
    concept any_are_python_like = (python_like<Ts> || ...);

    template <typename T>
    concept dynamic_type = 
        std::same_as<std::decay_t<T>, Handle> || std::same_as<std::decay_t<T>, Object>;

    template <typename T>
    concept originates_from_python =
        std::derived_from<std::remove_cvref_t<T>, Object> &&
        has_type<std::remove_cvref_t<T>> &&
        is_type<Type<std::remove_cvref_t<T>>> &&
        Type<std::remove_cvref_t<T>>::__python__::__origin__ == Origin::PYTHON;

    template <typename T>
    concept cpp_like = !python_like<T>;

    template <typename T>
    concept originates_from_cpp =
        std::derived_from<std::remove_cvref_t<T>, Object> &&
        has_type<std::remove_cvref_t<T>> &&
        is_type<Type<std::remove_cvref_t<T>>> &&
        Type<std::remove_cvref_t<T>>::__python__::__origin__ == Origin::CPP;

    template <typename T>
    concept cpp_or_originates_from_cpp = cpp_like<T> || originates_from_cpp<T>;

    template <cpp_or_originates_from_cpp T>
    using cpp_type = std::conditional_t<
        originates_from_cpp<T>,
        typename Type<std::remove_cvref_t<T>>::__python__::t_cpp,
        T
    >;

    template <typename T>
    concept type_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, TypeTag>;

    template <typename T>
    concept none_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, NoneType>;

    template <typename T>
    concept notimplemented_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, NotImplementedType>;

    template <typename T>
    concept ellipsis_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, EllipsisType>;

    template <typename T>
    concept slice_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, Slice>;

    template <typename T>
    concept module_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, ModuleTag>;

    template <typename T>
    concept bool_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, Bool>;

    template <typename T>
    concept int_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, Int>;

    template <typename T>
    concept float_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, Float>;

    template <typename T>
    concept str_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, Str>;

    template <typename T>
    concept bytes_like = (
        string_literal<T> ||
        std::same_as<std::decay_t<T>, void*> || (
            __as_object__<std::remove_cvref_t<T>>::enable &&
            std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, Bytes>
        )
    );

    template <typename T>
    concept bytearray_like = (
        string_literal<T> ||
        std::same_as<std::decay_t<T>, void*> || (
            __as_object__<std::remove_cvref_t<T>>::enable &&
            std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, ByteArray>
        )
    );

    template <typename T>
    concept anybytes_like = bytes_like<T> || bytearray_like<T>;

    template <typename T>
    concept timedelta_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, Timedelta>;

    template <typename T>
    concept timezone_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, Timezone>;

    template <typename T>
    concept date_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, Date>;

    template <typename T>
    concept time_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, Time>;

    template <typename T>
    concept datetime_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, Datetime>;

    template <typename T>
    concept range_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, Range>;

    template <typename T>
    concept tuple_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, TupleTag>;

    template <typename T>
    concept list_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, ListTag>;

    template <typename T>
    concept set_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, SetTag>;

    template <typename T>
    concept frozenset_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, FrozenSetTag>;

    template <typename T>
    concept anyset_like = set_like<T> || frozenset_like<T>;

    template <typename T>
    concept dict_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, DictTag>;

    template <typename T>
    concept mappingproxy_like =
        __as_object__<std::remove_cvref_t<T>>::enable &&
        std::derived_from<typename __as_object__<std::remove_cvref_t<T>>::type, MappingProxyTag>;

    template <typename T>
    concept anydict_like = dict_like<T> || mappingproxy_like<T>;

    /* NOTE: some binary operators (such as lexicographic comparisons) accept generic
     * containers, which may be combined with containers of different types.  In these
     * cases, the operator should be enabled if and only if it is also supported by the
     * respective element types.  This sounds simple, but is complicated by the
     * implementation of std::pair and std::tuple, which may contain heterogenous types.
     *
     * The Broadcast<> struct helps by recursively applying a scalar constraint over
     * the values of a generic container type, with specializations to account for
     * std::pair and std::tuple.  A generic specialization is provided for all types
     * that implement a nested `value_type`.  Note that the condition must be a type
     * trait (not a concept) in order to be valid as a template template parameter.
     */

    template <typename L, typename R>
    struct lt_comparable : BertrandTag {
        static constexpr bool value = requires(L a, R b) {
            { a < b } -> std::convertible_to<bool>;
        };
    };

    template <typename L, typename R>
    struct le_comparable : BertrandTag {
        static constexpr bool value = requires(L a, R b) {
            { a <= b } -> std::convertible_to<bool>;
        };
    };

    template <typename L, typename R>
    struct eq_comparable : BertrandTag {
        static constexpr bool value = requires(L a, R b) {
            { a == b } -> std::convertible_to<bool>;
        };
    };

    template <typename L, typename R>
    struct ne_comparable : BertrandTag {
        static constexpr bool value = requires(L a, R b) {
            { a != b } -> std::convertible_to<bool>;
        };
    };

    template <typename L, typename R>
    struct ge_comparable : BertrandTag {
        static constexpr bool value = requires(L a, R b) {
            { a >= b } -> std::convertible_to<bool>;
        };
    };

    template <typename L, typename R>
    struct gt_comparable : BertrandTag {
        static constexpr bool value = requires(L a, R b) {
            { a > b } -> std::convertible_to<bool>;
        };
    };

    template <
        template <typename, typename> typename Condition,
        typename L,
        typename R
    >
    struct Broadcast : BertrandTag {
        template <typename T>
        struct deref { using type = T; };
        template <iterable T>
        struct deref<T> { using type = iter_type<T>; };

        static constexpr bool value = Condition<
            typename deref<L>::type,
            typename deref<R>::type
        >::value;
    };

    template <
        template <typename, typename> typename Condition,
        typename T1,
        typename T2,
        typename T3,
        typename T4
    >
    struct Broadcast<Condition, std::pair<T1, T2>, std::pair<T3, T4>> : BertrandTag {
        static constexpr bool value =
            Broadcast<Condition, T1, std::pair<T3, T4>>::value &&
            Broadcast<Condition, T2, std::pair<T3, T4>>::value;
    };

    template <
        template <typename, typename> typename Condition,
        typename L,
        typename T1,
        typename T2
    >
    struct Broadcast<Condition, L, std::pair<T1, T2>> : BertrandTag {
        static constexpr bool value =
            Broadcast<Condition, L, T1>::value && Broadcast<Condition, L, T2>::value;
    };

    template <
        template <typename, typename> typename Condition,
        typename T1,
        typename T2,
        typename R
    >
    struct Broadcast<Condition, std::pair<T1, T2>, R> : BertrandTag {
        static constexpr bool value =
            Broadcast<Condition, T1, R>::value && Broadcast<Condition, T2, R>::value;
    };

    template <
        template <typename, typename> typename Condition,
        typename... Ts1,
        typename... Ts2
    >
    struct Broadcast<Condition, std::tuple<Ts1...>, std::tuple<Ts2...>> : BertrandTag {
        static constexpr bool value =
            (Broadcast<Condition, Ts1, std::tuple<Ts2...>>::value && ...);
    };

    template <
        template <typename, typename> typename Condition,
        typename L,
        typename... Ts
    >
    struct Broadcast<Condition, L, std::tuple<Ts...>> : BertrandTag {
        static constexpr bool value =
            (Broadcast<Condition, L, Ts>::value && ...);
    };

    template <
        template <typename, typename> typename Condition,
        typename... Ts,
        typename R
    >
    struct Broadcast<Condition, std::tuple<Ts...>, R> : BertrandTag {
        static constexpr bool value =
            (Broadcast<Condition, Ts, R>::value && ...);
    };

}


/* A simple tag struct that can be passed to an index or attribute assignment operator
to invoke a Python-level `@property` deleter, `__delattr__()`, or `__delitem__()`
method.  This is the closest equivalent to replicating Python's `del` keyword in the
cases where it matters, and is not superceded by automatic reference counting. */
struct del : impl::BertrandTag {};


/* Wrap a non-owning, mutable reference to a C++ object into a `py::Object` proxy that
exposes it to Python.  Note that this only works if a corresponding `py::Object`
subclass exists, which was declared using the `__python__` CRTP helper, and whose C++
type exactly matches the argument.

WARNING: This function is unsafe and should be used with caution.  It is the caller's
responsibility to make sure that the underlying object outlives the wrapper, otherwise
undefined behavior will occur.  It is mostly intended for internal use in order to
expose shared state to Python, for instance to model exported global variables. */
template <typename T>
    requires (
        __as_object__<T>::enable &&
        impl::originates_from_cpp<typename __as_object__<T>::type> &&
        std::same_as<T, typename __as_object__<T>::type::__python__::t_cpp>
    )
[[nodiscard]] auto wrap(T& obj) -> __as_object__<T>::type;


/* Wrap a non-owning, immutable reference to a C++ object into a `py::Object` proxy
that exposes it to Python.  Note that this only works if a corresponding `py::Object`
subclass exists, which was declared using the `__python__` CRTP helper, and whose C++
type exactly matches the argument.

WARNING: This function is unsafe and should be used with caution.  It is the caller's
responsibility to make sure that the underlying object outlives the wrapper, otherwise
undefined behavior will occur.  It is mostly intended for internal use in order to
expose shared state to Python, for instance to model exported global variables. */
template <typename T>
    requires (
        __as_object__<T>::enable &&
        impl::originates_from_cpp<typename __as_object__<T>::type> &&
        std::same_as<T, typename __as_object__<T>::type::__python__::t_cpp>
    )
[[nodiscard]] auto wrap(const T& obj) -> __as_object__<T>::type;


/* Retrieve a reference to the internal C++ object that backs a `py::Object` wrapper.
Note that this only works if the wrapper was declared using the `__python__` CRTP
helper.  If the wrapper does not own the backing object, this method will follow the
pointer to resolve the reference. */
template <typename T> requires (impl::cpp_or_originates_from_cpp<T>)
[[nodiscard]] auto& unwrap(T& obj);


/* Retrieve a reference to the internal C++ object that backs a `py::Object` wrapper.
Note that this only works if the wrapper was declared using the `__python__` CRTP
helper.  If the wrapper does not own the backing object, this method will follow the
pointer to resolve the reference. */
template <typename T> requires (impl::cpp_or_originates_from_cpp<T>)
[[nodiscard]] const auto& unwrap(const T& obj);


}  // namespace py


#endif

#if !defined(BERTRAND_PYTHON_INCLUDED) && !defined(LINTER)
#error "This file should not be included directly.  Please include <bertrand/python.h> instead."
#endif

#ifndef BERTRAND_PYTHON_SET_H
#define BERTRAND_PYTHON_SET_H

#include "common.h"


// TODO: check to make sure control structs are properly enabled after CTAD refactor


namespace bertrand {
namespace py {


/////////////////////////
////    FROZENSET    ////
/////////////////////////


template <std::derived_from<impl::FrozenSetTag> Self>
struct __getattr__<Self, "copy">                                : Returns<Function> {};
template <std::derived_from<impl::FrozenSetTag> Self>
struct __getattr__<Self, "isdisjoint">                          : Returns<Function> {};
template <std::derived_from<impl::FrozenSetTag> Self>
struct __getattr__<Self, "issubset">                            : Returns<Function> {};
template <std::derived_from<impl::FrozenSetTag> Self>
struct __getattr__<Self, "issuperset">                          : Returns<Function> {};
template <std::derived_from<impl::FrozenSetTag> Self>
struct __getattr__<Self, "union">                               : Returns<Function> {};
template <std::derived_from<impl::FrozenSetTag> Self>
struct __getattr__<Self, "intersection">                        : Returns<Function> {};
template <std::derived_from<impl::FrozenSetTag> Self>
struct __getattr__<Self, "difference">                          : Returns<Function> {};
template <std::derived_from<impl::FrozenSetTag> Self>
struct __getattr__<Self, "symmetric_difference">                : Returns<Function> {};
template <std::derived_from<impl::FrozenSetTag> Self>
struct __len__<Self>                                            : Returns<size_t> {};
template <std::derived_from<impl::FrozenSetTag> Self>
struct __hash__<Self>                                           : Returns<size_t> {};
template <std::derived_from<impl::FrozenSetTag> Self>
struct __iter__<Self>                                           : Returns<typename Self::key_type> {};
template <std::derived_from<impl::FrozenSetTag> Self>
struct __reversed__<Self>                                       : Returns<typename Self::key_type> {};
template <
    std::derived_from<impl::FrozenSetTag> Self,
    std::convertible_to<typename Self::key_type> Key
>
struct __contains__<Self, Key>                                  : Returns<bool> {};
template <std::derived_from<impl::FrozenSetTag> Self, impl::anyset_like T>
struct __lt__<Self, T>                                          : Returns<bool> {};
template <impl::anyset_like T, std::derived_from<impl::FrozenSetTag> Self>
    requires (!std::derived_from<T, impl::FrozenSetTag>)
struct __lt__<T, Self>                                          : Returns<bool> {};
template <std::derived_from<impl::FrozenSetTag> Self, impl::anyset_like T>
struct __le__<Self, T>                                          : Returns<bool> {};
template <impl::anyset_like T, std::derived_from<impl::FrozenSetTag> Self>
    requires (!std::derived_from<T, impl::FrozenSetTag>)
struct __le__<T, Self>                                          : Returns<bool> {};
template <std::derived_from<impl::FrozenSetTag> Self, impl::anyset_like T>
struct __eq__<Self, T>                                          : Returns<bool> {};
template <impl::anyset_like T, std::derived_from<impl::FrozenSetTag> Self>
    requires (!std::derived_from<T, impl::FrozenSetTag>)
struct __eq__<T, Self>                                          : Returns<bool> {};
template <std::derived_from<impl::FrozenSetTag> Self, impl::anyset_like T>
struct __ne__<Self, T>                                          : Returns<bool> {};
template <impl::anyset_like T, std::derived_from<impl::FrozenSetTag> Self>
    requires (!std::derived_from<T, impl::FrozenSetTag>)
struct __ne__<T, Self>                                          : Returns<bool> {};
template <std::derived_from<impl::FrozenSetTag> Self, impl::anyset_like T>
struct __ge__<Self, T>                                          : Returns<bool> {};
template <impl::anyset_like T, std::derived_from<impl::FrozenSetTag> Self>
    requires (!std::derived_from<T, impl::FrozenSetTag>)
struct __ge__<T, Self>                                          : Returns<bool> {};
template <std::derived_from<impl::FrozenSetTag> Self, impl::anyset_like T>
struct __gt__<Self, T>                                          : Returns<bool> {};
template <impl::anyset_like T, std::derived_from<impl::FrozenSetTag> Self>
    requires (!std::derived_from<T, impl::FrozenSetTag>)
struct __gt__<T, Self>                                          : Returns<bool> {};
template <std::derived_from<impl::FrozenSetTag> Self, typename T>
    requires (Self::template check<T>())
struct __or__<Self, T>                                          : Returns<Self> {};
template <
    std::derived_from<impl::FrozenSetTag> T,
    std::derived_from<impl::FrozenSetTag> Self
> requires (!T::template check<Self>() && Self::template check<T>())
struct __or__<T, Self>                                          : Returns<Self> {};
template <std::derived_from<impl::FrozenSetTag> Self, typename T>
    requires (Self::template check<T>())
struct __ior__<Self, T>                                         : Returns<Self&> {};
template <std::derived_from<impl::FrozenSetTag> Self, typename T>
    requires (Self::template check<T>())
struct __and__<Self, T>                                         : Returns<Self> {};
template <
    std::derived_from<impl::FrozenSetTag> T,
    std::derived_from<impl::FrozenSetTag> Self
> requires (!T::template check<Self>() && Self::template check<T>())
struct __and__<T, Self>                                         : Returns<Self> {};
template <std::derived_from<impl::FrozenSetTag> Self, typename T>
    requires (Self::template check<T>())
struct __iand__<Self, T>                                        : Returns<Self&> {};
template <std::derived_from<impl::FrozenSetTag> Self, typename T>
    requires (Self::template check<T>())
struct __sub__<Self, T>                                         : Returns<Self> {};
template <
    std::derived_from<impl::FrozenSetTag> T,
    std::derived_from<impl::FrozenSetTag> Self
> requires (!T::template check<Self>() && Self::template check<T>())
struct __sub__<T, Self>                                         : Returns<Self> {};
template <std::derived_from<impl::FrozenSetTag> Self, typename T>
    requires (Self::template check<T>())
struct __isub__<Self, T>                                        : Returns<Self&> {};
template <std::derived_from<impl::FrozenSetTag> Self, typename T>
    requires (Self::template check<T>())
struct __xor__<Self, T>                                         : Returns<Self> {};
template <
    std::derived_from<impl::FrozenSetTag> T,
    std::derived_from<impl::FrozenSetTag> Self
> requires (!T::template check<Self>() && Self::template check<T>())
struct __xor__<T, Self>                                         : Returns<Self> {};
template <std::derived_from<impl::FrozenSetTag> Self, typename T>
    requires (Self::template check<T>())
struct __ixor__<Self, T>                                        : Returns<Self&> {};


namespace impl {
namespace ops {

    template <typename Return, std::derived_from<FrozenSetTag> Self>
    struct len<Return, Self> {
        static size_t operator()(const Self& self) {
            return PySet_GET_SIZE(self.ptr());
        }
    };

    // TODO: to_object might need a rethink given CTAD container templates

    template <typename Return, std::derived_from<FrozenSetTag> Self, typename Key>
    struct contains<Return, Self, Key> {
        static bool operator()(const Self& self, const to_object<Key>& key) {
            int result = PySet_Contains(self.ptr(), key.ptr());
            if (result == -1) {
                Exception::from_python();
            }
            return result;
        }
    };

}
}


template <typename T>
FrozenSet(const std::initializer_list<T>&) -> FrozenSet<to_python<T>>;
template <impl::is_iterable T>
FrozenSet(T) -> FrozenSet<to_python<impl::dereference_type<T>>>;
template <typename T, typename... Args>
    requires (!impl::is_iterable<T> && !impl::str_like<T>)
FrozenSet(T, Args...) -> FrozenSet<Object>;
template <impl::str_like T>
FrozenSet(T) -> FrozenSet<Str>;
template <size_t N>
FrozenSet(const char(&)[N]) -> FrozenSet<Str>;


/* Represents a statically-typed Python `frozenset` object in C++. */
template <typename Key>
class FrozenSet : public Object, public impl::FrozenSetTag {
    using Base = Object;
    static_assert(
        std::derived_from<Key, Object>,
        "py::FrozenSet can only contain types derived from py::Object."
    );
    static_assert(
        impl::is_hashable<Key>,
        "py::FrozenSet can only contain hashable types."
    );

    static constexpr bool generic = std::same_as<Key, Object>;

    template <typename T>
    static constexpr bool typecheck = std::derived_from<T, Object> ?
        std::derived_from<T, Key> : std::convertible_to<T, Key>;

public:
    using impl::FrozenSetTag::type;

    using size_type = size_t;
    using key_type = Key;
    using value_type = Key;
    using pointer = key_type*;
    using reference = key_type&;
    using const_pointer = const key_type*;
    using const_reference = const key_type&;
    using iterator = impl::Iterator<impl::GenericIter<key_type>>;
    using const_iterator = impl::Iterator<impl::GenericIter<const key_type>>;
    using reverse_iterator = impl::ReverseIterator<impl::GenericIter<key_type>>;
    using const_reverse_iterator = impl::ReverseIterator<impl::GenericIter<const key_type>>;

    template <typename T>
    static consteval bool check() {
        if constexpr (!impl::frozenset_like<std::decay_t<T>>) {
            return false;
        } else if constexpr (impl::pybind11_like<std::decay_t<T>>) {
            return generic;
        } else if constexpr (impl::is_iterable<std::decay_t<T>>) {
            return typecheck<impl::dereference_type<std::decay_t<T>>>;
        } else {
            return false;
        }
    }

    template <typename T>
    static constexpr bool check(const T& obj) {
        if constexpr (impl::cpp_like<T>) {
            return check<T>();

        } else if constexpr (impl::is_object_exact<T>) {
            if constexpr (generic) {
                return obj.ptr() != nullptr && PyFrozenSet_Check(obj.ptr());
            } else {
                return (
                    obj.ptr() != nullptr && PyFrozenSet_Check(obj.ptr()) &&
                    std::ranges::all_of(obj, [](const auto& item) {
                        return key_type::check(item);
                    })
                );
            }

        } else if constexpr (
            std::derived_from<T, FrozenSet<Object>> ||
            std::derived_from<T, pybind11::frozenset>
        ) {
            if constexpr (generic) {
                return obj.ptr() != nullptr;
            } else {
                return (
                    obj.ptr() != nullptr &&
                    std::ranges::all_of(obj, [](const auto& item) {
                        return key_type::check(item);
                    })
                );
            }

        } else if constexpr (impl::frozenset_like<T>) {
            return obj.ptr() != nullptr && typecheck<impl::dereference_type<T>>;

        } else {
            return false;
        }
    }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    FrozenSet(Handle h, const borrowed_t& t) : Base(h, t) {}
    FrozenSet(Handle h, const stolen_t& t) : Base(h, t) {}

    template <typename Policy>
    FrozenSet(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<FrozenSet>(accessor).release(), stolen_t{})
    {}

    /* Default constructor.  Initializes to an empty set. */
    FrozenSet() : Base(PyFrozenSet_New(nullptr), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
    }

    /* Pack the contents of a braced initializer list into a new Python frozenset. */
    FrozenSet(const std::initializer_list<key_type>& contents) :
        Base(PyFrozenSet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            for (const key_type& item : contents) {
                if (PySet_Add(m_ptr, item.ptr())) {
                    Exception::from_python();
                }
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Copy/move constructors from equivalent pybind11 types or other frozensets with
    a narrower key type. */
    template <impl::python_like T> requires (check<T>())
    FrozenSet(T&& other) : Base(std::forward<T>(other)) {}

    /* Explicitly unpack an arbitrary Python container into a new py::FrozenSet. */
    template <impl::python_like T> requires (!impl::frozenset_like<T> && impl::is_iterable<T>)
    explicit FrozenSet(const T& contents) :
        Base(PyFrozenSet_New(contents.ptr()), stolen_t{})
    {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
    }

    /* Explicitly unpack an arbitrary C++ container into a new py::FrozenSet. */
    template <impl::cpp_like T> requires (impl::is_iterable<T>)
    explicit FrozenSet(const T& contents) : Base(PyFrozenSet_New(nullptr), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            for (const auto& item : contents) {
                if (PySet_Add(m_ptr, key_type(item).ptr())) {
                    Exception::from_python();
                }
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Construct a new FrozenSet from a pair of input iterators. */
    template <typename Iter, std::sentinel_for<Iter> Sentinel>
    explicit FrozenSet(Iter first, Sentinel last) :
        Base(PyFrozenSet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            while (first != last) {
                if (PySet_Add(m_ptr, key_type(*first).ptr())) {
                    Exception::from_python();
                }
                ++first;
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack a std::pair into a py::FrozenSet. */
    template <typename First, typename Second>
        requires (
            std::constructible_from<key_type, First> &&
            std::constructible_from<key_type, Second>
        )
    explicit FrozenSet(const std::pair<First, Second>& pair) :
        Base(PyFrozenSet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            if (PySet_Add(m_ptr, key_type(pair.first).ptr())) {
                Exception::from_python();
            }
            if (PySet_Add(m_ptr, key_type(pair.second).ptr())) {
                Exception::from_python();
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack a std::tuple into a py::FrozenSet. */
    template <typename... Args> requires (std::constructible_from<key_type, Args> && ...)
    explicit FrozenSet(const std::tuple<Args...>& tuple) :
        Base(PyFrozenSet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }

        auto unpack_tuple = [&]<size_t... Ns>(std::index_sequence<Ns...>) {
            auto insert = [](PyObject* m_ptr, const auto& item) {
                if (PySet_Add(m_ptr, item.ptr())) {
                    Exception::from_python();
                }
            };
            (insert(m_ptr, key_type(std::get<Ns>(tuple))), ...);
        };

        try {
            unpack_tuple(std::index_sequence_for<Args...>{});
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    
    /* Explicitly unpack a C++ string literal into a py::FrozenSet. */
    template <size_t N> requires (generic || std::same_as<key_type, Str>)
    explicit FrozenSet(const char (&string)[N]) : Base(PyFrozenSet_New(nullptr), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            for (size_t i = 0; i < N; ++i) {
                PyObject* item = PyUnicode_FromStringAndSize(string + i, 1);
                if (item == nullptr) {
                    Exception::from_python();
                }
                if (PySet_Add(m_ptr, item)) {
                    Exception::from_python();
                }
                Py_DECREF(item);
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack a C++ string pointer into a py::FrozenSet. */
    template <std::same_as<const char*> T> requires (generic || std::same_as<key_type, Str>)
    explicit FrozenSet(T string) : Base(PyFrozenSet_New(nullptr), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            for (const char* ptr = string; *ptr != '\0'; ++ptr) {
                PyObject* item = PyUnicode_FromStringAndSize(ptr, 1);
                if (item == nullptr) {
                    Exception::from_python();
                }
                if (PySet_Add(m_ptr, item)) {
                    Exception::from_python();
                }
                Py_DECREF(item);
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    ////////////////////////////////
    ////    PYTHON INTERFACE    ////
    ////////////////////////////////

    /* Equivalent to Python `set.copy()`. */
    inline FrozenSet copy() const {
        PyObject* result = PyFrozenSet_New(this->ptr());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<FrozenSet>(result);
    }

    /* Equivalent to Python `set.isdisjoint(other)`. */
    template <impl::is_iterable T>
        requires (std::convertible_to<impl::dereference_type<T>, key_type>)
    bool isdisjoint(const T& other) const;

    /* Equivalent to Python `set.isdisjoint(other)`, where other is given as a
    braced initializer list. */
    inline bool isdisjoint(const std::initializer_list<key_type>& other) const {
        for (const key_type& item : other) {
            if (contains(item)) {
                return false;
            }
        }
        return true;
    }

    /* Equivalent to Python `set.issubset(other)`. */
    template <impl::is_iterable T>
        requires (std::convertible_to<impl::dereference_type<T>, key_type>)
    inline bool issubset(const T& other) const;

    /* Equivalent to Python `set.issubset(other)`, where other is given as a
    braced initializer list. */
    inline bool issubset(const std::initializer_list<key_type>& other) const;

    /* Equivalent to Python `set.issuperset(other)`. */
    template <impl::is_iterable T>
        requires (std::convertible_to<impl::dereference_type<T>, key_type>)
    inline bool issuperset(const T& other) const;

    /* Equivalent to Python `set.issuperset(other)`, where other is given as a
    braced initializer list. */
    inline bool issuperset(const std::initializer_list<key_type>& other) const {
        for (const key_type& item : other) {
            if (!contains(item)) {
                return false;
            }
        }
        return true;
    }

    /* Equivalent to Python `set.union(*others)`. */
    template <impl::is_iterable... Args>
        requires (std::convertible_to<impl::dereference_type<Args>, key_type> && ...)
    inline FrozenSet union_(const Args&... others) const;

    /* Equivalent to Python `set.union(other)`, where other is given as a braced
    initializer list. */
    inline FrozenSet union_(const std::initializer_list<key_type>& other) const {
        PyObject* result = PyFrozenSet_New(this->ptr());
        if (result == nullptr) {
            Exception::from_python();
        }
        try {
            for (const key_type& item : other) {
                if (PySet_Add(result, item.ptr())) {
                    Exception::from_python();
                }
            }
            return reinterpret_steal<FrozenSet>(result);
        } catch (...) {
            Py_DECREF(result);
            throw;
        }
    }

    /* Equivalent to Python `set.intersection(other)`. */
    template <impl::is_iterable... Args>
        requires (std::convertible_to<impl::dereference_type<Args>, key_type> && ...)
    FrozenSet intersection(const Args&... others) const;

    /* Equivalent to Python `set.intersection(other)`, where other is given as a
    braced initializer list. */
    inline FrozenSet intersection(const std::initializer_list<key_type>& other) const {
        PyObject* result = PyFrozenSet_New(nullptr);
        if (result == nullptr) {
            Exception::from_python();
        }
        try {
            for (const key_type& item : other) {
                if (contains(item)) {
                    if (PySet_Add(result, item.ptr())) {
                        Exception::from_python();
                    }
                }
            }
            return reinterpret_steal<FrozenSet>(result);
        } catch (...) {
            Py_DECREF(result);
            throw;
        }
    }

    /* Equivalent to Python `set.difference(other)`. */
    template <impl::is_iterable... Args>
        requires (std::convertible_to<impl::dereference_type<Args>, key_type> && ...)
    FrozenSet difference(const Args&... others) const;

    /* Equivalent to Python `set.difference(other)`, where other is given as a
    braced initializer list. */
    inline FrozenSet difference(const std::initializer_list<key_type>& other) const {
        PyObject* result = PyFrozenSet_New(this->ptr());
        if (result == nullptr) {
            Exception::from_python();
        }
        try {
            for (const key_type& item : other) {
                if (PySet_Discard(result, item.ptr()) == -1) {
                    Exception::from_python();
                }
            }
            return reinterpret_steal<FrozenSet>(result);
        } catch (...) {
            Py_DECREF(result);
            throw;
        }
    }

    /* Equivalent to Python `set.symmetric_difference(other)`. */
    template <impl::is_iterable T>
        requires (std::convertible_to<impl::dereference_type<T>, key_type>)
    inline FrozenSet symmetric_difference(const T& other) const;

    /* Equivalent to Python `set.symmetric_difference(other)`, where other is given
    as a braced initializer list. */
    inline FrozenSet symmetric_difference(const std::initializer_list<key_type>& other) const {
        PyObject* result = PyFrozenSet_New(nullptr);
        if (result == nullptr) {
            Exception::from_python();
        }
        try {
            for (const key_type& item : other) {
                if (contains(item)) {
                    if (PySet_Discard(result, item.ptr()) == -1) {
                        Exception::from_python();
                    }
                } else {
                    if (PySet_Add(result, item.ptr())) {
                        Exception::from_python();
                    }
                }
            }
            return reinterpret_steal<FrozenSet>(result);
        } catch (...) {
            Py_DECREF(result);
            throw;
        }
    }

    /////////////////////////
    ////    OPERATORS    ////
    /////////////////////////

    inline friend FrozenSet operator|(
        const FrozenSet& self,
        const std::initializer_list<key_type>& other
    ) {
        return self.union_(other);
    }

    inline friend FrozenSet& operator|=(
        FrozenSet& self,
        const std::initializer_list<key_type>& other
    ) {
        self = self.union_(other);
        return self;
    }

    inline friend FrozenSet operator&(
        const FrozenSet& self,
        const std::initializer_list<key_type>& other
    ) {
        return self.intersection(other);
    }

    inline friend FrozenSet& operator&=(
        FrozenSet& self,
        const std::initializer_list<key_type>& other
    ) {
        self = self.intersection(other);
        return self;
    }

    inline friend FrozenSet operator-(
        const FrozenSet& self,
        const std::initializer_list<key_type>& other
    ) {
        return self.difference(other);
    }

    inline friend FrozenSet& operator-=(
        FrozenSet& self,
        const std::initializer_list<key_type>& other
    ) {
        self = self.difference(other);
        return self;
    }

    inline friend FrozenSet operator^(
        const FrozenSet& self,
        const std::initializer_list<key_type>& other
    ) {
        return self.symmetric_difference(other);
    }

    inline friend FrozenSet& operator^=(
        FrozenSet& self,
        const std::initializer_list<key_type>& other
    ) {
        self = self.symmetric_difference(other);
        return self;
    }

};


template <std::derived_from<impl::FrozenSetTag> Self, impl::cpp_like T>
    requires (impl::anyset_like<T>)
struct __cast__<Self, T> : Returns<T> {
    static T operator()(const Self& self) {
        T result;
        for (const auto& item : self) {
            result.insert(static_cast<typename T::value_type>(item));
        }
        return result;
    }
};


///////////////////
////    SET    ////
///////////////////


template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "add">                                 : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "remove">                              : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "discard">                             : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "pop">                                 : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "clear">                               : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "copy">                                : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "isdisjoint">                          : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "issubset">                            : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "issuperset">                          : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "union">                               : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "update">                              : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "intersection">                        : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "intersection_update">                 : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "difference">                          : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "difference_update">                   : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "symmetric_difference">                : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __getattr__<Self, "symmetric_difference_update">         : Returns<Function> {};
template <std::derived_from<impl::SetTag> Self>
struct __len__<Self>                                            : Returns<size_t> {};
template <std::derived_from<impl::SetTag> Self>
struct __iter__<Self>                                           : Returns<typename Self::key_type> {};
template <std::derived_from<impl::SetTag> Self>
struct __reversed__<Self>                                       : Returns<typename Self::key_type> {};
template <
    std::derived_from<impl::SetTag> Self,
    std::convertible_to<typename Self::key_type> Key
>
struct __contains__<Self, Key>                                  : Returns<bool> {};
template <std::derived_from<impl::SetTag> Self, impl::anyset_like T>
struct __lt__<Self, T>                                          : Returns<bool> {};
template <impl::anyset_like T, std::derived_from<impl::SetTag> Self>
    requires (!std::derived_from<T, impl::FrozenSetTag>)
struct __lt__<T, Self>                                          : Returns<bool> {};
template <std::derived_from<impl::SetTag> Self, impl::anyset_like T>
struct __le__<Self, T>                                          : Returns<bool> {};
template <impl::anyset_like T, std::derived_from<impl::SetTag> Self>
    requires (!std::derived_from<T, impl::FrozenSetTag>)
struct __le__<T, Self>                                          : Returns<bool> {};
template <std::derived_from<impl::SetTag> Self, impl::anyset_like T>
struct __eq__<Self, T>                                          : Returns<bool> {};
template <impl::anyset_like T, std::derived_from<impl::SetTag> Self>
    requires (!std::derived_from<T, impl::FrozenSetTag>)
struct __eq__<T, Self>                                          : Returns<bool> {};
template <std::derived_from<impl::SetTag> Self, impl::anyset_like T>
struct __ne__<Self, T>                                          : Returns<bool> {};
template <impl::anyset_like T, std::derived_from<impl::SetTag> Self>
    requires (!std::derived_from<T, impl::FrozenSetTag>)
struct __ne__<T, Self>                                          : Returns<bool> {};
template <std::derived_from<impl::SetTag> Self, impl::anyset_like T>
struct __ge__<Self, T>                                          : Returns<bool> {};
template <impl::anyset_like T, std::derived_from<impl::SetTag> Self>
    requires (!std::derived_from<T, impl::FrozenSetTag>)
struct __ge__<T, Self>                                          : Returns<bool> {};
template <std::derived_from<impl::SetTag> Self, impl::anyset_like T>
struct __gt__<Self, T>                                          : Returns<bool> {};
template <impl::anyset_like T, std::derived_from<impl::SetTag> Self>
    requires (!std::derived_from<T, impl::FrozenSetTag>)
struct __gt__<T, Self>                                          : Returns<bool> {};
template <std::derived_from<impl::SetTag> Self, typename T>
    requires (Self::template check<T>())
struct __or__<Self, T>                                          : Returns<Self> {};
template <
    std::derived_from<impl::SetTag> T,
    std::derived_from<impl::SetTag> Self
> requires (!T::template check<Self>() && Self::template check<T>())
struct __or__<T, Self>                                          : Returns<Self> {};
template <std::derived_from<impl::SetTag> Self, typename T>
    requires (Self::template check<T>())
struct __ior__<Self, T>                                         : Returns<Self&> {};
template <std::derived_from<impl::SetTag> Self, typename T>
    requires (Self::template check<T>())
struct __and__<Self, T>                                         : Returns<Self> {};
template <
    std::derived_from<impl::SetTag> T,
    std::derived_from<impl::SetTag> Self
> requires (!T::template check<Self>() && Self::template check<T>())
struct __and__<T, Self>                                         : Returns<Self> {};
template <std::derived_from<impl::SetTag> Self, typename T>
    requires (Self::template check<T>())
struct __iand__<Self, T>                                        : Returns<Self&> {};
template <std::derived_from<impl::SetTag> Self, typename T>
    requires (Self::template check<T>())
struct __sub__<Self, T>                                         : Returns<Self> {};
template <
    std::derived_from<impl::SetTag> T,
    std::derived_from<impl::SetTag> Self
> requires (!T::template check<Self>() && Self::template check<T>())
struct __sub__<T, Self>                                         : Returns<Self> {};
template <std::derived_from<impl::SetTag> Self, typename T>
    requires (Self::template check<T>())
struct __isub__<Self, T>                                        : Returns<Self&> {};
template <std::derived_from<impl::SetTag> Self, typename T>
    requires (Self::template check<T>())
struct __xor__<Self, T>                                         : Returns<Self> {};
template <
    std::derived_from<impl::SetTag> T,
    std::derived_from<impl::SetTag> Self
> requires (!T::template check<Self>() && Self::template check<T>())
struct __xor__<T, Self>                                         : Returns<Self> {};
template <std::derived_from<impl::SetTag> Self, typename T>
    requires (Self::template check<T>())
struct __ixor__<Self, T>                                        : Returns<Self&> {};


namespace impl {
namespace ops {

    template <typename Return, std::derived_from<SetTag> Self>
    struct len<Return, Self> {
        static size_t operator()(const Self& self) {
            return PySet_GET_SIZE(self.ptr());
        }
    };

    template <typename Return, std::derived_from<SetTag> Self, typename Key>
    struct contains<Return, Self, Key> {
        static bool operator()(const Self& self, const to_object<Key>& key) {
            int result = PySet_Contains(self.ptr(), key.ptr());
            if (result == -1) {
                Exception::from_python();
            }
            return result;
        }
    };

}
}


template <typename T>
Set(const std::initializer_list<T>&) -> Set<to_python<T>>;
template <impl::is_iterable T>
Set(T) -> Set<to_python<impl::dereference_type<T>>>;
template <typename T, typename... Args>
    requires (!impl::is_iterable<T> && !impl::str_like<T>)
Set(T, Args...) -> Set<Object>;
template <impl::str_like T>
Set(T) -> Set<Str>;
template <size_t N>
Set(const char(&)[N]) -> Set<Str>;


/* Represents a statically-typed Python set in C++. */
template <typename Key>
class Set : public Object, public impl::SetTag {
    using Base = Object;
    static_assert(
        std::derived_from<Key, Object>,
        "py::Set can only contain types derived from py::Object."
    );
    static_assert(
        impl::is_hashable<Key>,
        "py::Set can only contain hashable types."
    );

    static constexpr bool generic = std::same_as<Key, Object>;

    template <typename T>
    static constexpr bool typecheck = std::derived_from<T, Object> ?
        std::derived_from<T, Key> : std::convertible_to<T, Key>;

public:
    using impl::SetTag::type;

    using size_type = size_t;
    using key_type = Key;
    using value_type = Key;
    using pointer = value_type*;
    using reference = value_type&;
    using const_pointer = const value_type*;
    using const_reference = const value_type&;
    using iterator = impl::Iterator<impl::GenericIter<value_type>>;
    using const_iterator = impl::Iterator<impl::GenericIter<const value_type>>;
    using reverse_iterator = impl::ReverseIterator<impl::GenericIter<value_type>>;
    using const_reverse_iterator = impl::ReverseIterator<impl::GenericIter<const value_type>>;

    template <typename T>
    static consteval bool check() {
        if constexpr (!impl::set_like<std::decay_t<T>>) {
            return false;
        } else if constexpr (impl::pybind11_like<std::decay_t<T>>) {
            return generic;
        } else if constexpr (impl::is_iterable<std::decay_t<T>>) {
            return typecheck<impl::dereference_type<std::decay_t<T>>>;
        } else {
            return false;
        }
    }

    template <typename T>
    static constexpr bool check(const T& obj) {
        if constexpr (impl::cpp_like<T>) {
            return check<T>();

        } else if constexpr (impl::is_object_exact<T>) {
            if constexpr (generic) {
                return obj.ptr() != nullptr && PySet_Check(obj.ptr());
            } else {
                return (
                    obj.ptr() != nullptr && PySet_Check(obj.ptr()) &&
                    std::ranges::all_of(obj, [](const auto& item) {
                        return key_type::check(item);
                    })
                );
            }

        } else if constexpr (
            std::derived_from<T, Set<Object>> ||
            std::derived_from<T, pybind11::set>
        ) {
            if constexpr (generic) {
                return obj.ptr() != nullptr;
            } else {
                return (
                    obj.ptr() != nullptr &&
                    std::ranges::all_of(obj, [](const auto& item) {
                        return key_type::check(item);
                    })
                );
            }

        } else if constexpr (impl::set_like<T>) {
            return obj.ptr() != nullptr && typecheck<impl::dereference_type<T>>;

        } else {
            return false;
        }
    }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    Set(Handle h, const borrowed_t& t) : Base(h, t) {}
    Set(Handle h, const stolen_t& t) : Base(h, t) {}

    template <typename Policy>
    Set(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<Set>(accessor).release(), stolen_t{})
    {}

    /* Default constructor.  Initializes to an empty set. */
    Set() : Base(PySet_New(nullptr), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
    }

    /* Pack the contents of a braced initializer list into a new Python set. */
    Set(const std::initializer_list<key_type>& contents) :
        Base(PySet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            for (const key_type& item : contents) {
                if (PySet_Add(m_ptr, item.ptr())) {
                    Exception::from_python();
                }
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Copy/move constructors from equivalent pybind11 types or other sets with a
    narrower key type. */
    template <impl::python_like T> requires (check<T>())
    Set(T&& other) : Base(std::forward<T>(other)) {}

    /* Explicitly unpack an arbitrary Python container into a new py::Set. */
    template <impl::python_like T> requires (!impl::set_like<T> && impl::is_iterable<T>)
    explicit Set(const T& contents) : Base(PySet_New(contents.ptr()), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
    }

    /* Explicitly unpack an arbitrary C++ container into a new py::Set. */
    template <impl::cpp_like T> requires (impl::is_iterable<T>)
    explicit Set(const T& contents) : Base(PySet_New(nullptr), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            for (const auto& item : contents) {
                if (PySet_Add(m_ptr, key_type(item).ptr())) {
                    Exception::from_python();
                }
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Construct a new Set from a pair of input iterators. */
    template <typename Iter, std::sentinel_for<Iter> Sentinel>
    explicit Set(Iter first, Sentinel last) :
        Base(PySet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            while (first != last) {
                if (PySet_Add(m_ptr, key_type(*first).ptr())) {
                    Exception::from_python();
                }
                ++first;
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack a std::pair into a py::Set. */
    template <typename First, typename Second>
        requires (
            std::constructible_from<key_type, First> &&
            std::constructible_from<key_type, Second>
        )
    explicit Set(const std::pair<First, Second>& pair) :
        Base(PySet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            if (PySet_Add(m_ptr, key_type(pair.first).ptr())) {
                Exception::from_python();
            }
            if (PySet_Add(m_ptr, key_type(pair.second).ptr())) {
                Exception::from_python();
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack a std::tuple into a py::Set. */
    template <typename... Args> requires (std::constructible_from<key_type, Args> && ...)
    explicit Set(const std::tuple<Args...>& tuple) :
        Base(PySet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }

        auto unpack_tuple = [&]<size_t... Ns>(std::index_sequence<Ns...>) {
            auto insert = [](PyObject* m_ptr, const key_type& item) {
                if (PySet_Add(m_ptr, item.ptr())) {
                    Exception::from_python();
                }
            };
            (insert(m_ptr, key_type(std::get<Ns>(tuple))), ...);
        };

        try {
            unpack_tuple(std::index_sequence_for<Args...>{});
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack a C++ string literal into a py::Set. */
    template <size_t N> requires (generic || std::same_as<key_type, Str>)
    explicit Set(const char (&string)[N]) : Base(PySet_New(nullptr), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            for (size_t i = 0; i < N; ++i) {
                PyObject* item = PyUnicode_FromStringAndSize(string + i, 1);
                if (item == nullptr) {
                    Exception::from_python();
                }
                if (PySet_Add(m_ptr, item)) {
                    Exception::from_python();
                }
                Py_DECREF(item);
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack a C++ string pointer into a py::Set. */
    template <std::same_as<const char*> T> requires (generic || std::same_as<key_type, Str>)
    explicit Set(T string) : Base(PySet_New(nullptr), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            for (const char* ptr = string; *ptr != '\0'; ++ptr) {
                PyObject* item = PyUnicode_FromStringAndSize(ptr, 1);
                if (item == nullptr) {
                    Exception::from_python();
                }
                if (PySet_Add(m_ptr, item)) {
                    Exception::from_python();
                }
                Py_DECREF(item);
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    ////////////////////////////////
    ////    PYTHON INTERFACE    ////
    ////////////////////////////////

    /* Equivalent to Python `set.add(key)`. */
    inline void add(const key_type& key) {
        if (PySet_Add(this->ptr(), key.ptr())) {
            Exception::from_python();
        }
    }

    /* Equivalent to Python `set.remove(key)`. */
    inline void remove(const key_type& key) {
        int result = PySet_Discard(this->ptr(), key.ptr());
        if (result == -1) {
            Exception::from_python();
        } else if (result == 0) {
            throw KeyError(key);
        }
    }

    /* Equivalent to Python `set.discard(key)`. */
    inline void discard(const key_type& key) {
        if (PySet_Discard(this->ptr(), key.ptr()) == -1) {
            Exception::from_python();
        }
    }

    /* Equivalent to Python `set.pop()`. */
    inline key_type pop() {
        PyObject* result = PySet_Pop(this->ptr());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<key_type>(result);
    }

    /* Equivalent to Python `set.clear()`. */
    inline void clear() {
        if (PySet_Clear(this->ptr())) {
            Exception::from_python();
        }
    }

    /* Equivalent to Python `set.copy()`. */
    inline Set copy() const {
        PyObject* result = PySet_New(this->ptr());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<Set>(result);
    }

    /* Equivalent to Python `set.isdisjoint(other)`. */
    template <impl::is_iterable T>
        requires (std::convertible_to<impl::dereference_type<T>, key_type>)
    bool isdisjoint(const T& other) const;

    /* Equivalent to Python `set.isdisjoint(other)`, where other is given as a
    braced initializer list. */
    inline bool isdisjoint(const std::initializer_list<key_type>& other) const {
        for (const key_type& item : other) {
            if (contains(item)) {
                return false;
            }
        }
        return true;
    }

    /* Equivalent to Python `set.issubset(other)`. */
    template <impl::is_iterable T>
        requires (std::convertible_to<impl::dereference_type<T>, key_type>)
    inline bool issubset(const T& other) const;

    /* Equivalent to Python `set.issubset(other)`, where other is given as a
    braced initializer list. */
    inline bool issubset(const std::initializer_list<key_type>& other) const;

    /* Equivalent to Python `set.issuperset(other)`. */
    template <impl::is_iterable T>
        requires (std::convertible_to<impl::dereference_type<T>, key_type>)
    inline bool issuperset(const T& other) const;

    /* Equivalent to Python `set.issuperset(other)`, where other is given as a
    braced initializer list. */
    inline bool issuperset(const std::initializer_list<key_type>& other) const {
        for (const key_type& item : other) {
            if (!contains(item)) {
                return false;
            }
        }
        return true;
    }

    /* Equivalent to Python `set.union(*others)`. */
    template <impl::is_iterable... Args>
        requires (std::convertible_to<impl::dereference_type<Args>, key_type> && ...)
    inline Set union_(const Args&... others) const;

    /* Equivalent to Python `set.union(other)`, where other is given as a braced
    initializer list. */
    inline Set union_(const std::initializer_list<key_type>& other) const {
        PyObject* result = PySet_New(this->ptr());
        if (result == nullptr) {
            Exception::from_python();
        }
        try {
            for (const key_type& item : other) {
                if (PySet_Add(result, item.ptr())) {
                    Exception::from_python();
                }
            }
            return reinterpret_steal<Set>(result);
        } catch (...) {
            Py_DECREF(result);
            throw;
        }
    }

    /* Equivalent to Python `set.update(*others)`. */
    template <impl::is_iterable... Args>
        requires (std::convertible_to<impl::dereference_type<Args>, key_type> && ...)
    inline void update(const Args&... others);

    /* Equivalent to Python `set.update(<braced initializer list>)`. */
    inline void update(const std::initializer_list<key_type>& other) {
        for (const key_type& item : other) {
            add(item);
        }
    }

    /* Equivalent to Python `set.intersection(other)`. */
    template <impl::is_iterable... Args>
        requires (std::convertible_to<impl::dereference_type<Args>, key_type> && ...)
    Set intersection(const Args&... others) const;

    /* Equivalent to Python `set.intersection(other)`, where other is given as a
    braced initializer list. */
    inline Set intersection(const std::initializer_list<key_type>& other) const {
        PyObject* result = PySet_New(nullptr);
        if (result == nullptr) {
            Exception::from_python();
        }
        try {
            for (const key_type& item : other) {
                if (contains(item)) {
                    if (PySet_Add(result, item.ptr())) {
                        Exception::from_python();
                    }
                }
            }
            return reinterpret_steal<Set>(result);
        } catch (...) {
            Py_DECREF(result);
            throw;
        }
    }

    /* Equivalent to Python `set.intersection_update(*others)`. */
    template <impl::is_iterable... Args>
        requires (std::convertible_to<impl::dereference_type<Args>, key_type> && ...)
    inline void intersection_update(const Args&... others);

    /* Equivalent to Python `set.intersection_update(<braced initializer list>)`. */
    void intersection_update(const std::initializer_list<key_type>& other);

    /* Equivalent to Python `set.difference(other)`. */
    template <impl::is_iterable... Args>
        requires (std::convertible_to<impl::dereference_type<Args>, key_type> && ...)
    Set difference(const Args&... others) const;

    /* Equivalent to Python `set.difference(other)`, where other is given as a
    braced initializer list. */
    inline Set difference(const std::initializer_list<key_type>& other) const {
        PyObject* result = PySet_New(this->ptr());
        if (result == nullptr) {
            Exception::from_python();
        }
        try {
            for (const key_type& item : other) {
                if (PySet_Discard(result, item.ptr()) == -1) {
                    Exception::from_python();
                }
            }
            return reinterpret_steal<Set>(result);
        } catch (...) {
            Py_DECREF(result);
            throw;
        }
    }

    /* Equivalent to Python `set.difference_update(*others)`. */
    template <impl::is_iterable... Args>
        requires (std::convertible_to<impl::dereference_type<Args>, key_type> && ...)
    inline void difference_update(const Args&... others);

    /* Equivalent to Python `set.difference_update(<braced initializer list>)`. */
    inline void difference_update(const std::initializer_list<key_type>& other) {
        for (const key_type& item : other) {
            discard(item);
        }
    }

    /* Equivalent to Python `set.symmetric_difference(other)`. */
    template <impl::is_iterable T>
        requires (std::convertible_to<impl::dereference_type<T>, key_type>)
    inline Set symmetric_difference(const T& other) const;

    /* Equivalent to Python `set.symmetric_difference(other)`, where other is given
    as a braced initializer list. */
    inline Set symmetric_difference(const std::initializer_list<key_type>& other) const {
        PyObject* result = PySet_New(nullptr);
        if (result == nullptr) {
            Exception::from_python();
        }
        try {
            for (const key_type& item : other) {
                if (contains(item)) {
                    if (PySet_Discard(result, item.ptr()) == -1) {
                        Exception::from_python();
                    }
                } else {
                    if (PySet_Add(result, item.ptr())) {
                        Exception::from_python();
                    }
                }
            }
            return reinterpret_steal<Set>(result);
        } catch (...) {
            Py_DECREF(result);
            throw;
        }
    }

    /* Equivalent to Python `set.symmetric_difference_update(other)`. */
    template <impl::is_iterable T>
        requires (std::convertible_to<impl::dereference_type<T>, key_type>)
    inline void symmetric_difference_update(const T& other);

    /* Equivalent to Python `set.symmetric_difference_update(<braced initializer list>)`. */
    inline void symmetric_difference_update(const std::initializer_list<key_type>& other) {
        for (const key_type& item : other) {
            if (contains(item)) {
                discard(item);
            } else {
                add(item);
            }
        }
    }

    /////////////////////////
    ////    OPERATORS    ////
    /////////////////////////

    inline friend Set operator|(
        const Set& self,
        const std::initializer_list<key_type>& other
    ) {
        return self.union_(other);
    }

    inline friend Set& operator|=(
        Set& self,
        const std::initializer_list<key_type>& other
    ) {
        self.update(other);
        return self;
    }

    inline friend Set operator&(
        const Set& self,
        const std::initializer_list<key_type>& other
    ) {
        return self.intersection(other);
    }

    inline friend Set& operator&=(
        Set& self,
        const std::initializer_list<key_type>& other
    ) {
        self.intersection_update(other);
        return self;
    }

    inline friend Set& operator-=(
        Set& self,
        const std::initializer_list<key_type>& other
    ) {
        self.difference_update(other);
        return self;
    }

    inline friend Set operator-(
        const Set& self,
        const std::initializer_list<key_type>& other
    ) {
        return self.difference(other);
    }

    inline friend Set& operator^=(
        Set& self,
        const std::initializer_list<key_type>& other
    ) {
        self.symmetric_difference_update(other);
        return self;
    }

    inline friend Set operator^(
        const Set& self,
        const std::initializer_list<key_type>& other
    ) {
        return self.symmetric_difference(other);
    }

};


template <std::derived_from<impl::SetTag> Self, impl::cpp_like T>
    requires (impl::anyset_like<T>)
struct __cast__<Self, T> : Returns<T> {
    static T operator()(const Self& self) {
        T result;
        for (const auto& item : self) {
            result.insert(static_cast<typename T::value_type>(item));
        }
        return result;
    }
};


}  // namespace py
}  // namespace bertrand


#endif  // BERTRAND_PYTHON_SET_H

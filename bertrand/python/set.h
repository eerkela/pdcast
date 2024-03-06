#ifndef BERTRAND_PYTHON_INCLUDED
#error "This file should not be included directly.  Please include <bertrand/python.h> instead."
#endif

#ifndef BERTRAND_PYTHON_SET_H
#define BERTRAND_PYTHON_SET_H

#include "common.h"


namespace bertrand {
namespace py {


namespace impl {

    template <typename Derived>
    class ISet : public Object {
        using Base = Object;

        inline Derived* self() { return static_cast<Derived*>(this); }
        inline const Derived* self() const { return static_cast<const Derived*>(this); }

    protected:

        template <typename T>
        inline static void insert_from_tuple(PyObject* result, const T& item) {
            if (PySet_Add(result, detail::object_or_cast(item).ptr())) {
                throw error_already_set();
            }
        }

        template <typename... Args, size_t... N>
        inline static void unpack_tuple(
            PyObject* result,
            const std::tuple<Args...>& tuple,
            std::index_sequence<N...>
        ) {
            (insert_from_tuple(result, std::get<N>(tuple)), ...);
        }

        template <typename T>
        struct is_iterable_struct {
            static constexpr bool value = impl::is_iterable<T>;
        };

        template <typename... Args>
        static constexpr bool all_iterable =
            std::conjunction_v<is_iterable_struct<std::decay_t<Args>>...>;

    public:
        using Base::Base;

        ///////////////////////////
        ////    CONVERSIONS    ////
        ///////////////////////////

        /* Implicitly convert a py::FrozenSet into a C++ set or unordered_set. */
        template <
            typename T,
            std::enable_if_t<!impl::is_python<T> && impl::is_anyset_like<T>, int> = 0
        >
        inline operator T() const {
            T result;
            for (auto&& item : *self()) {
                result.insert(item.template cast<typename T::value_type>());
            }
        }

        //////////////////////////
        ////    PySet_ API    ////
        //////////////////////////

        /* Get the size of the set. */
        inline size_t size() const noexcept {
            return static_cast<size_t>(PySet_GET_SIZE(self()->ptr()));
        }

        /* Cehcek if the set is empty. */
        inline bool empty() const noexcept {
            return size() == 0;
        }
                                        
        ////////////////////////////////
        ////    PYTHON INTERFACE    ////
        ////////////////////////////////

        /* Equivalent to Python `set.copy()`. */
        inline Derived copy() const {
            PyObject* result = self()->alloc(self()->ptr());
            if (result == nullptr) {
                throw error_already_set();
            }
            return reinterpret_steal<Derived>(result);
        }

        /* Equivalent to Python `set.isdisjoint(other)`. */
        template <typename T, std::enable_if_t<impl::is_iterable<T>, int> = 0>
        inline bool isdisjoint(const T& other) const {
            if constexpr (impl::is_python<T>) {
                return static_cast<bool>(self()->attr("isdisjoint")(other));
            } else {
                for (auto&& item : other) {
                    if (contains(std::forward<decltype(item)>(item))) {
                        return false;
                    }
                }
                return true;
            }
        }

        /* Equivalent to Python `set.isdisjoint(other)`, where other is given as a
        braced initializer list. */
        inline bool isdisjoint(const std::initializer_list<impl::HashInitializer>& other) const {
            for (const impl::HashInitializer& item : other) {
                if (contains(item.first)) {
                    return false;
                }
            }
            return true;
        }

        /* Equivalent to Python `set.issubset(other)`. */
        template <typename T, std::enable_if_t<impl::is_iterable<T>, int> = 0>
        inline bool issubset(const T& other) const {
            return static_cast<bool>(self()->attr("issubset")(
                detail::object_or_cast(other)
            ));
        }

        /* Equivalent to Python `set.issubset(other)`, where other is given as a
        braced initializer list. */
        inline bool issubset(const std::initializer_list<impl::HashInitializer>& other) const {
            return static_cast<bool>(self()->attr("issubset")(Derived(other)));
        }

        /* Equivalent to Python `set.issuperset(other)`. */
        template <typename T, std::enable_if_t<impl::is_iterable<T>, int> = 0>
        inline bool issuperset(const T& other) const {
            if constexpr (impl::is_python<T>) {
                return static_cast<bool>(self()->attr("issuperset")(other));
            } else {
                for (auto&& item : other) {
                    if (!contains(std::forward<decltype(item)>(item))) {
                        return false;
                    }
                }
                return true;
            }
        }

        /* Equivalent to Python `set.issuperset(other)`, where other is given as a
        braced initializer list. */
        inline bool issuperset(
            const std::initializer_list<impl::HashInitializer>& other
        ) const {
            for (const impl::HashInitializer& item : other) {
                if (!contains(item.first)) {
                    return false;
                }
            }
            return true;
        }

        /* Equivalent to Python `set.union(*others)`. */
        template <typename... Args, std::enable_if_t<all_iterable<Args...>, int> = 0>
        inline Derived union_(Args&&... others) const {
            return self()->attr("union")(
                detail::object_or_cast(std::forward<Args>(others))...
            );
        }

        /* Equivalent to Python `set.union(other)`, where other is given as a braced
        initializer list. */
        inline Derived union_(const std::initializer_list<impl::HashInitializer>& other) const {
            PyObject* result = self()->alloc(self()->ptr());
            if (result == nullptr) {
                throw error_already_set();
            }
            try {
                for (const impl::HashInitializer& item : other) {
                    if (PySet_Add(result, item.first.ptr())) {
                        throw error_already_set();
                    }
                }
                return reinterpret_steal<Derived>(result);
            } catch (...) {
                Py_DECREF(result);
                throw;
            }
        }

        /* Equivalent to Python `set.intersection(other)`. */
        template <typename... Args, std::enable_if_t<all_iterable<Args...>, int> = 0>
        inline Derived intersection(Args&&... others) const {
            return self()->attr("intersection")(
                detail::object_or_cast(std::forward<Args>(others))...
            );
        }

        /* Equivalent to Python `set.intersection(other)`, where other is given as a
        braced initializer list. */
        inline Derived intersection(
            const std::initializer_list<impl::HashInitializer>& other
        ) const {
            PyObject* result = self()->alloc(nullptr);
            if (result == nullptr) {
                throw error_already_set();
            }
            try {
                for (const impl::HashInitializer& item : other) {
                    if (contains(item.first)) {
                        if (PySet_Add(result, item.first.ptr())) {
                            throw error_already_set();
                        }
                    }
                }
                return reinterpret_steal<Derived>(result);
            } catch (...) {
                Py_DECREF(result);
                throw;
            }
        }

        /* Equivalent to Python `set.difference(other)`. */
        template <typename... Args, std::enable_if_t<all_iterable<Args...>, int> = 0>
        inline Derived difference(Args&&... others) const {
            return self()->attr("difference")(
                detail::object_or_cast(std::forward<Args>(others))...
            );
        }

        /* Equivalent to Python `set.difference(other)`, where other is given as a
        braced initializer list. */
        inline Derived difference(
            const std::initializer_list<impl::HashInitializer>& other
        ) const {
            PyObject* result = self()->alloc(self()->ptr());
            if (result == nullptr) {
                throw error_already_set();
            }
            try {
                for (const impl::HashInitializer& item : other) {
                    if (PySet_Discard(result, item.first.ptr()) == -1) {
                        throw error_already_set();
                    }
                }
                return reinterpret_steal<Derived>(result);
            } catch (...) {
                Py_DECREF(result);
                throw;
            }
        }

        /* Equivalent to Python `set.symmetric_difference(other)`. */
        template <typename T, std::enable_if_t<impl::is_iterable<T>, int> = 0>
        inline Derived symmetric_difference(const T& other) const {
            return self()->attr("symmetric_difference")(detail::object_or_cast(other));
        }

        /* Equivalent to Python `set.symmetric_difference(other)`, where other is given
        as a braced initializer list. */
        inline Derived symmetric_difference(
            const std::initializer_list<impl::HashInitializer>& other
        ) const {
            PyObject* result = self()->alloc(nullptr);
            if (result == nullptr) {
                throw error_already_set();
            }
            try {
                for (const impl::HashInitializer& item : other) {
                    if (contains(item.first)) {
                        if (PySet_Discard(result, item.first.ptr()) == -1) {
                            throw error_already_set();
                        }
                    } else {
                        if (PySet_Add(result, item.first.ptr())) {
                            throw error_already_set();
                        }
                    }
                }
                return reinterpret_steal<Derived>(result);
            } catch (...) {
                Py_DECREF(result);
                throw;
            }
        }

        /////////////////////////
        ////    OPERATORS    ////
        /////////////////////////

        /* Equivalent to Python `key in set`. */
        template <typename T>
        inline bool contains(const T& key) const {
            int result = PySet_Contains(
                self()->ptr(),
                detail::object_or_cast(key).ptr()
            );
            if (result == -1) {
                throw error_already_set();
            }
            return result;
        }

        template <typename... Args>
        inline auto operator()(Args&&... args) const = delete;

        template <typename T>
        inline auto operator[](const T& key) = delete;

        using Base::operator|;
        using Base::operator&;
        using Base::operator-;
        using Base::operator^;

        inline Derived operator|(
            const std::initializer_list<impl::HashInitializer>& other
        ) const {
            return union_(other);
        }

        inline Derived operator&(
            const std::initializer_list<impl::HashInitializer>& other
        ) const {
            return intersection(other);
        }

        inline Derived operator-(
            const std::initializer_list<impl::HashInitializer>& other
        ) const {
            return difference(other);
        }

        inline Derived operator^(
            const std::initializer_list<impl::HashInitializer>& other
        ) const {
            return symmetric_difference(other);
        }

    };

}  // namespace impl


/* Wrapper around pybind11::frozenset that allows it to be directly initialized using
std::initializer_list and replicates the Python interface as closely as possible. */
class FrozenSet : public impl::ISet<FrozenSet> {
    using Base = impl::ISet<FrozenSet>;
    friend Base;

    /* This helper function is needed for ISet mixin. */
    inline static PyObject* alloc(PyObject* obj) {
        return PyFrozenSet_New(obj);
    }

    template <typename T>
    static constexpr bool constructor1 =
        impl::is_python<T> && !impl::is_frozenset_like<T> && impl::is_iterable<T>;
    template <typename T>
    static constexpr bool constructor2 = !impl::is_python<T> && impl::is_iterable<T>;

public:
    static Type type;

    template <typename T>
    static constexpr bool check() { return impl::is_frozenset_like<T>; }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    BERTRAND_OBJECT_COMMON(Base, FrozenSet, PyFrozenSet_Check)

    /* Default constructor.  Initializes to an empty set. */
    FrozenSet() : Base(PyFrozenSet_New(nullptr), stolen_t{}) {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
    }

    /* Pack the contents of a braced initializer list into a new Python frozenset. */
    FrozenSet(const std::initializer_list<impl::HashInitializer>& contents) :
        Base(PyFrozenSet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
        try {
            for (const impl::HashInitializer& item : contents) {
                if (PySet_Add(m_ptr, item.first.ptr())) {
                    throw error_already_set();
                }
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack an arbitrary Python container into a new py::FrozenSet. */
    template <typename T, std::enable_if_t<constructor1<T>, int> = 0>
    explicit FrozenSet(const T& contents) :
        Base(PyFrozenSet_New(contents.ptr()), stolen_t{})
    {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
    }

    /* Explicitly unpack an arbitrary C++ container into a new py::FrozenSet. */
    template <typename T, std::enable_if_t<constructor2<T>, int> = 0>
    explicit FrozenSet(const T& container) {
        m_ptr = PyFrozenSet_New(nullptr);
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
        try {
            for (auto&& item : container) {
                if (PySet_Add(
                    m_ptr,
                    detail::object_or_cast(std::forward<decltype(item)>(item)).ptr())
                ) {
                    throw error_already_set();
                }
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack a std::pair into a py::FrozenSet. */
    template <
        typename First,
        typename Second,
        std::enable_if_t<impl::is_hashable<First> && impl::is_hashable<Second>, int> = 0
    >
    explicit FrozenSet(const std::pair<First, Second>& pair) :
        Base(PyFrozenSet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
        try {
            if (PySet_Add(m_ptr, detail::object_or_cast(pair.first).ptr())) {
                throw error_already_set();
            }
            if (PySet_Add(m_ptr, detail::object_or_cast(pair.second).ptr())) {
                throw error_already_set();
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack a std::tuple into a py::FrozenSet. */
    template <
        typename... Args,
        std::enable_if_t<(impl::is_hashable<Args> && ...), int> = 0
    >
    explicit FrozenSet(const std::tuple<Args...>& tuple) :
        Base(PyFrozenSet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
        try {
            Base::unpack_tuple(m_ptr, tuple, std::index_sequence_for<Args...>{});
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /////////////////////////
    ////    OPERATORS    ////
    /////////////////////////

    inline FrozenSet& operator|=(const std::initializer_list<impl::HashInitializer>& other) {
        *this = union_(other);
        return *this;
    }

    inline FrozenSet& operator&=(const std::initializer_list<impl::HashInitializer>& other) {
        *this = intersection(other);
        return *this;
    }

    inline FrozenSet& operator-=(const std::initializer_list<impl::HashInitializer>& other) {
        *this = difference(other);
        return *this;
    }

    inline FrozenSet& operator^=(const std::initializer_list<impl::HashInitializer>& other) {
        *this = symmetric_difference(other);
        return *this;
    }

};


template <>
struct FrozenSet::__lt__<Object> : impl::Returns<bool> {};
template <typename T>
struct FrozenSet::__lt__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<bool> {};

template <>
struct FrozenSet::__le__<Object> : impl::Returns<bool> {};
template <typename T>
struct FrozenSet::__le__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<bool> {};

template <>
struct FrozenSet::__ge__<Object> : impl::Returns<bool> {};
template <typename T>
struct FrozenSet::__ge__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<bool> {};

template <>
struct FrozenSet::__gt__<Object> : impl::Returns<bool> {};
template <typename T>
struct FrozenSet::__gt__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<bool> {};

template <>
struct FrozenSet::__or__<Object> : impl::Returns<FrozenSet> {};
template <typename T>
struct FrozenSet::__or__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<FrozenSet> {};

template <>
struct FrozenSet::__and__<Object> : impl::Returns<FrozenSet> {};
template <typename T>
struct FrozenSet::__and__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<FrozenSet> {};

template <>
struct FrozenSet::__sub__<Object> : impl::Returns<FrozenSet> {};
template <typename T>
struct FrozenSet::__sub__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<FrozenSet> {};

template <>
struct FrozenSet::__xor__<Object> : impl::Returns<FrozenSet> {};
template <typename T>
struct FrozenSet::__xor__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<FrozenSet> {};

template <>
struct FrozenSet::__ior__<Object> : impl::Returns<FrozenSet&> {};
template <typename T>
struct FrozenSet::__ior__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<FrozenSet&> {};

template <>
struct FrozenSet::__iand__<Object> : impl::Returns<FrozenSet&> {};
template <typename T>
struct FrozenSet::__iand__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<FrozenSet&> {};

template <>
struct FrozenSet::__isub__<Object> : impl::Returns<FrozenSet&> {};
template <typename T>
struct FrozenSet::__isub__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<FrozenSet&> {};

template <>
struct FrozenSet::__ixor__<Object> : impl::Returns<FrozenSet&> {};
template <typename T>
struct FrozenSet::__ixor__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<FrozenSet&> {};


/* Wrapper around pybind11::set that allows it to be directly initialized using
std::initializer_list and replicates the Python interface as closely as possible. */
class Set : public impl::ISet<Set> {
    using Base = impl::ISet<Set>;
    friend Base;

    /* This helper function is needed for ISet mixin. */
    inline static PyObject* alloc(PyObject* obj) {
        return PySet_New(obj);
    }

    template <typename T>
    static constexpr bool constructor1 =
        impl::is_python<T> && !impl::is_set_like<T> && impl::is_iterable<T>;
    template <typename T>
    static constexpr bool constructor2 = !impl::is_python<T> && impl::is_iterable<T>;

public:
    static Type type;

    template <typename T>
    static constexpr bool check() { return impl::is_set_like<T>; }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    BERTRAND_OBJECT_COMMON(Base, Set, PySet_Check);

    /* Default constructor.  Initializes to an empty set. */
    Set() : Base(PySet_New(nullptr), stolen_t{}) {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
    }

    /* Pack the contents of a braced initializer list into a new Python set. */
    Set(const std::initializer_list<impl::HashInitializer>& contents) :
        Base(PySet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
        try {
            for (const impl::HashInitializer& item : contents) {
                if (PySet_Add(m_ptr, item.first.ptr())) {
                    throw error_already_set();
                }
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack an arbitrary Python container into a new py::Set. */
    template <typename T, std::enable_if_t<constructor1<T>, int> = 0>
    explicit Set(const T& contents) :
        Base(PySet_New(contents.ptr()), stolen_t{})
    {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
    }

    /* Explicitly unpack an arbitrary C++ container into a new py::Set. */
    template <typename T, std::enable_if_t<constructor2<T>, int> = 0>
    explicit Set(const T& contents) : Base(PySet_New(nullptr), stolen_t{}) {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
        try {
            for (auto&& item : contents) {
                if (PySet_Add(
                    m_ptr,
                    detail::object_or_cast(std::forward<decltype(item)>(item)).ptr())
                ) {
                    throw error_already_set();
                }
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack a std::pair into a py::Set. */
    template <
        typename First,
        typename Second,
        std::enable_if_t<impl::is_hashable<First> && impl::is_hashable<Second>, int> = 0
    >
    explicit Set(const std::pair<First, Second>& pair) :
        Base(PySet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
        try {
            if (PySet_Add(m_ptr, detail::object_or_cast(pair.first).ptr())) {
                throw error_already_set();
            }
            if (PySet_Add(m_ptr, detail::object_or_cast(pair.second).ptr())) {
                throw error_already_set();
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack a std::tuple into a py::Set. */
    template <
        typename... Args,
        std::enable_if_t<(impl::is_hashable<Args> && ...), int> = 0
    >
    explicit Set(const std::tuple<Args...>& tuple) :
        Base(PySet_New(nullptr), stolen_t{})
    {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
        try {
            Base::unpack_tuple(m_ptr, tuple, std::index_sequence_for<Args...>{});
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    ////////////////////////////////
    ////    PYTHON INTERFACE    ////
    ////////////////////////////////

    /* Equivalent to Python `set.add(key)`. */
    template <typename T, std::enable_if_t<impl::is_hashable<T>, int> = 0>
    inline void add(const T& key) {
        if (PySet_Add(this->ptr(), detail::object_or_cast(key).ptr())) {
            throw error_already_set();
        }
    }

    /* Equivalent to Python `set.remove(key)`. */
    template <typename T, std::enable_if_t<impl::is_hashable<T>, int> = 0>
    inline void remove(const T& key) {
        Object obj = detail::object_or_cast(key);
        int result = PySet_Discard(this->ptr(), obj.ptr());
        if (result == -1) {
            throw error_already_set();
        } else if (result == 0) {
            throw KeyError(repr(obj));
        }
    }

    /* Equivalent to Python `set.discard(key)`. */
    template <typename T, std::enable_if_t<impl::is_hashable<T>, int> = 0>
    inline void discard(const T& key) {
        if (PySet_Discard(this->ptr(), detail::object_or_cast(key).ptr()) == -1) {
            throw error_already_set();
        }
    }

    /* Equivalent to Python `set.pop()`. */
    inline Object pop() {
        PyObject* result = PySet_Pop(this->ptr());
        if (result == nullptr) {
            throw error_already_set();
        }
        return reinterpret_steal<Object>(result);
    }

    /* Equivalent to Python `set.clear()`. */
    inline void clear() {
        if (PySet_Clear(this->ptr())) {
            throw error_already_set();
        }
    }

    /* Equivalent to Python `set.update(*others)`. */
    template <typename... Args, std::enable_if_t<Base::all_iterable<Args...>, int> = 0>
    inline void update(Args&&... others) {
        this->attr("update")(
            detail::object_or_cast(std::forward<Args>(others))...
        );
    }

    /* Equivalent to Python `set.update(<braced initializer list>)`. */
    inline void update(const std::initializer_list<impl::HashInitializer>& other) {
        for (const impl::HashInitializer& item : other) {
            add(item.first);
        }
    }

    /* Equivalent to Python `set.intersection_update(*others)`. */
    template <typename... Args, std::enable_if_t<Base::all_iterable<Args...>, int> = 0>
    inline void intersection_update(Args&&... others) {
        this->attr("intersection_update")(
            detail::object_or_cast(std::forward<Args>(others))...
        );
    }

    /* Equivalent to Python `set.intersection_update(<braced initializer list>)`. */
    inline void intersection_update(
        const std::initializer_list<impl::HashInitializer>& other
    ) {
        this->attr("intersection_update")(Set(other));
    }

    /* Equivalent to Python `set.difference_update(*others)`. */
    template <typename... Args, std::enable_if_t<Base::all_iterable<Args...>, int> = 0>
    inline void difference_update(Args&&... others) {
        this->attr("difference_update")(
            detail::object_or_cast(std::forward<Args>(others))...
        );
    }

    /* Equivalent to Python `set.difference_update(<braced initializer list>)`. */
    inline void difference_update(
        const std::initializer_list<impl::HashInitializer>& other
    ) {
        for (const impl::HashInitializer& item : other) {
            discard(item.first);
        }
    }

    /* Equivalent to Python `set.symmetric_difference_update(other)`. */
    template <typename T, std::enable_if_t<impl::is_iterable<T>, int> = 0>
    inline void symmetric_difference_update(const T& other) {
        this->attr("symmetric_difference_update")(detail::object_or_cast(other));
    }

    /* Equivalent to Python `set.symmetric_difference_update(<braced initializer list>)`. */
    inline void symmetric_difference_update(
        const std::initializer_list<impl::HashInitializer>& other
    ) {
        for (const impl::HashInitializer& item : other) {
            if (contains(item.first)) {
                discard(item.first);
            } else {
                add(item.first);
            }
        }
    }

    /////////////////////////
    ////    OPERATORS    ////
    /////////////////////////

    /* Equivalent to Python `set |= <braced initializer list>`. */
    inline Set& operator|=(const std::initializer_list<impl::HashInitializer>& other) {
        update(other);
        return *this;
    }

    /* Equivalent to Python `set &= <braced initializer list>`. */
    inline Set& operator&=(const std::initializer_list<impl::HashInitializer>& other) {
        intersection_update(other);
        return *this;
    }

    /* Equivalent to Python `set -= <braced initializer list>`. */
    inline Set& operator-=(const std::initializer_list<impl::HashInitializer>& other) {
        difference_update(other);
        return *this;
    }

    /* Equivalent to Python `set ^= <braced initializer list>`. */
    inline Set& operator^=(const std::initializer_list<impl::HashInitializer>& other) {
        symmetric_difference_update(other);
        return *this;
    }

};

template <>
struct Set::__lt__<Object> : impl::Returns<bool> {};
template <typename T>
struct Set::__lt__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<bool> {};

template <>
struct Set::__le__<Object> : impl::Returns<bool> {};
template <typename T>
struct Set::__le__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<bool> {};

template <>
struct Set::__ge__<Object> : impl::Returns<bool> {};
template <typename T>
struct Set::__ge__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<bool> {};

template <>
struct Set::__gt__<Object> : impl::Returns<bool> {};
template <typename T>
struct Set::__gt__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<bool> {};

template <>
struct Set::__or__<Object> : impl::Returns<Set> {};
template <typename T>
struct Set::__or__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<Set> {};

template <>
struct Set::__and__<Object> : impl::Returns<Set> {};
template <typename T>
struct Set::__and__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<Set> {};

template <>
struct Set::__sub__<Object> : impl::Returns<Set> {};
template <typename T>
struct Set::__sub__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<Set> {};

template <>
struct Set::__xor__<Object> : impl::Returns<Set> {};
template <typename T>
struct Set::__xor__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<Set> {};

template <>
struct Set::__ior__<Object> : impl::Returns<Set&> {};
template <typename T>
struct Set::__ior__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<Set&> {};

template <>
struct Set::__iand__<Object> : impl::Returns<Set&> {};
template <typename T>
struct Set::__iand__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<Set&> {};

template <>
struct Set::__isub__<Object> : impl::Returns<Set&> {};
template <typename T>
struct Set::__isub__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<Set&> {};

template <>
struct Set::__ixor__<Object> : impl::Returns<Set&> {};
template <typename T>
struct Set::__ixor__<T, std::enable_if_t<impl::is_anyset_like<T>>> : impl::Returns<Set&> {};


}  // namespace python
}  // namespace bertrand


BERTRAND_STD_HASH(bertrand::py::FrozenSet)


#endif  // BERTRAND_PYTHON_SET_H

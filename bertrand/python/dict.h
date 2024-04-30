#if !defined(BERTRAND_PYTHON_INCLUDED) && !defined(LINTER)
#error "This file should not be included directly.  Please include <bertrand/python.h> instead."
#endif

#ifndef BERTRAND_PYTHON_DICT_H
#define BERTRAND_PYTHON_DICT_H

#include "common.h"
#include "set.h"


namespace bertrand {
namespace py {


////////////////////
////    KEYS    ////
////////////////////


template <>
struct __getattr__<KeysView, "mapping">                     : Returns<MappingProxy> {};
template <>
struct __getattr__<KeysView, "isdisjoint">                  : Returns<Function> {};

template <>
struct __len__<KeysView>                                    : Returns<size_t> {};
template <>
struct __iter__<KeysView>                                   : Returns<Object> {};
template <>
struct __reversed__<KeysView>                               : Returns<Object> {};
template <impl::is_hashable Key>
struct __contains__<KeysView, Key>                          : Returns<bool> {};
template <>
struct __lt__<KeysView, Object>                             : Returns<bool> {};
template <std::derived_from<KeysView> T>
struct __lt__<KeysView, T>                                  : Returns<bool> {};
template <impl::anyset_like T>
struct __lt__<KeysView, T>                                  : Returns<bool> {};
template <>
struct __le__<KeysView, Object>                             : Returns<bool> {};
template <std::derived_from<KeysView> T>
struct __le__<KeysView, T>                                  : Returns<bool> {};
template <impl::anyset_like T>
struct __le__<KeysView, T>                                  : Returns<bool> {};
template <>
struct __ge__<KeysView, Object>                             : Returns<bool> {};
template <std::derived_from<KeysView> T>
struct __ge__<KeysView, T>                                  : Returns<bool> {};
template <impl::anyset_like T>
struct __ge__<KeysView, T>                                  : Returns<bool> {};
template <>
struct __gt__<KeysView, Object>                             : Returns<bool> {};
template <std::derived_from<KeysView> T>
struct __gt__<KeysView, T>                                  : Returns<bool> {};
template <impl::anyset_like T>
struct __gt__<KeysView, T>                                  : Returns<bool> {};
template <>
struct __or__<KeysView, Object>                             : Returns<Set> {};
template <std::derived_from<KeysView> T>
struct __or__<KeysView, T>                                  : Returns<Set> {};
template <impl::anyset_like T>
struct __or__<KeysView, T>                                  : Returns<Set> {};
template <>
struct __and__<KeysView, Object>                            : Returns<Set> {};
template <std::derived_from<KeysView> T>
struct __and__<KeysView, T>                                 : Returns<Set> {};
template <impl::anyset_like T>
struct __and__<KeysView, T>                                 : Returns<Set> {};
template <>
struct __sub__<KeysView, Object>                            : Returns<Set> {};
template <std::derived_from<KeysView> T>
struct __sub__<KeysView, T>                                 : Returns<Set> {};
template <impl::anyset_like T>
struct __sub__<KeysView, T>                                 : Returns<Set> {};
template <>
struct __xor__<KeysView, Object>                            : Returns<Set> {};
template <std::derived_from<KeysView> T>
struct __xor__<KeysView, T>                                 : Returns<Set> {};
template <impl::anyset_like T>
struct __xor__<KeysView, T>                                 : Returns<Set> {};


/* Represents a statically-typed Python `dict.keys()` object in C++. */
class KeysView : public Object, public impl::KeysTag {
    using Base = Object;

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return std::derived_from<T, KeysView>;
    }

    template <typename T>
    static constexpr bool check(const T& obj) {
        if constexpr (impl::cpp_like<T>) {
            return check<T>();

        } else if constexpr (check<T>()) {
            return obj.ptr() != nullptr;

        } else if constexpr (impl::is_object_exact<T>) {
            if (obj.ptr() == nullptr) {
                return false;
            }
            int result = PyObject_IsInstance(
                obj,
                (PyObject*) &PyDictKeys_Type
            );
            if (result == -1) {
                Exception::from_python();
            }
            return result;

        } else {
            return false;
        }
    }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    KeysView(Handle h, const borrowed_t& t) : Base(h, t) {}
    KeysView(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    KeysView(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    KeysView(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<KeysView>(accessor).release(), stolen_t{})
    {}

    /* Explicitly create a keys view on an existing dictionary. */
    explicit KeysView(const pybind11::dict& dict) : Base(nullptr, stolen_t{}) {
        static const pybind11::str lookup = "keys";
        m_ptr = dict.attr(lookup)().release().ptr();
    }

    /* Explicitly create a keys view on an existing dictionary. */
    inline explicit KeysView(const Dict& dict);

    ////////////////////////////////
    ////    PYTHON INTERFACE    ////
    ////////////////////////////////

    /* Equivalent to Python `dict.keys().mapping`. */
    inline MappingProxy mapping() const;

    /* Equivalent to Python `dict.keys().isdisjoint(other)`. */
    template <impl::is_iterable T>
    inline bool isdisjoint(const T& other) const;

    /* Equivalent to Python `dict.keys().isdisjoint(<braced initializer list>)`. */
    inline bool isdisjoint(
        const std::initializer_list<impl::HashInitializer>& other
    ) const;

    /////////////////////////
    ////    OPERATORS    ////
    /////////////////////////

    inline friend Set operator|(
        const KeysView& self,
        const std::initializer_list<impl::HashInitializer>& other
    ) {
        PyObject* result = PyNumber_Or(self.ptr(), Set(other).ptr());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<Set>(result);
    }

    inline friend Set operator&(
        const KeysView& self,
        const std::initializer_list<impl::HashInitializer>& other
    ) {
        PyObject* result = PyNumber_And(self.ptr(), Set(other).ptr());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<Set>(result);
    }

    inline friend Set operator-(
        const KeysView& self,
        const std::initializer_list<impl::HashInitializer>& other
    ) {
        PyObject* result = PyNumber_Subtract(self.ptr(), Set(other).ptr());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<Set>(result);
    }

    inline friend Set operator^(
        const KeysView& self,
        const std::initializer_list<impl::HashInitializer>& other
    ) {
        PyObject* result = PyNumber_Xor(self.ptr(), Set(other).ptr());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<Set>(result);
    }

};


//////////////////////
////    VALUES    ////
//////////////////////


template <>
struct __getattr__<ValuesView, "mapping">                   : Returns<MappingProxy> {};

template <>
struct __len__<ValuesView>                                  : Returns<size_t> {};
template <>
struct __iter__<ValuesView>                                 : Returns<Object> {};
template <>
struct __reversed__<ValuesView>                             : Returns<Object> {};
template <typename T>
struct __contains__<ValuesView, T>                          : Returns<bool> {};


/* Represents a statically-typed Python `dict.values()` object in C++. */
class ValuesView : public Object, public impl::ValuesTag {
    using Base = Object;

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return std::derived_from<T, ValuesView>;
    }

    template <typename T>
    static constexpr bool check(const T& obj) {
        if constexpr (impl::cpp_like<T>) {
            return check<T>();

        } else if constexpr (check<T>()) {
            return obj.ptr() != nullptr;

        } else if constexpr (impl::is_object_exact<T>) {
            if (obj.ptr() == nullptr) {
                return false;
            }
            int result = PyObject_IsInstance(
                obj,
                (PyObject*) &PyDictValues_Type
            );
            if (result == -1) {
                Exception::from_python();
            }
            return result;

        } else {
            return false;
        }
    }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    ValuesView(Handle h, const borrowed_t& t) : Base(h, t) {}
    ValuesView(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    ValuesView(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    ValuesView(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<ValuesView>(accessor).release(), stolen_t{})
    {}

    /* Explicitly create a values view on an existing dictionary. */
    explicit ValuesView(const pybind11::dict& dict) : Base(nullptr, stolen_t{}) {
        static const pybind11::str lookup = "values";
        m_ptr = dict.attr(lookup)().release().ptr();
    }

    /* Explicitly create a values view on an existing dictionary. */
    inline explicit ValuesView(const Dict& dict);

    ///////////////////////////////
    ////   PYTHON INTERFACE    ////
    ///////////////////////////////

    /* Equivalent to Python `dict.values().mapping`. */
    inline MappingProxy mapping() const;

};


/////////////////////
////    ITEMS    ////
/////////////////////


template <>
struct __getattr__<ItemsView, "mapping">                    : Returns<MappingProxy> {};

template <>
struct __len__<ItemsView>                                   : Returns<size_t> {};
template <>
struct __iter__<ItemsView>                                  : Returns<Tuple<Object>> {};  // TODO: should yield Tuple<Key, Value> or std::pair<Key, Value> instead
template <>
struct __reversed__<ItemsView>                              : Returns<Tuple<Object>> {};  // TODO: should yield Tuple<Key, Value> or std::pair<Key, Value> instead
template <impl::tuple_like T>
struct __contains__<ItemsView, T>                           : Returns<bool> {};


/* Represents a statically-typed Python `dict.items()` object in C++. */
class ItemsView : public Object, public impl::ItemsTag {
    using Base = Object;

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return std::derived_from<T, ItemsView>;
    }

    template <typename T>
    static constexpr bool check(const T& obj) {
        if constexpr (impl::cpp_like<T>) {
            return check<T>();

        } else if constexpr (check<T>()) {
            return obj.ptr() != nullptr;

        } else if constexpr (impl::is_object_exact<T>) {
            if (obj.ptr() == nullptr) {
                return false;
            }
            int result = PyObject_IsInstance(
                obj,
                (PyObject*) &PyDictItems_Type
            );
            if (result == -1) {
                Exception::from_python();
            }
            return result;

        } else {
            return false;
        }
    }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    ItemsView(Handle h, const borrowed_t& t) : Base(h, t) {}
    ItemsView(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    ItemsView(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    ItemsView(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<ItemsView>(accessor).release(), stolen_t{})
    {}

    /* Explicitly create an items view on an existing dictionary. */
    explicit ItemsView(const pybind11::dict& dict) : Base(nullptr, stolen_t{}) {
        static const pybind11::str lookup = "items";
        m_ptr = dict.attr(lookup)().release().ptr();
    }

    /* Explicitly create an items view on an existing dictionary. */
    inline explicit ItemsView(const Dict& dict);

    ////////////////////////////////
    ////    PYTHON INTERFACE    ////
    ////////////////////////////////

    /* Equivalent to Python `dict.items().mapping`. */
    inline MappingProxy mapping() const;

};


////////////////////
////    DICT    ////
////////////////////


template <>
struct __getattr__<Dict, "fromkeys">                        : Returns<Function> {};
template <>
struct __getattr__<Dict, "copy">                            : Returns<Function> {};
template <>
struct __getattr__<Dict, "clear">                           : Returns<Function> {};
template <>
struct __getattr__<Dict, "get">                             : Returns<Function> {};
template <>
struct __getattr__<Dict, "pop">                             : Returns<Function> {};
template <>
struct __getattr__<Dict, "popitem">                         : Returns<Function> {};
template <>
struct __getattr__<Dict, "setdefault">                      : Returns<Function> {};
template <>
struct __getattr__<Dict, "update">                          : Returns<Function> {};
template <>
struct __getattr__<Dict, "keys">                            : Returns<Function> {};
template <>
struct __getattr__<Dict, "values">                          : Returns<Function> {};
template <>
struct __getattr__<Dict, "items">                           : Returns<Function> {};

template <>
struct __len__<Dict>                                        : Returns<size_t> {};
template <>
struct __iter__<Dict>                                       : Returns<Object> {};
template <>
struct __reversed__<Dict>                                   : Returns<Object> {};
template <impl::is_hashable Key>
struct __contains__<Dict, Key>                              : Returns<bool> {};
template <impl::is_hashable Key>
struct __getitem__<Dict, Key>                               : Returns<Object> {};
template <impl::is_hashable Key, typename Value>
struct __setitem__<Dict, Key, Value>                        : Returns<void> {};
template <impl::is_hashable Key>
struct __delitem__<Dict, Key>                               : Returns<void> {};
template <>
struct __or__<Dict, Object>                                 : Returns<Dict> {};
template <impl::dict_like T>
struct __or__<Dict, T>                                      : Returns<Dict> {};
template <>
struct __ior__<Dict, Object>                                : Returns<Dict&> {};
template <impl::dict_like T>
struct __ior__<Dict, T>                                     : Returns<Dict&> {};


/* Represents a statically-typed Python dictionary in C++. */
class Dict : public Object, public impl::DictTag {
    using Base = Object;

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return impl::dict_like<T>;
    }

    template <typename T>
    static constexpr bool check(const T& obj) {
        if constexpr (impl::cpp_like<T>) {
            return check<T>();
        } else if constexpr (check<T>()) {
            return obj.ptr() != nullptr;
        } else if constexpr (impl::is_object_exact<T>) {
            return obj.ptr() != nullptr && PyDict_Check(obj.ptr());
        } else {
            return false;
        }
    }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    Dict(Handle h, const borrowed_t& t) : Base(h, t) {}
    Dict(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    Dict(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    Dict(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<Dict>(accessor).release(), stolen_t{})
    {}

    /* Default constructor.  Initializes to empty dict. */
    Dict() : Base(PyDict_New(), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
    }

    /* Pack the given arguments into a dictionary using an initializer list. */
    Dict(const std::initializer_list<impl::DictInitializer>& contents) :
        Base(PyDict_New(), stolen_t{})
    {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            for (const impl::DictInitializer& item : contents) {
                if (PyDict_SetItem(m_ptr, item.key.ptr(), item.value.ptr())) {
                    Exception::from_python();
                }
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Construct a new dict from a pair of input iterators. */
    template <typename Iter, std::sentinel_for<Iter> Sentinel>
    explicit Dict(Iter first, Sentinel last) : Base(PyDict_New(), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            while (first != last) {
                auto&& [k, v] = *first;
                if (PyDict_SetItem(
                    m_ptr,
                    Object(std::forward<decltype(k)>(k)).ptr(),
                    Object(std::forward<decltype(v)>(v)).ptr()
                )) {
                    Exception::from_python();
                }
                ++first;
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Explicitly unpack an arbitrary Python container into a new py::Dict. */
    template <impl::python_like T>
        requires (impl::is_iterable<T> && !impl::dict_like<T>)
    explicit Dict(const T& contents) :
        Base(PyObject_CallOneArg((PyObject*) &PyDict_Type, contents.ptr()), stolen_t{})
    {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
    }

    /* Explicitly unpack a arbitrary C++ container into a new py::Dict. */
    template <impl::cpp_like T> requires (impl::is_iterable<T>)
    explicit Dict(T&& container) : Base(PyDict_New(), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
        try {
            for (auto&& [k, v] : container) {
                if (PyDict_SetItem(
                    m_ptr,
                    Object(std::forward<decltype(k)>(k)).ptr(),
                    Object(std::forward<decltype(v)>(v)).ptr()
                )) {
                    Exception::from_python();
                }
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    /* Construct a dictionary using pybind11-style keyword arguments.  This is
    technically superceeded by initializer lists, but it is provided for backwards
    compatibility with Python and pybind11. */
    template <
        typename... Args,
        typename collector = pybind11::detail::deferred_t<
            pybind11::detail::unpacking_collector<>, Args...
        >
    >
        requires (pybind11::args_are_all_keyword_or_ds<Args...>())
    explicit Dict(Args&&... args) :
        Dict(collector(std::forward<Args>(args)...).kwargs())
    {}

    /////////////////////////////
    ////    C++ INTERFACE    ////
    /////////////////////////////

    /* Equivalent to Python `dict.update(items)`, but does not overwrite keys. */
    template <impl::dict_like T>
    inline void merge(const T& items) {
        if (PyDict_Merge(this->ptr(), Object(items).ptr(), 0)) {
            Exception::from_python();
        }
    }

    /* Equivalent to Python `dict.update(items)`, but does not overwrite keys. */
    template <impl::is_iterable T> requires (!impl::dict_like<T>)
    inline void merge(const T& items) {
        if (PyDict_MergeFromSeq2(this->ptr(), Object(items).ptr(), 0)) {
            Exception::from_python();
        }
    }

    ////////////////////////////////
    ////    PYTHON INTERFACE    ////
    ////////////////////////////////

    /* Equivalent to Python `dict.clear()`. */
    inline void clear() { 
        PyDict_Clear(this->ptr());
    }

    /* Equivalent to Python `dict.copy()`. */
    inline Dict copy() const {
        PyObject* result = PyDict_Copy(this->ptr());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<Dict>(result);
    }

    /* Equivalent to Python `dict.fromkeys(keys)`.  Values default to None. */
    template <impl::is_iterable K>
    static inline Dict fromkeys(const K& keys) {
        PyObject* result = PyDict_New();
        if (result == nullptr) {
            Exception::from_python();
        }
        try {
            for (auto&& key : keys) {
                if (PyDict_SetItem(
                    result,
                    Object(std::forward<decltype(key)>(key)).ptr(),
                    Py_None
                )) {
                    Exception::from_python();
                }
            }
            return reinterpret_steal<Dict>(result);
        } catch (...) {
            Py_DECREF(result);
            throw;
        }
    }

    /* Equivalent to Python `dict.fromkeys(<braced initializer list>)`. */
    inline Dict fromkeys(const std::initializer_list<impl::HashInitializer>& keys) {
        PyObject* result = PyDict_New();
        if (result == nullptr) {
            Exception::from_python();
        }
        try {
            for (const impl::HashInitializer& init : keys) {
                if (PyDict_SetItem(
                    result,
                    init.value.ptr(),
                    Py_None
                )) {
                    Exception::from_python();
                }
            }
            return reinterpret_steal<Dict>(result);
        } catch (...) {
            Py_DECREF(result);
            throw;
        }
    }

    /* Equivalent to Python `dict.fromkeys(keys, value)`. */
    template <impl::is_iterable K>
    static inline Dict fromkeys(const K& keys, const Object& value) {
        PyObject* result = PyDict_New();
        if (result == nullptr) {
            Exception::from_python();
        }
        try {
            for (auto&& key : keys) {
                if (PyDict_SetItem(
                    result,
                    Object(std::forward<decltype(key)>(key)).ptr(),
                    value.ptr()
                )) {
                    Exception::from_python();
                }
            }
            return reinterpret_steal<Dict>(result);
        } catch (...) {
            Py_DECREF(result);
            throw;
        }
    }

    /* Equivalent to Python `dict.fromkeys(<braced initializer list>, value)`. */
    inline Dict fromkeys(
        const std::initializer_list<impl::HashInitializer>& keys,
        const Object& value
    ) {
        PyObject* result = PyDict_New();
        if (result == nullptr) {
            Exception::from_python();
        }
        try {
            for (const impl::HashInitializer& init : keys) {
                if (PyDict_SetItem(
                    result,
                    init.value.ptr(),
                    value.ptr()
                )) {
                    Exception::from_python();
                }
            }
            return reinterpret_steal<Dict>(result);
        } catch (...) {
            Py_DECREF(result);
            throw;
        }
    }

    /* Equivalent to Python `dict.get(key)`.  Returns None if the key is not found. */
    template <impl::is_hashable K>
    inline Object get(const K& key) const {
        PyObject* result = PyDict_GetItemWithError(this->ptr(), Object(key).ptr());
        if (result == nullptr) {
            if (PyErr_Occurred()) {
                Exception::from_python();
            }
            return reinterpret_borrow<Object>(Py_None);
        }
        return reinterpret_steal<Object>(result);
    }

    /* Equivalent to Python `dict.get(key, default_value)`. */
    template <impl::is_hashable K>
    inline Object get(const K& key, const Object& default_value) const {
        PyObject* result = PyDict_GetItemWithError(this->ptr(), Object(key).ptr());
        if (result == nullptr) {
            if (PyErr_Occurred()) {
                Exception::from_python();
            }
            return default_value;
        }
        return reinterpret_steal<Object>(result);
    }

    /* Equivalent to Python `dict.pop(key)`.  Returns None if the key is not found. */
    template <impl::is_hashable K>
    inline Object pop(const K& key) {
        PyObject* result = PyDict_GetItemWithError(this->ptr(), Object(key).ptr());
        if (result == nullptr) {
            if (PyErr_Occurred()) {
                Exception::from_python();
            }
            return reinterpret_borrow<Object>(Py_None);
        }
        if (PyDict_DelItem(this->ptr(), result)) {
            Exception::from_python();
        }
        return reinterpret_steal<Object>(result);
    }

    /* Equivalent to Python `dict.pop(key, default_value)`. */
    template <impl::is_hashable K>
    inline Object pop(const K& key, const Object& default_value) {
        PyObject* result = PyDict_GetItemWithError(this->ptr(), Object(key).ptr());
        if (result == nullptr) {
            if (PyErr_Occurred()) {
                Exception::from_python();
            }
            return default_value;
        }
        if (PyDict_DelItem(this->ptr(), result)) {
            Exception::from_python();
        }
        return reinterpret_steal<Object>(result);
    }

    /* Equivalent to Python `dict.popitem()`. */
    inline Object popitem();

    /* Equivalent to Python `dict.setdefault(key)`. */
    template <impl::is_hashable K>
    inline Object setdefault(const K& key) {
        PyObject* result = PyDict_SetDefault(
            this->ptr(),
            Object(key).ptr(),
            Py_None
        );
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<Object>(result);
    }

    /* Equivalent to Python `dict.setdefault(key, default_value)`. */
    template <impl::is_hashable K>
    inline Object setdefault(const K& key, const Object& default_value) {
        PyObject* result = PyDict_SetDefault(
            this->ptr(),
            Object(key).ptr(),
            default_value.ptr()
        );
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<Object>(result);
    }

    /* Equivalent to Python `dict.update(items)`. */
    template <impl::dict_like T>
    inline void update(const T& items) {
        if (PyDict_Merge(this->ptr(), items.ptr(), 1)) {
            Exception::from_python();
        }
    }

    /* Equivalent to Python `dict.update(items)`. */
    template <impl::is_iterable T> requires (!impl::dict_like<T>)
    inline void update(const T& items) {
        if constexpr (impl::python_like<T>) {
            if (PyDict_MergeFromSeq2(
                this->ptr(),
                Object(items).ptr(),
                1
            )) {
                Exception::from_python();
            }
        } else {
            for (auto&& [k, v] : items) {
                if (PyDict_SetItem(
                    this->ptr(),
                    Object(std::forward<decltype(k)>(k)).ptr(),
                    Object(std::forward<decltype(v)>(v)).ptr()
                )) {
                    Exception::from_python();
                }
            }
        }
    }

    /* Equivalent to Python `dict.update(<braced initializer list>)`. */
    inline void update(const std::initializer_list<impl::DictInitializer>& items) {
        for (const impl::DictInitializer& item : items) {
            if (PyDict_SetItem(this->ptr(), item.key.ptr(), item.value.ptr())) {
                Exception::from_python();
            }
        }
    }

    /////////////////////
    ////    VIEWS    ////
    /////////////////////

    // TODO: do these store a const Dict& ?  If so, then copy/move constructors are
    // disabled.  This probably isn't a bad thing.

    struct Keys {
        const Dict& dict;

        Keys(const Dict& dict) : dict(dict) {}

        // inline MappingProxy mapping() const {
        //     return MappingProxy(dict);
        // }

        // template <impl::is_iterable T>
        // inline bool isdisjoint(const T& other) const;

        // inline bool isdisjoint(
        //     const std::initializer_list<impl::HashInitializer>& other
        // ) const;

        // TODO: implement size(), begin(), end(), rbegin(), rend(), <, <=, ==, !=, >=,
        // >, |, &, -, ^

        inline auto begin() const {
            return impl::Iterator<impl::KeyIter<Object>>(dict);
        }

        inline auto end() const {
            return impl::Iterator<impl::KeyIter<Object>>();
        }

        // TODO: reverse iterators over dictionaries are somewhat complicated, since
        // keys are always yielded in forward order.  We'd need to make a temporary
        // stack to iterate over them in reverse order.

        // inline auto rbegin() const {
        //     return impl::ReverseIterator<impl::KeyIter<Object>>(dict);
        // }

        // inline auto rend() const {
        //     return impl::ReverseIterator<impl::KeyIter<Object>>(dict);
        // }

    };

    struct Values {

    };

    struct Items {

    };

    /* Equivalent to Python `dict.keys()`. */
    inline KeysView keys() const;

    /* Equivalent to Python `dict.values()`. */
    inline ValuesView values() const;

    /* Equivalent to Python `dict.items()`. */
    inline ItemsView items() const;

    /////////////////////////
    ////    OPERATORS    ////
    /////////////////////////

    inline friend Dict operator|(
        const Dict& self,
        const std::initializer_list<impl::DictInitializer>& other
    ) {
        Dict result = self.copy();
        result.update(other);
        return result;
    }


    inline friend Dict& operator|=(
        Dict& self,
        const std::initializer_list<impl::DictInitializer>& other
    ) {
        self.update(other);
        return self;
    }

};


/* Implicitly convert a py::Dict into a C++ mapping type. */
template <std::derived_from<Dict> Self, impl::cpp_like T> requires (impl::dict_like<T>)
struct __cast__<Self, T> {
    static T operator()(const Self& self) {
        T result;
        PyObject* key;
        PyObject* value;
        Py_ssize_t pos = 0;
        while (PyDict_Next(self.ptr(), &pos, &key, &value)) {
            using Key = typename T::key_type;
            using Value = typename T::mapped_type;
            Key converted_key = Handle(key).template cast<Key>();
            Value converted_value = Handle(value).template cast<Value>();
            result[converted_key] = converted_value;
        }
        return result;
    }
};


namespace impl {
namespace ops {

    template <typename Return, std::derived_from<DictTag> Self>
    struct len<Return, Self> {
        static size_t operator()(const Self& self) {
            return static_cast<size_t>(PyDict_Size(self.ptr()));
        }
    };

    template <typename Return, std::derived_from<DictTag> Self, typename Key>
    struct contains<Return, Self, Key> {
        static bool operator()(const Self& self, const to_object<Key>& key) {
            int result = PyDict_Contains(self.ptr(), key.ptr());
            if (result == -1) {
                Exception::from_python();
            }
            return result;
        }
    };

}
}


////////////////////////////
////    MAPPINGPROXY    ////
////////////////////////////


template <>
struct __getattr__<MappingProxy, "copy">                    : Returns<Function> {};
template <>
struct __getattr__<MappingProxy, "get">                     : Returns<Function> {};
template <>
struct __getattr__<MappingProxy, "keys">                    : Returns<Function> {};
template <>
struct __getattr__<MappingProxy, "values">                  : Returns<Function> {};
template <>
struct __getattr__<MappingProxy, "items">                   : Returns<Function> {};

template <>
struct __len__<MappingProxy>                                : Returns<size_t> {};
template <>
struct __iter__<MappingProxy>                               : Returns<Object> {};
template <>
struct __reversed__<MappingProxy>                           : Returns<Object> {};
template <impl::is_hashable Key>
struct __contains__<MappingProxy, Key>                      : Returns<bool> {};
template <impl::is_hashable Key>
struct __getitem__<MappingProxy, Key>                       : Returns<Object> {};
template <>
struct __or__<MappingProxy, Object>                         : Returns<Dict> {};
template <impl::dict_like T>
struct __or__<MappingProxy, T>                              : Returns<Dict> {};


/* Represents a statically-typed Python `MappingProxyType` object in C++. */
class MappingProxy : public Object, public impl::MappingProxyTag {
    using Base = Object;

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return impl::mappingproxy_like<T>;
    }

    template <typename T>
    static constexpr bool check(const T& obj) {
        if constexpr (impl::cpp_like<T>) {
            return check<T>();

        } else if constexpr (check<T>()) {
            return obj.ptr() != nullptr;

        } else if constexpr (impl::is_object_exact<T>) {
            if (obj.ptr() == nullptr) {
                return false;
            }
            int result = PyObject_IsInstance(
                obj,
                (PyObject*) &PyDictProxy_Type
            );
            if (result == -1) {
                Exception::from_python();
            }
            return result;

        } else {
            return false;
        }
    }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    MappingProxy(Handle h, const borrowed_t& t) : Base(h, t) {}
    MappingProxy(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    MappingProxy(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    MappingProxy(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<MappingProxy>(accessor).release(), stolen_t{})
    {}

    /* Construct a read-only view on an existing dictionary. */
    MappingProxy(const Dict& dict) : Base(PyDictProxy_New(dict.ptr()), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
    }

    ////////////////////////////////
    ////    PYTHON INTERFACE    ////
    ////////////////////////////////

    /* Equivalent to Python `mappingproxy.copy()`. */
    Dict copy() const;

    /* Equivalent to Python `mappingproxy.get(key)`. */
    template <impl::is_hashable K>
    inline Object get(const K& key) const;

    /* Equivalent to Python `mappingproxy.get(key, default)`. */
    template <impl::is_hashable K>
    inline Object get(const K& key, const Object& default_value) const;

    /* Equivalent to Python `mappingproxy.keys()`. */
    inline KeysView keys() const;

    /* Equivalent to Python `mappingproxy.values()`. */
    inline ValuesView values() const;

    /* Equivalent to Python `mappingproxy.items()`. */
    inline ItemsView items() const;

    /////////////////////////
    ////    OPERATORS    ////
    /////////////////////////

    inline friend Dict operator|(
        const MappingProxy& self,
        const std::initializer_list<impl::DictInitializer>& other
    ) {
        Dict result = self.copy();
        result |= other;
        return result;
    }

};


inline MappingProxy KeysView::mapping() const {
    return attr<"mapping">();
}


inline MappingProxy ValuesView::mapping() const {
    return attr<"mapping">();
}


inline MappingProxy ItemsView::mapping() const {
    return attr<"mapping">();
}


}  // namespace py
}  // namespace bertrand


#endif  // BERTRAND_PYTHON_DICT_H

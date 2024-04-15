#if !defined(BERTRAND_PYTHON_COMMON_INCLUDED) && !defined(LINTER)
#error "This file should not be included directly.  Please include <bertrand/common.h> instead."
#endif

#ifndef BERTRAND_PYTHON_COMMON_ITERATORS_H
#define BERTRAND_PYTHON_COMMON_ITERATORS_H

#include "declarations.h"
#include "concepts.h"
#include "exceptions.h"
#include "operators.h"
#include "object.h"


namespace bertrand {
namespace py {
namespace impl {


/* An optimized iterator that directly accesses tuple or list elements through the
CPython API. */
template <typename Policy>
class Iterator {
    static_assert(
        std::is_base_of_v<Object, typename Policy::value_type>,
        "Iterator must dereference to a subclass of py::Object.  Check your "
        "specialization of __iter__ for this type and ensure the Return type is "
        "derived from py::Object."
    );

protected:
    Policy policy;

    static constexpr bool random_access = std::is_same_v<
        typename Policy::iterator_category,
        std::random_access_iterator_tag
    >;
    static constexpr bool bidirectional = random_access || std::is_same_v<
        typename Policy::iterator_category,
        std::bidirectional_iterator_tag
    >;

public:
    using iterator_category        = Policy::iterator_category;
    using difference_type          = Policy::difference_type;
    using value_type               = Policy::value_type;
    using pointer                  = Policy::pointer;
    using reference                = Policy::reference;

    /* Default constructor.  Initializes to a sentinel iterator. */
    template <typename... Args>
    Iterator(Args&&... args) : policy(std::forward<Args>(args)...) {}

    /* Copy constructor. */
    Iterator(const Iterator& other) : policy(other.policy) {}

    /* Move constructor. */
    Iterator(Iterator&& other) : policy(std::move(other.policy)) {}

    /* Copy assignment operator. */
    Iterator& operator=(const Iterator& other) {
        policy = other.policy;
        return *this;
    }

    /* Move assignment operator. */
    Iterator& operator=(Iterator&& other) {
        policy = std::move(other.policy);
        return *this;
    }

    /////////////////////////////////
    ////    ITERATOR PROTOCOL    ////
    /////////////////////////////////

    /* Dereference the iterator. */
    inline value_type operator*() const {
        return policy.deref();
    }

    /* Dereference the iterator. */
    inline pointer operator->() const {
        return &(**this);
    }

    /* Advance the iterator. */
    inline Iterator& operator++() {
        policy.advance();
        return *this;
    }

    /* Advance the iterator. */
    inline Iterator operator++(int) {
        Iterator copy = *this;
        policy.advance();
        return copy;
    }

    /* Compare two iterators for equality. */
    inline bool operator==(const Iterator& other) const {
        return policy.compare(other.policy);
    }

    /* Compare two iterators for inequality. */
    inline bool operator!=(const Iterator& other) const {
        return !policy.compare(other.policy);
    }

    ///////////////////////////////////////
    ////    BIDIRECTIONAL ITERATORS    ////
    ///////////////////////////////////////

    /* Retreat the iterator. */
    template <typename T = Iterator> requires (bidirectional)
    inline Iterator& operator--() {
        policy.retreat();
        return *this;
    }

    /* Retreat the iterator. */
    template <typename T = Iterator> requires (bidirectional)
    inline Iterator operator--(int) {
        Iterator copy = *this;
        policy.retreat();
        return copy;
    }

    ///////////////////////////////////////
    ////    RANDOM ACCESS ITERATORS    ////
    ///////////////////////////////////////

    /* Advance the iterator by n steps. */
    template <typename T = Iterator> requires (random_access)
    inline Iterator operator+(difference_type n) const {
        Iterator copy = *this;
        copy += n;
        return copy;
    }

    /* Advance the iterator by n steps. */
    template <typename T = Iterator> requires (random_access)
    inline Iterator& operator+=(difference_type n) {
        policy.advance(n);
        return *this;
    }

    /* Retreat the iterator by n steps. */
    template <typename T = Iterator> requires (random_access)
    inline Iterator operator-(difference_type n) const {
        Iterator copy = *this;
        copy -= n;
        return copy;
    }

    /* Retreat the iterator by n steps. */
    template <typename T = Iterator> requires (random_access)
    inline Iterator& operator-=(difference_type n) {
        policy.retreat(n);
        return *this;
    }

    /* Calculate the distance between two iterators. */
    template <typename T = Iterator> requires (random_access)
    inline difference_type operator-(const Iterator& other) const {
        return policy.distance(other.policy);
    }

    /* Access the iterator at an offset. */
    template <typename T = Iterator> requires (random_access)
    inline value_type operator[](difference_type n) const {
        return *(*this + n);
    }

    /* Compare two iterators for ordering. */
    template <typename T = Iterator> requires (random_access)
    inline bool operator<(const Iterator& other) const {
        return !!policy && (*this - other) < 0;
    }

    /* Compare two iterators for ordering. */
    template <typename T = Iterator> requires (random_access)
    inline bool operator<=(const Iterator& other) const {
        return !!policy && (*this - other) <= 0;
    }

    /* Compare two iterators for ordering. */
    template <typename T = Iterator> requires (random_access)
    inline bool operator>=(const Iterator& other) const {
        return !policy || (*this - other) >= 0;
    }

    /* Compare two iterators for ordering. */
    template <typename T = Iterator> requires (random_access)
    inline bool operator>(const Iterator& other) const {
        return !policy || (*this - other) > 0;
    }

};


/* An adapter for an Iterator class that swaps the meanings of the increment and
decrement operators, converting a forward iterator into a reverse iterator. */
template <typename Policy>
class ReverseIterator : public Iterator<Policy> {
    using Base = Iterator<Policy>;
    static_assert(
        Base::bidirectional,
        "ReverseIterator can only be used with bidirectional iterators."
    );

public:
    using Base::Base;

    /* Advance the iterator. */
    inline ReverseIterator& operator++() {
        Base::operator--();
        return *this;
    }

    /* Advance the iterator. */
    inline ReverseIterator operator++(int) {
        ReverseIterator copy = *this;
        Base::operator--();
        return copy;
    }

    /* Retreat the iterator. */
    inline ReverseIterator& operator--() {
        Base::operator++();
        return *this;
    }

    /* Retreat the iterator. */
    inline ReverseIterator operator--(int) {
        ReverseIterator copy = *this;
        Base::operator++();
        return copy;
    }

    ////////////////////////////////////////
    ////    RANDOM ACCESS ITERATORS     ////
    ////////////////////////////////////////

    /* Advance the iterator by n steps. */
    template <typename T = ReverseIterator> requires (Base::random_access)
    inline ReverseIterator operator+(typename Base::difference_type n) const {
        ReverseIterator copy = *this;
        copy -= n;
        return copy;
    }

    /* Advance the iterator by n steps. */
    template <typename T = ReverseIterator> requires (Base::random_access)
    inline ReverseIterator& operator+=(typename Base::difference_type n) {
        Base::operator-=(n);
        return *this;
    }

    /* Retreat the iterator by n steps. */
    template <typename T = ReverseIterator> requires (Base::random_access)
    inline ReverseIterator operator-(typename Base::difference_type n) const {
        ReverseIterator copy = *this;
        copy += n;
        return copy;
    }

    /* Retreat the iterator by n steps. */
    template <typename T = ReverseIterator> requires (Base::random_access)
    inline ReverseIterator& operator-=(typename Base::difference_type n) {
        Base::operator+=(n);
        return *this;
    }

};


/* A generic iterator policy that uses Python's existing iterator protocol. */
template <typename Deref>
class GenericIter {
    Object iter;
    PyObject* curr;

public:
    using iterator_category         = std::input_iterator_tag;
    using difference_type           = std::ptrdiff_t;
    using value_type                = Deref;
    using pointer                   = Deref*;
    using reference                 = Deref&;

    /* Default constructor.  Initializes to a sentinel iterator. */
    GenericIter() :
        iter(reinterpret_steal<Object>(nullptr)), curr(nullptr)
    {}

    /* Wrap a raw Python iterator. */
    GenericIter(Object&& iterator) : iter(std::move(iterator)) {
        curr = PyIter_Next(iter.ptr());
        if (curr == nullptr && PyErr_Occurred()) {
            Exception::from_python();
        }
    }

    /* Copy constructor. */
    GenericIter(const GenericIter& other) : iter(other.iter), curr(other.curr) {
        Py_XINCREF(curr);
    }

    /* Move constructor. */
    GenericIter(GenericIter&& other) : iter(std::move(other.iter)), curr(other.curr) {
        other.curr = nullptr;
    }

    /* Copy assignment operator. */
    GenericIter& operator=(const GenericIter& other) {
        if (&other != this) {
            iter = other.iter;
            PyObject* temp = curr;
            Py_XINCREF(curr);
            curr = other.curr;
            Py_XDECREF(temp);
        }
        return *this;
    }

    /* Move assignment operator. */
    GenericIter& operator=(GenericIter&& other) {
        if (&other != this) {
            iter = std::move(other.iter);
            PyObject* temp = curr;
            curr = other.curr;
            other.curr = nullptr;
            Py_XDECREF(temp);
        }
        return *this;
    }

    ~GenericIter() {
        Py_XDECREF(curr);
    }

    /* Dereference the iterator. */
    inline Deref deref() const {
        if (curr == nullptr) {
            throw ValueError("attempt to dereference a null iterator.");
        }
        return reinterpret_borrow<Deref>(curr);
    }

    /* Advance the iterator. */
    inline void advance() {
        PyObject* temp = curr;
        curr = PyIter_Next(iter.ptr());
        Py_XDECREF(temp);
        if (curr == nullptr && PyErr_Occurred()) {
            Exception::from_python();
        }
    }

    /* Compare two iterators for equality. */
    inline bool compare(const GenericIter& other) const {
        return curr == other.curr;
    }

    inline explicit operator bool() const {
        return curr != nullptr;
    }

};


/* A random access iterator policy that directly addresses tuple elements using the
CPython API. */
template <typename Deref>
class TupleIter {
    Object tuple;
    PyObject* curr;
    Py_ssize_t index;

public:
    using iterator_category         = std::random_access_iterator_tag;
    using difference_type           = std::ptrdiff_t;
    using value_type                = Deref;
    using pointer                   = Deref*;
    using reference                 = Deref&;

    /* Sentinel constructor. */
    TupleIter(Py_ssize_t index) :
        tuple(reinterpret_steal<Object>(nullptr)), curr(nullptr), index(index)
    {}

    /* Construct an iterator from a tuple and a starting index. */
    TupleIter(const Object& tuple, Py_ssize_t index) :
        tuple(tuple), index(index)
    {
        if (index >= 0 && index < PyTuple_GET_SIZE(tuple.ptr())) {
            curr = PyTuple_GET_ITEM(tuple.ptr(), index);
        } else {
            curr = nullptr;
        }
    }

    /* Copy constructor. */
    TupleIter(const TupleIter& other) :
        tuple(other.tuple), curr(other.curr), index(other.index)
    {}

    /* Move constructor. */
    TupleIter(TupleIter&& other) :
        tuple(std::move(other.tuple)), curr(other.curr), index(other.index)
    {
        other.curr = nullptr;
    }

    /* Copy assignment operator. */
    TupleIter& operator=(const TupleIter& other) {
        if (&other != this) {
            tuple = other.tuple;
            curr = other.curr;
            index = other.index;
        }
        return *this;
    }

    /* Move assignment operator. */
    TupleIter& operator=(TupleIter&& other) {
        if (&other != this) {
            tuple = other.tuple;
            curr = other.curr;
            index = other.index;
            other.curr = nullptr;
        }
        return *this;
    }

    /* Dereference the iterator. */
    inline Deref deref() const {
        if (curr == nullptr) {
            throw ValueError("attempt to dereference a null iterator.");
        }
        return reinterpret_borrow<Deref>(curr);
    }

    /* Advance the iterator. */
    inline void advance(Py_ssize_t n = 1) {
        index += n;
        if (index >= 0 && index < PyTuple_GET_SIZE(tuple.ptr())) {
            curr = PyTuple_GET_ITEM(tuple.ptr(), index);
        } else {
            curr = nullptr;
        }
    }

    /* Compare two iterators for equality. */
    inline bool compare(const TupleIter& other) const {
        return curr == other.curr;
    }

    /* Retreat the iterator. */
    inline void retreat(Py_ssize_t n = 1) {
        index -= n;
        if (index >= 0 && index < PyTuple_GET_SIZE(tuple.ptr())) {
            curr = PyTuple_GET_ITEM(tuple.ptr(), index);
        } else {
            curr = nullptr;
        }
    }

    /* Calculate the distance between two iterators. */
    inline difference_type distance(const TupleIter& other) const {
        return index - other.index;
    }

    inline explicit operator bool() const {
        return curr != nullptr;
    }

};


/* A random access iterator policy that directly addresses list elements using the
CPython API. */
template <typename Deref>
class ListIter {
    Object list;
    PyObject* curr;
    Py_ssize_t index;

public:
    using iterator_category         = std::random_access_iterator_tag;
    using difference_type           = std::ptrdiff_t;
    using value_type                = Deref;
    using pointer                   = Deref*;
    using reference                 = Deref&;

    /* Default constructor.  Initializes to a sentinel iterator. */
    ListIter(Py_ssize_t index) :
        list(reinterpret_steal<Object>(nullptr)), curr(nullptr), index(index)
    {}

    /* Construct an iterator from a list and a starting index. */
    ListIter(const Object& list, Py_ssize_t index) :
        list(list), index(index)
    {
        if (index >= 0 && index < PyList_GET_SIZE(list.ptr())) {
            curr = PyList_GET_ITEM(list.ptr(), index);
        } else {
            curr = nullptr;
        }
    }

    /* Copy constructor. */
    ListIter(const ListIter& other) :
        list(other.list), curr(other.curr), index(other.index)
    {}

    /* Move constructor. */
    ListIter(ListIter&& other) :
        list(std::move(other.list)), curr(other.curr), index(other.index)
    {
        other.curr = nullptr;
    }

    /* Copy assignment operator. */
    ListIter& operator=(const ListIter& other) {
        if (&other != this) {
            list = other.list;
            curr = other.curr;
            index = other.index;
        }
        return *this;
    }

    /* Move assignment operator. */
    ListIter& operator=(ListIter&& other) {
        if (&other != this) {
            list = other.list;
            curr = other.curr;
            index = other.index;
            other.curr = nullptr;
        }
        return *this;
    }

    /* Dereference the iterator. */
    inline Deref deref() const {
        if (curr == nullptr) {
            throw IndexError("list index out of range");
        }
        return reinterpret_borrow<Deref>(curr);
    }

    /* Advance the iterator. */
    inline void advance(Py_ssize_t n = 1) {
        index += n;
        if (index >= 0 && index < PyList_GET_SIZE(list.ptr())) {
            curr = PyList_GET_ITEM(list.ptr(), index);
        } else {
            curr = nullptr;
        }
    }

    /* Compare two iterators for equality. */
    inline bool compare(const ListIter& other) const {
        return curr == other.curr;
    }

    /* Retreat the iterator. */
    inline void retreat(Py_ssize_t n = 1) {
        index -= n;
        if (index >= 0 && index < PyList_GET_SIZE(list.ptr())) {
            curr = PyList_GET_ITEM(list.ptr(), index);
        } else {
            curr = nullptr;
        }
    }

    /* Calculate the distance between two iterators. */
    inline difference_type distance(const ListIter& other) const {
        return index - other.index;
    }

    inline explicit operator bool() const {
        return curr != nullptr;
    }

};


/* An iterator policy that extracts keys from a dictionary using PyDict_Next(). */
template <typename Deref>
class KeyIter {
    Object dict;
    PyObject* curr;
    Py_ssize_t pos;

public:
    using iterator_category         = std::input_iterator_tag;
    using difference_type           = std::ptrdiff_t;
    using value_type                = Deref;
    using pointer                   = Deref*;
    using reference                 = Deref&;

    /* Default constructor.  Initializes to a sentinel iterator. */
    KeyIter() :
        dict(reinterpret_steal<Object>(nullptr)), curr(nullptr), pos(0)
    {}

    /* Construct an iterator from a dictionary. */
    KeyIter(const Object& dict) : dict(dict), pos(0) {
        if (!PyDict_Next(dict.ptr(), &pos, &curr, nullptr)) {
            curr = nullptr;
        }
    }

    /* Copy constructor. */
    KeyIter(const KeyIter& other) :
        dict(other.dict), curr(other.curr), pos(other.pos)
    {}

    /* Move constructor. */
    KeyIter(KeyIter&& other) :
        dict(std::move(other.dict)), curr(other.curr), pos(other.pos)
    {
        other.curr = nullptr;
    }

    /* Copy assignment operator. */
    KeyIter& operator=(const KeyIter& other) {
        if (&other != this) {
            dict = other.dict;
            curr = other.curr;
            pos = other.pos;
        }
        return *this;
    }

    /* Move assignment operator. */
    KeyIter& operator=(KeyIter&& other) {
        if (&other != this) {
            dict = other.dict;
            curr = other.curr;
            pos = other.pos;
            other.curr = nullptr;
        }
        return *this;
    }

    /* Dereference the iterator. */
    inline Deref deref() const {
        if (curr == nullptr) {
            throw StopIteration("end of dictionary keys");
        }
        return reinterpret_borrow<Deref>(curr);
    }

    /* Advance the iterator. */
    inline void advance() {
        if (!PyDict_Next(dict.ptr(), &pos, &curr, nullptr)) {
            curr = nullptr;
        }
    }

    /* Compare two iterators for equality. */
    inline bool compare(const KeyIter& other) const {
        return curr == other.curr;
    }

    inline explicit operator bool() const {
        return curr != nullptr;
    }

};


/* An iterator policy that extracts values from a dictionary using PyDict_Next(). */
template <typename Deref>
class ValueIter {
    Object dict;
    PyObject* curr;
    Py_ssize_t pos;

public:
    using iterator_category         = std::input_iterator_tag;
    using difference_type           = std::ptrdiff_t;
    using value_type                = Deref;
    using pointer                   = Deref*;
    using reference                 = Deref&;

    /* Default constructor.  Initializes to a sentinel iterator. */
    ValueIter() :
        dict(reinterpret_steal<Object>(nullptr)), curr(nullptr), pos(0)
    {}

    /* Construct an iterator from a dictionary. */
    ValueIter(const Object& dict) : dict(dict), pos(0) {
        if (!PyDict_Next(dict.ptr(), &pos, nullptr, &curr)) {
            curr = nullptr;
        }
    }

    /* Copy constructor. */
    ValueIter(const ValueIter& other) :
        dict(other.dict), curr(other.curr), pos(other.pos)
    {}

    /* Move constructor. */
    ValueIter(ValueIter&& other) :
        dict(std::move(other.dict)), curr(other.curr), pos(other.pos)
    {
        other.curr = nullptr;
    }

    /* Copy assignment operator. */
    ValueIter& operator=(const ValueIter& other) {
        if (&other != this) {
            dict = other.dict;
            curr = other.curr;
            pos = other.pos;
        }
        return *this;
    }

    /* Move assignment operator. */
    ValueIter& operator=(ValueIter&& other) {
        if (&other != this) {
            dict = other.dict;
            curr = other.curr;
            pos = other.pos;
            other.curr = nullptr;
        }
        return *this;
    }

    /* Dereference the iterator. */
    inline Deref deref() const {
        if (curr == nullptr) {
            throw StopIteration("end of dictionary values");
        }
        return reinterpret_borrow<Deref>(curr);
    }

    /* Advance the iterator. */
    inline void advance() {
        if (!PyDict_Next(dict.ptr(), &pos, nullptr, &curr)) {
            curr = nullptr;
        }
    }

    /* Compare two iterators for equality. */
    inline bool compare(const ValueIter& other) const {
        return curr == other.curr;
    }

    inline explicit operator bool() const {
        return curr != nullptr;
    }

};


/* An iterator policy that extracts key-value pairs from a dictionary using
PyDict_Next(). */
template <typename Deref>
class ItemIter {
    Object dict;
    PyObject* key;
    PyObject* value;
    Py_ssize_t pos;

public:
    using iterator_category         = std::input_iterator_tag;
    using difference_type           = std::ptrdiff_t;
    using value_type                = Deref;
    using pointer                   = Deref*;
    using reference                 = Deref&;

    /* Default constructor.  Initializes to a sentinel iterator. */
    ItemIter() :
        dict(reinterpret_steal<Object>(nullptr)),
        key(nullptr),
        value(nullptr),
        pos(0)
    {}

    /* Construct an iterator from a dictionary. */
    ItemIter(const Object& dict) : dict(dict), pos(0) {
        if (!PyDict_Next(dict.ptr(), &pos, &key, &value)) {
            key = nullptr;
            value = nullptr;
        }
    }

    /* Copy constructor. */
    ItemIter(const ItemIter& other) :
        dict(other.dict), key(other.key), value(other.value), pos(other.pos)
    {}

    /* Move constructor. */
    ItemIter(ItemIter&& other) :
        dict(std::move(other.dict)), key(other.key), value(other.value), pos(other.pos)
    {
        other.key = nullptr;
        other.value = nullptr;
    }

    /* Copy assignment operator. */
    ItemIter& operator=(const ItemIter& other) {
        if (&other != this) {
            dict = other.dict;
            key = other.key;
            value = other.value;
            pos = other.pos;
        }
        return *this;
    }

    /* Move assignment operator. */
    ItemIter& operator=(ItemIter&& other) {
        if (&other != this) {
            dict = other.dict;
            key = other.key;
            value = other.value;
            pos = other.pos;
            other.key = nullptr;
            other.value = nullptr;
        }
        return *this;
    }

    /* Dereference the iterator. */
    inline Deref deref() const {
        if (key == nullptr || value == nullptr) {
            throw StopIteration("end of dictionary items");
        }
        return Deref(key, value);
    }

    /* Advance the iterator. */
    inline void advance() {
        if (!PyDict_Next(dict.ptr(), &pos, &key, &value)) {
            key = nullptr;
            value = nullptr;
        }
    }

    /* Compare two iterators for equality. */
    inline bool compare(const ItemIter& other) const {
        return key == other.key && value == other.value;
    }

    inline explicit operator bool() const {
        return key != nullptr && value != nullptr;
    }

};


}  // namespace impl
}  // namespace py
}  // namespace bertrand


#endif  // BERTRAND_PYTHON_COMMON_ITERATORS_H
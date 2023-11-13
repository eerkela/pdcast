// include guard: BERTRAND_STRUCTS_LINKED_LIST_H
#ifndef BERTRAND_STRUCTS_LINKED_LIST_H
#define BERTRAND_STRUCTS_LINKED_LIST_H

#include <cstddef>  // size_t
#include <optional>  // std::optional
#include <Python.h>  // CPython API
#include "core/view.h"  // ListView
#include "base.h"  // LinkedBase


#include "../util/args.h"  // PyArgs


#include "algorithms/add.h"
#include "algorithms/append.h"
#include "algorithms/concatenate.h"
#include "algorithms/contains.h"
#include "algorithms/count.h"
#include "algorithms/extend.h"
#include "algorithms/index.h"
#include "algorithms/insert.h"
#include "algorithms/lexical_compare.h"
#include "algorithms/pop.h"
#include "algorithms/position.h"
#include "algorithms/remove.h"
#include "algorithms/repeat.h"
#include "algorithms/reverse.h"
#include "algorithms/rotate.h"
#include "algorithms/slice.h"
#include "algorithms/sort.h"


namespace bertrand {
namespace structs {
namespace linked {


/* A modular linked list class that mimics the Python list interface in C++. */
template <
    typename NodeType,
    typename SortPolicy = linked::MergeSort,
    typename LockPolicy = util::BasicLock
>
class LinkedList : public LinkedBase<linked::ListView<NodeType>, LockPolicy> {
    using Base = LinkedBase<linked::ListView<NodeType>, LockPolicy>;

public:
    using View = linked::ListView<NodeType>;
    using Node = typename View::Node;
    using Value = typename Node::Value;

    template <linked::Direction dir>
    using Iterator = typename View::template Iterator<dir>;
    template <linked::Direction dir>
    using ConstIterator = typename View::template ConstIterator<dir>;

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    // inherit constructors from LinkedBase
    using Base::Base;
    using Base::operator=;

    //////////////////////////////
    ////    LIST INTERFACE    ////
    //////////////////////////////

    /* LinkedLists implement the full Python list interface with equivalent semantics to
     * the built-in Python list type, as well as a few addons from `collections.deque`.
     * There are only a few differences:
     *
     *      1.  The append() and extend() methods accept a second boolean argument that
     *          signals whether the item(s) should be inserted at the beginning of the
     *          list or at the end.  This is similar to the appendleft() and
     *          extendleft() methods of `collections.deque`.
     *      2.  The count() method accepts optional `start` and `stop` arguments that
     *          specify a slice of the list to search within.  This is similar to the
     *          index() method of the built-in Python list.
     *      3.  LinkedLists are able to store non-Python C++ types, but only when
     *          declared from C++ code.  LinkedLists are available from Python, but can
     *          only store Python objects (i.e. PyObject*) when declared from a Python
     *          context.
     *
     * Otherwise, everything should behave exactly as expected, with similar overall
     * performance to a built-in Python list (random access limitations of linked lists
     * notwithstanding.)
     */

    /* Add an item to the end of the list. */
    inline void append(Value& item, bool left = false) {
        linked::append(this->view, item, left);
    }

    /* Insert an item into the list at the specified index. */
    inline void insert(long long index, Value& item) {
        linked::insert(this->view, index, item);
    }

    /* Extend the list by appending elements from an iterable. */
    template <typename Container>
    inline void extend(Container& items, bool left = false) {
        linked::extend(this->view, items, left);
    }

    /* Get the index of an item within the list. */
    inline size_t index(
        const Value& item,
        std::optional<long long> start = std::nullopt,
        std::optional<long long> stop = std::nullopt
    ) const {
        return linked::index(this->view, item, start, stop);
    }

    /* Count the number of occurrences of an item within the list. */
    inline size_t count(
        const Value& item,
        std::optional<long long> start = std::nullopt,
        std::optional<long long> stop = std::nullopt
    ) const {
        return linked::count(this->view, item, start, stop);
    }

    /* Check if the list contains a certain item. */
    inline bool contains(Value& item) const {
        return linked::contains(this->view, item);
    }

    /* Remove the first occurrence of an item from the list. */
    inline void remove(Value& item) {
        linked::remove(this->view, item);
    }

    /* Remove an item from the list and return its value. */
    inline Value pop(long long index = -1) {
        return linked::pop(this->view, index);
    }

    /* Remove all elements from the list. */
    inline void clear() {
        this->view.clear();
    }

    /* Return a shallow copy of the list. */
    inline LinkedList copy() const {
        return LinkedList(this->view.copy());
    }

    /* Sort the list in-place according to an optional key func. */
    template <typename Func>
    inline void sort(Func key = nullptr, bool reverse = false) {
        linked::sort<SortPolicy>(this->view, key, reverse);
    }

    /* Reverse the order of elements in the list in-place. */
    inline void reverse() {
        linked::reverse(this->view);
    }

    /* Shift all elements in the list to the right by the specified number of steps. */
    inline void rotate(long long steps = 1) {
        linked::rotate(this->view, steps);
    }

    ///////////////////////
    ////    PROXIES    ////
    ///////////////////////

    /* Proxies allow access to a particular element or slice of a list, allowing
     * convenient, Python-like syntax for list operations. 
     *
     * ElementProxies are returned by the array index operator [] when given with a
     * single numeric argument.  This argument can be negative following the same
     * semantics as built-in Python lists (i.e. -1 refers to the last element, and
     * overflow results in an error).  Each proxy offers the following methods:
     *
     *      Value get(): return the value at the current index.
     *      void set(Value& value): set the value at the current index.
     *      void del(): delete the value at the current index.
     *      void insert(Value& value): insert a value at the current index.
     *      Value pop(): remove the value at the current index and return it.
     *      operator Value(): implicitly coerce the proxy to its value in function
     *          calls and other contexts.
     *      operator=(Value& value): set the value at the current index using
     *          assignment syntax.
     *
     * SliceProxies are returned by the `slice()` factory method, which can accept
     * either a Python slice object or separate start, stop, and step arguments, each
     * of which are optional, and can be negative following the same semantics as
     * above.  Each proxy exposes the following methods:
     *
     *      LinkedList get(): return a new list containing the contents of the slice.
     *      void set(PyObject* items): overwrite the contents of the slice with the
     *          contents of the iterable.
     *      void del(): remove the slice from the list.
     *      Iterator iter(): return a coupled iterator over the slice.
     *          NOTE: slice iterators may not yield results in the same order as the
     *          step size would indicate.  This is because slices are traversed in
     *          such a way as to minimize the number of nodes that must be visited and
     *          avoid backtracking.  See linked/algorithms/slice.h for more details.
     *      Iterator begin():  return an iterator to the first element of the slice.
     *          See note above.
     *      Iterator end(): return an iterator to terminate the slice.
     */

    /* Get a proxy for a value at a particular index of the list. */
    inline linked::ElementProxy<View> operator[](long long index) {
        return linked::position(this->view, index);
    }

    /* Get a proxy for a slice within the list. */
    template <typename... Args>
    inline linked::SliceProxy<View, LinkedList> slice(Args&&... args) {
        return linked::slice<View, LinkedList>(
            this->view,
            std::forward<Args>(args)...
        );
    }

    //////////////////////////////////
    ////    OPERATOR OVERLOADS    ////
    //////////////////////////////////

    /* NOTE: operators are implemented as non-member functions for commutativity.
     * Namely, the supported operators are as follows:
     *      (+)     concatenation
     *      (*)     repetition
     *      (<)     lexicographic less-than comparison
     *      (<=)    lexicographic less-than-or-equal-to comparison
     *      (==)    lexicographic equality comparison
     *      (!=)    lexicographic inequality comparison
     *      (>=)    lexicographic greater-than-or-equal-to comparison
     *      (>)     lexicographic greater-than comparison
     *
     * These all work similarly to their Python equivalents except that they can accept
     * any iterable container in either C++ or Python to compare against.  This
     * symmetry is provided by the universal utility functions in structs/util/iter.h
     * and structs/util/python.h.
     */

};


/////////////////////////////
////    CONCATENATION    ////
/////////////////////////////


/* Concatenate a LinkedList with an arbitrary C++/Python container to produce a new
list. */
template <typename Container, typename... Ts>
LinkedList<Ts...> operator+(const LinkedList<Ts...>& lhs, const Container& rhs) {
    return linked::concatenate(lhs.view, rhs);
}


/* Concatenate a LinkedList with an arbitrary C++/Python container to produce a new
list (reversed). */
template <typename Container, typename... Ts>
LinkedList<Ts...> operator+(const Container& lhs, const LinkedList<Ts...>& rhs) {
    return linked::concatenate(lhs, rhs.view);
}


/* Concatenate a LinkedList with an arbitrary C++/Python container in-place. */
template <typename Container, typename... Ts>
LinkedList<Ts...>& operator+=(LinkedList<Ts...>& lhs, const Container& rhs) {
    linked::extend(lhs.view, rhs, false);
    return lhs;
}


// /* Allow Python-style concatenation between list-like C++ containers and Linked data
// structures. */
// template <typename T, typename Derived>
// inline auto operator+(const T& lhs, const Derived& rhs)
//     -> std::enable_if_t<
//         // first, check that T is a list-like container with a range-based insert
//         // method that returns an iterator.  This is true for all STL containers.
//         std::is_same_v<
//             decltype(
//                 std::declval<T>().insert(
//                     std::declval<T>().end(),
//                     std::declval<Derived>().begin(),
//                     std::declval<Derived>().end()
//                 )
//             ),
//             typename T::iterator
//         >,
//         // next, check that Derived inherits from Concatenateable
//         std::enable_if_t<Concatenateable<Derived>::enable, T>
//     >
// {
//     T result = lhs;
//     result.insert(result.end(), rhs.begin(), rhs.end());  // STL compliant
//     return result;
// }


//////////////////////////
////    REPETITION    ////
//////////////////////////


/* Repeat the elements of a LinkedList the specified number of times. */
template <typename Integer, typename... Ts>
inline LinkedList<Ts...> operator*(const LinkedList<Ts...>& list, const Integer rhs) {
    return linked::repeat(list.view, rhs);
}


/* Repeat the elements of a LinkedList the specified number of times (reversed). */
template <typename Integer, typename... Ts>
inline LinkedList<Ts...> operator*(const Integer lhs, const LinkedList<Ts...>& list) {
    return linked::repeat(list.view, lhs);
}


/* Repeat the elements of a LinkedList in-place the specified number of times. */
template <typename Integer, typename... Ts>
inline LinkedList<Ts...>& operator*=(LinkedList<Ts...>& list, const Integer rhs) {
    linked::repeat_inplace(list.view, rhs);
    return list;
}


////////////////////////////////////////
////    LEXICOGRAPHIC COMPARISON    ////
////////////////////////////////////////


/* Apply a lexicographic `<` comparison between the elements of a LinkedList and
another container.  */
template <typename Container, typename... Ts>
inline bool operator<(const LinkedList<Ts...>& lhs, const Container& rhs) {
    return linked::lexical_lt(lhs, rhs);
}


/* Apply a lexicographic `<` comparison between the elements of a LinkedList and
another container (reversed).  */
template <typename Container, typename... Ts>
inline bool operator<(const Container& lhs, const LinkedList<Ts...>& rhs) {
    return linked::lexical_lt(lhs, rhs);
}


/* Apply a lexicographic `<=` comparison between the elements of a LinkedList and
another container.  */
template <typename Container, typename... Ts>
inline bool operator<=(const LinkedList<Ts...>& lhs, const Container& rhs) {
    return linked::lexical_le(lhs, rhs);
}


/* Apply a lexicographic `<=` comparison between the elements of a LinkedList and
another container (reversed).  */
template <typename Container, typename... Ts>
inline bool operator<=(const Container& lhs, const LinkedList<Ts...>& rhs) {
    return linked::lexical_lt(lhs, rhs);
}


/* Apply a lexicographic `==` comparison between the elements of a LinkedList and
another container.  */
template <typename Container, typename... Ts>
inline bool operator==(const LinkedList<Ts...>& lhs, const Container& rhs) {
    return linked::lexical_eq(lhs, rhs);
}


/* Apply a lexicographic `==` comparison between the elements of a LinkedList and
another container (reversed).  */
template <typename Container, typename... Ts>
inline bool operator==(const Container& lhs, const LinkedList<Ts...>& rhs) {
    return linked::lexical_eq(lhs, rhs);
}


/* Apply a lexicographic `!=` comparison between the elements of a LinkedList and
another container.  */
template <typename Container, typename... Ts>
inline bool operator!=(const LinkedList<Ts...>& lhs, const Container& rhs) {
    return !linked::lexical_eq(lhs, rhs);
}


/* Apply a lexicographic `!=` comparison between the elements of a LinkedList and
another container (reversed).  */
template <typename Container, typename... Ts>
inline bool operator!=(const Container& lhs, const LinkedList<Ts...>& rhs) {
    return !linked::lexical_eq(lhs, rhs);
}


/* Apply a lexicographic `>=` comparison between the elements of a LinkedList and
another container.  */
template <typename Container, typename... Ts>
inline bool operator>=(const LinkedList<Ts...>& lhs, const Container& rhs) {
    return linked::lexical_ge(lhs, rhs);
}


/* Apply a lexicographic `>=` comparison between the elements of a LinkedList and
another container (reversed).  */
template <typename Container, typename... Ts>
inline bool operator>=(const Container& lhs, const LinkedList<Ts...>& rhs) {
    return linked::lexical_ge(lhs, rhs);
}


/* Apply a lexicographic `>` comparison between the elements of a LinkedList and
another container.  */
template <typename Container, typename... Ts>
inline bool operator>(const LinkedList<Ts...>& lhs, const Container& rhs) {
    return linked::lexical_gt(lhs, rhs);
}


/* Apply a lexicographic `>` comparison between the elements of a LinkedList and
another container (reversed).  */
template <typename Container, typename... Ts>
inline bool operator>(const Container& lhs, const LinkedList<Ts...>& rhs) {
    return linked::lexical_gt(lhs, rhs);
}


//////////////////////////////
////    PYTHON WRAPPER    ////
//////////////////////////////


// TODO: initializers should be const-correct at all levels
// -> const PyObject* iterable, const PyObject* spec


/* A class that binds the appropriate methods for the given view as a std::variant
of templated `ListView` types. */
class PyLinkedList : public PyLinkedBase<PyLinkedList> {
    using Base = PyLinkedBase<PyLinkedList>;
    using SelfRef = cython::SelfRef<PyLinkedList>;
    using WeakRef = typename SelfRef::WeakRef;
    using SingleList = LinkedList<
        linked::SingleNode<PyObject*>,
        linked::MergeSort,
        util::BasicLock
    >;
    using DoubleList = LinkedList<
        linked::DoubleNode<PyObject*>,
        linked::MergeSort,
        util::BasicLock
    >;

    /* A discriminated union representing all the LinkedList implementations that are
    constructable from Python. */
    using Variant = std::variant<
        SingleList,
        DoubleList
    >;

    friend Base;

    PyObject_HEAD
    Variant variant;

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    /* Select the appropriate variant based on constructor arguments. */
    template <typename... Args>
    inline static Variant get_variant(bool singly_linked, Args&&... args) {
        if (singly_linked) {
            return SingleList(std::forward<Args>(args)...);
        } else {
            return DoubleList(std::forward<Args>(args)...);
        }
    }

    /* Construct an empty PyLinkedList. */
    PyLinkedList(
        std::optional<size_t> max_size,
        PyObject* spec,
        bool singly_linked
    ) : Base(), variant(get_variant(singly_linked, max_size, spec))
    {}

    /* Construct a PyLinkedList by unpacking an input iterable. */
    PyLinkedList(
        PyObject* iterable,
        std::optional<size_t> max_size,
        PyObject* spec,
        bool reverse,
        bool singly_linked
    ) : Base(), variant(get_variant(singly_linked, iterable, max_size, spec, reverse))
    {}

    /* Construct a PyLinkedList around an existing C++ LinkedList. */
    template <typename List>
    explicit PyLinkedList(List&& list) : Base(), variant(std::move(list)) {}

public:

    /* Initialize a LinkedList instance from Python. */
    inline static int __init__(
        PyLinkedList* self,
        PyObject* args,
        PyObject* kwargs
    ) {
        using Args = util::PyArgs<util::CallProtocol::KWARGS>;
        using util::ValueError;
        try {
            // parse arguments
            Args pyargs(args, kwargs);
            PyObject* iterable = pyargs.parse(
                "iterable", Base::none_to_null, (PyObject*) nullptr
            );
            std::optional<size_t> max_size = pyargs.parse(
                "max_size",
                [](PyObject* obj) -> std::optional<size_t> {
                    if (obj == Py_None) return std::nullopt;
                    long long result = Base::parse_index(obj);
                    if (result < 0) throw ValueError("max_size cannot be negative");
                    return std::make_optional(static_cast<size_t>(result));
                },
                std::optional<size_t>()
            );
            PyObject* spec = pyargs.parse("spec", Base::none_to_null, (PyObject*) nullptr);
            bool reverse = pyargs.parse("reverse", Base::is_truthy, false);
            bool singly_linked = pyargs.parse("singly_linked", Base::is_truthy, false);
            pyargs.finalize();

            // initialize
            if (iterable == nullptr) {
                new (self) PyLinkedList(max_size, spec, singly_linked);
            } else {
                new (self) PyLinkedList(iterable, max_size, spec, reverse, singly_linked);
            }

            // exit normally
            return 0;

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return -1;
        }
    }

    /* Implement `LinkedList.append()` in Python. */
    static PyObject* append(
        PyLinkedList* self,
        PyObject* const* args,
        Py_ssize_t nargs,
        PyObject* kwnames
    ) {
        using Args = util::PyArgs<util::CallProtocol::VECTORCALL>;
        try {
            // parse arguments
            Args pyargs(args, nargs, kwnames);
            PyObject* item = pyargs.parse("item");
            bool left = pyargs.parse("left", Base::is_truthy,false);
            pyargs.finalize();

            // invoke equivalent C++ method
            std::visit(
                [&item, &left](auto& list) {
                    list.append(item, left);
                },
                self->variant
            );

            // exit normally
            Py_RETURN_NONE;

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.insert()` in Python. */
    static PyObject* insert(
        PyLinkedList* self,
        PyObject* const* args,
        Py_ssize_t nargs,
        PyObject* kwnames
    ) {
        using Args = util::PyArgs<util::CallProtocol::VECTORCALL>;
        try {
            // parse arguments
            Args pyargs(args, nargs, kwnames);
            long long index = pyargs.parse("index", Base::parse_index);
            PyObject* item = pyargs.parse("item");
            pyargs.finalize();

            // invoke equivalent C++ method
            std::visit(
                [&index, &item](auto& list) {
                    list.insert(index, item);
                },
                self->variant
            );

            // exit normally
            Py_RETURN_NONE;

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.extend()` in Python. */
    static PyObject* extend(
        PyLinkedList* self,
        PyObject* const* args,
        Py_ssize_t nargs,
        PyObject* kwnames
    ) {
        using Args = util::PyArgs<util::CallProtocol::VECTORCALL>;
        try {
            // parse arguments
            Args pyargs(args, nargs, kwnames);
            PyObject* items = pyargs.parse("items");
            bool left = pyargs.parse("left", Base::is_truthy, false);
            pyargs.finalize();

            // invoke equivalent C++ method
            std::visit(
                [&items, &left](auto& list) {
                    list.extend(items, left);
                },
                self->variant
            );

            // exit normally
            Py_RETURN_NONE;

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.index()` in Python. */
    static PyObject* index(
        PyLinkedList* self,
        PyObject* const* args,
        Py_ssize_t nargs,
        PyObject* kwnames
    ) {
        using Args = util::PyArgs<util::CallProtocol::VECTORCALL>;
        using Index = std::optional<long long>;
        try {
            // parse arguments
            Args pyargs(args, nargs, kwnames);
            PyObject* item = pyargs.parse("item");
            Index start = pyargs.parse(
                "start", Base::parse_opt_index, Index()
            );
            Index stop = pyargs.parse(
                "stop", Base::parse_opt_index, Index()
            );
            pyargs.finalize();

            // invoke equivalent C++ method
            size_t result = std::visit(
                [&item, &start, &stop](auto& list) {
                    return list.index(item, start, stop);
                },
                self->variant
            );

            // return as Python integer
            return PyLong_FromSize_t(result);

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.count()` in Python. */
    static PyObject* count(
        PyLinkedList* self,
        PyObject* const* args,
        Py_ssize_t nargs,
        PyObject* kwnames
    ) {
        using Args = util::PyArgs<util::CallProtocol::VECTORCALL>;
        using Index = std::optional<long long>;
        try {
            // parse arguments
            Args pyargs(args, nargs, kwnames);
            PyObject* item = pyargs.parse("item");
            Index start = pyargs.parse(
                "start", Base::parse_opt_index, Index()
            );
            Index stop = pyargs.parse(
                "stop", Base::parse_opt_index, Index()
            );
            pyargs.finalize();

            // invoke equivalent C++ method
            size_t result = std::visit(
                [&item, &start, &stop](auto& list) {
                    return list.count(item, start, stop);
                },
                self->variant
            );

            // return as Python integer
            return PyLong_FromSize_t(result);

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.remove()` in Python. */
    static PyObject* remove(PyLinkedList* self, PyObject* item) {
        try {
            // invoke equivalent C++ method
            std::visit(
                [&item](auto& list) {
                    list.remove(item);
                },
                self->variant
            );

            // exit normally
            Py_RETURN_NONE;

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.pop()` in Python. */
    static PyObject* pop(
        PyLinkedList* self,
        PyObject* const* args,
        Py_ssize_t nargs
    ) {
        using Args = util::PyArgs<util::CallProtocol::FASTCALL>;
        try {
            // parse arguments
            Args pyargs(args, nargs);
            long long index = pyargs.parse(
                "index", Base::parse_index, (long long) -1
            );
            pyargs.finalize();

            // invoke equivalent C++ method
            return std::visit(
                [&index](auto& list) {
                    return list.pop(index);  // returns new reference
                },
                self->variant
            );

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.clear()` in Python. */
    static PyObject* clear(PyLinkedList* self, PyObject* /* ignored */) {
        try {
            // invoke equivalent C++ method
            std::visit(
                [](auto& list) {
                    list.clear();
                },
                self->variant
            );

            // exit normally
            Py_RETURN_NONE;

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.copy()` in Python. */
    static PyObject* copy(PyLinkedList* self, PyObject* /* ignored */) {
        // allocate new Python list
        PyLinkedList* result = reinterpret_cast<PyLinkedList*>(
            Base::__new__(&Type, nullptr, nullptr)
        );
        if (result == nullptr) return nullptr;  // propagate

        // delegate to equivalent C++ method
        try {
            return std::visit(
                [&result](auto& list) {
                    new (result) PyLinkedList(list.copy());
                    return reinterpret_cast<PyObject*>(result);
                },
                self->variant
            );

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            Py_DECREF(result);
            return nullptr;
        }
    }

    /* Implement `LinkedList.sort()` in Python. */
    static PyObject* sort(
        PyLinkedList* self,
        PyObject* const* args,
        Py_ssize_t nargs,
        PyObject* kwnames
    ) {
        using Args = util::PyArgs<util::CallProtocol::VECTORCALL>;
        try {
            // parse arguments
            Args pyargs(args, nargs, kwnames);
            if (pyargs.positional() != 0) {
                PyErr_Format(
                    PyExc_TypeError,
                    "sort() takes no positional arguments",
                    pyargs.positional()
                );
                return nullptr;
            }
            PyObject* key = pyargs.parse("key", Base::none_to_null, (PyObject*) nullptr);
            bool reverse = pyargs.parse("reverse", Base::is_truthy, false);
            pyargs.finalize();

            // invoke equivalent C++ method
            std::visit(
                [&key, &reverse](auto& list) {
                    list.sort(key, reverse);
                },
                self->variant
            );

            // exit normally
            Py_RETURN_NONE;

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.reverse()` in Python. */
    static PyObject* reverse(PyLinkedList* self, PyObject* /* ignored */) {
        try {
            // invoke equivalent C++ method
            std::visit(
                [](auto& list) {
                    list.reverse();
                },
                self->variant
            );

            // exit normally
            Py_RETURN_NONE;

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.rotate()` in Python. */
    static PyObject* rotate(
        PyLinkedList* self, 
        PyObject* const* args,
        Py_ssize_t nargs
    ) {
        using Args = util::PyArgs<util::CallProtocol::FASTCALL>;
        try {
            // parse arguments
            Args pyargs(args, nargs);
            long long steps = pyargs.parse("steps", Base::parse_index, (long long) 1);
            pyargs.finalize();

            // invoke equivalent C++ method
            std::visit(
                [&steps](auto& list) {
                    list.rotate(steps);
                },
                self->variant
            );

            // exit normally
            Py_RETURN_NONE;

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.__contains__()` in Python. */
    static int __contains__(PyLinkedList* self, PyObject* item) {
        try {
            // invoke equivalent C++ method
            return std::visit(
                [&item](auto& list) {
                    return list.contains(item);
                },
                self->variant
            );

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return -1;
        }
    }

    /* Implement `LinkedList.__getitem__()` in Python. */
    static PyObject* __getitem__(PyLinkedList* self, PyObject* key) {
        try {
            // check for integer index
            if (PyIndex_Check(key)) {
                long long index = Base::parse_index(key);
                PyObject* result = std::visit(
                    [&index](auto& list) {
                        return list[index].get();
                    },
                    self->variant
                );
                return Py_XNewRef(result);  // return borrowed reference
            }

            // check for slice
            if (PySlice_Check(key)) {
                PyLinkedList* result = reinterpret_cast<PyLinkedList*>(
                    Base::__new__(&Type, nullptr, nullptr)
                );
                if (result == nullptr) throw util::catch_python();
                return std::visit(
                    [&result, &key](auto& list) {
                        new (result) PyLinkedList(list.slice(key).get());
                        return reinterpret_cast<PyObject*>(result);
                    },
                    self->variant
                );
            }

            // unrecognized key type
            PyErr_Format(
                PyExc_TypeError,
                "list indices must be integers or slices, not %s",
                Py_TYPE(key)->tp_name
            );
            return nullptr;

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.__setitem__()/__delitem__()` in Python (slice). */
    static int __setitem__(
        PyLinkedList* self,
        PyObject* key,
        PyObject* items
    ) {
        try {
            // check for integer index
            if (PyIndex_Check(key)) {
                long long index = Base::parse_index(key);
                std::visit(
                    [&index, &items](auto& list) {
                        if (items == nullptr) {
                            list[index].del();
                        } else {
                            list[index].set(items);
                        }
                    },
                    self->variant
                );
                return 0;
            }

            // check for slice
            if (PySlice_Check(key)) {
                std::visit(
                    [&key, &items](auto& list) {
                        if (items == nullptr) {
                            list.slice(key).del();
                        } else {
                            list.slice(key).set(items);
                        }
                    },
                    self->variant
                );
                return 0;
            }

            // unrecognized key type
            PyErr_Format(
                PyExc_TypeError,
                "list indices must be integers or slices, not %s",
                Py_TYPE(key)->tp_name
            );
            return -1;

        } catch (...) {
            util::throw_python();
            return -1;
        }
    }

    /* Implement `LinkedList.__add__()/__radd__()` in Python. */
    static PyObject* __add__(PyObject* lhs, PyObject* rhs) {
        // differentiate between left/right operands
        PyLinkedList* self;
        PyObject* other;
        if (PyObject_TypeCheck(lhs, &PyLinkedList::Type)) {
            self = reinterpret_cast<PyLinkedList*>(lhs);
            other = rhs;
        } else if (PyObject_TypeCheck(rhs, &Type)) {
            self = reinterpret_cast<PyLinkedList*>(rhs);
            other = lhs;
        } else {
            Py_RETURN_NOTIMPLEMENTED;  // continue with overload resolution
        }

        // allocate new Python list
        PyLinkedList* result = reinterpret_cast<PyLinkedList*>(
            Base::__new__(&Type, nullptr, nullptr)
        );
        if (result == nullptr) return nullptr;  // propagate

        // delegate to equivalent C++ operator
        try {
            return std::visit(
                [&other, &result](auto& list) {
                    new (result) PyLinkedList(list + other);
                    return reinterpret_cast<PyObject*>(result);
                },
                self->variant
            );

        // translate C++ errors into Python exceptions
        } catch (...) {
            Py_DECREF(result);
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.__iadd__()` in Python. */
    static PyObject* __iadd__(PyLinkedList* self, PyObject* other) {
        try {
            std::visit(
                [&other](auto& list) {
                    list += other;
                },
                self->variant
            );
            Py_INCREF(self);
            return reinterpret_cast<PyObject*>(self);

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.__mul__()__rmul__()` in Python. */
    static PyObject* __mul__(PyLinkedList* self, Py_ssize_t count) {
        PyLinkedList* result = reinterpret_cast<PyLinkedList*>(
            Base::__new__(&Type, nullptr, nullptr)
        );
        if (result == nullptr) return nullptr;  // propagate

        // delegate to equivalent C++ operator
        try {
            return std::visit(
                [&count, &result](auto& list) {
                    new (result) PyLinkedList(list * count);
                    return reinterpret_cast<PyObject*>(result);
                },
                self->variant
            );

        // translate C++ errors into Python exceptions
        } catch (...) {
            Py_DECREF(result);
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.__imul__()` in Python. */
    static PyObject* __imul__(PyLinkedList* self, Py_ssize_t count) {
        try {
            std::visit(
                [&count](auto& list) {
                    list *= count;
                },
                self->variant
            );
            Py_INCREF(self);
            return reinterpret_cast<PyObject*>(self);

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.__lt__()/__le__()/__eq__()/__ne__()/__ge__()/__gt__()` in
    Python. */
    static PyObject* __richcompare__(
        PyLinkedList* self,
        PyObject* other,
        int cmp
    ) {
        try {
            bool result = std::visit(
                [&other, &cmp](auto& list) {
                    switch (cmp) {
                        case Py_LT:
                            return list < other;
                        case Py_LE:
                            return list <= other;
                        case Py_EQ:
                            return list == other;
                        case Py_NE:
                            return list != other;
                        case Py_GE:
                            return list >= other;
                        case Py_GT:
                            return list > other;
                        default:
                            throw util::ValueError("invalid comparison operator");
                    }
                },
                self->variant
            );
            return PyBool_FromLong(result);
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.__str__()` in Python. */
    static PyObject* __str__(PyLinkedList* self) {
        try {
            std::ostringstream stream;
            stream << "[";
            std::visit(
                [&stream](auto& list) {
                    auto it = list.begin();
                    if (it != list.end()) {
                        stream << util::repr(*it);
                        ++it;
                    }
                    for (; it != list.end(); ++it) {
                        stream << ", " << util::repr(*it);
                    }
                },
                self->variant
            );
            stream << "]";
            return PyUnicode_FromString(stream.str().c_str());

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `LinkedList.__repr__()` in Python. */
    static PyObject* __repr__(PyLinkedList* self) {
        try {
            std::ostringstream stream;
            stream << "LinkedList([";
            std::visit(
                [&stream](auto& list) {
                    auto it = list.begin();
                    if (it != list.end()) {
                        stream << util::repr(*it);
                        ++it;
                    }
                    for (; it != list.end(); ++it) {
                        stream << ", " << util::repr(*it);
                    }
                },
                self->variant
            );
            stream << "])";
            return PyUnicode_FromString(stream.str().c_str());

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

private:

    /* Implement `PySequence_GetItem()` in CPython API. */
    static PyObject* __getitem_scalar__(PyLinkedList* self, Py_ssize_t index) {
        try {
            PyObject* result = std::visit(
                [&index](auto& list) {
                    return list[index].get();
                },
                self->variant
            );
            return Py_NewRef(result);

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return nullptr;
        }
    }

    /* Implement `PySequence_SetItem()` in CPython API. */
    static int __setitem_scalar__(
        PyLinkedList* self,
        Py_ssize_t index,
        PyObject* item
    ) {
        try {
            std::visit(
                [&index, &item](auto& list) {
                    if (item == nullptr) {
                        list[index].del();
                    } else {
                        list[index].set(item);
                    }
                },
                self->variant
            );
            return 0;

        // translate C++ errors into Python exceptions
        } catch (...) {
            util::throw_python();
            return -1;
        }
    }

    /* docstrings for public Python attributes. */
    struct docs {

    };

    ////////////////////////////////
    ////    PYTHON INTERNALS    ////
    ////////////////////////////////

    /* Vtable containing Python @property definitions for the LinkedList. */
    inline static PyGetSetDef properties[] = {
        {
            "capacity",
            (getter) Base::capacity,
            NULL,
            PyDoc_STR(Base::docs::capacity.data()),
        },
        {
            "max_size",
            (getter) Base::max_size,
            NULL,
            PyDoc_STR(Base::docs::max_size.data())
        },
        {
            "dynamic",
            (getter) Base::dynamic,
            NULL,
            PyDoc_STR(Base::docs::dynamic.data())
        },
        {
            "frozen",
            (getter) Base::frozen,
            NULL,
            PyDoc_STR(Base::docs::frozen.data())
        },
        {
            "nbytes",
            (getter) Base::nbytes,
            NULL,
            PyDoc_STR(Base::docs::nbytes.data())
        },
        {
            "specialization",
            (getter) Base::specialization,
            NULL,
            PyDoc_STR(Base::docs::specialization.data())
        },
        {NULL}  // sentinel
    };

    /* Vtable containing Python method definitions for the LinkedList. */
    inline static PyMethodDef methods[] = {
        {
            "__reversed__",
            (PyCFunction) Base::__reversed__,
            METH_NOARGS,
            "docstring for __reversed__()"
        },
        {
            "defragment",
            (PyCFunction) Base::defragment,
            METH_NOARGS,
            PyDoc_STR(Base::docs::defragment.data())
        },
        {
            "specialize",
            (PyCFunction) Base::specialize,
            METH_O,
            PyDoc_STR(Base::docs::specialize.data())
        },
        {
            "append",
            (PyCFunction) append,
            METH_FASTCALL | METH_KEYWORDS,
            "docstring for append()"
        },
        {
            "insert",
            (PyCFunction) insert,
            METH_FASTCALL | METH_KEYWORDS,
            "docstring for insert()"
        },
        {
            "extend",
            (PyCFunction) extend,
            METH_FASTCALL | METH_KEYWORDS,
            "docstring for extend()"
        },
        {
            "index",
            (PyCFunction) index,
            METH_FASTCALL | METH_KEYWORDS,
            "docstring for index()"
        },
        {
            "count",
            (PyCFunction) count,
            METH_FASTCALL | METH_KEYWORDS,
            "docstring for count()"
        },
        {
            "remove",
            (PyCFunction) remove,
            METH_O,
            "docstring for remove()"
        },
        {
            "pop",
            (PyCFunction) pop,
            METH_FASTCALL,
            "docstring for pop()"
        },
        {
            "clear",
            (PyCFunction) clear,
            METH_NOARGS,
            "docstring for clear()"
        },
        {
            "copy",
            (PyCFunction) copy,
            METH_NOARGS,
            "docstring for copy()"
        },
        {
            "sort",
            (PyCFunction) sort,
            METH_FASTCALL | METH_KEYWORDS,
            "docstring for sort()"
        },
        {
            "reverse",
            (PyCFunction) reverse,
            METH_NOARGS,
            "docstring for reverse()"
        },
        {
            "rotate",
            (PyCFunction) rotate,
            METH_FASTCALL,
            "docstring for rotate()"
        },
        {NULL}  // sentinel
    };

    /* Vtable containing special methods related to Python's mapping protocol. */
    inline static PyMappingMethods mapping = [] {
        PyMappingMethods slots;
        slots.mp_length = (lenfunc) Base::__len__;
        slots.mp_subscript = (binaryfunc) __getitem__;
        slots.mp_ass_subscript = (objobjargproc) __setitem__;
        return slots;
    }();

    /* Vtable containing special methods related to Python's sequence protocol. */
    inline static PySequenceMethods sequence = [] {
        PySequenceMethods slots;
        slots.sq_length = (lenfunc) Base::__len__;
        slots.sq_concat = (binaryfunc) __add__;
        slots.sq_repeat = (ssizeargfunc) __mul__;
        slots.sq_item = (ssizeargfunc) __getitem_scalar__;
        slots.sq_ass_item = (ssizeobjargproc) __setitem_scalar__;
        slots.sq_contains = (objobjproc) __contains__;
        slots.sq_inplace_concat = (binaryfunc) __iadd__;
        slots.sq_inplace_repeat = (ssizeargfunc) __imul__;
        return slots;
    }();

    /* Initialize a PyTypeObject to represent the list in Python. */
    static PyTypeObject init_type() {
        return {
            .ob_base = PyObject_HEAD_INIT(NULL)
            .tp_name = "LinkedList",
            .tp_basicsize = sizeof(PyLinkedList),
            .tp_itemsize = 0,
            .tp_dealloc = (destructor) Base::__dealloc__,
            .tp_repr = (reprfunc) __repr__,
            .tp_as_sequence = &sequence,
            .tp_as_mapping = &mapping,
            .tp_hash = (hashfunc) PyObject_HashNotImplemented,  // not hashable
            .tp_str = (reprfunc) __str__,
            .tp_flags = (
                Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC |
                Py_TPFLAGS_IMMUTABLETYPE | Py_TPFLAGS_SEQUENCE
                // add Py_TPFLAGS_MANAGED_WEAKREF for Python 3.12+
            ),
            .tp_doc = "docstring for LinkedList",
            .tp_traverse = (traverseproc) Base::__traverse__,
            .tp_clear = (inquiry) Base::__clear__,
            .tp_richcompare = (richcmpfunc) __richcompare__,
            .tp_iter = (getiterfunc) Base::__iter__,
            .tp_methods = methods,
            .tp_getset = properties,
            .tp_init = (initproc) __init__,
            .tp_new = (newfunc) Base::__new__,
        };
    };

    /* The final Python type. */
    inline static PyTypeObject Type = init_type();

};


}  // namespace linked
}  // namespace structs
}  // namespace bertrand


#endif  // BERTRAND_STRUCTS_LINKED_LIST_H

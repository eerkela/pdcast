// include guard: BERTRAND_STRUCTS_UTIL_ITER_H
#ifndef BERTRAND_STRUCTS_UTIL_ITER_H
#define BERTRAND_STRUCTS_UTIL_ITER_H

#include <iterator>  // std::iterator_traits
#include <type_traits>  // std::enable_if_t, std::is_same_v, std::void_t
#include <Python.h>  // CPython API
#include <utility>  // std::declval, std::move
#include "func.h"  // identity, FuncTraits<>
#include "name.h"  // PyName<>
#include "slot.h"  // Slot<>


/* The `iter()` method represents a two-way bridge between Python and C++ containers
implementing the standard iterator interface.  It can be invoked as follows:

    for (auto item : iter(container)) {
        // do something with item
    }

Where `container` is any C++ or Python container that implements the standard iterator
interface in its respective language.  On the C++ side, this includes all STL
containers, as well as any custom container that exposes some combination of `begin()`,
`end()`, `rbegin()`, `rend()`, etc.  On the Python side, it includes built-in lists,
tuples, sets, strings, dictionaries, and any other object that implements the
`__iter__()` and/or `__reversed__()` magic methods, including custom classes.

When called with a C++ container, the `iter()` method produces a proxy that forwards
the container's original iterator interface (however it is defined).  The proxy uses
these methods to generate equivalent Python iterators with corresponding `__iter__()`
and `__next__()` methods, which can be returned directly to the Python interpreter.
This translation works as long as the C++ iterators dereference to PyObject*, or if a
custom conversion function is provided via the optional `convert` argument.  This
allows users to insert a scalar conversion in between the iterator dereference and the
return of the `__next__()` method on the Python side.  For example, if the C++ iterator
dereferences to a custom struct, the user can provide an inline lambda that translates
the struct into a valid PyObject*, which is returned to Python like normal.  This
conversion can be invoked as follows:

    return iter(container, [](MyStruct& s) { return do_something(s); }).python();

Which returns a Python iterator that yields the result of `do_something(s)` for every
`s` in `container`.

When called with a Python container, the `iter()` method produces an equivalent proxy
that wraps the `PyObject_GetIter()` C API function and exposes a standard C++ iterator
interface on the other side.  Just like the C++ to Python translation, custom
conversion functions can be added in between the result of the `__next__()` method on
the Python side and the iterator dereference on the C++ side:

    for (auto item : iter(container, [](PyObject* obj) { return do_something(obj); })) {
        // item is the result of `do_something(obj)` for every `obj` in `container`
    }

Note that due to the dynamic nature of Python's type system, conversions of this sort
require foreknowledge of the container's specific element type in order to perform the
casts necessary to narrow Python types to their C++ counterparts.  To facilitate this,
each of the data structures exposed in the `bertrand::structs` namespace support
optional Python-side type specialization, which can be used to enforce homogeneity at
the container level.  With this in place, users can safely convert the contents of the
container to a specific C++ type without having to worry about type errors or
unexpected behavior.
*/


namespace bertrand {
namespace structs {
namespace util {


/////////////////////////////////
////    ITERATOR WRAPPERS    ////
/////////////////////////////////


/* NOTE: CoupledIterators are used to share state between the begin() and end()
 * iterators in a loop and generally simplify the overall iterator interface.  They act
 * like pass-through decorators for the begin() iterator, and contain their own end()
 * iterator to terminate the loop.  This means we can write loops as follows:
 *
 * for (auto iter = view.iter(); iter != iter.end(); ++iter) {
 *     // full access to iter
 * }
 * 
 * Rather than the more verbose:
 * 
 * for (auto iter = view.begin(), end = view.end(); iter != end; ++iter) {
 *      // same as above
 * }
 * 
 * Both generate identical code, but the former is more concise and easier to read.  It
 * also allows any arguments provided to the call operator to be passed through to both
 * the begin() and end() iterators, which can be used to share state between the two.
 */


/* A coupled pair of begin() and end() iterators to simplify the iterator interface. */
template <typename IteratorType>
class CoupledIterator {
public:
    using Iterator = IteratorType;

    // iterator tags for std::iterator_traits
    using iterator_category     = typename Iterator::iterator_category;
    using difference_type       = typename Iterator::difference_type;
    using value_type            = typename Iterator::value_type;
    using pointer               = typename Iterator::pointer;
    using reference             = typename Iterator::reference;

    // couple the begin() and end() iterators into a single object
    CoupledIterator(const Iterator& first, const Iterator& second) :
        first(std::move(first)), second(std::move(second))
    {}

    // allow use of the CoupledIterator in a range-based for loop
    Iterator& begin() { return first; }
    Iterator& end() { return second; }

    // pass iterator protocol through to begin()
    inline value_type operator*() const { return *first; }
    inline CoupledIterator& operator++() { ++first; return *this; }
    inline bool operator!=(const Iterator& other) const { return first != other; }

    // conditionally compile all other methods based on Iterator interface.
    // NOTE: this uses SFINAE to detect the presence of these methods on the template
    // Iterator.  If the Iterator does not implement the named method, then it will not
    // be compiled, and users will get compile-time errors if they try to access it.
    // This avoids the need to manually extend the CoupledIterator interface to match
    // that of the Iterator.  See https://en.cppreference.com/w/cpp/language/sfinae
    // for more information.

    template <typename T = Iterator>
    inline auto prev() const -> decltype(std::declval<T>().prev()) {
        return first.prev();
    }

    template <typename T = Iterator>
    inline auto curr() const -> decltype(std::declval<T>().curr()) {
        return first.curr();
    }

    template <typename T = Iterator>
    inline auto next() const -> decltype(std::declval<T>().next()) {
        return first.next();
    }

    template <typename T = Iterator>
    inline auto insert(value_type value) -> decltype(std::declval<T>().insert(value)) {
        return first.insert(value);  // void
    }

    template <typename T = Iterator>
    inline auto drop() -> decltype(std::declval<T>().drop()) {
        return first.drop();
    }

    template <typename T = Iterator>
    inline auto replace(value_type value) -> decltype(std::declval<T>().replace(value)) {
        return first.replace(value);
    }

    template <typename T = Iterator>
    inline auto index() -> decltype(std::declval<T>().index()) const {
        return first.index();
    }

    template <typename T = Iterator>
    inline auto idx() -> decltype(std::declval<T>().idx()) const {
        return first.idx();
    }

protected:
    Iterator first, second;
};


/* NOTE: ConvertedIterators can be used to apply a custom conversion function to the
 * result of a standard C++ iterator's dereference operator.  This is useful for
 * applying conversions during iteration, which may be necessary when translating
 * between C++ and Python types, for example.
 *
 * ConvertedIterators use SFINAE and compile-time reflection to detect the presence of
 * the standard iterator interface, and to adjust the return type of the relevant
 * dereference operator(s).  This is inferred automatically at compile-time directly
 * from the provided conversion function, allowing for a unified interface across all
 * types of iterators.
 *
 * Note that any additional (non-operator) methods that are exposed by the underlying
 * iterator are not forwarded to the ConvertedIterator wrapper due to limitations with
 * dynamic forwarding in C++.  The ConvertedIterator does, however, expose the wrapped
 * iterator as a public attribute, which can be used to access these methods directly
 * if needed.
 */


/* A decorator for a standard C++ iterator that applies a custom conversion at
each step. */
template <typename Iterator, typename Func>
class ConvertedIterator {
    Func convert;

    /* Ensure that Func is callable with a single argument of the iterator's
    dereferenced value type and infer the corresponding return type. */
    using ConvTraits = FuncTraits<Func, decltype(*std::declval<Iterator>())>;
    using ReturnType = typename ConvTraits::ReturnType;

    /* Get iterator_traits from wrapped iterator. */
    using IterTraits = std::iterator_traits<Iterator>;

    /* Force SFINAE evaluation of the templated type. */
    template <typename T>
    static constexpr bool exists = std::is_same_v<T, T>;

    /* Detect whether the templated type supports the -> operator. */
    template <typename T, typename = void>  // default
    struct arrow_operator {
        using type = void;
        static constexpr bool value = false;
    };
    template <typename T>  // specialization for smart pointers
    struct arrow_operator<T, std::void_t<decltype(std::declval<T>().operator->())>> {
        using type = decltype(std::declval<T>().operator->());
        static constexpr bool value = true;
    };
    template <typename T>  // specialization for raw pointers
    struct arrow_operator<T*> {
        using type = T*;
        static constexpr bool value = true;
    };

public:
    Iterator wrapped;

    /* Forwards for std::iterator_traits. */
    using iterator_category = std::enable_if_t<
        exists<typename IterTraits::iterator_category>,
        typename IterTraits::iterator_category
    >;
    using pointer = std::enable_if_t<
        exists<typename IterTraits::pointer>,
        typename IterTraits::pointer
    >;
    using reference = std::enable_if_t<
        exists<typename IterTraits::reference>,
        typename IterTraits::reference
    >;
    using value_type = std::enable_if_t<
        exists<typename IterTraits::value_type>,
        typename IterTraits::value_type
    >;
    using difference_type = std::enable_if_t<
        exists<typename IterTraits::difference_type>,
        typename IterTraits::difference_type
    >;

    /* Construct a converted iterator from a standard C++ iterator and a conversion
    function. */
    inline ConvertedIterator(Iterator& i, Func f) : convert(f), wrapped(i) {}
    inline ConvertedIterator(Iterator&& i, Func f) : convert(f), wrapped(std::move(i)) {}
    inline ConvertedIterator(const ConvertedIterator& other) :
        convert(other.convert), wrapped(other.wrapped)
    {}
    inline ConvertedIterator(ConvertedIterator&& other) :
        convert(std::move(other.convert)), wrapped(std::move(other.wrapped))
    {}
    inline ConvertedIterator& operator=(const ConvertedIterator& other) {
        convert = other.convert;
        wrapped = other.wrapped;
        return *this;
    }
    inline ConvertedIterator& operator=(ConvertedIterator&& other) {
        convert = std::move(other.convert);
        wrapped = std::move(other.wrapped);
        return *this;
    }

    /* Dereference the iterator and apply the conversion function. */
    inline ReturnType operator*() const {
        return convert(*wrapped);
    }
    template <typename T>
    inline ReturnType operator[](T&& index) const {
        return convert(wrapped[index]);
    }
    template <
        bool cond = arrow_operator<ReturnType>::value,
        std::enable_if_t<cond, int> = 0
    >
    inline auto operator->() const -> typename arrow_operator<ReturnType>::type {
        return this->operator*().operator->();
    }

    /* Forward all other methods to the wrapped iterator. */
    inline ConvertedIterator& operator++() {
        ++wrapped;
        return *this;
    }
    inline ConvertedIterator operator++(int) {
        ConvertedIterator temp(*this);
        ++wrapped;
        return temp;
    }
    inline ConvertedIterator& operator--() {
        --wrapped;
        return *this;
    }
    inline ConvertedIterator operator--(int) {
        ConvertedIterator temp(*this);
        --wrapped;
        return temp;
    }
    inline bool operator==(const ConvertedIterator& other) const {
        return wrapped == other.wrapped;
    }
    inline bool operator!=(const ConvertedIterator& other) const {
        return wrapped != other.wrapped;
    }
    template <typename T>
    inline ConvertedIterator& operator+=(T&& other) {
        wrapped += other;
        return *this;
    }
    template <typename T>
    inline ConvertedIterator& operator-=(T&& other) {
        wrapped -= other;
        return *this;
    }
    inline bool operator<(const ConvertedIterator& other) const {
        return wrapped < other.wrapped;
    }
    inline bool operator>(const ConvertedIterator& other) const {
        return wrapped > other.wrapped;
    }
    inline bool operator<=(const ConvertedIterator& other) const {
        return wrapped <= other.wrapped;
    }
    inline bool operator>=(const ConvertedIterator& other) const {
        return wrapped >= other.wrapped;
    }

    /* operator+ implemented as a non-member function for commutativity. */
    template <typename T, typename _Iterator, typename _Func>
    friend ConvertedIterator<_Iterator, _Func> operator+(
        const ConvertedIterator<_Iterator, _Func>& iter,
        T n
    );
    template <typename T, typename _Iterator, typename _Func>
    friend ConvertedIterator<_Iterator, _Func> operator+(
        T n,
        const ConvertedIterator<_Iterator, _Func>& iter
    );

    /* operator- implemented as a non-member function for commutativity. */
    template <typename T, typename _Iterator, typename _Func>
    friend ConvertedIterator<_Iterator, _Func> operator-(
        const ConvertedIterator<_Iterator, _Func>& iter,
        T n
    );
    template <typename T, typename _Iterator, typename _Func>
    friend ConvertedIterator<_Iterator, _Func> operator-(
        T n,
        const ConvertedIterator<_Iterator, _Func>& iter
    );

};


/* Non-member operator+ overload to allow for commutativity. */
template <typename T, typename Iterator, typename Func>
ConvertedIterator<Iterator, Func> operator+(
    const ConvertedIterator<Iterator, Func>& iter,
    T n
) {
    return ConvertedIterator<Iterator, Func>(iter.wrapped + n, iter.convert);
}


/* Non-member operator+ overload to allow for commutativity. */
template <typename T, typename Iterator, typename Func>
ConvertedIterator<Iterator, Func> operator+(
    T n,
    const ConvertedIterator<Iterator, Func>& iter
) {
    return ConvertedIterator<Iterator, Func>(n + iter.wrapped, iter.convert);
}


/* Non-member operator- overload to allow for commutativity. */
template <typename T, typename Iterator, typename Func>
ConvertedIterator<Iterator, Func> operator-(
    const ConvertedIterator<Iterator, Func>& iter,
    T n
) {
    return ConvertedIterator<Iterator, Func>(iter.wrapped - n, iter.convert);
}


/* Non-member operator- overload to allow for commutativity. */
template <typename T, typename Iterator, typename Func>
ConvertedIterator<Iterator, Func> operator-(
    T n,
    const ConvertedIterator<Iterator, Func>& iter
) {
    return ConvertedIterator<Iterator, Func>(n - iter.wrapped, iter.convert);
}


/* NOTE: PyIterators are wrappers around standard C++ iterators that allow them to be
 * used from Python.  They are implemented using a C-style PyTypeObject definition to
 * expose the __iter__() and __next__() magic methods, which are used to implement the
 * iterator protocol in Python.  These Python methods simply delegate to the minimal
 * C++ forward iterator interface, which must include:
 *
 *      1. operator*() to dereference the iterator
 *      2. operator++() to preincrement the iterator
 *      3. operator!=() to terminate the sequence
 *
 * The only other requirement is that the iterator must dereference to PyObject*, or be
 * converted to PyObject* via a custom conversion function.  This ensures that the
 * items yielded by the iterator are compatible with the Python C API, and can be
 * passed to other Python functions without issue.  Failure to handle these will result
 * in compile-time errors.
 *
 * NOTE: PyIterators, just like other bertrand-enabled Python wrappers around C++
 * objects (e.g. PyLock, etc.), use compile-time type information (CTTI) to build their
 * respective PyTypeObject definitions, which are guaranteed to be unique for each of
 * the wrapped iterator types.  This allows the wrapper to be applied generically to
 * any C++ type without any additional configuration from the user.  The only potential
 * complication is in deriving an appropriate dotted name for the Python type, which
 * normally requires the use of compiler-specific macros.
 *
 * A robust solution to this problem is provided by the PyName<> class, which can
 * generate a mangled, Python-compatible name for any C++ type, taking into account
 * namespaces, templates, and other common C++ constructs.  This approach should work
 * for all major compilers (including GCC, Clang, and MSVC-based solutions), but should
 * it fail, a custom name can be provided by specializing the PyName<> template for the
 * desired type.  See the PyName<> documentation for more information.
 */


/* A wrapper around a C++ iterator that allows it to be used from Python. */
template <typename Iterator>
class PyIterator {
    // sanity check
    static_assert(
        std::is_convertible_v<typename Iterator::value_type, PyObject*>,
        "Iterator must dereference to PyObject*"
    );

    /* Store coupled iterators as raw data buffers.
    
    NOTE: PyObject_New() does not allow for traditional stack allocation like we would
    normally use to store the wrapped iterators.  Instead, we have to delay construction
    until the init() method is called.  We could use pointers to heap-allocate memory
    for this, but this adds extra allocation overhead.  Using raw data buffers avoids
    this and places the iterators on the stack, where they belong. */
    PyObject_HEAD
    Slot<Iterator> first;
    Slot<Iterator> second;

    /* Force users to use init() factory method. */
    PyIterator() = delete;
    PyIterator(const PyIterator&) = delete;
    PyIterator(PyIterator&&) = delete;

public:

    /* Construct a Python iterator from a C++ iterator range. */
    inline static PyObject* init(Iterator&& begin, Iterator&& end) {
        // create new iterator instance
        PyIterator* result = PyObject_New(PyIterator, &Type);
        if (result == nullptr) {
            throw std::runtime_error("could not allocate Python iterator");
        }

        // initialize (NOTE: PyObject_New() does not call stack constructors)
        new (&(result->first)) Slot<Iterator>();
        new (&(result->second)) Slot<Iterator>();

        // construct iterators within raw storage
        result->first.construct(std::move(begin));
        result->second.construct(std::move(end));

        // return as PyObject*
        return reinterpret_cast<PyObject*>(result);
    }

    /* Construct a Python iterator from a coupled iterator. */
    inline static PyObject* init(CoupledIterator<Iterator>&& iter) {
        return init(iter.begin(), iter.end());
    }

    /* Call next(iter) from Python. */
    inline static PyObject* iter_next(PyIterator* self) {
        Iterator& begin = *(self->first);
        Iterator& end = *(self->second);

        if (!(begin != end)) {  // terminate the sequence
            PyErr_SetNone(PyExc_StopIteration);
            return nullptr;
        }

        // increment iterator and return current value
        PyObject* result = *begin;
        ++begin;
        return Py_NewRef(result);  // new reference
    }

    /* Free the Python iterator when its reference count falls to zero. */
    inline static void dealloc(PyIterator* self) {
        Type.tp_free(self);
    }

private:
    /* Initialize a PyTypeObject to represent this iterator from Python. */
    static PyTypeObject init_type() {
        PyTypeObject type_obj;  // zero-initialize
        type_obj.tp_name = PyName<Iterator>.data();
        type_obj.tp_doc = "Python-compatible wrapper around a C++ iterator.";
        type_obj.tp_basicsize = sizeof(PyIterator);
        type_obj.tp_flags = (
            Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE |
            Py_TPFLAGS_DISALLOW_INSTANTIATION
        );
        type_obj.tp_alloc = PyType_GenericAlloc;
        type_obj.tp_iter = PyObject_SelfIter;
        type_obj.tp_iternext = (iternextfunc) iter_next;
        type_obj.tp_dealloc = (destructor) dealloc;

        // register iterator type with Python
        if (PyType_Ready(&type_obj) < 0) {
            throw std::runtime_error("could not initialize PyIterator type");
        }
        return type_obj;
    }

    /* C-style Python type declaration. */
    inline static PyTypeObject Type = init_type();

};


////////////////////////////
////    C++ BINDINGS    ////
////////////////////////////


/* C++ bindings consist of a battery of compile-time SFINAE checks to detect the
 * presence and return types of the standard C++ iterator interface, including the
 * following methods:
 *      begin()
 *      cbegin()
 *      end()
 *      cend()
 *      rbegin()
 *      crbegin()
 *      rend()
 *      crend()
 *
 * Which can be defined as either member methods, non-member ADL methods, or via the
 * equivalent standard library functions (in order of preference).  The first one found
 * is forwarded as the proxy's own begin(), cbegin(), end(), etc. member methods,
 * which standardizes the interface for all types of iterable containers.
 *
 * Python iterators are then constructed by coupling various pairs of `begin()` and
 * `end()` iterators and packaging them into an appropriate PyIterator wrapper, as
 * returned by the following proxy methods:
 *      python()            // (begin() + end())
 *      cpython()           // (cbegin() + cend())
 *      rpython()           // (rbegin() + rend())
 *      crpython()          // (crbegin() + crend())
 */


/* A collection of SFINAE traits introspecting a container's iterator interface. */
template <typename Container, typename Func = identity>
class ContainerTraits {
    static constexpr bool is_identity = std::is_same_v<Func, identity>;

    /* Create a wrapper around an iterator that applies a conversion function to the
    result of its dereference operator. */
    template <typename Iter, bool cond = false>
    struct _conversion_wrapper {
        using type = ConvertedIterator<Iter, Func>;
        inline static type decorate(Iter&& iter, Func func) {
            return type(std::move(iter), func);
        }
    };

    /* If no conversion function is given, return the iterator unmodified. */
    template <typename Iter>
    struct _conversion_wrapper<Iter, true> {
        using type = Iter;
        inline static type decorate(Iter&& iter, Func func) {
            return iter;
        }
    };

    template <typename Iter>
    using conversion_wrapper = _conversion_wrapper<Iter, is_identity>;

    /* NOTE: using a preprocessor macro avoids a lot of boilerplate when it comes to
    instantiating correct SFINAE iterator traits, but can be a bit intimidating to
    read.  The basic idea is as follows:
    
    For each iterator method - begin(), end(), rbegin(), etc. - we check for 3 possible
    configurations to be as generic as possible:

        1.  A member method of the same name within the Iterable type itself.
                e.g. iterable.begin()
        2.  A non-member ADL method within the same namespace as the Iterable type.
                e.g. begin(iterable)
        3.  An equivalently-named standard library method.
                e.g. std::begin(iterable)

    These are checked in order at compile time, and the first one found is passed
    through to the proxy's `.begin()` member, which represents a unified interface for
    all types of iterable containers.  If none of these methods exist, the proxy's
    `.begin()` member is not defined, and any attempt to use it will result in a
    compile error.

    If a conversion function is supplied to the proxy, then the result of the
    `.begin()` method is wrapped in a ConvertedIterator<>, which applies the conversion
    function at the point of dereference.  For this to work, the underlying iterator
    must be copy/move constructible, and must implement any combination of the standard
    operator overloads (e.g. `*`, `[]`, `->`, `++`, `--`, `==`, `!=`, etc.).
    Additionally, the supplied conversion function must be invocable with the result of
    the iterator's original dereference operator.  If any of these conditions are not
    met, it will result in a compile error.

    Lastly, the conversion function's return type (evaluated at compile time) will be
    used to set the `value_type` of the converted iterator, and if it is convertible to
    PyObject*, then Python-compatible iterators can be constructed from it using the
    proxy's `python()`, `cpython()`, `rpython()`, and `crpython()` methods.  If the
    result of the conversion is not Python-compatible, then these methods will not be
    defined, and any attempt to use them will result in a compile error.

    See https://en.cppreference.com/w/cpp/language/adl for more information on ADL and
    non-member functions, and https://en.cppreference.com/w/cpp/language/sfinae for
    a reference on SFINAE substitution and compile-time metaprogramming. */
    #define TRAIT_FLAG(FLAG_NAME, STATEMENT) \
        /* Flags gate the SFINAE detection, ensuring that we stop at the first match */ \
        template <typename Iterable, typename = void> \
        struct FLAG_NAME : std::false_type {}; \
        template <typename Iterable> \
        struct FLAG_NAME<Iterable, std::void_t<decltype(STATEMENT)>> : std::true_type {}; \

    #define ITER_TRAIT(METHOD) \
        /* Default specialization for methods that don't exist on the Iterable type. */ \
        template < \
            typename Iterable, \
            typename MemberEnable = void, \
            typename ADLEnable = void, \
            typename STDEnable = void \
        > \
        struct _##METHOD { \
            static constexpr bool exists = false; \
            using type = void; \
        }; \
        /* First, check for a member method of the same name within Iterable. */ \
        template <typename Iterable> \
        struct _##METHOD< \
            Iterable, \
            std::void_t<decltype(std::declval<Iterable&>().METHOD())> \
        > { \
            static constexpr bool exists = true; \
            using base_type = decltype(std::declval<Iterable&>().METHOD()); \
            using wrapper = conversion_wrapper<base_type>; \
            using type = typename wrapper::type; \
            static inline type call(Iterable& iterable, Func func) { \
                return wrapper::decorate(iterable.METHOD(), func); \
            } \
        }; \
        TRAIT_FLAG(has_member_##METHOD, std::declval<Iterable&>().METHOD()) \
        /* Second, check for a non-member ADL method within the same namespace. */ \
        template <typename Iterable> \
        struct _##METHOD< \
            Iterable, \
            void, \
            std::enable_if_t< \
                !has_member_##METHOD<Iterable>::value, \
                std::void_t<decltype(METHOD(std::declval<Iterable&>()))> \
            > \
        > { \
            static constexpr bool exists = true; \
            using base_type = decltype(METHOD(std::declval<Iterable&>())); \
            using wrapper = conversion_wrapper<base_type>; \
            using type = typename wrapper::type; \
            static inline type call(Iterable& iterable, Func func) { \
                return wrapper::decorate(METHOD(iterable), func); \
            } \
        }; \
        TRAIT_FLAG(has_adl_##METHOD, METHOD(std::declval<Iterable&>())) \
        /* Third, check for an equivalently-named standard library method. */ \
        template <typename Iterable> \
        struct _##METHOD< \
            Iterable, \
            void, \
            void, \
            std::enable_if_t< \
                !has_member_##METHOD<Iterable>::value && \
                !has_adl_##METHOD<Iterable>::value, \
                std::void_t<decltype(std::METHOD(std::declval<Iterable&>()))> \
            > \
        > { \
            static constexpr bool exists = true; \
            using base_type = decltype(std::METHOD(std::declval<Iterable&>())); \
            using wrapper = conversion_wrapper<base_type>; \
            using type = typename wrapper::type; \
            static inline type call(Iterable& iterable, Func func) { \
                return wrapper::decorate(std::METHOD(iterable), func); \
            } \
        }; \

    /* Detect presence of iterator interface on underlying container */
    ITER_TRAIT(begin)
    ITER_TRAIT(cbegin)
    ITER_TRAIT(end)
    ITER_TRAIT(cend)
    ITER_TRAIT(rbegin)
    ITER_TRAIT(crbegin)
    ITER_TRAIT(rend)
    ITER_TRAIT(crend)

    #undef ITER_TRAIT
    #undef TRAIT_FLAG

    /* NOTE: With some care, we can synthesize some of the methods that don't exist on
     * the underlying container.  For example, if the container does not implement
     * `cbegin()`, but does implement a const overload for `begin()`, then we can
     * synthesize a corresponding `cbegin()` method by simply delegating to the
     * const overload.  This is done by simply redirecting `cbegin()` calls on the
     * proxy to the corresponding `begin()` method on a const container.
     */

    /* A collection of SFINAE traits that delegates proxy calls to the appropriate
    iterator implementation. */
    template <typename _Iterable>
    struct _Traits {
        using Begin = std::conditional_t<
            _begin<_Iterable>::exists,
            _begin<_Iterable>,
            _cbegin<_Iterable>
        >;
        using CBegin = std::conditional_t<
            _cbegin<_Iterable>::exists,
            _cbegin<_Iterable>,
            _begin<const _Iterable>
        >;
        using End = std::conditional_t<
            _end<_Iterable>::exists,
            _end<_Iterable>,
            _cend<_Iterable>
        >;
        using CEnd = std::conditional_t<
            _cend<_Iterable>::exists,
            _cend<_Iterable>,
            _end<const _Iterable>
        >;
        using RBegin = std::conditional_t<
            _rbegin<_Iterable>::exists,
            _rbegin<_Iterable>,
            _crbegin<_Iterable>
        >;
        using CRBegin = std::conditional_t<
            _crbegin<_Iterable>::exists,
            _crbegin<_Iterable>,
            _rbegin<const _Iterable>
        >;
        using REnd = std::conditional_t<
            _rend<_Iterable>::exists,
            _rend<_Iterable>,
            _crend<_Iterable>
        >;
        using CREnd = std::conditional_t<
            _crend<_Iterable>::exists,
            _crend<_Iterable>,
            _rend<const _Iterable>
        >;
    };

    /* A collection of SFINAE traits that delegates proxy calls to the appropriate
    iterator implementation. */
    template <typename _Iterable>
    struct _Traits<const _Iterable> {
        using Begin = std::conditional_t<
            _begin<const _Iterable>::exists,
            _begin<const _Iterable>,
            _cbegin<const _Iterable>
        >;
        using CBegin = std::conditional_t<
            _cbegin<const _Iterable>::exists,
            _cbegin<const _Iterable>,
            _begin<const _Iterable>
        >;
        using End = std::conditional_t<
            _end<const _Iterable>::exists,
            _end<const _Iterable>,
            _cend<const _Iterable>
        >;
        using CEnd = std::conditional_t<
            _cend<const _Iterable>::exists,
            _cend<const _Iterable>,
            _end<const _Iterable>
        >;
        using RBegin = std::conditional_t<
            _rbegin<const _Iterable>::exists,
            _rbegin<const _Iterable>,
            _crbegin<const _Iterable>
        >;
        using CRBegin = std::conditional_t<
            _crbegin<const _Iterable>::exists,
            _crbegin<const _Iterable>,
            _rbegin<const _Iterable>
        >;
        using REnd = std::conditional_t<
            _rend<const _Iterable>::exists,
            _rend<const _Iterable>,
            _crend<const _Iterable>
        >;
        using CREnd = std::conditional_t<
            _crend<const _Iterable>::exists,
            _crend<const _Iterable>,
            _rend<const _Iterable>
        >;
    };

public:
    using Begin = typename _Traits<Container>::Begin;
    using CBegin = typename _Traits<Container>::CBegin;
    using End = typename _Traits<Container>::End;
    using CEnd = typename _Traits<Container>::CEnd;
    using RBegin = typename _Traits<Container>::RBegin;
    using CRBegin = typename _Traits<Container>::CRBegin;
    using REnd = typename _Traits<Container>::REnd;
    using CREnd = typename _Traits<Container>::CREnd;
};


/* A proxy for a C++ container that allows iteration from both C++ and Python. */
template <typename Container, typename Func, bool rvalue>
class IterProxy {
    // NOTE: if rvalue is true, then we own the container.  Otherwise, we only keep
    // a reference to it.
    std::conditional_t<rvalue, Container, Container&> container;
    Func convert;

public:
    using Traits = ContainerTraits<Container>;

    /////////////////////////////
    ////    C++ INTERFACE    ////
    /////////////////////////////

    /* The proxy uses SFINAE to expose only those methods that exist on the underlying
     * container.  The others are not compiled, and any attempt to use them will result
     * in a compile error.
     */

    /* Delegate to the container's begin() method, if it exists. */
    template <bool cond = Traits::Begin::exists>
    inline auto begin() -> std::enable_if_t<cond, typename Traits::Begin::type> {
        return Traits::Begin::call(this->container, this->convert);
    }

    /* Delegate to the container's cbegin() method, if it exists. */
    template <bool cond = Traits::CBegin::exists>
    inline auto cbegin() -> std::enable_if_t<cond, typename Traits::CBegin::type> {
        return Traits::CBegin::call(this->container, this->convert);
    }

    /* Delegate to the container's end() method, if it exists. */
    template <bool cond = Traits::End::exists>
    inline auto end() -> std::enable_if_t<cond, typename Traits::End::type> {
        return Traits::End::call(this->container, this->convert);
    }

    /* Delegate to the container's cend() method, if it exists. */
    template <bool cond = Traits::CEnd::exists>
    inline auto cend() -> std::enable_if_t<cond, typename Traits::CEnd::type> {
        return Traits::CEnd::call(this->container, this->convert);
    }

    /* Delegate to the container's rbegin() method, if it exists. */
    template <bool cond = Traits::RBegin::exists>
    inline auto rbegin() -> std::enable_if_t<cond, typename Traits::RBegin::type> {
        return Traits::RBegin::call(this->container, this->convert);
    }

    /* Delegate to the container's crbegin() method, if it exists. */
    template <bool cond = Traits::CRBegin::exists>
    inline auto crbegin() -> std::enable_if_t<cond, typename Traits::CRBegin::type> {
        return Traits::CRBegin::call(this->container, this->convert);
    }

    /* Delegate to the container's rend() method, if it exists. */
    template <bool cond = Traits::REnd::exists>
    inline auto rend() -> std::enable_if_t<cond, typename Traits::REnd::type> {
        return Traits::REnd::call(this->container, this->convert);
    }

    /* Delegate to the container's crend() method, if it exists. */
    template <bool cond = Traits::CREnd::exists>
    inline auto crend() -> std::enable_if_t<cond, typename Traits::CREnd::type> {
        return Traits::CREnd::call(this->container, this->convert);
    }

    /////////////////////////////////
    ////    COUPLED ITERATORS    ////
    /////////////////////////////////

    /* The typical C++ syntax for iterating over a container is a bit clunky at times,
     * especially when it comes to reverse iteration.  Normally, this requires separate
     * calls to `rbegin()` and `rend()`, which are then passed to a manual for loop
     * construction.  This is not very ergonomic, and can be a bit confusing at times.
     * Coupled iterators solve that.
     *
     * A coupled iterator represents a pair of `begin()` and `end()` iterators that are
     * bound into a single object.  This allows for the following syntax:
     *
     *      for (auto& item : iter(container).iter()) {
     *          // forward iteration
     *      }
     *      for (auto& item : iter(container).citer()) {
     *          // forward iteration over const container
     *      }
     *      for (auto& item : iter(container).reverse()) {
     *          // reverse iteration
     *      }
     *      for (auto& item : iter(container).creverse()) {
     *          // reverse iteration over const container
     *      }
     *
     * Which is considerably more readable than the equivalent:
     *
     *      for (auto it = container.rbegin(), end = container.rend(); it != end; ++it) {
     *          // reverse iteration
     *      }
     *
     * NOTE: the `iter()` method is not strictly necessary since the proxy itself
     * implements the standard iterator interface.  As a result, the following syntax
     * is identical in most cases:
     *
     *      for (auto& item : iter(container)) {
     *          // forward iteration
     *      }
     *
     * Lastly, coupled iterators can also be used in manual loop constructions if
     * access to the underlying iterator is required:
     *
     *      for (auto it = iter(container).reverse(); it != it.end(); ++it) {
     *          // reverse iteration
     *      }
     *
     * The `it` variable can then be used just like an ordinary `rbegin()` iterator.
     */

    /* Create a coupled iterator over the container using the begin()/end() methods. */
    template <bool cond = Traits::Begin::exists && Traits::End::exists>
    inline auto forward() -> std::enable_if_t<
        cond, CoupledIterator<typename Traits::Begin::type>
    > {
        return CoupledIterator<typename Traits::Begin::type>(begin(), end());
    }

    /* Create a coupled iterator over the container using the cbegin()/cend() methods. */
    template <bool cond = Traits::CBegin::exists && Traits::CEnd::exists>
    inline auto cforward() -> std::enable_if_t<
        cond, CoupledIterator<typename Traits::CBegin::type>
    > {
        return CoupledIterator<typename Traits::CBegin::type>(cbegin(), cend());
    }

    /* Create a coupled iterator over the container using the rbegin()/rend() methods. */
    template <bool cond = Traits::RBegin::exists && Traits::REnd::exists>
    inline auto reverse() -> std::enable_if_t<
        cond, CoupledIterator<typename Traits::RBegin::type>
    > {
        return CoupledIterator<typename Traits::RBegin::type>(rbegin(), rend());
    }

    /* Create a coupled iterator over the container using the crbegin()/crend() methods. */
    template <bool cond = Traits::CRBegin::exists && Traits::CREnd::exists>
    inline auto creverse() -> std::enable_if_t<
        cond, CoupledIterator<typename Traits::CRBegin::type>
    > {
        return CoupledIterator<typename Traits::CRBegin::type>(crbegin(), crend());
    }

    ////////////////////////////////
    ////    PYTHON INTERFACE    ////
    ////////////////////////////////

    /* If the container's iterators dereference to PyObject* (or can be converted to it
     * using an inline conversion function), then the proxy can produce Python iterators
     * straight from C++.  This allows C++ objects to be iterated over directly from
     * Python using standard `for .. in ..` syntax.
     *
     * Doing so typically requires Cython, since the `iter()` function is only exposed
     * at the C++ level.  The cython/iter.pxd header contains the necessary Cython
     * declarations to do this, and can be included in any Cython module that needs to
     * iterate over C++ containers.
     *
     * This functionality is also baked into the Python-side equivalents of the data
     * structures exposed in the `bertrand::structs` namespace.  For example, here's
     * the implementation of the `__iter__()` method for the `LinkedList` class:
     *
     *      def __iter__(self):
     *          return <object>(iter(self.variant).python())
     *
     * This would ordinarily be an extremely delicate operation with lots of potential
     * for inefficiency and error, but the proxy's unified interface handles all of the
     * heavy lifting for us and yields a valid Python iterator with minimal overhead.
     */

    /* Create a forward Python iterator over the container using the begin()/end()
    methods. */
    template <bool cond = Traits::Begin::exists && Traits::End::exists>
    inline auto python() -> std::enable_if_t<cond, PyObject*> {
        using Iter = PyIterator<typename Traits::Begin::type>;
        return Iter::init(begin(), end());
    }

    /* Create a forward Python iterator over the container using the cbegin()/cend()
    methods. */
    template <bool cond = Traits::CBegin::exists && Traits::CEnd::exists>
    inline auto cpython() -> std::enable_if_t<cond, PyObject*> {
        using Iter = PyIterator<typename Traits::CBegin::type>;
        return Iter::init(cbegin(), cend());
    }

    /* Create a backward Python iterator over the container using the rbegin()/rend()
    methods. */
    template <bool cond = Traits::RBegin::exists && Traits::REnd::exists>
    inline auto rpython() -> std::enable_if_t<cond, PyObject*> {
        using Iter = PyIterator<typename Traits::RBegin::type>;
        return Iter::init(rbegin(), rend());
    }

    /* Create a backward Python iterator over the container using the crbegin()/crend()
    methods. */
    template <bool cond = Traits::CRBegin::exists && Traits::CREnd::exists>
    inline auto crpython() -> std::enable_if_t<cond, PyObject*> {
        using Iter = PyIterator<typename Traits::CRBegin::type>;
        return Iter::init(crbegin(), crend());
    }

private:
    /* IterProxies can only be constructed through `iter()` factory function. */
    template <typename _Container>
    friend IterProxy<_Container, identity, false> iter(_Container& container);
    template <typename _Container>
    friend IterProxy<const _Container, identity, false> iter(const _Container& container);
    template <typename _Container, typename _Func>
    friend IterProxy<_Container, _Func, false> iter(_Container& container, _Func convert);
    template <typename _Container, typename _Func>
    friend IterProxy<const _Container, _Func, false> iter(
        const _Container& container,
        _Func convert
    );
    template <typename _Container>
    friend IterProxy<_Container, identity, true> iter(_Container&& container);
    template <typename _Container>
    friend IterProxy<const _Container, identity, true> iter(const _Container&& container);
    template <typename _Container, typename _Func>
    friend IterProxy<_Container, _Func, true> iter(_Container&& container, _Func convert);
    template <typename _Container, typename _Func>
    friend IterProxy<const _Container, _Func, true> iter(
        const _Container&& container,
        _Func convert
    );

    /* Construct an iterator proxy around a an lvalue container. */
    template <bool cond = !rvalue, std::enable_if_t<cond, int> = 0>
    IterProxy(Container& c) : container(c), convert(Func{}) {}
    template <bool cond = !rvalue, std::enable_if_t<cond, int> = 0>
    IterProxy(Container& c, Func f) : container(c), convert(f) {}

    /* Construct an iterator proxy around an rvalue container. */
    template <bool cond = rvalue, std::enable_if_t<cond, int> = 0>
    IterProxy(Container&& c) : container(std::move(c)), convert(Func{}) {}
    template <bool cond = rvalue, std::enable_if_t<cond, int> = 0>
    IterProxy(Container&& c, Func f) : container(std::move(c)), convert(f) {}
};


///////////////////////////////
////    PYTHON BINDINGS    ////
///////////////////////////////


/* Python bindings involve retrieving a forward or backward Python iterator directly
 * from the CPython API and exposing it to C++ using a standard iterator interface with
 * RAII semantics.  This abstracts away the CPython API (and the associated reference
 * counting/error handling) and allows for standard C++ loop constructs to be used
 * directly on Python containers using the same syntax as C++ containers.
 */


/* A wrapper around a Python iterator that manages reference counts and enables
for-each loop syntax in C++. */
template <typename Func, bool is_const>
class PyIterProxy {
    using Container = std::conditional_t<is_const, const PyObject, PyObject>;
    Container* const container;  // ptr cannot be reassigned
    Func convert;
    static constexpr bool is_identity = std::is_same_v<Func, identity>;

public:

    ///////////////////////
    ////    WRAPPER    ////
    ///////////////////////

    /* A C++ wrapper around a Python iterator that exposes a standard interface. */
    class Iterator {
        Func convert;
        PyObject* py_iterator;
        PyObject* curr;

        /* Ensure that Func is callable with a single argument of the iterator's
        dereferenced value type and infer the corresponding return type. */
        using ConvTraits = FuncTraits<Func, PyObject*>;
        using ReturnType = typename ConvTraits::ReturnType;

    public:
        // iterator tags for std::iterator_traits
        using iterator_category     = std::forward_iterator_tag;
        using difference_type       = std::ptrdiff_t;
        using value_type            = std::remove_reference_t<ReturnType>;
        using pointer               = value_type*;
        using reference             = value_type&;

        /* Get current item. */
        value_type operator*() const {
            if constexpr (is_identity) {
                return curr;  // no conversion necessary
            } else {
                return convert(curr);
            }
        }

        /* Advance to next item. */
        Iterator& operator++() {
            Py_DECREF(curr);
            curr = PyIter_Next(py_iterator);
            if (curr == nullptr && PyErr_Occurred()) {
                throw std::runtime_error("could not get next(iterator)");
            }
            return *this;
        }

        /* Terminate sequence. */
        bool operator!=(const Iterator& other) const { return curr != other.curr; }

        /* Copy constructor. */
        Iterator(const Iterator& other) :
            convert(other.convert), py_iterator(other.py_iterator), curr(other.curr)
        {
            Py_XINCREF(py_iterator);
            Py_XINCREF(curr);
        }

        /* Move constructor. */
        Iterator(Iterator&& other) :
            convert(std::move(other.convert)), py_iterator(other.py_iterator),
            curr(other.curr)
        {
            other.py_iterator = nullptr;
            other.curr = nullptr;
        }

        /* Copy assignment. */
        Iterator& operator=(const Iterator& other) {
            Py_XINCREF(py_iterator);
            Py_XINCREF(curr);
            convert = other.convert;
            py_iterator = other.py_iterator;
            curr = other.curr;
            return *this;
        }

        /* Handle reference counts if an iterator is destroyed partway through
        iteration. */
        ~Iterator() {
            Py_XDECREF(py_iterator);
            Py_XDECREF(curr);
        }

    private:
        friend PyIterProxy;

        /* Return an iterator to the start of the sequence. */
        Iterator(PyObject* i, Func f) : convert(f), py_iterator(i), curr(nullptr) {
            // NOTE: py_iterator is a borrowed reference from PyObject_GetIter()
            if (py_iterator != nullptr) {
                curr = PyIter_Next(py_iterator);  // get first item
                if (curr == nullptr && PyErr_Occurred()) {
                    Py_DECREF(py_iterator);
                    throw catch_python<std::runtime_error>();
                }
            }
        }

        /* Return an iterator to the end of the sequence. */
        Iterator(Func f) : convert(f), py_iterator(nullptr), curr(nullptr) {}
    };

    /////////////////////////////
    ////    C++ INTERFACE    ////
    /////////////////////////////

    /* Get a forward iterator over a mutable container. */
    inline Iterator begin() const {
        return Iterator(this->python(), this->convert);
    }

    /* Get a forward iterator to terminate the loop. */
    inline Iterator end() const {
        return Iterator(this->convert);
    }

    /* Get a forward const iterator over an immutable container. */
    inline Iterator cbegin() const {
        return begin();
    }

    /* Get a forward const iterator to terminate the loop. */
    inline Iterator cend() const {
        return end();
    }

    /* Get a reverse iterator over a mutable container. */
    inline Iterator rbegin() const {
        return Iterator(this->rpython(), this->convert);
    }

    /* Get a reverse iterator to terminate the loop. */
    inline Iterator rend() const {
        return Iterator(this->convert);
    }

    /* Get a reverse const iterator over an immutable container. */
    inline Iterator crbegin() const {
        return rbegin();
    }

    /* Get a reverse const iterator to terminate the loop. */
    inline Iterator crend() const {
        return rend();
    }

    /////////////////////////////////
    ////    COUPLED ITERATORS    ////
    /////////////////////////////////

    /* Create a coupled iterator over the container using the begin()/end() methods. */
    inline auto iter() const {
        return CoupledIterator<Iterator>(begin(), end());
    }

    /* Create a coupled iterator over the container using the cbegin()/cend() methods. */
    inline auto citer() const {
        return CoupledIterator<Iterator>(cbegin(), cend());
    }

    /* Create a coupled iterator over the container using the rbegin()/rend() methods. */
    inline auto reverse() const {
        return CoupledIterator<Iterator>(rbegin(), rend());
    }

    /* Create a coupled iterator over the container using the crbegin()/crend() methods. */
    inline auto creverse() const {
        return CoupledIterator<Iterator>(crbegin(), crend());
    }

    ////////////////////////////////
    ////    PYTHON INTERFACE    ////
    ////////////////////////////////

    /* Get a forward Python iterator over a mutable container. */
    inline PyObject* python() const {
        PyObject* iter = PyObject_GetIter(this->container);
        if (iter == nullptr && PyErr_Occurred()) {
            throw catch_python<type_error>();
        }
        return iter;
    }

    /* Get a forward Python iterator over an immutable container. */
    inline PyObject* cpython() const {
        return this->python();
    }

    /* Get a reverse Python iterator over a mutable container. */
    inline PyObject* rpython() const {
        PyObject* attr = PyObject_GetAttrString(this->container, "__reversed__");
        if (attr == nullptr && PyErr_Occurred()) {
            throw catch_python<type_error>();
        }
        PyObject* iter = PyObject_CallObject(attr, nullptr);
        Py_DECREF(attr);
        if (iter == nullptr && PyErr_Occurred()) {
            throw catch_python<type_error>();
        }
        return iter;  // new reference
    }

    /* Get a reverse Python iterator over an immutable container. */
    inline PyObject* crpython() const {
        return this->rpython();
    }

private:
    /* PyIterProxies can only be constructed through `iter()` factory function. */
    friend PyIterProxy<identity, false> iter(PyObject* container);
    friend PyIterProxy<identity, true> iter(const PyObject* container);
    template <typename _Func>
    friend PyIterProxy<_Func, false> iter(PyObject* container, _Func convert);
    template <typename _Func>
    friend PyIterProxy<_Func, true> iter(const PyObject* container, _Func convert);

    /* Construct an iterator proxy around a python container. */
    PyIterProxy(Container* c) : container(c), convert(Func{}) {}
    PyIterProxy(Container* c, Func f) : container(c), convert(f) {}

};


//////////////////////////////
////    ITER() FACTORY    ////
//////////////////////////////


/* Create a C++ to Python iterator proxy for a mutable C++ lvalue container. */
template <typename Container>
inline IterProxy<Container, identity, false> iter(Container& container) {
    return IterProxy<Container, identity, false>(container);
}


/* Create a C++ to Python iterator proxy for a const lvalue container. */
template <typename Container>
inline IterProxy<const Container, identity, false> iter(const Container& container) {
    return IterProxy<const Container, identity, false>(container);
}


/* Create a C++ to Python iterator proxy for a mutable C++ lvalue container. */
template <typename Container, typename Func>
inline IterProxy<Container, Func, false> iter(Container& container, Func convert) {
    return IterProxy<Container, Func, false>(container, convert);
}


/* Create a C++ to Python iterator proxy for a const lvalue container. */
template <typename Container, typename Func>
inline IterProxy<const Container, Func, false> iter(
    const Container& container,
    Func convert
) {
    return IterProxy<const Container, Func, false>(container, convert);
}


/* Create a C++ to Python iterator proxy for a mutable rvalue container. */
template <typename Container>
inline IterProxy<Container, identity, true> iter(Container&& container) {
    return IterProxy<Container, identity, true>(std::move(container));
}


/* Create a C++ to Python iterator proxy for a const rvalue container. */
template <typename Container>
inline IterProxy<const Container, identity, true> iter(const Container&& container) {
    return IterProxy<const Container, identity, true>(std::move(container));
}


/* Create a C++ to Python iterator proxy for a mutable rvalue container. */
template <typename Container, typename Func>
inline IterProxy<Container, Func, true> iter(Container&& container, Func convert) {
    return IterProxy<Container, Func, true>(std::move(container), convert);
}


/* Create a C++ to Python iterator proxy for a const rvalue container. */
template <typename Container, typename Func>
inline IterProxy<const Container, Func, true> iter(
    const Container&& container,
    Func convert
) {
    return IterProxy<const Container, Func, true>(std::move(container), convert);
}


/* Create a Python to C++ iterator proxy for a mutable Python container. */
inline PyIterProxy<identity, false> iter(PyObject* container) {
    return PyIterProxy<identity, false>(container);
}


/* Create a Python to C++ iterator proxy for a const Python container. */
inline PyIterProxy<identity, true> iter(const PyObject* container) {
    return PyIterProxy<identity, true>(container);
}


/* Create a Python to C++ iterator proxy for a mutable Python container. */
template <typename Func>
inline PyIterProxy<Func, false> iter(PyObject* container, Func convert) {
    return PyIterProxy<Func, false>(container, convert);
}


/* Create a Python to C++ iterator proxy for a const Python container. */
template <typename Func>
inline PyIterProxy<Func, true> iter(const PyObject* container, Func convert) {
    return PyIterProxy<Func, true>(container, convert);
}


}  // namespace util
}  // namespace structs
}  // namespace bertrand


#endif  // BERTRAND_STRUCTS_UTIL_ITER_H

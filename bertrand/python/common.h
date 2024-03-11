#include "pyport.h"
#include "pytypedefs.h"
#if !defined(BERTRAND_PYTHON_INCLUDED) && !defined(LINTER)
#error "This file should not be included directly.  Please include <bertrand/python.h> instead."
#endif

#ifndef BERTRAND_PYTHON_COMMON_H
#define BERTRAND_PYTHON_COMMON_H

#include <algorithm>
#include <chrono>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <Python.h>
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/eval.h>
#include <pybind11/functional.h>
#include <pybind11/iostream.h>
#include <pybind11/numpy.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>


/* NOTES ON PERFORMANCE:
 * In general, bertrand should be quite efficient, and generally trade blows with
 * native Python in most respects.  It expands out to raw CPython API calls, so
 * properly optimized (i.e. type safe) code should retain as much performance as
 * possible, and may even gain some due to specific optimizations at the C++ level.
 * There are, however, a few things to keep in mind:
 *
 *  1.  A null pointer check followed by a type check is implicitly incurred whenever a
 *      generalized py::Object is narrowed to a more specific type, such as py::Int or
 *      py::List.  This is necessary to ensure type safety, and is optimized for
 *      built-in types, but can become a pessimization if done frequently, especially
 *      in tight loops.  If you find yourself doing this, consider either converting to
 *      strict types earlier in the code, which will allow the compiler to enforce
 *      these rules at compile time, or keeping all object interactions generic to
 *      prevent thrashing.  Generally, the most common case where this can be a problem
 *      is when assigning the result of a generic attribute lookup (`.attr()`), index
 *      (`[]`), or call (`()`) operator to a strict type, since these operators return
 *      py::Object instances by default.  If you naively bind these to a strict type
 *      (e.g. `py::Int x = py::List{1, 2, 3}[1]` or `py::Str y = x.attr("__doc__")`),
 *      then the runtime check will be implicitly incurred to make the operation type
 *      safe.  See point #2 below for a workaround.  In the future, bertrand will
 *      likely offer typed alternatives for the basic containers, which can promote
 *      some of these checks to compile time, but they can never be completely
 *      eliminated.
 *  2.  For cases where the type of a generic object is known in advance, it is
 *      possible to bypass the runtime check by using `py::reinterpret_borrow<T>(obj)`
 *      or `py::reinterpret_steal<T>(obj.release())`.  These functions are not type
 *      safe, and should be used with caution (especially the latter, which can lead to
 *      memory leaks if used incorrectly).  However, they can be useful when working
 *      with custom types, as in most cases a method's return type and reference count
 *      will be known ahead of time, making the runtime check redundant.  In most other
 *      cases, it is not recommended to use these functions, as they can lead to subtle
 *      bugs and crashes if their assumptions are incorrect.
 *  3.  There is a penalty for copying data across the Python/C++ boundary.  This is
 *      generally quite small (even for lists and other container types), but it can
 *      add up if done frequently.  If you find yourself repeatedly copying large
 *      amounts of data between Python and C++, you should reconsider your design or
 *      use the buffer protocol to avoid the copy.  This is especially true for NumPy
 *      arrays, which can be accessed directly as C++ arrays without copying.
 *  4.  Python (at least for now) does not play well with multithreaded code, and
 *      neither does bertrand.  If you need to use Python in a multithreaded context,
 *      consider offloading the work to C++ and passing the results back to Python.
 *      This unlocks full native parallelism, with SIMD, OpenMP, and other tools at
 *      your disposal.  If you must use Python, consider using the
 *      `py::gil_scoped_release` guard to release the GIL while doing C++ work, and
 *      then reacquire it via RAII before returning to Python.
 *  5.  Lastly, Bertrand makes it possible to store arbitrary Python objects with
 *      static duration using the py::Static<> wrapper, which can reduce net
 *      allocations and improve performance.  This is especially true for global
 *      objects like modules and scripts, which can be cached and reused across the
 *      lifetime of the program.
 */


using namespace pybind11::literals;
namespace bertrand {
namespace py {


/////////////////////////////////////////
////     INHERITED FROM PYBIND11     ////
/////////////////////////////////////////


/* Pybind11 has rich support for converting between Python and C++ types, calling
 * Python functions from C++ (and vice versa), and exposing C++ types to Python.  We
 * don't change any of this behavior, meaning extensions should work with pybind11 as
 * expected.
 *
 * Pybind11 documentation:
 *     https://pybind11.readthedocs.io/en/stable/
 */


// binding functions
// using pybind11::cast;
using pybind11::reinterpret_borrow;
using pybind11::reinterpret_steal;
using pybind11::implicitly_convertible;
using pybind11::args_are_all_keyword_or_ds;
using pybind11::make_tuple;  // TODO: unnecessary
using pybind11::make_iterator;  // TODO: roll into Iterator() constructor
using pybind11::make_key_iterator;  // offer as static method in Iterator:: namespace
using pybind11::make_value_iterator;  // same as above
using pybind11::initialize_interpreter;
using pybind11::scoped_interpreter;
// PYBIND11_MODULE                      <- macros don't respect namespaces
// PYBIND11_EMBEDDED_MODULE
// PYBIND11_OVERRIDE
// PYBIND11_OVERRIDE_PURE
// PYBIND11_OVERRIDE_NAME
// PYBIND11_OVERRIDE_PURE_NAME
using pybind11::get_override;
using pybind11::cpp_function;  // TODO: unnecessary, use py::Function() instead
using pybind11::scoped_ostream_redirect;
using pybind11::scoped_estream_redirect;
using pybind11::add_ostream_redirect;


// annotations
using pybind11::is_method;
using pybind11::is_setter;
using pybind11::is_operator;
using pybind11::is_final;
using pybind11::scope;
using pybind11::doc;
using pybind11::name;
using pybind11::sibling;
using pybind11::base;
using pybind11::keep_alive;
using pybind11::multiple_inheritance;
using pybind11::dynamic_attr;
using pybind11::buffer_protocol;
using pybind11::metaclass;
using pybind11::custom_type_setup;
using pybind11::module_local;
using pybind11::arithmetic;
using pybind11::prepend;
using pybind11::call_guard;
using pybind11::arg;
using pybind11::arg_v;
using pybind11::kw_only;
using pybind11::pos_only;

namespace detail = pybind11::detail;


// wrapper types
template <typename... Args>
using Class = pybind11::class_<Args...>;
using Handle = pybind11::handle;
using Iterator = pybind11::iterator;
using WeakRef = pybind11::weakref;
using Capsule = pybind11::capsule;
using Buffer = pybind11::buffer;  // TODO: place in buffer.h along with memoryview
using MemoryView = pybind11::memoryview;
using Bytes = pybind11::bytes;  // TODO: place in str.h with bytearray.  They use an API mixin
using Bytearray = pybind11::bytearray;
class Object;
class NotImplementedType;
class Bool;
class Int;
class Float;
class Complex;
class Slice;
class Range;
class List;
class Tuple;
class Set;
class FrozenSet;
class KeysView;
class ItemsView;
class ValuesView;
class Dict;
class MappingProxy;
class Str;
class Type;
class Code;
class Frame;
class Function;
class Method;
class ClassMethod;
class StaticMethod;
class Property;
class Timedelta;
class Timezone;
class Date;
class Time;
class Datetime;

// TODO: Regex should be placed in bertrand:: namespace.  It doesn't actually wrap a
// python object, so it shouldn't be in the py:: namespace

class Regex;  // TODO: incorporate more fully (write pybind11 bindings so that it can be passed into Python scripts)


//////////////////////////
////    EXCEPTIONS    ////
//////////////////////////


/* Pybind11 exposes some, but not all of the built-in Python errors.  We expand them
 * here so that users never reach for an error that doesn't exist, and we replicate the
 * standard error hierarchy so that users can use identical semantics to normal Python.
 *
 * CPython exception types:
 *      https://docs.python.org/3/c-api/exceptions.html#standard-exceptions
 *
 * Hierarchy:
 *      https://docs.python.org/3/library/exceptions.html#exception-hierarchy
 */


// These exceptions have no python equivalent
using pybind11::error_already_set;
using CastError = pybind11::cast_error;
using ReferenceCastError = pybind11::reference_cast_error;


#define PYTHON_EXCEPTION(base, cls, exc)                                                \
    class PYBIND11_EXPORT_EXCEPTION cls : public base {                                 \
    public:                                                                             \
        using base::base;                                                               \
        cls() : cls("") {}                                                              \
        explicit cls(Handle obj) : base([&obj] {                                        \
            PyObject* string = PyObject_Str(obj.ptr());                                 \
            if (string == nullptr) {                                                    \
                throw error_already_set();                                              \
            }                                                                           \
            Py_ssize_t size;                                                            \
            const char* data = PyUnicode_AsUTF8AndSize(string, &size);                  \
            if (data == nullptr) {                                                      \
                Py_DECREF(string);                                                      \
                throw error_already_set();                                              \
            }                                                                           \
            std::string result(data, size);                                             \
            Py_DECREF(string);                                                          \
            return result;                                                              \
        }()) {}                                                                         \
        void set_error() const override { PyErr_SetString(exc, what()); }               \
    };                                                                                  \


using Exception = pybind11::builtin_exception;
PYTHON_EXCEPTION(Exception, ArithmeticError, PyExc_ArithmeticError)
    PYTHON_EXCEPTION(ArithmeticError, FloatingPointError, PyExc_OverflowError)
    PYTHON_EXCEPTION(ArithmeticError, OverflowError, PyExc_OverflowError)
    PYTHON_EXCEPTION(ArithmeticError, ZeroDivisionError, PyExc_ZeroDivisionError)
PYTHON_EXCEPTION(Exception, AssertionError, PyExc_AssertionError)
PYTHON_EXCEPTION(Exception, AttributeError, PyExc_AttributeError)
PYTHON_EXCEPTION(Exception, BufferError, PyExc_BufferError)
PYTHON_EXCEPTION(Exception, EOFError, PyExc_EOFError)
PYTHON_EXCEPTION(Exception, ImportError, PyExc_ImportError)
    PYTHON_EXCEPTION(ImportError, ModuleNotFoundError, PyExc_ModuleNotFoundError)
PYTHON_EXCEPTION(Exception, LookupError, PyExc_LookupError)
    PYTHON_EXCEPTION(LookupError, IndexError, PyExc_IndexError)
    PYTHON_EXCEPTION(LookupError, KeyError, PyExc_KeyError)
PYTHON_EXCEPTION(Exception, MemoryError, PyExc_MemoryError)
PYTHON_EXCEPTION(Exception, NameError, PyExc_NameError)
    PYTHON_EXCEPTION(NameError, UnboundLocalError, PyExc_UnboundLocalError)
PYTHON_EXCEPTION(Exception, OSError, PyExc_OSError)
    PYTHON_EXCEPTION(OSError, BlockingIOError, PyExc_BlockingIOError)
    PYTHON_EXCEPTION(OSError, ChildProcessError, PyExc_ChildProcessError)
    PYTHON_EXCEPTION(OSError, ConnectionError, PyExc_ConnectionError)
        PYTHON_EXCEPTION(ConnectionError, BrokenPipeError, PyExc_BrokenPipeError)
        PYTHON_EXCEPTION(ConnectionError, ConnectionAbortedError, PyExc_ConnectionAbortedError)
        PYTHON_EXCEPTION(ConnectionError, ConnectionRefusedError, PyExc_ConnectionRefusedError)
        PYTHON_EXCEPTION(ConnectionError, ConnectionResetError, PyExc_ConnectionResetError)
    PYTHON_EXCEPTION(OSError, FileExistsError, PyExc_FileExistsError)
    PYTHON_EXCEPTION(OSError, FileNotFoundError, PyExc_FileNotFoundError)
    PYTHON_EXCEPTION(OSError, InterruptedError, PyExc_InterruptedError)
    PYTHON_EXCEPTION(OSError, IsADirectoryError, PyExc_IsADirectoryError)
    PYTHON_EXCEPTION(OSError, NotADirectoryError, PyExc_NotADirectoryError)
    PYTHON_EXCEPTION(OSError, PermissionError, PyExc_PermissionError)
    PYTHON_EXCEPTION(OSError, ProcessLookupError, PyExc_ProcessLookupError)
    PYTHON_EXCEPTION(OSError, TimeoutError, PyExc_TimeoutError)
PYTHON_EXCEPTION(Exception, ReferenceError, PyExc_ReferenceError)
PYTHON_EXCEPTION(Exception, RuntimeError, PyExc_RuntimeError)
    PYTHON_EXCEPTION(RuntimeError, NotImplementedError, PyExc_NotImplementedError)
    PYTHON_EXCEPTION(RuntimeError, RecursionError, PyExc_RecursionError)
PYTHON_EXCEPTION(Exception, StopAsyncIteration, PyExc_StopAsyncIteration)
PYTHON_EXCEPTION(Exception, StopIteration, PyExc_StopIteration)
PYTHON_EXCEPTION(Exception, SyntaxError, PyExc_SyntaxError)
    PYTHON_EXCEPTION(SyntaxError, IndentationError, PyExc_IndentationError)
        PYTHON_EXCEPTION(IndentationError, TabError, PyExc_TabError)
PYTHON_EXCEPTION(Exception, SystemError, PyExc_SystemError)
PYTHON_EXCEPTION(Exception, TypeError, PyExc_TypeError)
PYTHON_EXCEPTION(Exception, ValueError, PyExc_ValueError)
    PYTHON_EXCEPTION(ValueError, UnicodeError, PyExc_UnicodeError)
        PYTHON_EXCEPTION(UnicodeError, UnicodeDecodeError, PyExc_UnicodeDecodeError)
        PYTHON_EXCEPTION(UnicodeError, UnicodeEncodeError, PyExc_UnicodeEncodeError)
        PYTHON_EXCEPTION(UnicodeError, UnicodeTranslateError, PyExc_UnicodeTranslateError)


#undef PYTHON_EXCEPTION


//////////////////////////////
////    BUILT-IN TYPES    ////
//////////////////////////////


/* Pybind11's wrapper classes cover most of the Python standard library, but not all of
 * it, and not with the same syntax as normal Python.  They're also all given in
 * lowercase C++ style, which can cause ambiguities with native C++ types (e.g.
 * pybind11::int_) and non-member functions of the same name.  As such, bertrand
 * provides its own set of wrappers that extend the pybind11 types to the whole CPython
 * API.  These wrappers are designed to be used with nearly identical semantics to the
 * Python types they represent, making them more self-documenting and easier to use
 * from C++.  For questions, refer to the Python documentation first and then the
 * source code for the types themselves, which are provided in named header files
 * within this directory.
 *
 * The final syntax is very similar to standard pybind11.  For example, the following
 * pybind11 code:
 *
 *    py::list foo = py::cast(std::vector<int>{1, 2, 3});
 *    py::int_ bar = foo.attr("pop")();
 *
 * Would be written as:
 *
 *    py::List foo = {1, 2, 3};
 *    py::Int bar = foo.pop();
 *
 * Which closely mimics Python:
 *
 *    foo = [1, 2, 3]
 *    bar = foo.pop()
 *
 * Note that the initializer list syntax is standardized for all container types, as
 * well as any function/method that expects a sequence.  This gives a direct equivalent
 * to Python's tuple, list, set, and dict literals, which can be expressed as:
 *
 *     py::Tuple{1, "a", true};                     // (1, "a", True)
 *     py::List{1, "a", true};                      // [1, 2, 3]
 *     py::Set{1, "a", true};                       // {1, 2, 3}
 *     py::Dict{{1, 3.0}, {"a", 2}, {true, "x"}};   // {1: 3.0, "a": 2, True: "x"}
 *
 * Note also that these initializer lists can contain any type, including mixed types.
 *
 * Built-in Python types:
 *    https://docs.python.org/3/library/stdtypes.html
 */


namespace impl {

    /* Helper function triggers implicit conversion operators and/or implicit
    constructors, but not explicit ones.  In contrast, static_cast<>() will trigger
    explicit constructors on the target type, which can give unexpected results and
    violate bertrand's strict type safety. */
    template <typename U>
    inline static decltype(auto) implicit_cast(U&& value) {
        return std::forward<U>(value);
    }

    namespace concepts {

        namespace categories {

            struct Base {
                static constexpr bool boollike = false;
                static constexpr bool intlike = false;
                static constexpr bool floatlike = false;
                static constexpr bool complexlike = false;
                static constexpr bool strlike = false;
                static constexpr bool timedeltalike = false;
                static constexpr bool timezonelike = false;
                static constexpr bool datelike = false;
                static constexpr bool timelike = false;
                static constexpr bool datetimelike = false;
                static constexpr bool tuplelike = false;
                static constexpr bool listlike = false;
                static constexpr bool setlike = false;
                static constexpr bool dictlike = false;
            };

            template <typename T>
            class Traits : public Base {};

            template <typename T>
            struct Traits<std::complex<T>> : public Base {
                static constexpr bool complexlike = true;
            };

            template <typename... Args>
            struct Traits<std::chrono::duration<Args...>> : public Base {
                static constexpr bool timedeltalike = true;
            };

            template <typename... Args>
            struct Traits<std::chrono::time_point<Args...>> : public Base {
                static constexpr bool timelike = true;
            };

            // TODO: std::time_t?

            template <typename... Args>
            struct Traits<std::pair<Args...>> : public Base {
                static constexpr bool tuplelike = true;
            };

            template <typename... Args>
            struct Traits<std::tuple<Args...>> : public Base {
                static constexpr bool tuplelike = true;
            };

            template <typename T, size_t N>
            struct Traits<std::array<T, N>> : public Base {
                static constexpr bool listlike = true;
            };

            template <typename... Args>
            struct Traits<std::vector<Args...>> : public Base {
                static constexpr bool listlike = true;
            };

            template <typename... Args>
            struct Traits<std::deque<Args...>> : public Base {
                static constexpr bool listlike = true;
            };

            template <typename... Args>
            struct Traits<std::list<Args...>> : public Base {
                static constexpr bool listlike = true;
            };

            template <typename... Args>
            struct Traits<std::forward_list<Args...>> : public Base {
                static constexpr bool listlike = true;
            };

            template <typename... Args>
            struct Traits<std::set<Args...>> : public Base {
                static constexpr bool setlike = true;
            };

            template <typename... Args>
            struct Traits<std::unordered_set<Args...>> : public Base {
                static constexpr bool setlike = true;
            };

            template <typename... Args>
            struct Traits<std::map<Args...>> : public Base {
                static constexpr bool dictlike = true;
            };

            template <typename... Args>
            struct Traits<std::unordered_map<Args...>> : public Base {
                static constexpr bool dictlike = true;
            };

        }  // namespace categories

        template <typename From, typename To, typename = void>
        constexpr bool has_conversion_operator = false;
        template <typename From, typename To>
        constexpr bool has_conversion_operator<
            From, To, std::void_t<decltype(std::declval<From>().operator To())>
        > = true;

        template <typename From, typename To, typename = void>
        constexpr bool explicitly_convertible_to = false;
        template <typename From, typename To>
        constexpr bool explicitly_convertible_to<
            From, To, std::void_t<decltype(static_cast<To>(std::declval<From>()))>
        > = true;

        template <typename T>
        concept python_like = detail::is_pyobject<T>::value;

        template <typename T>
        concept accessor_like = (
            std::is_base_of_v<detail::obj_attr_accessor, T> ||
            std::is_base_of_v<detail::str_attr_accessor, T> ||
            std::is_base_of_v<detail::item_accessor, T> ||
            std::is_base_of_v<detail::sequence_accessor, T> ||
            std::is_base_of_v<detail::tuple_accessor, T> ||
            std::is_base_of_v<detail::list_accessor, T>
        );

        template <typename T>
        concept sequence_like = requires(const T& t) {
            { std::begin(t) } -> std::input_or_output_iterator;
            { std::end(t) } -> std::input_or_output_iterator;
            { t.size() } -> std::convertible_to<size_t>;
            { t[0] };
        };

        template <typename T>
        concept bool_like = (
            std::is_same_v<bool, T> ||
            std::is_base_of_v<py::Bool, T> ||
            std::is_base_of_v<pybind11::bool_, T>
        );

        template <typename T>
        concept int_like = (
            (std::is_integral_v<T> && !std::is_same_v<T, bool>) ||
            std::is_base_of_v<Int, T> ||
            std::is_base_of_v<pybind11::int_, T>
        );

        template <typename T>
        concept float_like = (
            std::is_floating_point_v<T> ||
            std::is_base_of_v<Float, T> ||
            std::is_base_of_v<pybind11::float_, T>
        );

        template <typename T>
        concept complex_like = (
            std::is_base_of_v<std::complex<float>, T> ||
            std::is_base_of_v<std::complex<double>, T> ||
            std::is_base_of_v<std::complex<long double>, T> ||
            std::is_base_of_v<Complex, T>
        );

        template <typename T>
        concept str_like = (
            std::is_same_v<const char*, T> ||
            std::is_base_of_v<std::string, T> ||
            std::is_base_of_v<std::string_view, T> ||
            std::is_base_of_v<Str, T> ||
            std::is_base_of_v<pybind11::str, T>
        );

        template <typename T>
        concept timedelta_like = (
            categories::Traits<T>::timedeltalike ||
            std::is_base_of_v<Timedelta, T>
        );

        template <typename T>
        concept timezone_like = (
            categories::Traits<T>::timezonelike ||
            std::is_base_of_v<Timezone, T>
        );

        template <typename T>
        concept date_like = (
            categories::Traits<T>::datelike ||
            std::is_base_of_v<Date, T>
        );

        template <typename T>
        concept time_like = (
            categories::Traits<T>::timelike ||
            std::is_base_of_v<Time, T>
        );

        template <typename T>
        concept datetime_like = (
            categories::Traits<T>::datetimelike ||
            std::is_base_of_v<Datetime, T>
        );

        template <typename T>
        concept slice_like = (
            std::is_base_of_v<Slice, T> ||
            std::is_base_of_v<pybind11::slice, T>
        );

        template <typename T>
        concept range_like = (
            std::is_base_of_v<Range, T>
        );

        template <typename T>
        concept tuple_like = (
            categories::Traits<T>::tuplelike ||
            std::is_base_of_v<Tuple, T> ||
            std::is_base_of_v<pybind11::tuple, T>
        );

        template <typename T>
        concept list_like = (
            categories::Traits<T>::listlike ||
            std::is_base_of_v<List, T> ||
            std::is_base_of_v<pybind11::list, T>
        );

        template <typename T>
        concept set_like = (
            categories::Traits<T>::setlike ||
            std::is_base_of_v<Set, T> ||
            std::is_base_of_v<pybind11::set, T>
        );

        template <typename T>
        concept frozenset_like = (
            categories::Traits<T>::setlike ||
            std::is_base_of_v<FrozenSet, T> ||
            std::is_base_of_v<pybind11::frozenset, T>
        );

        template <typename T>
        concept anyset_like = set_like<T> || frozenset_like<T>;

        template <typename T>
        concept dict_like = (
            categories::Traits<T>::dictlike ||
            std::is_base_of_v<Dict, T> ||
            std::is_base_of_v<pybind11::dict, T>
        );

        template <typename T>
        concept mappingproxy_like = (
            categories::Traits<T>::dictlike ||
            std::is_base_of_v<MappingProxy, T>
        );

        template <typename T>
        concept anydict_like = dict_like<T> || mappingproxy_like<T>;

        template <typename T>
        concept type_like = (
            std::is_base_of_v<Type, T> ||
            std::is_base_of_v<pybind11::type, T>
        );

        template <typename T>
        concept has_size = requires(const T& t) {
            { t.size() } -> std::convertible_to<size_t>;
        };

        template <typename T>
        concept has_empty = requires(const T& t) {
            { t.empty() } -> std::convertible_to<bool>;
        };

        template <typename T>
        concept has_reserve = requires(T& t, size_t n) {
            { t.reserve(n) };
        };

        template <typename T>
        concept is_hashable = requires(const T& t) {
            { std::hash<T>{}(t) } -> std::convertible_to<size_t>;
        };

        template <typename T>
        concept is_iterable = requires(const T& t) {
            { std::begin(t) } -> std::input_or_output_iterator;
            { std::end(t) } -> std::input_or_output_iterator;
        };

        template <typename T>
        concept reverse_iterable = requires(const T& t) {
            { std::rbegin(t) } -> std::input_or_output_iterator;
            { std::rend(t) } -> std::input_or_output_iterator;
        };

        template <typename T>
        concept has_to_string = requires(const T& t) {
            { std::to_string(t) } -> std::convertible_to<std::string>;
        };

        template <typename T>
        concept has_stream_insertion = requires(std::ostream& os, const T& t) {
            { os << t } -> std::convertible_to<std::ostream&>;
        };

        /* NOTE: reverse operators sometimes conflict with standard library iterators, so
        we need some way of detecting them.  This is somewhat hacky, but it seems to
        work. */
        template <typename T, typename = void>
        constexpr bool is_std_iterator = false;
        template <typename T>
        constexpr bool is_std_iterator<
            T, std::void_t<decltype(
                std::declval<typename std::iterator_traits<T>::iterator_category>()
            )>
        > = true;

        /* SFINAE condition allows py::iter() to work on both python and C++ types. */
        template <typename T, typename = void>
        constexpr bool pybind11_iterable = false;
        template <typename T>
        constexpr bool pybind11_iterable<
            T, std::void_t<decltype(pybind11::iter(std::declval<T>()))>
        > = true;

        template <typename T, typename = void>
        struct OverloadsCallable : std::false_type {};
        template <typename T>
        struct OverloadsCallable<T, std::void_t<decltype(&T::operator())>> :
            std::true_type
        {};

        /* SFINAE condition is used to recognize callable C++ types without regard to their
        argument signatures. */
        template <typename T>
        static constexpr bool is_callable_any = std::disjunction_v<
            std::is_function<std::remove_pointer_t<std::decay_t<T>>>,
            std::is_member_function_pointer<std::decay_t<T>>,
            OverloadsCallable<std::decay_t<T>>
        >;

        /* Base class for CallTraits tags, which contain SFINAE information about a
        callable Python/C++ object, as returned by `py::callable()`. */
        template <typename Func>
        class CallTraitsBase {
        protected:
            const Func& func;

        public:
            constexpr CallTraitsBase(const Func& func) : func(func) {}

            friend std::ostream& operator<<(std::ostream& os, const CallTraitsBase& traits) {
                if (traits) {
                    os << "True";
                } else {
                    os << "False";
                }
                return os;
            }

        };

        /* Return tag for `py::callable()` when one or more template parameters are
        supplied, representing hypothetical arguments to the function. */
        template <typename Func, typename... Args>
        struct CallTraits : public CallTraitsBase<Func> {
            struct NoReturn {};

        private:
            using Base = CallTraitsBase<Func>;

            /* SFINAE struct gets return type if Func is callable with the given arguments.
            Otherwise defaults to NoReturn. */
            template <typename T, typename = void>
            struct GetReturn { using type = NoReturn; };
            template <typename T>
            struct GetReturn<
                T, std::void_t<decltype(std::declval<T>()(std::declval<Args>()...))>
            > {
                using type = decltype(std::declval<T>()(std::declval<Args>()...));
            };

        public:
            using Base::Base;

            /* Get the return type of the function with the given arguments.  Defaults to
            NoReturn if the function is not callable with those arguments. */
            using Return = typename GetReturn<Func>::type;

            /* Implicitly convert the tag to a constexpr bool. */
            template <typename T = Func> requires (!impl::python_like<T>)
            inline constexpr operator bool() const {
                return std::is_invocable_v<Func, Args...>;
            }

            /* Implicitly convert to a runtime boolean by directly inspecting a Python code
            object.  Note that the introspection is very lightweight and basic.  It first
            checks `std::is_invocable<Func, Args...>` to see if all arguments can be
            converted to Python objects, and then confirms that their number matches those
            of the underlying code object.  This includes accounting for default values and
            missing keyword-only arguments, while enforcing a C++-style calling convention.
            Note that this check does not account for variadic arguments, which are not
            represented in the code object itself. */
            template <typename T = Func> requires (impl::python_like<T>)
            operator bool() const {
                if constexpr(std::is_same_v<Return, NoReturn>) {
                    return false;
                } else {
                    static constexpr Py_ssize_t expected = sizeof...(Args);

                    // check Python object is callable
                    if (!PyCallable_Check(this->func.ptr())) {
                        return false;
                    }

                    // Get code object associated with callable (borrowed ref)
                    PyCodeObject* code = (PyCodeObject*) PyFunction_GetCode(this->func.ptr());
                    if (code == nullptr) {
                        return false;
                    }

                    // get number of positional/positional-only arguments from code object
                    Py_ssize_t n_args = code->co_argcount;
                    if (expected > n_args) {
                        return false;  // too many arguments
                    }

                    // get number of positional defaults from function object (borrowed ref)
                    PyObject* defaults = PyFunction_GetDefaults(this->func.ptr());
                    Py_ssize_t n_defaults = 0;
                    if (defaults != nullptr) {
                        n_defaults = PyTuple_Size(defaults);
                    }
                    if (expected < (n_args - n_defaults)) {
                        return false;  // too few arguments
                    }

                    // check for presence of unfilled keyword-only arguments
                    if (code->co_kwonlyargcount > 0) {
                        PyObject* kwdefaults = PyObject_GetAttrString(
                            this->func.ptr(),
                            "__kwdefaults__"
                        );
                        if (kwdefaults == nullptr) {
                            PyErr_Clear();
                            return false;
                        }
                        Py_ssize_t n_kwdefaults = 0;
                        if (kwdefaults != Py_None) {
                            n_kwdefaults = PyDict_Size(kwdefaults);
                        }
                        Py_DECREF(kwdefaults);
                        if (n_kwdefaults < code->co_kwonlyargcount) {
                            return false;
                        }
                    }

                    // NOTE: we cannot account for variadic arguments, which are not
                    // represented in the code object.  This is a limitation of the Python
                    // C API

                    return true;
                }
            }

        };

        /* Template specialization for wildcard callable matching.  Note that for technical
        reasons, it is easier to swap the meaning of the void parameter in this case, so
        that the behavior of each class is self-consistent. */
        template <typename Func>
        class CallTraits<Func, void> : public CallTraitsBase<Func> {
            using Base = CallTraitsBase<Func>;

        public:
            using Base::Base;

            // NOTE: Return type is not well-defined for wildcard matching.  Attempting to
            // access it will result in a compile error.

            /* Implicitly convert the tag to a constexpr bool. */
            template <typename T = Func> requires (!impl::python_like<T>)
            inline constexpr operator bool() const {
                return is_callable_any<Func>;
            }

            /* Implicitly convert the tag to a runtime bool. */
            template <typename T = Func> requires (impl::python_like<T>)
            inline operator bool() const {
                return PyCallable_Check(this->func.ptr());
            }

        };

    }  // namespace concepts
    using namespace concepts;

    template <typename T>
    struct __dereference__ { static constexpr bool enable = false; };  // TODO: rename to __unpack__?
    template <typename T, typename... Args>
    struct __call__ { static constexpr bool enable = false; };
    template <typename T>
    struct __len__ { static constexpr bool enable = false; };
    template <typename T, typename Key>
    struct __contains__ { static constexpr bool enable = false; };
    template <typename T>
    struct __iter__ { static constexpr bool enable = false; };
    template <typename T>
    struct __reversed__ { static constexpr bool enable = false; };
    template <typename T, typename Key>
    struct __getitem__ { static constexpr bool enable = false; };  // TODO: constrain accessor conversions
    template <typename T, typename Key, typename Value>
    struct __setitem__ { static constexpr bool enable = false; };  // TODO: constrain accessor assignment
    template <typename T, typename Key>
    struct __delitem__ { static constexpr bool enable = false; };  // TODO: enable/disable .del()
    template <typename T>
    struct __pos__ { static constexpr bool enable = false; };
    template <typename T>
    struct __neg__ { static constexpr bool enable = false; };
    template <typename T>
    struct __abs__ { static constexpr bool enable = false; };  // TODO: enable/disable py::abs() and set return value
    template <typename T>
    struct __invert__ { static constexpr bool enable = false; };
    template <typename T>
    struct __increment__ { static constexpr bool enable = false; };  // ++
    template <typename T>
    struct __decrement__ { static constexpr bool enable = false; };  // --
    template <typename L, typename R>
    struct __lt__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __le__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __eq__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __ne__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __ge__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __gt__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __add__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __sub__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __mul__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __truediv__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __mod__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __lshift__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __rshift__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __and__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __xor__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __or__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __iadd__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __isub__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __imul__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __itruediv__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __imod__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __ilshift__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __irshift__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __iand__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __ixor__ { static constexpr bool enable = false; };
    template <typename L, typename R>
    struct __ior__ { static constexpr bool enable = false; };

    /* Base class for enabled operators.  Encodes the return type as a template
    parameter. */
    template <typename T>
    struct Returns {
        static constexpr bool enable = true;
        using Return = T;
    };

    template <typename Obj, typename Key>
    class ItemAccessor;

    /* Tag class to identify wrappers during SFINAE checks. */
    struct WrapperTag {};

    template <typename T>
    constexpr bool is_wrapper = std::is_base_of_v<WrapperTag, T>;

    struct SliceInitializer;

}  // namespace impl


/* A revised pybind11::object interface that allows implicit conversions to subtypes
(applying a type check on the way), explicit conversions to arbitrary C++ types via
static_cast<>, cross-language math operators, and generalized slice/attr syntax. */
class Object : public pybind11::object {
    using Base = pybind11::object;

    /* Some operators can only be defined as member functions within this class, which
     * complicates the control struct approach to operator overloading.  To work around
     * this, we can delegate to an equivalent static member that accepts a reference to
     * `this` as its first argument.  This allows us to correctly deduce the most-
     * derived type and enable/disable the control struct accordingly.  The member
     * operator then delegates to the static equivalent, and gives the expected
     * behavior.  This slightly complicates error messages, but is much cleaner than
     * relying on the preprocessor, and does not interfere with custom overloads that
     * can be supplied by subclasses.
     */

    template <typename T> requires (impl::__dereference__<T>::enable)
    inline static auto dereference_impl(const T& obj) {
        return obj.pybind11_deref();
    }

    template <typename T, typename... Args>
        requires (impl::__call__<T, Args...>::enable)
    inline static auto call_impl(const T& obj, Args&&... args)
        -> impl::__call__<T, Args...>::Return;  // defined in python.h

    template <typename T> requires (impl::__len__<T>::enable)
    inline static size_t size_impl(const T& obj) {
        static_assert(
            std::is_same_v<typename impl::__len__<T>::Return, size_t>,
            "size() operator must return a size_t for compatibility with C++ "
            "containers.  Check your specialization of __len__ for these types and "
            "ensure the Return type is set to size_t."
        );
        Py_ssize_t result = PyObject_Size(obj.ptr());
        if (result < 0) {
            throw error_already_set();
        }
        return static_cast<size_t>(result);
    }

    template <typename T, typename Key>
        requires (impl::__contains__<T, Key>::enable)
    inline static bool contains_impl(const T& obj, const Key& key) {
        static_assert(
            std::is_same_v<typename impl::__contains__<T, Key>::Return, bool>,
            "contains() operator must return a boolean value.  Check your "
            "specialization of __contains__ for these types and ensure the Return "
            "type is set to bool."
        );
        return obj.pybind11_contains(key);
    }

    template <typename T> requires (impl::__iter__<T>::enable)
    inline static Iterator begin_impl(const T& obj) {
        // TODO: use return type to fill in the iterator's dereference type.
        return obj.pybind11_begin();
    }

    template <typename T> requires (impl::__iter__<T>::enable)
    inline static Iterator end_impl(const T& obj) {
        return Iterator::sentinel();
    }

    template <typename T> requires (impl::__reversed__<T>::enable)
    inline static Iterator rbegin_impl(const T& obj) {
        return reinterpret_steal<Iterator>(obj.attr("__reversed__")().release());
    }

    template <typename T> requires (impl::__reversed__<T>::enable)
    inline static Iterator rend_impl(const T& obj) {
        return Iterator::sentinel();
    }

    // TODO: implement getitem/setitem accessors using control structs
    template <typename T, typename Key> requires (impl::__getitem__<T, Key>::enable)
    inline static auto getitem_impl(const T& obj, const Key& key)
        -> impl::__getitem__<T, Key>::Return
    {
        return impl::ItemAccessor<Key, T>(obj.pybind11_getitem(key));
    }

    /* We need to forward the original pybind11 implementations for the above methods
     * to work.  Since these are fully generic, we don't need to modify their behavior
     * except to enforce static types where applicable.
     */

    inline decltype(auto) pybind11_deref() const {
        return Base::operator*();
    }

    template <typename... Args>
    inline decltype(auto) pybind11_call(Args&&... args) const {
        return Base::operator()(std::forward<Args>(args)...);
    }

    template <typename Key>
    inline bool pybind11_contains(const Key& key) const {
        return Base::contains(key);
    }

    inline auto pybind11_begin() const {
        return Base::begin();
    }

    inline auto pybind11_end() const {
        return Base::end();
    }

    inline auto pybind11_getitem(handle key) const {
        return Base::operator[](key);
    }

    inline auto pybind11_getitem(Object&& key) const {
        return Base::operator[](std::move(key));
    }

    inline auto pybind11_getitem(const char* key) const {
        return Base::operator[](key);
    }

    /* Access an item from the dict using a string. */
    inline auto pybind11_getitem(const std::string& key) const {
        return Base::operator[](key.c_str());
    }

    /* Access an item from the dict using a string. */
    inline auto pybind11_getitem(const std::string_view& key) const {
        return Base::operator[](key.data());
    }

    auto pybind11_getitem(
        const std::initializer_list<impl::SliceInitializer>& slice
    ) const;  // TODO: defined in python.h??? (or later in this file)

    template <typename T> requires (!impl::python_like<T>)
    inline auto pybind11_getitem(const T& key) const {
        return Base::operator[](detail::object_or_cast(key));
    }

public:
    static Type type;

    /* Check whether the templated type is considered object-like at compile time. */
    template <typename T>
    static constexpr bool check() { return std::is_base_of_v<pybind11::object, T>; }

    /* Check whether a C++ value is considered object-like at compile time. */
    template <typename T> requires (!impl::python_like<T>)
    static constexpr bool check(const T& value) { return check<T>(); }

    /* Check whether a Python value is considered object-like at runtime. */
    template <typename T> requires (impl::python_like<T>)
    static constexpr bool check(const T& value) { return value.ptr() != nullptr; }

    /* Identical to the above, but pybind11 expects a method of this name. */
    template <typename T> requires (!impl::python_like<T>)
    static constexpr bool check_(const T& value) { return check(value); }
    template <typename T> requires (impl::python_like<T>)
    static constexpr bool check_(const T& value) { return check(value); }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    /* Default constructor.  Initializes to a null object, which should not be used
    without initialization.  Note that this is one of the only ways in which a null
    pointer can be injected into the bertrand type system, the other being interactions
    with a wrapper that has been moved from. */
    Object() = default;  // TODO: make this protected?

    /* reinterpret_borrow/steal constructors.  The tags themselves are protected and
    only accessible within subclasses of pybind11::object. */
    Object(pybind11::handle h, const borrowed_t& t) : Base(h, t) {}
    Object(pybind11::handle h, const stolen_t& t) : Base(h, t) {}

    /* Copy constructor.  Borrows a reference to an existing python object. */
    Object(const pybind11::object& o) : Base(o) {}

    /* Move constructor.  Steals a reference to a rvalue python object. */
    Object(pybind11::object&& o) : Base(std::move(o)) {}

    /* Convert an accessor into a generic Object. */
    template <typename Policy>
    Object(const detail::accessor<Policy> &a) : Base(pybind11::object(a)) {}

    /* Convert any non-callable C++ value into a generic python object. */
    template <typename T> requires (!impl::python_like<T> && !impl::is_callable_any<T>)
    Object(const T& value) : Base(pybind11::cast(value).release(), stolen_t{}) {}

    /* Convert any callable C++ value into a generic python object. */
    template <typename T> requires (!impl::python_like<T> && impl::is_callable_any<T>)
    Object(const T& value);  // defined in python.h

    using Base::operator=;

    /* Assign any C++ value to the object wrapper. */
    template <typename T> requires (!impl::python_like<T>)
    Object& operator=(T&& value) {
        Base::operator=(Object(std::forward<T>(value)));
        return *this;
    }

    ///////////////////////////
    ////    CONVERSIONS    ////
    ///////////////////////////

    /* NOTE: the Object wrapper can be implicitly converted to any of its subclasses by
     * applying a runtime type check during the assignment.  This allows us to safely
     * convert from a generic object to a more specialized type without worrying about
     * type mismatches or triggering arbitrary conversion logic.  It allows us to
     * write code like this:
     *
     *      py::Object obj = true;
     *      py::Bool b = obj;
     *
     * But not like this:
     *
     *      py::Object obj = true;
     *      py::Str s = obj;  // throws a TypeError
     *
     * While simultaneously preserving the ability to explicitly convert using a normal
     * constructor call:
     *
     *      py::Object obj = true;
     *      py::Str s(obj);
     *
     * Which is identical to calling the `str()` type at the python level.  Note that
     * the implicit conversion operator is only enabled for Object itself, and is
     * explicitly deleted in all of its subclasses.  This prevents implicit conversions
     * between subclasses and promotes any attempt to do so into a compile-time error.
     * For instance:
     *
     *      py::Bool b = true;
     *      py::Str s = b;  // fails to compile, calls a deleted function
     *
     * In general, this makes assignment via the `=` operator type-safe by default,
     * while explicit constructors are reserved for non-trivial conversions and/or
     * packing in the case of containers.
     */

    /* Implicitly convert an Object wrapper to one of its subclasses, applying a
    runtime type check to the underlying value. */
    template <typename T> requires (std::is_base_of_v<Object, T>)
    inline operator T() const {
        if (!T::check(*this)) {
            throw impl::noconvert<T>(this->ptr());
        }
        return reinterpret_borrow<T>(this->ptr());
    }

    /* Implicitly convert an Object to a wrapper class, which moves it into a managed
    buffer for static storage duration, etc. */
    template <typename T> requires (impl::is_wrapper<T>)
    inline operator T() const {
        return T(this->operator typename T::Wrapped());
    }

    /* Explicitly convert to any other non-Object type using pybind11's type casting
    mechanism. */
    template <typename T>
        requires (!impl::is_wrapper<T> && !std::is_base_of_v<Object, T>)
    inline explicit operator T() const {
        return Base::cast<T>();
    }

    /* Contextually convert an Object into a boolean for use in if/else statements,
    with the same semantics as Python. */
    inline explicit operator bool() const {
        int result = PyObject_IsTrue(this->ptr());
        if (result == -1) {
            throw error_already_set();
        }
        return result;
    }

    /* Explicitly cast to a string representation.  For some reason,
    pybind11::cast<std::string>() doesn't always work for arbitrary types.  This
    corrects that and gives the same results as Python `str(obj)`. */
    inline explicit operator std::string() const {
        PyObject* str = PyObject_Str(this->ptr());
        if (str == nullptr) {
            throw error_already_set();
        }
        Py_ssize_t size;
        const char* data = PyUnicode_AsUTF8AndSize(str, &size);
        if (data == nullptr) {
            Py_DECREF(str);
            throw error_already_set();
        }
        std::string result(data, size);
        Py_DECREF(str);
        return result;
    }

    ////////////////////////////////
    ////    ATTRIBUTE ACCESS    ////
    ////////////////////////////////

    inline impl::ObjAttrAccessor attr(Handle key) const {
        return Base::attr(key);
    }

    inline impl::ObjAttrAccessor attr(Handle&& key) const {
        return Base::attr(std::move(key));
    }

    inline impl::StrAttrAccessor attr(const char* key) const {
        return Base::attr(key);
    }

    inline impl::StrAttrAccessor attr(const std::string& key) const {
        return Base::attr(key.c_str());
    }

    inline impl::StrAttrAccessor attr(const std::string_view& key) const {
        return Base::attr(key.data());
    }

    /////////////////////////
    ////    OPERATORS    ////
    /////////////////////////

    inline Object* operator&() { return this; }
    inline const Object* operator&() const { return this; }

    inline auto operator*() { return dereference_impl(*this); }

    template <typename... Args>
    inline auto operator()(Args&&... args) const;  // defined in python.h

    inline size_t size() const { return size_impl(*this); }

    template <typename T>
    inline auto contains(const T& key) const { return contains_impl(*this, key); }

    inline auto begin() const { return begin_impl(*this); }
    inline auto end() const { return end_impl(*this); }
    inline auto rbegin() const { return rbegin_impl(*this); }
    inline auto rend() const { return rend_impl(*this); }

    template <typename T>
    inline impl::ItemAccessor operator[](T&& key) const {
        return getitem_impl(*this, std::forward<T>(key));
    }

};


namespace impl {

    template <>
    struct __dereference__<Object> : Returns<Object> {};  // TODO: return type not considered
    template <typename ... Args>
    struct __call__<Object, Args...> : Returns<Object> {};
    template <>
    struct __len__<Object> : Returns<size_t> {};
    template <typename T>
    struct __contains__<Object, T> : Returns<bool> {};
    template <>
    struct __iter__<Object> : Returns<Object> {};
    template <>
    struct __reversed__<Object> : Returns<Object> {};
    template <typename T>
    struct __getitem__<Object, T> : Returns<Object> {};
    template <typename T>
    struct __setitem__<Object, T, Object> : Returns<void> {};
    template <typename T>
    struct __delitem__<Object, T> : Returns<void> {};
    template <>
    struct __pos__<Object> : impl::Returns<Object> {};
    template <>
    struct __neg__<Object> : impl::Returns<Object> {};
    template <>
    struct __abs__<Object> : impl::Returns<Object> {};
    template <>
    struct __invert__<Object> : impl::Returns<Object> {};
    template <>
    struct __increment__<Object> : impl::Returns<Object> {};
    template <>
    struct __decrement__<Object> : impl::Returns<Object> {};
    template <typename T>
    struct __lt__<Object, T> : impl::Returns<bool> {};
    template <typename T>
    struct __lt__<T, Object> : impl::Returns<bool> {};
    template <typename T>
    struct __le__<Object, T> : impl::Returns<bool> {};
    template <typename T>
    struct __le__<T, Object> : impl::Returns<bool> {};
    template <typename T1, typename T2> requires (std::is_base_of_v<Object, T1>)
    struct __eq__<T1, T2> : impl::Returns<bool> {};
    template <typename T1, typename T2> requires (std::is_base_of_v<Object, T2>)
    struct __eq__<T1, T2> : impl::Returns<bool> {};
    template <typename T1, typename T2> requires (std::is_base_of_v<Object, T1>)
    struct __ne__<T1, T2> : impl::Returns<bool> {};
    template <typename T1, typename T2> requires (std::is_base_of_v<Object, T2>)
    struct __ne__<T1, T2> : impl::Returns<bool> {};
    template <typename T>
    struct __ge__<Object, T> : impl::Returns<bool> {};
    template <typename T>
    struct __ge__<T, Object> : impl::Returns<bool> {};
    template <typename T>
    struct __gt__<Object, T> : impl::Returns<bool> {};
    template <typename T>
    struct __gt__<T, Object> : impl::Returns<bool> {};
    template <typename T>
    struct __add__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __add__<T, Object> : impl::Returns<Object> {};
    template <typename T>
    struct __sub__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __sub__<T, Object> : impl::Returns<Object> {};
    template <typename T>
    struct __mul__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __mul__<T, Object> : impl::Returns<Object> {};
    template <typename T>
    struct __truediv__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __truediv__<T, Object> : impl::Returns<Object> {};
    template <typename T>
    struct __mod__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __mod__<T, Object> : impl::Returns<Object> {};
    template <typename T>
    struct __lshift__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __lshift__<T, Object> : impl::Returns<Object> {};
    template <typename T>
    struct __rshift__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __rshift__<T, Object> : impl::Returns<Object> {};
    template <typename T>
    struct __and__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __and__<T, Object> : impl::Returns<Object> {};
    template <typename T>
    struct __or__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __or__<T, Object> : impl::Returns<Object> {};
    template <typename T>
    struct __xor__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __xor__<T, Object> : impl::Returns<Object> {};
    template <typename T>
    struct __iadd__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __isub__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __imul__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __itruediv__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __imod__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __ilshift__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __irshift__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __iand__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __ior__<Object, T> : impl::Returns<Object> {};
    template <typename T>
    struct __ixor__<Object, T> : impl::Returns<Object> {};


    /* Standardized error message for type narrowing via pybind11 accessors or the
    generic Object wrapper. */
    template <typename Derived>
    TypeError noconvert(PyObject* obj) {
        pybind11::type source = pybind11::type::of(obj);
        pybind11::type dest = Derived::type;
        const char* source_name = reinterpret_cast<PyTypeObject*>(source.ptr())->tp_name;
        const char* dest_name = reinterpret_cast<PyTypeObject*>(dest.ptr())->tp_name;

        std::ostringstream msg;
        msg << "cannot convert python object from type '" << source_name;
        msg << "' to type '" << dest_name << "'";
        return TypeError(msg.str());
    }


    // TODO: static method enable trick does not work.  We have to use CRTP to
    // enable the correct operators.  This is a hard limitation until C++23 deducing
    // `this`.


    // TODO: Proxy has to use CRTP to call the correct dereference operator


    /* Base class for all accessor proxies.  Stores an arbitrary object in a buffer and
    forwards its interface using pointer semantics. */
    template <typename Obj, typename Derived>
    class Proxy {
        mutable alignas (Obj) unsigned char buffer[sizeof(Obj)];
        bool initialized;

        Derived& derived() { return static_cast<Derived&>(*this); }
        const Derived& derived() const { return static_cast<const Derived&>(*this); }

    protected:
        struct alloc {};

        /* Protected constructor for manual initialization. */
        Proxy(const alloc&) : initialized(false) {}

    public:
        using Wrapped = Obj;

        ////////////////////////////
        ////    CONSTRUCTORS    ////
        ////////////////////////////

        /* Default constructor. */
        Proxy() : initialized(true) {
            new (buffer) Obj();
        }

        /* Copy constructor. */
        Proxy(const Proxy& other) : initialized(other.initialized) {
            if (initialized) {
                new (buffer) Obj(reinterpret_cast<Obj&>(other.buffer));
            }
        }

        /* Move constructor. */
        Proxy(Proxy&& other) : initialized(other.initialized) {
            if (initialized) {
                new (buffer) Obj(std::move(reinterpret_cast<Obj&>(other.buffer)));
            }
        }

        /* Forwarding copy constructor for wrapped type. */
        Proxy(const T& value) : initialized(true) {
            new (buffer) Obj(value);
        }

        /* Forwarding move constructor for wrapped type. */
        Proxy(T&& value) : initialized(true) {
            new (buffer) Obj(std::move(value));
        }

        /* Copy assignment operator. */
        Proxy& operator=(const Proxy& other) {
            if (&other != this) {
                if (initialized) {
                    initialized = false;
                    reinterpret_cast<Obj&>(buffer).~Obj();
                }
                if (other.initialized) {
                    new (buffer) Obj(reinterpret_cast<Obj&>(other.buffer));
                    initialized = true;
                }
            }
            return *this;
        }

        /* Move assignment operator. */
        Proxy& operator=(Proxy&& other) {
            if (&other != this) {
                if (initialized) {
                    initialized = false;
                    reinterpret_cast<Obj&>(buffer).~Obj();
                }
                if (other.initialized) {
                    other.initialized = false;
                    new (buffer) Obj(std::move(reinterpret_cast<Obj&>(other.buffer)));
                    initialized = true;
                }
            }
            return *this;
        }

        /* Forwarding copy assignment operator. */
        Proxy& operator=(const Obj& value) {
            if (initialized) {
                initialized = false;
                reinterpret_cast<Obj&>(buffer).~Obj();
            }
            new (buffer) Obj(value);
            initialized = true;
            return *this;
        }

        /* Forwarding move assignment operator. */
        Proxy& operator=(Obj&& value) {
            if (initialized) {
                initialized = false;
                reinterpret_cast<Obj&>(buffer).~Obj();
            }
            new (buffer) Obj(std::move(value));
            initialized = true;
            return *this;
        }

        /* Destructor.  Can be avoided by manually clearing the initialized flag. */
        ~Proxy() {
            if (initialized) {
                reinterpret_cast<Obj&>(buffer).~Obj();
            }
        }

        ////////////////////////////////////
        ////    FORWARDING INTERFACE    ////
        ////////////////////////////////////

        inline operator*() {
            if (!initialized) {
                throw ValueError(
                    "dereferencing an uninitialized accessor.  Either the accessor was "
                    "moved from or not properly constructed to begin with."
                );
            }
            return reinterpret_cast<Obj&>(buffer);
        }

        inline const Obj& operator*() const {
            if (!initialized) {
                throw ValueError(
                    "dereferencing an uninitialized accessor.  Either the accessor was "
                    "moved from or not properly constructed to begin with."
                );
            }
            return reinterpret_cast<Obj&>(buffer);
        }

        inline Obj* operator->() {
            return &(*derived());
        };

        inline const Obj* operator->() const {
            return &(*derived());
        };

        inline operator Obj&() {
            return &(*derived());
        }

        inline operator const Obj&() const {
            return &(*derived());
        }

        inline operator Obj() const {
            return &(*derived());
        }

        template <typename T> requires (std::is_convertible_v<Obj, T>)
        inline operator T() const {
            return implicit_cast<T>(*derived());
        }

        template <typename T> requires (!std::is_convertible_v<Obj, T>)
        inline explicit operator T() const {
            return static_cast<T>(*derived());
        }

        /////////////////////////
        ////    OPERATORS    ////
        /////////////////////////

        template <typename... Args>
        inline auto operator()(Args&&... args) const {
            return (*derived())(std::forward<Args>(args)...);
        }

        template <typename T>
        inline auto operator[](T&& key) const {
            return (*derived())[std::forward<T>(key)];
        }

        inline auto size() const { return (*derived()).size(); }
        inline auto begin() const { return (*derived()).begin(); }
        inline auto end() const { return (*derived()).end(); }
        inline auto rbegin() const { return (*derived()).rbegin(); }
        inline auto rend() const { return (*derived()).rend(); }

        // TODO: operators forwarded to the underlying object

    };


    /* A type-safe accessor class that uses the __getitem__, __setitem__, and
    __delitem__ control structs to promote static type safety. */
    template <typename Obj, typename Key>
    class ItemAccessor :
        public Proxy<typename __getitem__<Obj, Key>::Return, ItemAccessor>
    {
    public:
        using Return = typename __getitem__<Obj, Key>::Return;
        static_assert(
            std::is_base_of_v<Object, Return>,
            "index operator must return a subclass of py::Object.  Check your "
            "specialization of __getitem__ for these types and ensure the Return "
            "type is set to a subclass of py::Object."
        );

    private:
        using Base = Proxy<Return, ItemAccessor>;
        Handle container;
        Object key;

        void get_item() {
            if (container == nullptr) {
                throw ValueError(
                    "dereferencing an uninitialized accessor.  Either the "
                    "accessor was moved from or not properly constructed to begin "
                    "with."
                );
            }
            PyObject* result = PyObject_GetItem(container.ptr(), key.ptr());
            if (result == nullptr) {
                throw error_already_set();
            }
            new (Base::buffer) Return(reinterpret_steal<Return>(result));
            Base::initialized = true;
        }

    public:

        ////////////////////////////
        ////    CONSTRUCTORS    ////
        ////////////////////////////

        ItemAccessor(Handle container, const Key& key) :
            Base(alloc{}), container(container), key(key)
        {}

        ItemAccessor(Handle container, Key&& key) :
            Base(alloc{}), container(container), key(std::move(key))
        {}

        ItemAccessor(const ItemAccessor& other) :
            Base(other), container(other.container), key(other.key)
        {}

        ItemAccessor(ItemAccessor&& other) :
            Base(std::move(other)), container(other.container), key(std::move(other.key))
        {}

        ItemAccessor& operator=(const ItemAccessor& other) {
            if (&other != this) {
                container = other.container;
                key = other.key;
                if (Base::initialized) {
                    Base::initialized = false;
                    reinterpret_cast<Return&>(Base::buffer).~Return();
                }
                if (other.initialized) {
                    new (Base::buffer) Return(reinterpret_cast<Return&>(other.buffer));
                    Base::initialized = true;
                }
            }
            return *this;
        }

        ItemAccessor& operator=(ItemAccessor&& other) {
            if (&other != this) {
                container = other.container;
                key = std::move(other.key);
                if (Base::initialized) {
                    Base::initialized = false;
                    reinterpret_cast<Return&>(Base::buffer).~Return();
                }
                if (other.initialized) {
                    other.initialized = false;
                    new (Base::buffer) Return(
                        std::move(reinterpret_cast<Return&>(other.buffer))
                    );
                    Base::initialized = true;
                }
            }
            return *this;
        }

        ////////////////////////////////////
        ////    FORWARDING INTERFACE    ////
        ////////////////////////////////////

        /* pybind11's item accessors only perform the lookup when the accessor is
         * converted to a value, which we can hook to provide strong type safety.  In
         * this case, the accessor is only convertible to the return type specified by
         * __getitem__, and forwards all other conversions to that type specifically.
         * This allows us to write code like this:
         *
         *      template <>
         *      struct impl::__getitem__<List, Slice> : impl::Returns<List> {};
         *
         *      py::List list = {1, 2, 3, 4};
         *      py::List slice = list[{1, 3}];
         *
         *      void foo(const std::vector<int>& vec) {}
         *      foo(list[{1, 3}]);  // List is implicitly convertible to vector
         *
         * But not like this:
         *
         *      py::Int item = list[{1, 3}];  // compile error, List is not convertible to Int
         */

        inline Return& operator*() {
            if (!Base::initialized) {
                get_item();
            }
            return reinterpret_cast<Return&>(Base::buffer);
        }

        inline const Return& operator*() const {
            if (!Base::initialized) {
                get_item();
            }
            return reinterpret_cast<Return&>(Base::buffer);
        }

        //////////////////////////
        ////    ASSIGNMENT    ////
        //////////////////////////

        /* Similarly, assigning to a pybind11 wrapper corresponds to a Python
         * __setitem__ call, and we can carry strong type safety here as well.  By
         * specializing __setitem__ for the accessor's key type, we can constrain the
         * types that can be assigned to the container, allowing us to enforce
         * compile-time type safety.  We can thus write code like this:
         *
         *      template <impl::list_like Value>
         *      struct impl::__setitem__<List, Slice, Value> : impl::Returns<void> {};
         *
         *      py::List list = {1, 2, 3, 4};
         *      list[{1, 3}] = py::List{5, 6};
         *      list[{1, 3}] = std::vector<int>{7, 8};
         *
         * But not like this:
         *
         *      list[{1, 3}] = 5;  // compile error, int is not list-like
         */

        template <typename T> requires (__setitem__<Obj, Key, T>::enable)
        inline ItemAccessor& operator=(T&& value) {
            static_assert(
                std::is_void_v<typename __setitem__<Obj, Key, T>::Return>,
                "index assignment operator must return void.  Check your "
                "specialization of __setitem__ for these types and ensure the Return "
                "type is set to void."
            );
            new (buffer) Return(std::forward<T>(value));
            initialized = true;
            if (PyObject_SetItem(
                container.ptr(),
                key.ptr(),
                reinterpret_cast<Return&>(buffer).ptr()
            ) < 0) {
                throw error_already_set();
            }
            return *this;
        }

        ////////////////////////
        ////    DELETION    ////
        ////////////////////////

        /* C++'s delete operator is meant for an entirely different purpose than
         * Python `del`, so we can't piggyback off it here.  Instead, we offer a
         * separate `.del()` method that behaves the same way and is only enabled if
         * the __delitem__ struct is specialized for the accessor's key type.
         *
         *      template <impl::int_like T>
         *      struct impl::__delitem__<List, T> : impl::Returns<void> {};
         *      template <>
         *      struct impl::__delitem__<List, Slice> : impl::Returns<void> {};
         *
         *      py::List list = {1, 2, 3, 4};
         *      list1[0].del();  // valid, single items can be deleted
         *      list[{0, 2}].del();  // valid, slices can be deleted
         *
         * If __delitem__ is not specialized for a given key type, the `.del()` method
         * will result in a compile error, giving full control over the types that can
         * be deleted from the container.
         */

        template <typename T> requires (__delitem__<Obj, T>::enable)
        inline void del() {
            static_assert(
                std::is_void_v<typename __delitem__<Obj, T>::Return>,
                "index deletion operator must return void.  Check your specialization "
                "of __delitem__ for these types and ensure the Return type is set to "
                "void."
            );
            if (PyObject_DelItem(container.ptr(), key.ptr()) < 0) {
                throw error_already_set();
            }
            if (initialized) {
                reinterpret_cast<Return&>(buffer).~Return();
                initialized = false;
            }
        }

    };


    /* A type-safe accessor class that allows attributes to be used more consistently
    with bertrand's operator overloads.  This does not (and can not) apply any strict
    typing rules, but it does make using attributes more intuitive in C++. */
    template <typename Obj, typename Key>
    class AttrAccessor : public Proxy<Object, AttrAccessor> {



    };






    #define CONVERTIBLE_ACCESSOR(name, base)                                            \
        struct name : public detail::base {                                             \
            using detail::base::base;                                                   \
            using detail::base::operator=;                                              \
            name(const detail::base& accessor) : detail::base(accessor) {}              \
            name(detail::base&& accessor) : detail::base(std::move(accessor)) {}        \
                                                                                        \
            template <typename... Args>                                                 \
            inline Object operator()(Args&&... args) const;                             \
                                                                                        \
            inline explicit operator bool() const {                                     \
                int result = PyObject_IsTrue(this->ptr());                              \
                if (result == -1) {                                                     \
                    throw error_already_set();                                          \
                }                                                                       \
                return result;                                                          \
            }                                                                           \
                                                                                        \
            inline explicit operator std::string() const {                              \
                PyObject* str = PyObject_Str(this->ptr());                              \
                if (str == nullptr) {                                                   \
                    throw error_already_set();                                          \
                }                                                                       \
                Py_ssize_t size;                                                        \
                const char* data = PyUnicode_AsUTF8AndSize(str, &size);                 \
                if (data == nullptr) {                                                  \
                    Py_DECREF(str);                                                     \
                    throw error_already_set();                                          \
                }                                                                       \
                std::string result(data, size);                                         \
                Py_DECREF(str);                                                         \
                return result;                                                          \
            }                                                                           \
                                                                                        \
            template <typename T> requires (impl::is_wrapper<T>)                        \
            inline operator T() const {                                                 \
                return T(this->operator typename T::Wrapped());                         \
            }                                                                           \
                                                                                        \
            template <typename T>                                                       \
                requires (!impl::is_wrapper<T> && !impl::python_like<T>)                \
            inline operator T() const {                                                 \
                return detail::base::template cast<T>();                                \
            }                                                                           \
                                                                                        \
            template <typename T> requires (std::is_base_of_v<pybind11::object, T>)     \
            inline operator T() const {                                                 \
                pybind11::object other(*this);                                          \
                if (!T::check(other)) {                                                 \
                    throw impl::noconvert<T>(this->ptr());                              \
                }                                                                       \
                return reinterpret_steal<T>(other.release());                           \
            }                                                                           \
        };                                                                              \

    CONVERTIBLE_ACCESSOR(ObjAttrAccessor, obj_attr_accessor)
    CONVERTIBLE_ACCESSOR(StrAttrAccessor, str_attr_accessor)
    CONVERTIBLE_ACCESSOR(ItemAccessor, item_accessor)
    CONVERTIBLE_ACCESSOR(SequenceAccessor, sequence_accessor)
    CONVERTIBLE_ACCESSOR(TupleAccessor, tuple_accessor)
    CONVERTIBLE_ACCESSOR(ListAccessor, list_accessor)

    #undef CONVERTIBLE_ACCESSOR


    /* Mixin holding an optimized reverse iterator for data structures that allow
    direct access to the underlying array.  This avoids the overhead of going through
    the Python interpreter to obtain a reverse iterator, and brings it up to parity
    with forward iteration. */
    template <typename Derived>
    struct ReverseIterable {
        /* An optimized reverse iterator that bypasses the python interpreter. */
        struct ReverseIterator {
            using iterator_category     = std::forward_iterator_tag;
            using difference_type       = std::ptrdiff_t;
            using value_type            = Object;
            using pointer               = value_type*;
            using reference             = value_type&;

            PyObject** array;
            Py_ssize_t index;

            inline ReverseIterator(PyObject** array, Py_ssize_t index) :
                array(array), index(index)
            {}
            inline ReverseIterator(Py_ssize_t index) : array(nullptr), index(index) {}
            inline ReverseIterator(const ReverseIterator&) = default;
            inline ReverseIterator(ReverseIterator&&) = default;
            inline ReverseIterator& operator=(const ReverseIterator&) = default;
            inline ReverseIterator& operator=(ReverseIterator&&) = default;

            inline Handle operator*() const {
                return array[index];
            }

            inline ReverseIterator& operator++() {
                --index;
                return *this;
            }

            inline bool operator==(const ReverseIterator& other) const {
                return index == other.index;
            }

            inline bool operator!=(const ReverseIterator& other) const {
                return index != other.index;
            }
        };
    };

    /* Types for which is_reverse_iterable is true will use custom reverse iterators
    when py::reversed() is called on them.  These types must expose a ReverseIterator
    class that implements the reverse iteration. */
    template <typename T, typename = void>
    constexpr bool is_reverse_iterable = false;
    template <typename T>
    constexpr bool is_reverse_iterable<
        T, std::enable_if_t<std::is_base_of_v<Tuple, T>>
    > = true;
    template <typename T>
    constexpr bool is_reverse_iterable<
        T, std::enable_if_t<std::is_base_of_v<List, T>>
    > = true;


    /* All subclasses of py::Object must define these constructors, which are taken
    directly from PYBIND11_OBJECT_COMMON and cover the basic object creation and
    conversion logic.

    The check() function will be called whenever a generic Object wrapper is implicitly
    converted to this type.  It should return true if and only if the object has a
    compatible type, and it will never be passed a null pointer.  If it returns false,
    a TypeError will be raised during the assignment.

    Also included are generic copy and move constructors that work with any object type
    that is like this one.  It depends on each type implementing the `like` trait
    before invoking this macro.

    Lastly, a generic assignment operator is provided that triggers implicit
    conversions to this type for any inputs that support them.  This is especially
    nice for initializer-list syntax, enabling containers to be assigned to like this:

        py::List list = {1, 2, 3, 4, 5};
        list = {5, 4, 3, 2, 1};
    */
    #define BERTRAND_OBJECT_COMMON(parent, cls, check_func)                             \
        /* Overload check() for C++ values using template metaprogramming. */           \
        template <typename T> requires (!impl::python_like<T>)                          \
        static constexpr bool check(const T&) {                                         \
            return check<T>();                                                          \
        }                                                                               \
                                                                                        \
        /* Overload check() for Python objects using check_func. */                     \
        template <typename T> requires(impl::python_like<T>)                            \
        static constexpr bool check(const T& obj) {                                     \
            return obj.ptr() != nullptr && check_func(obj.ptr());                       \
        }                                                                               \
                                                                                        \
        /* For compatibility with pybind11, which expects these methods. */             \
        template <typename T> requires (!impl::python_like<T>)                          \
        static constexpr bool check_(const T& value) { return check(value); }           \
        template <typename T> requires (impl::python_like<T>)                           \
        static constexpr bool check_(const T& value) { return check(value); }           \
                                                                                        \
        /* Inherit tagged borrow/steal and copy/move constructors. */                   \
        cls(Handle h, const borrowed_t& t) : parent(h, t) {}                            \
        cls(Handle h, const stolen_t& t) : parent(h, t) {}                              \
        cls(const cls& value) : parent(value) {}                                        \
        cls(cls&& value) : parent(std::move(value)) {}                                  \
                                                                                        \
        /* Convert an accessor into this type. */                                       \
        template <typename Policy>                                                      \
        cls(const detail::accessor<Policy> &a) {                                        \
            pybind11::object obj(a);                                                    \
            if (!check(obj)) {                                                          \
                throw impl::noconvert<cls>(obj.ptr());                                  \
            }                                                                           \
            m_ptr = obj.release().ptr();                                                \
        }                                                                               \
                                                                                        \
        /* Trigger implicit conversions to this type via the assignment operator. */    \
        template <typename T> requires (std::is_convertible_v<T, cls>)                  \
        cls& operator=(T&& value) {                                                     \
            if constexpr (std::is_same_v<cls, std::decay_t<T>>) {                       \
                if (this != &value) {                                                   \
                    parent::operator=(std::forward<T>(value));                          \
                }                                                                       \
            } else if constexpr (impl::has_conversion_operator<std::decay_t<T>, cls>) { \
                parent::operator=(value.operator cls());                                \
            } else {                                                                    \
                parent::operator=(cls(std::forward<T>(value)));                         \
            }                                                                           \
            return *this;                                                               \
        }                                                                               \
                                                                                        \
        /* Delete type narrowing operator inherited from Object. */                     \
        template <typename T> requires (std::is_base_of_v<Object, T>)                   \
        operator T() const = delete;                                                    \

}  // namespace impl


////////////////////////
////    SEQUENCE    ////
////////////////////////


/* Mixin holding operator overloads for types implementing the sequence protocol,
which makes them both concatenatable and repeatable. */
class Sequence : public Object {
    using Base = Object;

public:

    template <typename T>
    static constexpr bool check() { return impl::sequence_like<T>; }

    BERTRAND_OBJECT_COMMON(Base, Sequence, PySequence_Check)

    /* Unpack an existing Python sequence into a new tuple or list object. */
    template <impl::python_like T>
    explicit Sequence(const T& value) : Base(
        PySequence_Fast(value.ptr(), "expected a sequence"),
        stolen_t{}
    ) {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
    }

    /* Unpack a C++ sequence into a new Python tuple or list object. */
    template <typename T> requires (!impl::python_like<T> && impl::sequence_like<T>)
    explicit Sequence(const T& value) : Base(PyList_New(value.size())) {
        if (m_ptr == nullptr) {
            throw error_already_set();
        }
        try {
            for (Py_ssize_t i = 0; i < value.size(); ++i) {
                PyList_SET_ITEM(m_ptr, i, Object(value[i]).release().ptr());
            }
        } catch (...) {
            Py_DECREF(m_ptr);
            throw;
        }
    }

    // TODO: retain type-narrowing operator from Object.  In fact, might not need
    // to use BERTRAND_OBJECT_COMMON at all.



    /* Equivalent to Python `sequence.count(value)`, but also takes optional
    start/stop indices similar to `sequence.index()`. */
    template <typename T>
    inline Py_ssize_t count(
        const T& value,
        Py_ssize_t start = 0,
        Py_ssize_t stop = -1
    ) const {
        if (start != 0 || stop != -1) {
            PyObject* slice = PySequence_GetSlice(this->ptr(), start, stop);
            if (slice == nullptr) {
                throw error_already_set();
            }
            Py_ssize_t result = PySequence_Count(
                slice,
                detail::object_or_cast(value).ptr()
            );
            Py_DECREF(slice);
            if (result == -1 && PyErr_Occurred()) {
                throw error_already_set();
            }
            return result;
        } else {
            Py_ssize_t result = PySequence_Count(
                this->ptr(),
                detail::object_or_cast(value).ptr()
            );
            if (result == -1 && PyErr_Occurred()) {
                throw error_already_set();
            }
            return result;
        }
    }

    /* Equivalent to Python `s.index(value[, start[, stop]])`. */
    template <typename T>
    inline Py_ssize_t index(
        const T& value,
        Py_ssize_t start = 0,
        Py_ssize_t stop = -1
    ) const {
        if (start != 0 || stop != -1) {
            PyObject* slice = PySequence_GetSlice(this->ptr(), start, stop);
            if (slice == nullptr) {
                throw error_already_set();
            }
            Py_ssize_t result = PySequence_Index(
                slice,
                detail::object_or_cast(value).ptr()
            );
            Py_DECREF(slice);
            if (result == -1 && PyErr_Occurred()) {
                throw error_already_set();
            }
            return result;
        } else {
            Py_ssize_t result = PySequence_Index(
                this->ptr(),
                detail::object_or_cast(value).ptr()
            );
            if (result == -1 && PyErr_Occurred()) {
                throw error_already_set();
            }
            return result;
        }
    }

    /* Equivalent to Python `sequence + items`. */
    template <typename T>
    inline Object concat(const T& items) const {
        PyObject* result = PySequence_Concat(
            this->ptr(),
            detail::object_or_cast(items).ptr()
        );
        if (result == nullptr) {
            throw error_already_set();
        }
        return reinterpret_steal<Object>(result);
    }

    /* Equivalent to Python `sequence * repetitions`. */
    inline Object repeat(Py_ssize_t repetitions) const {
        PyObject* result = PySequence_Repeat(
            this->ptr(),
            repetitions
        );
        if (result == nullptr) {
            throw error_already_set();
        }
        return reinterpret_steal<Object>(result);
    }

    template <typename T>
    inline Object operator+(const T& items) const {
        return this->concat(items);  // implemented in global operator
    }

    inline Object operator*(Py_ssize_t repetitions) {
        return this->repeat(repetitions);  // implemented in global operator
    }

    friend inline Object operator*(Py_ssize_t repetitions, const SequenceOps& seq) {
        return seq.repeat(repetitions);
    }

    template <typename T>
    inline Object& operator+=(const T& items) {
        PyObject* result = PySequence_InPlaceConcat(
            this->ptr(),
            detail::object_or_cast(items).ptr()
        );
        if (result == nullptr) {
            throw error_already_set();
        }
        if (result == this->ptr()) {
            Py_DECREF(result);
        } else {
            *this = reinterpret_steal<Object>(result);
        }
        return *this;
    }

    inline Object& operator*=(Py_ssize_t repetitions) {
        PyObject* result = PySequence_InPlaceRepeat(this->ptr(), repetitions);
        if (result == nullptr) {
            throw error_already_set();
        }
        if (result == this->ptr()) {
            Py_DECREF(result);
        } else {
            *this = reinterpret_steal<Object>(result);
        }
        return *this;
    }

};


/////////////////////////
////    OPERATORS    ////
/////////////////////////


template <typename T> requires (impl::__pos__<T>::enable)
inline auto operator+(const T& value) {
    using Return = impl::__pos__<T>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "Unary positive operator must return a py::Object subclass.  Check your "
        "specialization of __pos__ for this type and ensure the Return type is set to "
        "a py::Object subclass."
    );
    PyObject* result = PyNumber_Positive(detail::object_or_cast(value).ptr());
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


template <typename T> requires (impl::__neg__<T>::enable)
inline auto operator-(const T& value) {
    using Return = impl::__neg__<T>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "Unary negative operator must return a py::Object subclass.  Check your "
        "specialization of __neg__ for this type and ensure the Return type is set to "
        "a py::Object subclass."
    );
    PyObject* result = PyNumber_Negative(detail::object_or_cast(value).ptr());
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


template <typename T> requires (impl::__invert__<T>::enable)
inline auto operator~(const T& value) {
    using Return = impl::__invert__<T>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "Bitwise NOT operator must return a py::Object subclass.  Check your "
        "specialization of __invert__ for this type and ensure the Return type is set "
        "to a py::Object subclass."
    );
    PyObject* result = PyNumber_Invert(detail::object_or_cast(value).ptr());
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


// template <typename T> requires (impl::__abs__<T>::enable)
// inline auto abs(const T& value) {
//     using Return = impl::__abs__<T>::Return;
//     static_assert(
//         std::is_base_of_v<Object, Return>,
//         "Absolute value operator must return a py::Object subclass.  Check your "
//         "specialization of __abs__ for this type and ensure the Return type is set to "
//         "a py::Object subclass."
//     );
//     PyObject* result = PyNumber_Absolute(detail::object_or_cast(value).ptr());
//     if (result == nullptr) {
//         throw error_already_set();
//     }
//     return reinterpret_steal<Return>(result);
// }


template <typename T> requires (impl::__increment__<T>::enable)
inline T& operator++(T& value) {
    using Return = impl::__increment__<T>::Return;
    static_assert(
        std::is_same_v<Return, T>,
        "Increment operator must return a reference to the derived type.  Check your "
        "specialization of __increment__ for this type and ensure the Return type is "
        "set to the derived type."
    );
    static const pybind11::int_ one = 1;
    PyObject* result = PyNumber_InPlaceAdd(
        detail::object_or_cast(value).ptr(),
        one.ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    }
    if (result == value.ptr()) {
        Py_DECREF(result);
    } else {
        value = reinterpret_steal<Return>(result);
    }
    return value;
}


template <typename T> requires (impl::__increment__<T>::enable)
inline T operator++(T& value, int) {
    using Return = impl::__increment__<T>::Return;
    static_assert(
        std::is_same_v<Return, T>,
        "Increment operator must return a reference to the derived type.  Check your "
        "specialization of __increment__ for this type and ensure the Return type is "
        "set to the derived type."
    );
    Return result = value;
    ++value;
    return result;
}


template <typename T> requires (impl::__decrement__<T>::enable)
inline T& operator--(T& value) {
    using Return = impl::__decrement__<T>::Return;
    static_assert(
        std::is_same_v<Return, T>,
        "Decrement operator must return a reference to the derived type.  Check your "
        "specialization of __decrement__ for this type and ensure the Return type is "
        "set to the derived type."
    );
    static const pybind11::int_ one = 1;
    PyObject* result = PyNumber_InPlaceSubtract(
        detail::object_or_cast(value).ptr(),
        one.ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    }
    if (result == value.ptr()) {
        Py_DECREF(result);
    } else {
        value = reinterpret_steal<Return>(result);
    }
    return value;
}


template <typename T> requires (impl::__decrement__<T>::enable)
inline T operator--(T& value, int) {
    using Return = impl::__decrement__<T>::Return;
    static_assert(
        std::is_same_v<Return, T>,
        "Decrement operator must return a reference to the derived type.  Check your "
        "specialization of __decrement__ for this type and ensure the Return type is "
        "set to the derived type."
    );
    Return result = value;
    --value;
    return result;
}


template <typename L, typename R> requires (impl::__lt__<L, R>::enable)
inline bool operator<(const L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__lt__<L, R>::Return, bool>,
        "Less-than operator must return a boolean value.  Check your "
        "specialization of __lt__ for these types and ensure the Return type is "
        "set to bool."
    );
    int result = PyObject_RichCompareBool(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr(),
        Py_LT
    );
    if (result == -1) {
        throw error_already_set();
    }
    return result;
}


template <typename L, typename R> requires (impl::__le__<L, R>::enable)
inline bool operator<=(const L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__le__<L, R>::Return, bool>,
        "Less-than-or-equal operator must return a boolean value.  Check your "
        "specialization of __le__ for this type and ensure the Return type is "
        "set to bool."
    );
    int result = PyObject_RichCompareBool(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr(),
        Py_LE
    );
    if (result == -1) {
        throw error_already_set();
    }
    return result;
}


// TODO: __eq__ must be disabled by default.  Use a std::is_base_of_v<> overload to
// enable it for all types that derive from Object.
template <typename L, typename R> requires (impl::__eq__<L, R>::enable)
inline bool operator==(const L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__eq__<L, R>::Return, bool>,
        "Equality operator must return a boolean value.  Check your "
        "specialization of __eq__ for this type and ensure the Return type is "
        "set to bool."
    );
    int result = PyObject_RichCompareBool(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr(),
        Py_EQ
    );
    if (result == -1) {
        throw error_already_set();
    }
    return result;
}


// TODO: maybe __ne__ can be inferred from __eq__ or vice versa?
template <typename L, typename R> requires (impl::__ne__<L, R>::enable)
inline bool operator!=(const L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__ne__<L, R>::Return, bool>,
        "Inequality operator must return a boolean value.  Check your "
        "specialization of __ne__ for this type and ensure the Return type is "
        "set to bool."
    );
    int result = PyObject_RichCompareBool(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr(),
        Py_NE
    );
    if (result == -1) {
        throw error_already_set();
    }
    return result;
}


template <typename L, typename R> requires (impl::__ge__<L, R>::enable)
inline bool operator>=(const L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__ge__<L, R>::Return, bool>,
        "Greater-than-or-equal operator must return a boolean value.  Check your "
        "specialization of __ge__ for this type and ensure the Return type is "
        "set to bool."
    );
    int result = PyObject_RichCompareBool(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr(),
        Py_GE
    );
    if (result == -1) {
        throw error_already_set();
    }
    return result;
}


template <typename L, typename R> requires (impl::__gt__<L, R>::enable)
inline bool operator>(const L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__gt__<L, R>::Return, bool>,
        "Greater-than operator must return a boolean value.  Check your "
        "specialization of __gt__ for this type and ensure the Return type is "
        "set to bool."
    );
    int result = PyObject_RichCompareBool(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr(),
        Py_GT
    );
    if (result == -1) {
        throw error_already_set();
    }
    return result;
}


template <typename L, typename R> requires (impl::__add__<L, R>::enable)
inline auto operator+(const L& lhs, const R& rhs) -> impl::__add__<L, R>::Return {
    using Return = impl::__add__<L, R>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "Addition operator must return a py::Object subclass.  Check your "
        "specialization of __add__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_Add(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


template <typename L, typename R> requires (impl::__sub__<L, R>::enable)
inline auto operator-(const L& lhs, const R& rhs) -> impl::__sub__<L, R>::Return {
    using Return = impl::__sub__<L, R>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "Subtraction operator must return a py::Object subclass.  Check your "
        "specialization of __sub__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_Subtract(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


template <typename L, typename R> requires (impl::__mul__<L, R>::enable)
inline auto operator*(const L& lhs, const R& rhs) -> impl::__mul__<L, R>::Return {
    using Return = impl::__mul__<L, R>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "Multiplication operator must return a py::Object subclass.  Check your "
        "specialization of __mul__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_Multiply(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


template <typename L, typename R> requires (impl::__truediv__<L, R>::enable)
inline auto operator/(const L& lhs, const R& rhs) -> impl::__truediv__<L, R>::Return {
    using Return = impl::__truediv__<L, R>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "True division operator must return a py::Object subclass.  Check your "
        "specialization of __truediv__ for this type and ensure the Return type "
        "is derived from py::Object."
    );
    PyObject* result = PyNumber_TrueDivide(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


template <typename L, typename R> requires (impl::__mod__<L, R>::enable)
inline auto operator%(const L& lhs, const R& rhs) -> impl::__mod__<L, R>::Return {
    using Return = impl::__mod__<L, R>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "Modulus operator must return a py::Object subclass.  Check your "
        "specialization of __mod__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_Remainder(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


template <typename L, typename R> requires (impl::__lshift__<L, R>::enable)
inline auto operator<<(const L& lhs, const R& rhs) -> impl::__lshift__<L, R>::Return {
    using Return = impl::__lshift__<L, R>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "Left shift operator must return a py::Object subclass.  Check your "
        "specialization of __lshift__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_Lshift(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


template <typename L, typename R> requires (impl::__rshift__<L, R>::enable)
inline auto operator>>(const L& lhs, const R& rhs) -> impl::__rshift__<L, R>::Return {
    using Return = impl::__rshift__<L, R>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "Right shift operator must return a py::Object subclass.  Check your "
        "specialization of __rshift__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_Rshift(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


template <typename L, typename R> requires (impl::__and__<L, R>::enable)
inline auto operator&(const L& lhs, const R& rhs) -> impl::__and__<L, R>::Return {
    using Return = impl::__and__<L, R>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "Bitwise AND operator must return a py::Object subclass.  Check your "
        "specialization of __and__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_And(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


template <typename L, typename R> requires (impl::__or__<L, R>::enable)
inline auto operator|(const L& lhs, const R& rhs) -> impl::__or__<L, R>::Return {
    using Return = impl::__or__<L, R>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "Bitwise OR operator must return a py::Object subclass.  Check your "
        "specialization of __or__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_Or(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


template <typename L, typename R> requires (impl::__xor__<L, R>::enable)
inline auto operator^(const L& lhs, const R& rhs) -> impl::__xor__<L, R>::Return {
    using Return = impl::__xor__<L, R>::Return;
    static_assert(
        std::is_base_of_v<Object, Return>,
        "Bitwise XOR operator must return a py::Object subclass.  Check your "
        "specialization of __xor__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_Xor(
        detail::object_or_cast(lhs).ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    }
    return reinterpret_steal<Return>(result);
}


template <typename L, typename R>
    requires (impl::__iadd__<L, R>::enable && std::is_base_of_v<Object, L>)
inline L& operator+=(L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__iadd__<L, R>::Return, L>,
        "In-place addition operator must return a py::Object subclass.  Check your "
        "specialization of __iadd__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_InPlaceAdd(
        lhs.ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    } else if (result == lhs.ptr()) {
        Py_DECREF(result);
    } else {
        lhs = reinterpret_steal<L>(result);
    }
    return lhs;
}


template <typename L, typename R>
    requires (impl::__isub__<L, R>::enable && std::is_base_of_v<Object, L>)
inline L& operator-=(L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__isub__<L, R>::Return, L>,
        "In-place subtraction operator must return a py::Object subclass.  Check your "
        "specialization of __isub__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_InPlaceSubtract(
        lhs.ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    } else if (result == lhs.ptr()) {
        Py_DECREF(result);
    } else {
        lhs = reinterpret_steal<L>(result);
    }
    return lhs;
}


template <typename L, typename R>
    requires (impl::__imul__<L, R>::enable && std::is_base_of_v<Object, L>)
inline L& operator*=(L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__imul__<L, R>::Return, L>,
        "In-place multiplication operator must return a py::Object subclass.  Check your "
        "specialization of __imul__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_InPlaceMultiply(
        lhs.ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    } else if (result == lhs.ptr()) {
        Py_DECREF(result);
    } else {
        lhs = reinterpret_steal<L>(result);
    }
    return lhs;
}


template <typename L, typename R>
    requires (impl::__itruediv__<L, R>::enable && std::is_base_of_v<Object, L>)
inline L& operator/=(L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__itruediv__<L, R>::Return, L>,
        "In-place true division operator must return a py::Object subclass.  Check your "
        "specialization of __itruediv__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_InPlaceTrueDivide(
        lhs.ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    } else if (result == lhs.ptr()) {
        Py_DECREF(result);
    } else {
        lhs = reinterpret_steal<L>(result);
    }
    return lhs;
}


template <typename L, typename R>
    requires (impl::__imod__<L, R>::enable && std::is_base_of_v<Object, L>)
inline L& operator%=(L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__imod__<L, R>::Return, L>,
        "In-place modulus operator must return a py::Object subclass.  Check your "
        "specialization of __imod__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_InPlaceRemainder(
        lhs.ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    } else if (result == lhs.ptr()) {
        Py_DECREF(result);
    } else {
        lhs = reinterpret_steal<L>(result);
    }
    return lhs;
}


template <typename L, typename R>
    requires (impl::__ilshift__<L, R>::enable && std::is_base_of_v<Object, L>)
inline L& operator<<=(L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__ilshift__<L, R>::Return, L>,
        "In-place left shift operator must return a py::Object subclass.  Check your "
        "specialization of __ilshift__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_InPlaceLshift(
        lhs.ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    } else if (result == lhs.ptr()) {
        Py_DECREF(result);
    } else {
        lhs = reinterpret_steal<L>(result);
    }
    return lhs;
}


template <typename L, typename R>
    requires (impl::__irshift__<L, R>::enable && std::is_base_of_v<Object, L>)
inline L& operator>>=(L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__irshift__<L, R>::Return, L>,
        "In-place right shift operator must return a py::Object subclass.  Check your "
        "specialization of __irshift__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_InPlaceRshift(
        lhs.ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    } else if (result == lhs.ptr()) {
        Py_DECREF(result);
    } else {
        lhs = reinterpret_steal<L>(result);
    }
    return lhs;
}


template <typename L, typename R>
    requires (impl::__iand__<L, R>::enable && std::is_base_of_v<Object, L>)
inline L& operator&=(L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__iand__<L, R>::Return, L>,
        "In-place bitwise AND operator must return a py::Object subclass.  Check your "
        "specialization of __iand__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_InPlaceAnd(
        lhs.ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    } else if (result == lhs.ptr()) {
        Py_DECREF(result);
    } else {
        lhs = reinterpret_steal<L>(result);
    }
    return lhs;
}


template <typename L, typename R>
    requires (impl::__ior__<L, R>::enable && std::is_base_of_v<Object, L>)
inline L& operator|=(L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__ior__<L, R>::Return, L>,
        "In-place bitwise OR operator must return a py::Object subclass.  Check your "
        "specialization of __ior__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_InPlaceOr(
        lhs.ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    } else if (result == lhs.ptr()) {
        Py_DECREF(result);
    } else {
        lhs = reinterpret_steal<L>(result);
    }
    return lhs;
}


template <typename L, typename R>
    requires (impl::__ixor__<L, R>::enable && std::is_base_of_v<Object, L>)
inline L& operator^=(L& lhs, const R& rhs) {
    static_assert(
        std::is_same_v<typename impl::__ixor__<L, R>::Return, L>,
        "In-place bitwise XOR operator must return a py::Object subclass.  Check your "
        "specialization of __ixor__ for this type and ensure the Return type is "
        "derived from py::Object."
    );
    PyObject* result = PyNumber_InPlaceXor(
        lhs.ptr(),
        detail::object_or_cast(rhs).ptr()
    );
    if (result == nullptr) {
        throw error_already_set();
    } else if (result == lhs.ptr()) {
        Py_DECREF(result);
    } else {
        lhs = reinterpret_steal<L>(result);
    }
    return lhs;
}


/* Stream an Object to obtain its Python `repr()`. */
inline std::ostream& operator<<(std::ostream& os, const Object& obj) {
    PyObject* repr = PyObject_Repr(obj.ptr());
    if (repr == nullptr) {
        throw error_already_set();
    }
    Py_ssize_t size;
    const char* data = PyUnicode_AsUTF8AndSize(repr, &size);
    if (data == nullptr) {
        Py_DECREF(repr);
        throw error_already_set();
    }
    os.write(data, size);
    Py_DECREF(repr);
    return os;
}


///////////////////////////////////
////    TYPE-SAFE OPERATORS    ////
///////////////////////////////////


class NoneType;
namespace impl {

    /* A simple struct that converts a generic C++ object into a Python equivalent in
    its constructor.  This is used in conjunction with std::initializer_list to parse
    mixed-type lists in a type-safe manner. */
    struct Initializer {
        Object first;
        template <typename T> requires (!std::is_same_v<std::decay_t<T>, Handle>)
        Initializer(T&& value) : first(std::forward<T>(value)) {}
    };

    /* An Initializer that explicitly requires a string argument. */
    struct StringInitializer : Initializer {
        template <typename T> requires (impl::str_like<std::decay_t<T>>)
        StringInitializer(T&& value) : Initializer(std::forward<T>(value)) {}
    };

    /* An initializer that explicitly requires an integer or None. */
    struct SliceInitializer : Initializer {
        template <typename T>
            requires (
                impl::int_like<std::decay_t<T>> ||
                std::is_same_v<std::decay_t<T>, NoneType>
            )
        SliceInitializer(T&& value) : Initializer(std::forward<T>(value)) {}
    };

    /* An Initializer that converts its argument to a python object and asserts that it
    is hashable, for static analysis. */
    struct HashInitializer : Initializer {
        template <typename K>
            requires (
                !std::is_same_v<std::decay_t<K>, Handle> &&
                impl::is_hashable<std::decay_t<K>>
            )
        HashInitializer(K&& key) : Initializer(std::forward<K>(key)) {}
    };

    /* A hashed Initializer that also stores a second item for dict-like access. */
    struct DictInitializer : Initializer {
        Object second;
        template <typename K, typename V>
            requires (
                !std::is_same_v<std::decay_t<K>, Handle> &&
                !std::is_same_v<std::decay_t<V>, Handle> &&
                impl::is_hashable<std::decay_t<K>>
            )
        DictInitializer(K&& key, V&& value) :
            Initializer(std::forward<K>(key)), second(std::forward<V>(value))
        {}
    };

    template <typename T>
    constexpr bool is_initializer = std::is_base_of_v<Initializer, T>;

    /* A mixin class for transparent Object<> wrappers that forwards the basic interface. */
    template <typename T>
    class Wrapper : WrapperTag {
        static_assert(
            std::is_base_of_v<pybind11::object, T>,
            "Wrapper<T> requires T to be a subclass of pybind11::object"
        );

    protected:
        mutable alignas (T) unsigned char buffer[sizeof(T)];
        bool initialized;

        struct alloc_t {};
        Wrapper(const alloc_t&) : initialized(false) {}

    public:
        using Wrapped = T;

        /* Explicitly create an empty wrapper with uninitialized memory. */
        inline static Wrapper alloc() {
            return Wrapper(alloc_t{});
        }

        ////////////////////////////
        ////    CONSTRUCTORS    ////
        ////////////////////////////

        /* Default constructor. */
        Wrapper() : initialized(true) {
            new (buffer) T();
        }

        /* Copy constructor. */
        Wrapper(const Wrapper& other) : initialized(true) {
            new (buffer) T(*other);
        }

        /* Move constructor. */
        Wrapper(Wrapper&& other) : initialized(true) {
            new (buffer) T(std::move(*other));
        }

        /* Forwarding copy constructor for wrapped type. */
        Wrapper(const T& other) : initialized(true) {
            new (buffer) T(other);
        }

        /* Forwarding move constructor for wrapped type. */
        Wrapper(T&& other) : initialized(true) {
            new (buffer) T(std::move(other));
        }

        /* Forwarding assignment operator. */
        template <typename U>
        Wrapper& operator=(U&& other) {
            **this = std::forward<U>(other);
            return *this;
        }

        /* Copy assignment operator. */
        Wrapper& operator=(const Wrapper& other) {
            if (&other != this) {
                bool old_initialized = initialized;
                bool new_initialized = other.initialized;
                T& old_wrapped = **this;
                const T& new_wrapped = *other;

                initialized = false;  // prevent this dereference during copy
                if (old_initialized) {
                    old_wrapped.~T();
                }

                if (new_initialized) {
                    new (buffer) T(new_wrapped);
                    initialized = true;  // allow this dereference
                }
            }
            return *this;
        }

        /* Move assignment operator. */
        Wrapper& operator=(Wrapper&& other) {
            if (&other != this) {
                bool old_initialized = initialized;
                bool new_initialized = other.initialized;
                T& old_wrapped = **this;
                T& new_wrapped = *other;

                initialized = false;  // prevent this dereference during move
                if (old_initialized) {
                    old_wrapped.~T();
                }

                if (new_initialized) {
                    other.initialized = false;  // prevent other dereference
                    new (buffer) T(std::move(new_wrapped));
                    initialized = true;  // allow this dereference
                }
            }
            return *this;
        }

        /* Destructor.  Subclasses can avoid calling this by manually clearing the
        `initialized` flag in their own destructor. */
        ~Wrapper() {
            if (initialized) {
                (**this).~T();
            }
        }

        /* Implicitly convert to the wrapped type. */
        inline operator T&() {
            return **this;
        }

        /* Implicitly convert to the wrapped type. */
        inline operator const T&() const {
            return **this;
        }

        /* Implicitly convert to the wrapped type. */
        inline operator T() {
            return **this;
        }
        /* Forward all other implicit conversions to the wrapped type. */
        template <typename U>
            requires (!std::is_same_v<T, U> && std::is_convertible_v<T, U>)
        inline operator U() const {
            return impl::implicit_cast<U>(**this);
        }

        /* Forward all other explicit conversions to the wrapped type. */
        template <typename U>
            requires (!std::is_same_v<T, U> && !std::is_convertible_v<T, U>)
        inline explicit operator U() const {
            return static_cast<U>(**this);
        }

        ////////////////////////////////////
        ////    FORWARDING INTERFACE    ////
        ////////////////////////////////////

        /* Wrappers can be used more or less just like a regular object, except that any
        * instance of the dot operator (.) should be replaced with the arrow operator (->)
        * instead.  This allows them to be treated like a pointer conceptually, except all
        * of the ordinary object operators are also forwarded at the same time.
        */

        /* Dereference to get the underlying object. */
        inline T& operator*() {
            if (initialized) {
                return reinterpret_cast<T&>(buffer);
            } else {
                throw ValueError(
                    "dereferencing an uninitialized wrapper.  Either the object was moved "
                    "from or not properly constructed to begin with."
                );
            }
        }

        /* Dereference to get the underlying object. */
        inline const T& operator*() const {
            if (initialized) {
                return reinterpret_cast<const T&>(buffer);
            } else {
                throw ValueError(
                    "dereferencing an uninitialized wrapper.  Either the object was moved "
                    "from or not properly constructed to begin with."
                );
            }
        }

        /* Use the arrow operator to access an attribute on the object. */
        inline T* operator->() {
            return &(**this);
        }

        /* Use the arrow operator to access an attribute on the object. */
        inline const T* operator->() const {
            return &(**this);
        }

        /* Forward to the object's call operator. */
        template <typename... Args>
        inline auto operator()(Args&&... args) const {
            return (**this)(std::forward<Args>(args)...);
        }

        /* Forward to the object's index operator. */
        template <typename U>
        inline auto operator[](U&& key) const {
            return (**this)[std::forward<U>(key)];
        }

        /* Forward to the object's iterator methods. */
        inline auto begin() const { return (**this).begin(); }
        inline auto end() const { return (**this).end(); }

        /////////////////////////
        ////    OPERATORS    ////
        /////////////////////////

        // TODO: include these in global operator overloads, rather than relisting
        // them here?

        /* We also have to forward all math operators for consistent behavior. */

        #define UNARY_OPERATOR(opcode, op)                                              \
            inline auto opcode() const { return op(**this); }                           \

        #define BINARY_OPERATOR(opcode, op)                                             \
            template <typename U>                                                       \
            inline auto opcode(const U& other) const { return (**this) op other; }      \

        #define REVERSE_OPERATOR(opcode, op)                                            \
            template <typename U>                                                       \
                requires (                                                              \
                    !std::is_base_of_v<Object, U> &&                                    \
                    !std::is_base_of_v<std::ostream, U> &&                              \
                    !impl::is_std_iterator<U>                                           \
                )                                                                       \
            inline friend auto opcode(const U& other, const Wrapper& self) {            \
                return other op *self;                                                  \
            }                                                                           \

        #define INPLACE_OPERATOR(opcode, op)                                            \
            template <typename U>                                                       \
            inline Wrapper& opcode(const U& other) {                                    \
                **this op other;                                                        \
                return *this;                                                           \
            }                                                                           \

        UNARY_OPERATOR(operator~, ~)
        UNARY_OPERATOR(operator+, +)
        UNARY_OPERATOR(operator-, -)
        BINARY_OPERATOR(operator<, <)
        BINARY_OPERATOR(operator<=, <=)
        BINARY_OPERATOR(operator==, ==)
        BINARY_OPERATOR(operator!=, !=)
        BINARY_OPERATOR(operator>=, >=)
        BINARY_OPERATOR(operator>, >)
        REVERSE_OPERATOR(operator<, <)
        REVERSE_OPERATOR(operator<=, <=)
        REVERSE_OPERATOR(operator==, ==)
        REVERSE_OPERATOR(operator!=, !=)
        REVERSE_OPERATOR(operator>=, >=)
        REVERSE_OPERATOR(operator>, >)
        BINARY_OPERATOR(operator+, +)
        BINARY_OPERATOR(operator-, -)
        BINARY_OPERATOR(operator*, *)
        BINARY_OPERATOR(operator/, /)
        BINARY_OPERATOR(operator%, %)
        BINARY_OPERATOR(operator<<, <<)
        BINARY_OPERATOR(operator>>, >>)
        BINARY_OPERATOR(operator&, &)
        BINARY_OPERATOR(operator|, |)
        BINARY_OPERATOR(operator^, ^)
        REVERSE_OPERATOR(operator+, +)
        REVERSE_OPERATOR(operator-, -)
        REVERSE_OPERATOR(operator*, *)
        REVERSE_OPERATOR(operator/, /)
        REVERSE_OPERATOR(operator%, %)
        REVERSE_OPERATOR(operator<<, <<)
        REVERSE_OPERATOR(operator>>, >>)
        REVERSE_OPERATOR(operator&, &)
        REVERSE_OPERATOR(operator|, |)
        REVERSE_OPERATOR(operator^, ^)
        INPLACE_OPERATOR(operator+=, +=)
        INPLACE_OPERATOR(operator-=, -=)
        INPLACE_OPERATOR(operator*=, *=)
        INPLACE_OPERATOR(operator/=, /=)
        INPLACE_OPERATOR(operator%=, %=)
        INPLACE_OPERATOR(operator<<=, <<=)
        INPLACE_OPERATOR(operator>>=, >>=)
        INPLACE_OPERATOR(operator&=, &=)
        INPLACE_OPERATOR(operator|=, |=)
        INPLACE_OPERATOR(operator^=, ^=)

        #undef UNARY_OPERATOR
        #undef BINARY_OPERATOR
        #undef REVERSE_OPERATOR
        #undef INPLACE_OPERATOR

        inline Wrapper* operator&() {
            return this;
        }

        inline const Wrapper* operator&() const {
            return this;
        }

    };

}  // namespace impl


//////////////////////////////////
////    OPERATOR OVERLOADS    ////
//////////////////////////////////


/* Forward to the object's stream insertion operator.  Equivalent to Python
`repr(obj)`. */
template <typename T>
inline std::ostream& operator<<(std::ostream& os, const impl::Wrapper<T>& obj) {
    os << *obj;
    return os;
}


/* A lightweight proxy that allows an arbitrary Python object to be stored with static
duration.

Normally, storing a static Python object is unsafe because it can lead to a situation
where the Python interpreter is in an invalid state at the time the object's destructor
is called, causing a memory access violation during shutdown.  This class avoids that
by checking `Py_IsInitialized()` and only invoking the destructor if it evaluates to
true.  This technically means that we leave an unbalanced reference to the object, but
since the Python interpreter is shutting down anyways, it doesn't matter.  Python will
clean up the object regardless of its reference count. */
template <typename T>
struct Static : public impl::Wrapper<T> {
    using impl::Wrapper<T>::Wrapper;
    using impl::Wrapper<T>::operator=;

    /* Explicitly create an empty wrapper with uninitialized memory. */
    inline static Static alloc() {
        return Static(typename impl::Wrapper<T>::alloc_t{});
    }

    /* Destructor avoids calling the Wrapper's destructor if the Python interpreter is
    not currently initialized. */
    ~Static() {
        this->initialized &= Py_IsInitialized();
    }

};



// TODO: these classes can go immediately after Object?


/* Object subclass that represents Python's global None singleton. */
class NoneType : public Object {
    using Base = Object;

public:
    static Type type;

    BERTRAND_OBJECT_COMMON(Base, NoneType, Py_IsNone)
    NoneType() : Base(Py_None, borrowed_t{}) {}
};


/* Object subclass that represents Python's global NotImplemented singleton. */
class NotImplementedType : public Object {
    using Base = Object;

    inline static int check_not_implemented(PyObject* obj) {
        int result = PyObject_IsInstance(obj, (PyObject*) Py_TYPE(Py_NotImplemented));
        if (result == -1) {
            throw error_already_set();
        }
        return result;
    }

public:
    static Type type;

    BERTRAND_OBJECT_COMMON(Base, NotImplementedType, check_not_implemented)
    NotImplementedType() : Base(Py_NotImplemented, borrowed_t{}) {}
};


/* Object subclass representing Python's global Ellipsis singleton. */
class EllipsisType : public Object {
    using Base = Object;

    inline static int check_ellipsis(PyObject* obj) {
        int result = PyObject_IsInstance(obj, (PyObject*) Py_TYPE(Py_Ellipsis));
        if (result == -1) {
            throw error_already_set();
        }
        return result;
    }

public:
    static Type type;

    BERTRAND_OBJECT_COMMON(Base, EllipsisType, check_ellipsis)
    EllipsisType() : Base(Py_Ellipsis, borrowed_t{}) {}
};


/* Singletons for immortal Python objects. */
static const NoneType None;
static const EllipsisType Ellipsis;
static const NotImplementedType NotImplemented;


/* Object subclass that represents an imported Python module. */
class Module : public Object {
    using Base = Object;

public:
    static Type type;

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    BERTRAND_OBJECT_COMMON(Base, Module, PyModule_Check)

    /* Default module constructor deleted for clarity. */
    Module() = delete;

    /* Explicitly create a new module object from a statically-allocated (but
    uninitialized) PyModuleDef struct. */
    explicit Module(const char* name, const char* doc, PyModuleDef* def) {
        def = new (def) PyModuleDef{
            /* m_base */ PyModuleDef_HEAD_INIT,
            /* m_name */ name,
            /* m_doc */ pybind11::options::show_user_defined_docstrings() ? doc : nullptr,
            /* m_size */ -1,
            /* m_methods */ nullptr,
            /* m_slots */ nullptr,
            /* m_traverse */ nullptr,
            /* m_clear */ nullptr,
            /* m_free */ nullptr
        };
        m_ptr = PyModule_Create(def);
        if (m_ptr == nullptr) {
            if (PyErr_Occurred()) {
                throw error_already_set();
            }
            pybind11::pybind11_fail(
                "Internal error in pybind11::module_::create_extension_module()"
            );
        }
    }

    //////////////////////////////////
    ////    PYBIND11 INTERFACE    ////
    //////////////////////////////////

    /* Equivalent to pybind11::module_::def(). */
    template <typename Func, typename... Extra>
    Module& def(const char* name_, Func&& f, const Extra&... extra) {
        pybind11::cpp_function func(
            std::forward<Func>(f),
            pybind11::name(name_),
            pybind11::scope(*this),
            pybind11::sibling(getattr(*this, name_, None)),
            extra...
        );
        // NB: allow overwriting here because cpp_function sets up a chain with the
        // intention of overwriting (and has already checked internally that it isn't
        // overwriting non-functions).
        add_object(name_, func, true /* overwrite */);
        return *this;
    }

    /* Equivalent to pybind11::module_::def_submodule(). */
    Module def_submodule(const char* name, const char* doc = nullptr) {
        const char* this_name = PyModule_GetName(m_ptr);
        if (this_name == nullptr) {
            throw error_already_set();
        }
        std::string full_name = std::string(this_name) + '.' + name;
        Handle submodule = PyImport_AddModule(full_name.c_str());
        if (!submodule) {
            throw error_already_set();
        }
        auto result = reinterpret_borrow<Module>(submodule);
        if (doc && pybind11::options::show_user_defined_docstrings()) {
            result.attr("__doc__") = pybind11::str(doc);
        }
        attr(name) = result;
        return result;
    }

    /* Reload the module or throws `error_already_set`. */
    inline void reload() {
        PyObject *obj = PyImport_ReloadModule(this->ptr());
        if (obj == nullptr) {
            throw error_already_set();
        }
        *this = reinterpret_steal<Module>(obj);
    }

    /* Equivalent to pybind11::module_::add_object(). */
    PYBIND11_NOINLINE void add_object(
        const char* name,
        Handle obj,
        bool overwrite = false
    ) {
        if (!overwrite && pybind11::hasattr(*this, name)) {
            pybind11::pybind11_fail(
                "Error during initialization: multiple incompatible definitions with name \""
                + std::string(name) + "\"");
        }
        PyModule_AddObjectRef(ptr(), name, obj.ptr());
    }

    // TODO: is create_extension_module() necessary?  It might be called by internal
    // pybind11 code, but I'm not sure.

    /* Equivalent to pybind11::module_::create_extension_module() */
    static Module create_extension_module(
        const char* name,
        const char* doc,
        PyModuleDef* def
    ) {
        return Module(name, doc, def);
    }

};


inline impl::ItemAccessor Object::operator[](
    const std::initializer_list<impl::SliceInitializer>& slice
) const {
    if (slice.size() > 3) {
        throw ValueError("slices must be of the form {[start[, stop[, step]]]}");
    }
    size_t i = 0;
    std::array<Object, 3> params {None, None, None};
    for (const impl::SliceInitializer& item : slice) {
        params[i++] = item.first;
    }
    return Base::operator[](pybind11::slice(params[0], params[1], params[2]));
}


////////////////////////////////
////    GLOBAL FUNCTIONS    ////
////////////////////////////////


/* Equivalent to Python `import module` */
inline Static<Module> import(const char* name) {
    if (Py_IsInitialized()) {
        PyObject *obj = PyImport_ImportModule(name);
        if (obj == nullptr) {
            throw error_already_set();
        }
        return Static<Module>(reinterpret_steal<Module>(obj));
    } else {
        return Static<Module>::alloc();  // return an empty wrapper
    }
}


/* Equivalent to Python `iter(obj)` except that it can also accept C++ containers and
generate Python iterators over them.  Note that C++ types as rvalues are not allowed,
and will trigger a compiler error. */
template <impl::is_iterable T>
inline Iterator iter(T&& obj) {
    if constexpr (impl::pybind11_iterable<std::decay_t<T>>) {
        return pybind11::iter(obj);
    } else {
        static_assert(
            !std::is_rvalue_reference_v<decltype(obj)>,
            "passing an rvalue to py::iter() is unsafe"
        );
        return pybind11::make_iterator(obj.begin(), obj.end());
    }
}


/* Equivalent to Python `len(obj)`, but also accepts C++ types implementing a .size()
method.  Returns nullopt if the size could not be determined. */
template <typename T>
inline std::optional<size_t> len(const T& obj) {
    if constexpr (impl::python_like<T>) {
        try {
            return pybind11::len(obj);
        } catch (...) {
            return std::nullopt;
        }
    } else if constexpr (impl::has_size<T>) {
        return obj.size();
    } else {
        return std::nullopt;
    }
}


/* Equivalent to Python `repr(obj)`, but returns a std::string and attempts to
represent C++ types using std::to_string or the stream insertion operator (<<).  If all
else fails, falls back to typeid(obj).name(). */
template <typename T>
inline std::string repr(const T& obj) {
    if constexpr (impl::has_stream_insertion<T>) {
        std::ostringstream stream;
        stream << obj;
        return stream.str();

    } else if constexpr (impl::has_to_string<T>) {
        return std::to_string(obj);

    } else {
        try {
            return pybind11::repr(obj).template cast<std::string>();
        } catch (...) {
            return typeid(obj).name();
        }
    }
}


}  // namespace py
}  // namespace bertrand


//////////////////////////////
////    STL EXTENSIONS    ////
//////////////////////////////


/* Bertrand overloads std::hash<> for all Python objects hashable so that they can be
 * used in STL containers like std::unordered_map and std::unordered_set.  This is
 * accomplished by overloading std::hash<> for the relevant types, which also allows
 * us to promote hash-not-implemented errors to compile-time.
 */


#define BERTRAND_STD_HASH(cls)                                                          \
namespace std {                                                                         \
    template <>                                                                         \
    struct hash<cls> {                                                                  \
        size_t operator()(const cls& obj) const {                                       \
            return pybind11::hash(obj);                                                 \
        }                                                                               \
    };                                                                                  \
}                                                                                       \


BERTRAND_STD_HASH(bertrand::py::Buffer)
BERTRAND_STD_HASH(bertrand::py::Bytearray)
BERTRAND_STD_HASH(bertrand::py::Bytes)
BERTRAND_STD_HASH(bertrand::py::Capsule)
BERTRAND_STD_HASH(bertrand::py::EllipsisType)
BERTRAND_STD_HASH(bertrand::py::Handle)
BERTRAND_STD_HASH(bertrand::py::Iterator)
BERTRAND_STD_HASH(bertrand::py::MemoryView)
BERTRAND_STD_HASH(bertrand::py::Module)
BERTRAND_STD_HASH(bertrand::py::NoneType)
BERTRAND_STD_HASH(bertrand::py::NotImplementedType)
BERTRAND_STD_HASH(bertrand::py::Object)
BERTRAND_STD_HASH(bertrand::py::WeakRef)


#define BERTRAND_STD_EQUAL_TO(cls)                                                      \
namespace std {                                                                         \
    template <>                                                                         \
    struct equal_to<cls> {                                                              \
        bool operator()(const cls& a, const cls& b) const {                             \
            return a.equal(b);                                                          \
        }                                                                               \
    };                                                                                  \
}                                                                                       \


BERTRAND_STD_EQUAL_TO(bertrand::py::Handle)
BERTRAND_STD_EQUAL_TO(bertrand::py::Iterator)
BERTRAND_STD_EQUAL_TO(bertrand::py::WeakRef)
BERTRAND_STD_EQUAL_TO(bertrand::py::Capsule)
BERTRAND_STD_EQUAL_TO(bertrand::py::Buffer)
BERTRAND_STD_EQUAL_TO(bertrand::py::MemoryView)
BERTRAND_STD_EQUAL_TO(bertrand::py::Bytes)
BERTRAND_STD_EQUAL_TO(bertrand::py::Bytearray)


#endif // BERTRAND_PYTHON_COMMON_H

#if !defined(BERTRAND_PYTHON_INCLUDED) && !defined(LINTER)
#error "This file should not be included directly.  Please include <bertrand/python.h> instead."
#endif

#ifndef BERTRAND_PYTHON_FUNC_H
#define BERTRAND_PYTHON_FUNC_H

#include <fstream>

#include "common.h"
#include "dict.h"
#include "str.h"
#include "tuple.h"
#include "list.h"
#include "type.h"


#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#include <cstdlib>
#elif defined(_MSC_VER)
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif


namespace bertrand {
namespace py {


////////////////////
////    CODE    ////
////////////////////


template <std::derived_from<Code> T>
struct __getattr__<T, "co_name">                                : Returns<Str> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_qualname">                            : Returns<Str> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_argcount">                            : Returns<Int> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_posonlyargcount">                     : Returns<Int> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_kwonlyargcount">                      : Returns<Int> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_nlocals">                             : Returns<Int> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_varnames">                            : Returns<Tuple<Str>> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_cellvars">                            : Returns<Tuple<Str>> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_freevars">                            : Returns<Tuple<Str>> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_code">                                : Returns<Bytes> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_consts">                              : Returns<Tuple<Object>> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_names">                               : Returns<Tuple<Str>> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_filename">                            : Returns<Str> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_firstlineno">                         : Returns<Int> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_stacksize">                           : Returns<Int> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_flags">                               : Returns<Int> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_positions">                           : Returns<Function<
    Object()
>> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_lines">                               : Returns<Function<
    Object()
>> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "replace">                                : Returns<Function<
    Object(typename Arg<"kwargs", const Object&>::kwargs)
>> {};


/* A new subclass of pybind11::object that represents a compiled Python code object,
enabling seamless embedding of Python as a scripting language within C++.

This class is extremely powerful, and is best explained by example:

    // in source.py
    import numpy as np
    print(np.arange(10))

    // in main.cpp
    static const py::Code script("source.py");
    script();  // prints [0 1 2 3 4 5 6 7 8 9]

.. note::

    Note that the script in this example is stored in a separate file, which can
    contain arbitrary Python source code.  The file is read and compiled into an
    interactive code object with static storage duration, which is cached for the
    duration of the program.

This creates an embedded Python script that can be executed like a normal function.
Here, the script is stateless, and can be executed without context.  Most of the time,
this won't be the case, and data will need to be passed into the script to populate its
namespace.  For instance:

    static const py::Code script = R"(
        print("Hello, " + name + "!")  # name is not defined in this context
    )"_python;

.. note::

    Note the user-defined `_python` literal used to create the script.  This is
    equivalent to calling the `Code` constructor on a separate file, but allows the
    script to be written directly within the C++ source code.  The same effect can be
    achieved via the `Code::compile()` helper method if the `py::literals` namespace is
    not available. 

If we try to execute this script without a context, we'll get a ``NameError`` just
like normal Python:

    script();  // NameError: name 'name' is not defined

We can solve this by building a context dictionary and passing it into the script as
its global namespace.

    script({{"name", "World"}});  // prints Hello, World!

This uses the ordinary py::Dict constructors, which can take arbitrary C++ objects and
pass them seamlessly to Python.  If we want to do the opposite and extract data from
the script back to C++, then we can use its return value, which is another dictionary
containing the context after execution.  For instance:

    py::Dict context = R"(
        x = 1
        y = 2
        z = 3
    )"_python();

    py::print(context);  // prints {"x": 1, "y": 2, "z": 3}

Combining these features allows us to create a two-way data pipeline between C++ and
Python:

    py::Int z = R"(
        def func(x, y):
            return x + y

        z = func(a, b)
    )"_python({{"a", 1}, {"b", 2}})["z"];

    py::print(z);  // prints 3

In this example, data originates in C++, passes through python for processing, and then
returns smoothly to C++ with automatic error propagation, reference counting, and type
conversions at every step.

In the previous example, the input dictionary exists only for the duration of the
script's execution, and is discarded immediately afterwards.  However, it is also
possible to pass a mutable reference to an external dictionary, which will be updated
in-place as the script executes.  This allows multiple scripts to be chained using a
shared context, without ever leaving the Python interpreter.  For instance:

    static const py::Code script1 = R"(
        x = 1
        y = 2
    )"_python;

    static const py::Code script2 = R"(
        z = x + y
        del x, y
    )"_python;

    py::Dict context;
    script1(context);
    script2(context);
    py::print(context);  // prints {"z": 3}

Users can, of course, inspect or modify the context between scripts, either to extract
results or pass new data into the next script in the chain.  This makes it possible to
create arbitrarily complex, mixed-language workflows with minimal fuss.

    py::Dict context = R"(
        spam = 0
        eggs = 1
    )"_python();

    context["ham"] = std::vector<int>{1, 1, 2, 3, 5, 8, 13, 21, 34, 55};

    std::vector<int> fibonacci = R"(
        result = []
        for x in ham:
            spam, eggs = (spam + eggs, spam)
            assert(x == spam)
            result.append(eggs)
    )"_python(context)["result"];

    py::print(fibonacci);  // prints [0, 1, 1, 2, 3, 5, 8, 13, 21, 34]

This means that Python can be easily included as an inline scripting language in any
C++ application, with minimal overhead and full compatibility in both directions.  Each
script is evaluated just like an ordinary Python file, and there are no restrictions on
what can be done inside them.  This includes importing modules, defining classes and
functions to be exported back to C++, interacting with the file system, third-party
libraries, client code, and more.  Similarly, it is executed just like normal Python
bytecode, and should not suffer any significant performance penalties beyond copying
data into or out of the context.  This is especially true for static code objects,
which are compiled once and then cached for repeated use.

    static const py::Code script = R"(
        print(x)
    )"_python;

    script({{"x", "hello"}});
    script({{"x", "from"}});
    script({{"x", "the"}});
    script({{"x", "other"}});
    script({{"x", "side"}});
*/
class Code : public Object {
    using Base = Object;

    PyCodeObject* self() const {
        return reinterpret_cast<PyCodeObject*>(this->ptr());
    }

    template <typename T>
    static PyObject* build(const T& text) {
        std::string line;
        std::string parsed;
        std::istringstream stream(text);
        size_t min_indent = std::numeric_limits<size_t>::max();

        // find minimum indentation
        while (std::getline(stream, line)) {
            if (line.empty()) {
                continue;
            }
            size_t indent = line.find_first_not_of(" \t");
            if (indent != std::string::npos) {
                min_indent = std::min(min_indent, indent);
            }
        }

        // dedent if necessary
        if (min_indent != std::numeric_limits<size_t>::max()) {
            std::string temp;
            std::istringstream stream2(text);
            while (std::getline(stream2, line)) {
                if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
                    temp += '\n';
                } else {
                    temp += line.substr(min_indent) + '\n';
                }
            }
            parsed = temp;
        } else {
            parsed = text;
        }

        PyObject* result = Py_CompileString(
            parsed.c_str(),
            "<embedded Python script>",
            Py_file_input
        );
        if (result == nullptr) {
            Exception::from_python();
        }
        return result;
    }

    static PyObject* load(const char* path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw FileNotFoundError(std::string("'") + path + "'");
        }
        std::istreambuf_iterator<char> begin(file), end;
        PyObject* result = Py_CompileString(
            std::string(begin, end).c_str(),
            path,
            Py_file_input
        );
        if (result == nullptr) {
            Exception::from_python();
        }
        return result;
    }

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return std::derived_from<T, Code>;
    }

    template <typename T>
    static constexpr bool check(const T& obj) {
        if constexpr (impl::cpp_like<T>) {
            return check<T>();
        } else if constexpr (check<T>()) {
            return obj.ptr() != nullptr;
        } else if constexpr (impl::python_like<T>) {
            return obj.ptr() != nullptr && PyCode_Check(obj.ptr());
        } else {
            return false;
        }
    }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    Code(Handle h, const borrowed_t& t) : Base(h, t) {}
    Code(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    Code(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    Code(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<Code>(accessor).release(), stolen_t{})
    {}

    /* Compile a Python source file into an interactive code object. */
    explicit Code(const char* path) : Base(load(path), stolen_t{}) {}

    /* Compile a Python source file into an interactive code object. */
    explicit Code(const std::string& path) : Code(path.c_str()) {}

    /* Compile a Python source file into an interactive code object. */
    explicit Code(const std::string_view& path) : Code(path.data()) {}

    /* Parse and compile a source string into a Python code object. */
    static Code compile(const char* source) {
        return reinterpret_steal<Code>(build(source));
    }

    /* Parse and compile a source string into a Python code object. */
    static Code compile(const std::string& source) {
        return reinterpret_steal<Code>(build(source));
    }

    /* Parse and compile a source string into a Python code object. */
    static Code compile(const std::string_view& path) {
        return compile(std::string{path.data(), path.size()});
    }

    /////////////////////////////
    ////    C++ INTERFACE    ////
    /////////////////////////////

    /* Execute the code object without context. */
    BERTRAND_NOINLINE Dict<Str, Object> operator()() const {
        py::Dict<Str, Object> context;
        PyObject* result = PyEval_EvalCode(this->ptr(), context.ptr(), context.ptr());
        if (result == nullptr) {
            Exception::from_python(1);
        }
        Py_DECREF(result);  // always None
        return context;
    }

    /* Execute the code object with the given context. */
    BERTRAND_NOINLINE Dict<Str, Object>& operator()(Dict<Str, Object>& context) const {
        PyObject* result = PyEval_EvalCode(this->ptr(), context.ptr(), context.ptr());
        if (result == nullptr) {
            Exception::from_python(1);
        }
        Py_DECREF(result);  // always None
        return context;
    }

    /* Execute the code object with the given context. */
    BERTRAND_NOINLINE Dict<Str, Object> operator()(Dict<Str, Object>&& context) const {
        PyObject* result = PyEval_EvalCode(this->ptr(), context.ptr(), context.ptr());
        if (result == nullptr) {
            Exception::from_python(1);
        }
        Py_DECREF(result);  // always None
        return std::move(context);
    }

    /////////////////////
    ////    SLOTS    ////
    /////////////////////

    /* Get the name of the file from which the code was compiled. */
    Str filename() const {
        return reinterpret_borrow<Str>(self()->co_filename);
    }

    /* Get the function's base name. */
    Str name() const {
        return reinterpret_borrow<Str>(self()->co_name);
    }

    /* Get the function's qualified name. */
    Str qualname() const {
        return reinterpret_borrow<Str>(self()->co_qualname);
    }

    /* Get the first line number of the function. */
    Py_ssize_t line_number() const noexcept {
        return self()->co_firstlineno;
    }

    /* Get the total number of positional arguments for the function, including
    positional-only arguments and those with default values (but not variable
    or keyword-only arguments). */
    Py_ssize_t argcount() const noexcept {
        return self()->co_argcount;
    }

    /* Get the number of positional-only arguments for the function, including
    those with default values.  Does not include variable positional or keyword
    arguments. */
    Py_ssize_t posonlyargcount() const noexcept {
        return self()->co_posonlyargcount;
    }

    /* Get the number of keyword-only arguments for the function, including those
    with default values.  Does not include positional-only or variable
    positional/keyword arguments. */
    Py_ssize_t kwonlyargcount() const noexcept {
        return self()->co_kwonlyargcount;
    }

    /* Get the number of local variables used by the function (including all
    parameters). */
    Py_ssize_t nlocals() const noexcept {
        return self()->co_nlocals;
    }

    /* Get a tuple containing the names of the local variables in the function,
    starting with parameter names. */
    Tuple<Str> varnames() const {
        return attr<"co_varnames">();
    }

    /* Get a tuple containing the names of local variables that are referenced by
    nested functions within this function (i.e. those that are stored in a
    PyCell). */
    Tuple<Str> cellvars() const {
        return attr<"co_cellvars">();
    }

    /* Get a tuple containing the names of free variables in the function (i.e.
    those that are not stored in a PyCell). */
    Tuple<Str> freevars() const {
        return attr<"co_freevars">();
    }

    /* Get the required stack space for the code object. */
    Py_ssize_t stacksize() const noexcept {
        return self()->co_stacksize;
    }

    /* Get the bytecode buffer representing the sequence of instructions in the
    function. */
    Bytes bytecode() const;

    /* Get a tuple containing the literals used by the bytecode in the function. */
    Tuple<Object> consts() const {
        return reinterpret_borrow<Tuple<Object>>(self()->co_consts);
    }

    /* Get a tuple containing the names used by the bytecode in the function. */
    Tuple<Str> names() const {
        return reinterpret_borrow<Tuple<Str>>(self()->co_names);
    }

    /* Get an integer encoding flags for the Python interpreter. */
    int flags() const noexcept {
        return self()->co_flags;
    }

};


/////////////////////
////    FRAME    ////
/////////////////////


template <std::derived_from<Frame> T>
struct __getattr__<T, "f_back">                                 : Returns<Frame> {};
template <std::derived_from<Frame> T>
struct __getattr__<T, "f_code">                                 : Returns<Code> {};
template <std::derived_from<Frame> T>
struct __getattr__<T, "f_locals">                               : Returns<Dict<Str, Object>> {};
template <std::derived_from<Frame> T>
struct __getattr__<T, "f_globals">                              : Returns<Dict<Str, Object>> {};
template <std::derived_from<Frame> T>
struct __getattr__<T, "f_builtins">                             : Returns<Dict<Str, Object>> {};
template <std::derived_from<Frame> T>
struct __getattr__<T, "f_lasti">                                : Returns<Int> {};
template <std::derived_from<Frame> T>
struct __getattr__<T, "f_trace">                                : Returns<Object> {};
template <std::derived_from<Frame> T>
struct __setattr__<T, "f_trace", Object>                        : Returns<void> {};
template <std::derived_from<Frame> T>
struct __delattr__<T, "f_trace">                                : Returns<void> {};
template <std::derived_from<Frame> T>
struct __getattr__<T, "f_trace_lines">                          : Returns<Bool> {};
template <std::derived_from<Frame> T>
struct __setattr__<T, "f_trace_lines", Bool>                    : Returns<void> {};
template <std::derived_from<Frame> T>
struct __getattr__<T, "f_trace_opcodes">                        : Returns<Bool> {};
template <std::derived_from<Frame> T>
struct __setattr__<T, "f_trace_opcodes", Bool>                  : Returns<void> {};
template <std::derived_from<Frame> T>
struct __getattr__<T, "f_lineno">                               : Returns<Bool> {};
template <std::derived_from<Frame> T>
struct __setattr__<T, "f_lineno", Int>                          : Returns<void> {};
template <std::derived_from<Frame> T>
struct __getattr__<T, "clear">                                  : Returns<Function<
    void()
>> {};


/* Represents a statically-typed Python frame object in C++.  These are the same frames
returned by the `inspect` module and listed in exception tracebacks.  They can be used
to run Python code in an interactive loop via the embedded code object. */
class Frame : public Object {
    using Base = Object;

    PyFrameObject* self() const {
        return reinterpret_cast<PyFrameObject*>(this->ptr());
    }

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return std::derived_from<T, Frame>;
    }

    template <typename T>
    static constexpr bool check(const T& obj) {
        if constexpr (impl::cpp_like<T>) {
            return check<T>();
        } else if constexpr (check<T>()) {
            return obj.ptr() != nullptr;
        } else if constexpr (impl::is_object_exact<T>) {
            return obj.ptr() != nullptr && PyFrame_Check(obj.ptr());
        } else {
            return false;
        }
    }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    Frame(Handle h, const borrowed_t& t) : Base(h, t) {}
    Frame(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    Frame(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    Frame(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<Frame>(accessor).release(), stolen_t{})
    {}

    /* Default constructor.  Initializes to the current execution frame. */
    Frame() : Base(reinterpret_cast<PyObject*>(PyEval_GetFrame()), stolen_t{}) {
        if (m_ptr == nullptr) {
            throw RuntimeError("no frame is currently executing");
        }
    }

    /* Construct an empty frame from a function name, file name, and line number.  This
    is primarily used to represent C++ contexts in Python exception tracebacks, etc. */
    Frame(
        const char* funcname,
        const char* filename,
        int lineno,
        PyThreadState* thread_state = nullptr
    ) : Base(
        reinterpret_cast<PyObject*>(impl::StackFrame(
            funcname,
            filename,
            lineno,
            false,
            thread_state
        ).to_python()),
        borrowed_t{}
    ) {}

    /* Construct an empty frame from a cpptrace::stacktrace_frame object. */
    Frame(
        const cpptrace::stacktrace_frame& frame,
        PyThreadState* thread_state = nullptr
    ) : Base(
        reinterpret_cast<PyObject*>(impl::StackFrame(
            frame,
            thread_state
        ).to_python()),
        borrowed_t{}
    ) {}

    /* Skip backward a number of frames on construction. */
    explicit Frame(size_t skip) :
        Base(reinterpret_cast<PyObject*>(PyEval_GetFrame()), stolen_t{})
    {
        if (m_ptr == nullptr) {
            throw RuntimeError("no frame is currently executing");
        }
        for (size_t i = 0; i < skip; ++i) {
            m_ptr = reinterpret_cast<PyObject*>(PyFrame_GetBack(self()));
            if (m_ptr == nullptr) {
                throw IndexError("frame index out of range");
            }
        }
    }

    /////////////////////////////
    ////    C++ INTERFACE    ////
    /////////////////////////////

    /* Get the next outer frame from this one. */
    Frame back() const {
        PyFrameObject* result = PyFrame_GetBack(self());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<Frame>(reinterpret_cast<PyObject*>(result));
    }

    /* Get the code object associated with this frame. */
    Code code() const {
        return reinterpret_steal<Code>(
            reinterpret_cast<PyObject*>(PyFrame_GetCode(self()))  // never null
        );
    }

    /* Get the line number that the frame is currently executing. */
    int line_number() const noexcept {
        return PyFrame_GetLineNumber(self());
    }

    /* Execute the code object stored within the frame using its current context.  This
    is the main entry point for the Python interpreter, and is used behind the scenes
    whenever a program is run. */
    Object operator()() const {
        PyObject* result = PyEval_EvalFrame(self());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<Object>(result);
    }

    #if (PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 11)

        /* Get the frame's builtin namespace. */
        Dict<Str, Object> builtins() const {
            return reinterpret_steal<Dict<Str, Object>>(
                PyFrame_GetBuiltins(self())
            );
        }

        /* Get the frame's globals namespace. */
        Dict<Str, Object> globals() const {
            PyObject* result = PyFrame_GetGlobals(self());
            if (result == nullptr) {
                Exception::from_python();
            }
            return reinterpret_steal<Dict<Str, Object>>(result);
        }

        /* Get the frame's locals namespace. */
        Dict<Str, Object> locals() const {
            PyObject* result = PyFrame_GetLocals(self());
            if (result == nullptr) {
                Exception::from_python();
            }
            return reinterpret_steal<Dict<Str, Object>>(result);
        }

        /* Get the generator, coroutine, or async generator that owns this frame, or
        nullopt if this frame is not owned by a generator. */
        std::optional<Object> generator() const {
            PyObject* result = PyFrame_GetGenerator(self());
            if (result == nullptr) {
                return std::nullopt;
            } else {
                return std::make_optional(reinterpret_steal<Object>(result));
            }
        }

        /* Get the "precise instruction" of the frame object, which is an index into
        the bytecode of the last instruction that was executed by the frame's code
        object. */
        int last_instruction() const noexcept {
            return PyFrame_GetLasti(self());
        }

    #endif

    #if (PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 12)

        /* Get a named variable from the frame's context.  Can raise if the variable is
        not present in the frame. */
        Object get(const Str& name) const {
            PyObject* result = PyFrame_GetVar(self(), name.ptr());
            if (result == nullptr) {
                Exception::from_python();
            }
            return reinterpret_steal<Object>(result);
        }

    #endif

};


// ///////////////////////////
// ////    CLASSMETHOD    ////
// ///////////////////////////


// template <std::derived_from<ClassMethod> T>
// struct __getattr__<T, "__func__">                               : Returns<Function> {};
// template <std::derived_from<ClassMethod> T>
// struct __getattr__<T, "__wrapped__">                            : Returns<Function> {};


// /* Represents a statically-typed Python `classmethod` object in C++.  Note that this
// is a pure descriptor class, and is not callable by itself.  It behaves similarly to the
// @classmethod decorator, and can be attached to py::Type objects through normal
// attribute assignment. */
// class ClassMethod : public Object {
//     using Base = Object;

// public:
//     static const Type type;

//     template <typename T>
//     static consteval bool check() {
//         return std::derived_from<T, ClassMethod>;
//     }

//     template <typename T>
//     static constexpr bool check(const T& obj) {
//         if constexpr (impl::cpp_like<T>) {
//             return check<T>();

//         } else if constexpr (check<T>()) {
//             return obj.ptr() != nullptr;

//         } else if constexpr (impl::is_object_exact<T>) {
//             if (obj.ptr() == nullptr) {
//                 return false;
//             }
//             int result = PyObject_IsInstance(
//                 obj.ptr(),
//                 (PyObject*) &PyClassMethodDescr_Type
//             );
//             if (result == -1) {
//                 Exception::from_python();
//             }
//             return result;

//         } else {
//             return false;
//         }
//     }

//     ClassMethod(Handle h, const borrowed_t& t) : Base(h, t) {}
//     ClassMethod(Handle h, const stolen_t& t) : Base(h, t) {}

//     template <impl::pybind11_like T> requires (check<T>())
//     ClassMethod(T&& other) : Base(std::forward<T>(other)) {}

//     template <typename Policy>
//     ClassMethod(const pybind11::detail::accessor<Policy>& accessor) :
//         Base(Base::from_pybind11_accessor<ClassMethod>(accessor).release(), stolen_t{})
//     {}

//     /* Wrap an existing Python function as a classmethod descriptor. */
//     ClassMethod(Function func) : Base(PyClassMethod_New(func.ptr()), stolen_t{}) {
//         if (m_ptr == nullptr) {
//             Exception::from_python();
//         }
//     }

//     /* Get the underlying function. */
//     Function function() const {
//         return reinterpret_steal<Function>(Object(attr<"__func__">()).release());
//     }

// };


// ////////////////////////////
// ////    STATICMETHOD    ////
// ////////////////////////////


// template <std::derived_from<StaticMethod> T>
// struct __getattr__<T, "__func__">                               : Returns<Function> {};
// template <std::derived_from<StaticMethod> T>
// struct __getattr__<T, "__wrapped__">                            : Returns<Function> {};


// /* Represents a statically-typed Python `staticmethod` object in C++.  Note that this
// is a pure descriptor class, and is not callable by itself.  It behaves similarly to the
// @staticmethod decorator, and can be attached to py::Type objects through normal
// attribute assignment. */
// class StaticMethod : public Object {
//     using Base = Object;

// public:
//     static const Type type;

//     template <typename T>
//     static consteval bool check() {
//         return std::derived_from<T, StaticMethod>;
//     }

//     template <typename T>
//     static constexpr bool check(const T& obj) {
//         if constexpr (impl::cpp_like<T>) {
//             return check<T>();

//         } else if constexpr (check<T>()) {
//             return obj.ptr() != nullptr;

//         } else if constexpr (impl::is_object_exact<T>) {
//             if (obj.ptr() == nullptr) {
//                 return false;
//             }
//             int result = PyObject_IsInstance(
//                 obj.ptr(),
//                 (PyObject*) &PyStaticMethod_Type
//             );
//             if (result == -1) {
//                 Exception::from_python();
//             }
//             return result;

//         } else {
//             return false;
//         }
//     }

//     StaticMethod(Handle h, const borrowed_t& t) : Base(h, t) {}
//     StaticMethod(Handle h, const stolen_t& t) : Base(h, t) {}

//     template <impl::pybind11_like T> requires (check<T>())
//     StaticMethod(T&& other) : Base(std::forward<T>(other)) {}

//     template <typename Policy>
//     StaticMethod(const pybind11::detail::accessor<Policy>& accessor) :
//         Base(Base::from_pybind11_accessor<StaticMethod>(accessor).release(), stolen_t{})
//     {}

//     /* Wrap an existing Python function as a staticmethod descriptor. */
//     StaticMethod(Function func) : Base(PyStaticMethod_New(func.ptr()), stolen_t{}) {
//         if (m_ptr == nullptr) {
//             Exception::from_python();
//         }
//     }

//     /* Get the underlying function. */
//     Function function() const {
//         return reinterpret_steal<Function>(Object(attr<"__func__">()).release());
//     }

// };


// ////////////////////////
// ////    PROPERTY    ////
// ////////////////////////


// namespace impl {
//     static const Type PyProperty = reinterpret_borrow<Type>(
//         reinterpret_cast<PyObject*>(&PyProperty_Type)
//     );
// }


// template <std::derived_from<Property> T>
// struct __getattr__<T, "fget">                                   : Returns<Function> {};
// template <std::derived_from<Property> T>
// struct __getattr__<T, "fset">                                   : Returns<Function> {};
// template <std::derived_from<Property> T>
// struct __getattr__<T, "fdel">                                   : Returns<Function> {};
// template <std::derived_from<Property> T>
// struct __getattr__<T, "getter">                                 : Returns<Function> {};
// template <std::derived_from<Property> T>
// struct __getattr__<T, "setter">                                 : Returns<Function> {};
// template <std::derived_from<Property> T>
// struct __getattr__<T, "deleter">                                : Returns<Function> {};


// /* Represents a statically-typed Python `property` object in C++.  Note that this is a
// pure descriptor class, and is not callable by itself.  It behaves similarly to the
// @property decorator, and can be attached to py::Type objects through normal attribute
// assignment. */
// class Property : public Object {
//     using Base = Object;

// public:
//     static const Type type;

//     template <typename T>
//     static consteval bool check() {
//         return std::derived_from<T, Property>;
//     }

//     template <typename T>
//     static constexpr bool check(const T& obj) {
//         if constexpr (impl::cpp_like<T>) {
//             return check<T>();

//         } else if constexpr (check<T>()) {
//             return obj.ptr() != nullptr;

//         } else if constexpr (impl::is_object_exact<T>) {
//             if (obj.ptr() == nullptr) {
//                 return false;
//             }
//             int result = PyObject_IsInstance(
//                 obj.ptr(),
//                 impl::PyProperty.ptr());
//             if (result == -1) {
//                 Exception::from_python();
//             }
//             return result;

//         } else {
//             return false;
//         }
//     }

//     Property(Handle h, const borrowed_t& t) : Base(h, t) {}
//     Property(Handle h, const stolen_t& t) : Base(h, t) {}

//     template <impl::pybind11_like T> requires (check<T>())
//     Property(T&& other) : Base(std::forward<T>(other)) {}

//     template <typename Policy>
//     Property(const pybind11::detail::accessor<Policy>& accessor) :
//         Base(Base::from_pybind11_accessor<Property>(accessor).release(), stolen_t{})
//     {}

//     /* Wrap an existing Python function as a getter in a property descriptor. */
//     Property(const Function& getter) :
//         Base(impl::PyProperty(getter).release(), stolen_t{})
//     {}

//     /* Wrap existing Python functions as getter and setter in a property descriptor. */
//     Property(const Function& getter, const Function& setter) :
//         Base(impl::PyProperty(getter, setter).release(), stolen_t{})
//     {}

//     /* Wrap existing Python functions as getter, setter, and deleter in a property
//     descriptor. */
//     Property(const Function& getter, const Function& setter, const Function& deleter) :
//         Base(impl::PyProperty(getter, setter, deleter).release(), stolen_t{})
//     {}

//     /* Get the function being used as a getter. */
//     Function fget() const {
//         return reinterpret_steal<Function>(Object(attr<"fget">()).release());
//     }

//     /* Get the function being used as a setter. */
//     Function fset() const {
//         return reinterpret_steal<Function>(Object(attr<"fset">()).release());
//     }

//     /* Get the function being used as a deleter. */
//     Function fdel() const {
//         return reinterpret_steal<Function>(Object(attr<"fdel">()).release());
//     }

// };


}  // namespace py
}  // namespace bertrand


namespace pybind11 {
namespace detail {

template <bertrand::py::impl::is_callable_any T>
struct type_caster<T> {
    PYBIND11_TYPE_CASTER(T, _("callable"));

    /* Convert a Python object into a C++ callable. */
    bool load(handle src, bool convert) {
        return false;
    }

    /* Convert a C++ callable into a Python object. */
    static handle cast(const T& src, return_value_policy policy, handle parent) {
        return bertrand::py::Function(src).release();
    }

};

}  // namespace detail
}  // namespace pybind11


#endif  // BERTRAND_PYTHON_FUNC_H

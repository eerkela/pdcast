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
struct __getattr__<T, "co_positions">                           : Returns<Function> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "co_lines">                               : Returns<Function> {};
template <std::derived_from<Code> T>
struct __getattr__<T, "replace">                                : Returns<Function> {};


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

    inline PyCodeObject* self() const {
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
    inline Str filename() const {
        return reinterpret_borrow<Str>(self()->co_filename);
    }

    /* Get the function's base name. */
    inline Str name() const {
        return reinterpret_borrow<Str>(self()->co_name);
    }

    /* Get the function's qualified name. */
    inline Str qualname() const {
        return reinterpret_borrow<Str>(self()->co_qualname);
    }

    /* Get the first line number of the function. */
    inline Py_ssize_t line_number() const noexcept {
        return self()->co_firstlineno;
    }

    /* Get the total number of positional arguments for the function, including
    positional-only arguments and those with default values (but not variable
    or keyword-only arguments). */
    inline Py_ssize_t argcount() const noexcept {
        return self()->co_argcount;
    }

    /* Get the number of positional-only arguments for the function, including
    those with default values.  Does not include variable positional or keyword
    arguments. */
    inline Py_ssize_t posonlyargcount() const noexcept {
        return self()->co_posonlyargcount;
    }

    /* Get the number of keyword-only arguments for the function, including those
    with default values.  Does not include positional-only or variable
    positional/keyword arguments. */
    inline Py_ssize_t kwonlyargcount() const noexcept {
        return self()->co_kwonlyargcount;
    }

    /* Get the number of local variables used by the function (including all
    parameters). */
    inline Py_ssize_t nlocals() const noexcept {
        return self()->co_nlocals;
    }

    /* Get a tuple containing the names of the local variables in the function,
    starting with parameter names. */
    inline Tuple<Str> varnames() const {
        return attr<"co_varnames">();
    }

    /* Get a tuple containing the names of local variables that are referenced by
    nested functions within this function (i.e. those that are stored in a
    PyCell). */
    inline Tuple<Str> cellvars() const {
        return attr<"co_cellvars">();
    }

    /* Get a tuple containing the names of free variables in the function (i.e.
    those that are not stored in a PyCell). */
    inline Tuple<Str> freevars() const {
        return attr<"co_freevars">();
    }

    /* Get the required stack space for the code object. */
    inline Py_ssize_t stacksize() const noexcept {
        return self()->co_stacksize;
    }

    /* Get the bytecode buffer representing the sequence of instructions in the
    function. */
    inline Bytes bytecode() const;  // defined in func.h

    /* Get a tuple containing the literals used by the bytecode in the function. */
    inline Tuple<Object> consts() const {
        return reinterpret_borrow<Tuple<Object>>(self()->co_consts);
    }

    /* Get a tuple containing the names used by the bytecode in the function. */
    inline Tuple<Str> names() const {
        return reinterpret_borrow<Tuple<Str>>(self()->co_names);
    }

    /* Get an integer encoding flags for the Python interpreter. */
    inline int flags() const noexcept {
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
struct __getattr__<T, "f_trace">                                : Returns<Function> {};
template <std::derived_from<Frame> T>
struct __setattr__<T, "f_trace", Function>                      : Returns<void> {};
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
struct __getattr__<T, "clear">                                  : Returns<Function> {};


/* Represents a statically-typed Python frame object in C++.  These are the same frames
returned by the `inspect` module and listed in exception tracebacks.  They can be used
to run Python code in an interactive loop via the embedded code object. */
class Frame : public Object {
    using Base = Object;

    inline PyFrameObject* self() const {
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
    inline Frame back() const {
        PyFrameObject* result = PyFrame_GetBack(self());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<Frame>(reinterpret_cast<PyObject*>(result));
    }

    /* Get the code object associated with this frame. */
    inline Code code() const {
        return reinterpret_steal<Code>(
            reinterpret_cast<PyObject*>(PyFrame_GetCode(self()))  // never null
        );
    }

    /* Get the line number that the frame is currently executing. */
    inline int line_number() const noexcept {
        return PyFrame_GetLineNumber(self());
    }

    /* Execute the code object stored within the frame using its current context.  This
    is the main entry point for the Python interpreter, and is used behind the scenes
    whenever a program is run. */
    inline Object operator()() const {
        PyObject* result = PyEval_EvalFrame(self());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_steal<Object>(result);
    }

    #if (PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 11)

        /* Get the frame's builtin namespace. */
        inline Dict<Str, Object> builtins() const {
            return reinterpret_steal<Dict<Str, Object>>(
                PyFrame_GetBuiltins(self())
            );
        }

        /* Get the frame's globals namespace. */
        inline Dict<Str, Object> globals() const {
            PyObject* result = PyFrame_GetGlobals(self());
            if (result == nullptr) {
                Exception::from_python();
            }
            return reinterpret_steal<Dict<Str, Object>>(result);
        }

        /* Get the frame's locals namespace. */
        inline Dict<Str, Object> locals() const {
            PyObject* result = PyFrame_GetLocals(self());
            if (result == nullptr) {
                Exception::from_python();
            }
            return reinterpret_steal<Dict<Str, Object>>(result);
        }

        /* Get the generator, coroutine, or async generator that owns this frame, or
        nullopt if this frame is not owned by a generator. */
        inline std::optional<Object> generator() const {
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
        inline int last_instruction() const noexcept {
            return PyFrame_GetLasti(self());
        }

    #endif

    #if (PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 12)

        /* Get a named variable from the frame's context.  Can raise if the variable is
        not present in the frame. */
        inline Object get(const Str& name) const {
            PyObject* result = PyFrame_GetVar(self(), name.ptr());
            if (result == nullptr) {
                Exception::from_python();
            }
            return reinterpret_steal<Object>(result);
        }

    #endif

};


////////////////////////////////////
////    ARGUMENT ANNOTATIONS    ////
////////////////////////////////////


// TODO: need to figure out a way to handle optional arguments with default values
// in signatures.

// py::Function func = [](const py::Arg<"x", py::Int>::optional& x = 1, ...)


// TODO: Arg should go in proxies.h


namespace impl {

    struct ArgTag {};

}


/* A simple data struct representing a bound keyword argument to a Python function,
which is checked at compile time. */
template <StaticStr Name, typename T>
struct Arg : public impl::ArgTag {
    static constexpr bool is_optional = false;
    static constexpr StaticStr name = Name;
    using type = T;

    T value;

    Arg(T&& value) : value(std::forward<T>(value)) {}
    Arg(const Arg& other) : value(other.value) {}
    Arg(Arg&& other) : value(std::move(other.value)) {}

    const T& operator*() const { return value; }
    const T* operator->() const { return &value; }
    operator T&() { return value; }
    operator const T&() const { return value; }
};


template <StaticStr Name, typename T>
struct Arg<Name, std::optional<T>> {
    static constexpr bool is_optional = true;
    static constexpr StaticStr name = Name;
    using type = T;

    std::optional<T> value;

    Arg(const T& value) : value(value) {}
    Arg(T&& value) : value(std::move(value)) {}
    Arg(const Arg& other) : value(other.value) {}
    Arg(Arg&& other) : value(std::move(other.value)) {}

    const std::optional<T>& operator*() const { return value; }
    const std::optional<T>* operator->() const { return &value; }
    operator std::optional<T>&() { return value; }
    operator const std::optional<T>&() const { return value; }

};


namespace impl {

    /* A compile-time tag that allows for the familiar `py::arg<"name"> = value`
    syntax.  The `py::arg<"name">` bit resolves to an instance of this class, and the
    argument becomes bound when the `=` operator is applied to it. */
    template <StaticStr name>
    struct UnboundArg {
        template <typename T>
        constexpr Arg<name, std::decay_t<T>> operator=(T&& value) const {
            return {std::forward<T>(value)};
        }
    };

}


/* Compile-time factory for `UnboundArgument` tags. */
template <StaticStr name>
static constexpr impl::UnboundArg<name> arg_ {};


template <typename Return, typename... Args>
class Function_ {
protected:
    static constexpr bool accepts_keywords = (std::derived_from<Args, impl::ArgTag> || ...);
    static constexpr bool accepts_variadic_positional = false;
    static constexpr bool accepts_variadic_keyword = false;

    static constexpr size_t missing = sizeof...(Args);

    template <typename... As>
    static constexpr bool callable_with = [] {
        // TODO: eventually, ensure that the function is callable with the given
        // arguments to constrain the call operator and give good error messages.
        return true;
    }();


    template <typename... Ts>
    static constexpr size_t n_positional = 0;
    template <typename First, typename... Rest>
    static constexpr size_t n_positional<First, Rest...> =
        std::derived_from<std::decay_t<First>, impl::ArgTag> ? 0 : 1 + n_positional<Rest...>;

    /* index uses template recursion to determine the position of a keyword argument T
    in the expected signature (Args...).  If no match is not found, then the index will
    be equal to `missing`. */
    template <typename T, typename... Ts>
    struct index {
        static constexpr size_t value = 0;
    };

    template <typename T, typename First, typename... Rest>
    struct index<T, First, Rest...> {
        template <typename T1, typename T2>
        struct compare {
            static constexpr bool match = false;
        };

        template <typename T1, std::derived_from<impl::ArgTag> T2>
        struct compare<T1, T2> {
            static constexpr bool match = (T1::name == T2::name);
        };

        using T1 = std::decay_t<T>;
        using T2 = std::decay_t<First>;
        static constexpr size_t value = compare<T1, T2>::match ?
            0 : 1 + index<T, Rest...>::value;            
    };

    template <size_t I, typename A, typename... As>
    auto extract(const std::tuple<As...>& args) const {
        if constexpr (std::derived_from<std::decay_t<A>, impl::ArgTag>) {
            static constexpr size_t idx = index<A, Args...>::value;
            if constexpr (idx == missing) {
                // TODO: static assert should be handled by template constraint
                static_assert(std::decay_t<A>::is_optional, "bro, you forgot something");
                return std::nullopt;

            } else {
                return *std::get<idx>(args);  // TODO: is index correct in this case?
            }

        } else {
            return std::get<I>(args);
        }
    };

    template <size_t... Is, typename... As>
    auto call(std::index_sequence<Is...>, const std::tuple<As...>& args) const {
        return func(extract<Is, As>(args)...);
    }

public:
    std::function<Return(Args...)> func;

    template <typename T>
    Function_(T&& func) : func(std::forward<T>(func)) {}

    template <typename... As> requires (callable_with<As...>)
    Return operator()(As&&... args) const {
        return call(std::index_sequence_for<As...>{}, std::tie(args...));
    }

};


template <typename R, typename... A>
class Function_<R(*)(A...)> : public Function_<R, A...> {};
template <typename R, typename C, typename... A>
class Function_<R(C::*)(A...)> : public Function_<R, A...> {};
template <typename R, typename C, typename... A>
class Function_<R(C::*)(A...) const> : public Function_<R, A...> {};
template <typename R, typename C, typename... A>
class Function_<R(C::*)(A...) volatile> : public Function_<R, A...> {};
template <typename R, typename C, typename... A>
class Function_<R(C::*)(A...) const volatile> : public Function_<R, A...> {};
template <impl::has_call_operator T>
class Function_<T> : public Function_<decltype(&T::operator())> {};


template <typename R, typename... A>
Function_(R(*)(A...)) -> Function_<R, A...>;
template <typename R, typename C, typename... A>
Function_(R(C::*)(A...)) -> Function_<R, A...>;
template <typename R, typename C, typename... A>
Function_(R(C::*)(A...) const) -> Function_<R, A...>;
template <typename R, typename C, typename... A>
Function_(R(C::*)(A...) volatile) -> Function_<R, A...>;
template <typename R, typename C, typename... A>
Function_(R(C::*)(A...) const volatile) -> Function_<R, A...>;
template <impl::has_call_operator T>
Function_(T) -> Function_<decltype(&T::operator())>;



// template <typename T>
// struct Args {
//     Tuple<T> args;
//     template <typename... As>
//     Args(Tuple<T>&& args) : args(std::move(args)) {}
// };



// TODO: Kwargs will need to accept a bunch of py::Arg instances

// template <typename T>
// struct Kwargs {
//     Dict<Str, T> kwargs;
// };


////////////////////////
////    FUNCTION    ////
////////////////////////


template <typename... Args>
struct __call__<Function, Args...>                              : Returns<Object> {};
template <std::derived_from<Function> T>
struct __getattr__<T, "__globals__">                            : Returns<Dict<Str, Object>> {};
template <std::derived_from<Function> T>
struct __getattr__<T, "__closure__">                            : Returns<Tuple<Object>> {};
template <std::derived_from<Function> T>
struct __getattr__<T, "__defaults__">                           : Returns<Tuple<Object>> {};
template <std::derived_from<Function> T>
struct __getattr__<T, "__code__">                               : Returns<Code> {};
template <std::derived_from<Function> T>
struct __getattr__<T, "__annotations__">                        : Returns<Dict<Str, Object>> {};
template <std::derived_from<Function> T>
struct __getattr__<T, "__kwdefaults__">                         : Returns<Dict<Str, Object>> {};
template <std::derived_from<Function> T>
struct __getattr__<T, "__func__">                               : Returns<Function> {};


/* Represents a statically-typed Python function in C++.  Note that this can either be
a direct Python function or a C++ function wrapped to look like a Python function to
calling code.  In the latter case, it will appear to Python as a built-in function, for
which no code object will be compiled. */
class Function : public Object {
    using Base = Object;

    inline PyObject* self() const {
        PyObject* result = this->ptr();
        if (PyMethod_Check(result)) {
            result = PyMethod_GET_FUNCTION(result);
        }
        return result;
    }

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return impl::is_callable_any<T>;
    }

    template <typename T>
    static constexpr bool check(const T& obj) {
        if constexpr (impl::cpp_like<T>) {
            return check<T>();
        } else if constexpr (check<T>()) {
            return obj.ptr() != nullptr;
        } else if constexpr (impl::is_object_exact<T>) {
            return obj.ptr() != nullptr && (
                PyFunction_Check(obj.ptr()) ||
                PyCFunction_Check(obj.ptr()) ||
                PyMethod_Check(obj.ptr())
            );
        } else {
            return false;
        }
    }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    Function(Handle h, const borrowed_t& t) : Base(h, t) {}
    Function(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    Function(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    Function(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<Function>(accessor).release(), stolen_t{})
    {}

    /* Implicitly convert a C++ function or callable object into a py::Function. */
    template <impl::cpp_like T> requires (check<T>())
    Function(T&& func) :
        Base(pybind11::cpp_function(std::forward<T>(func)).release(), stolen_t{})
    {}

    /////////////////////////////
    ////    C++ INTERFACE    ////
    /////////////////////////////

    /* Get the module in which this function was defined. */
    inline Module module_() const {
        PyObject* result = PyFunction_GetModule(self());
        if (result == nullptr) {
            throw TypeError("function has no module");
        }
        return reinterpret_borrow<Module>(result);
    }

    /* Get the code object that is executed when this function is called. */
    inline Code code() const {
        if (PyCFunction_Check(this->ptr())) {
            throw RuntimeError("C++ functions do not have code objects");
        }
        PyObject* result = PyFunction_GetCode(self());
        if (result == nullptr) {
            throw RuntimeError("function does not have a code object");
        }
        return reinterpret_borrow<Code>(result);
    }

    /* Get the globals dictionary associated with the function object. */
    inline Dict<Str, Object> globals() const {
        PyObject* result = PyFunction_GetGlobals(self());
        if (result == nullptr) {
            Exception::from_python();
        }
        return reinterpret_borrow<Dict<Str, Object>>(result);
    }

    /* Get the name of the file from which the code was compiled. */
    inline std::string filename() const {
        return code().filename();
    }

    /* Get the first line number of the function. */
    inline size_t line_number() const {
        return code().line_number();
    }

    /* Get the function's base name. */
    inline std::string name() const {
        return code().name();
    }

    /* Get the function's qualified name. */
    inline std::string qualname() const {
        return code().qualname();
    }

    /* Get the total number of positional or keyword arguments for the function,
    including positional-only parameters and excluding keyword-only parameters. */
    inline size_t argcount() const {
        return code().argcount();
    }

    /* Get the number of positional-only arguments for the function.  Does not include
    variable positional or keyword arguments. */
    inline size_t posonlyargcount() const {
        return code().posonlyargcount();
    }

    /* Get the number of keyword-only arguments for the function.  Does not include
    positional-only or variable positional/keyword arguments. */
    inline size_t kwonlyargcount() const {
        return code().kwonlyargcount();
    }

    // /* Get a read-only dictionary mapping argument names to their default values. */
    // inline MappingProxy<Dict<Str, Object>> defaults() const {
    //     Code code = this->code();

    //     // check for positional defaults
    //     PyObject* pos_defaults = PyFunction_GetDefaults(self());
    //     if (pos_defaults == nullptr) {
    //         if (code.kwonlyargcount() > 0) {
    //             Object kwdefaults = attr<"__kwdefaults__">();
    //             if (kwdefaults.is(None)) {
    //                 return MappingProxy(Dict<Str, Object>{});
    //             } else {
    //                 return MappingProxy(reinterpret_steal<Dict<Str, Object>>(
    //                     kwdefaults.release())
    //                 );
    //             }
    //         } else {
    //             return MappingProxy(Dict<Str, Object>{});
    //         }
    //     }

    //     // extract positional defaults
    //     size_t argcount = code.argcount();
    //     Tuple<Object> defaults = reinterpret_borrow<Tuple<Object>>(pos_defaults);
    //     Tuple<Str> names = code.varnames()[{argcount - defaults.size(), argcount}];
    //     Dict result;
    //     for (size_t i = 0; i < defaults.size(); ++i) {
    //         result[names[i]] = defaults[i];
    //     }

    //     // merge keyword-only defaults
    //     if (code.kwonlyargcount() > 0) {
    //         Object kwdefaults = attr<"__kwdefaults__">();
    //         if (!kwdefaults.is(None)) {
    //             result.update(Dict(kwdefaults));
    //         }
    //     }
    //     return result;
    // }

    // /* Set the default value for one or more arguments.  If nullopt is provided,
    // then all defaults will be cleared. */
    // inline void defaults(Dict&& dict) {
    //     Code code = this->code();

    //     // TODO: clean this up.  The logic should go as follows:
    //     // 1. check for positional defaults.  If found, build a dictionary with the new
    //     // values and remove them from the input dict.
    //     // 2. check for keyword-only defaults.  If found, build a dictionary with the
    //     // new values and remove them from the input dict.
    //     // 3. if any keys are left over, raise an error and do not update the signature
    //     // 4. set defaults to Tuple(positional_defaults.values()) and update kwdefaults
    //     // in-place.

    //     // account for positional defaults
    //     PyObject* pos_defaults = PyFunction_GetDefaults(self());
    //     if (pos_defaults != nullptr) {
    //         size_t argcount = code.argcount();
    //         Tuple<Object> defaults = reinterpret_borrow<Tuple<Object>>(pos_defaults);
    //         Tuple<Str> names = code.varnames()[{argcount - defaults.size(), argcount}];
    //         Dict positional_defaults;
    //         for (size_t i = 0; i < defaults.size(); ++i) {
    //             positional_defaults[*names[i]] = *defaults[i];
    //         }

    //         // merge new defaults with old ones
    //         for (const Object& key : positional_defaults) {
    //             if (dict.contains(key)) {
    //                 positional_defaults[key] = dict.pop(key);
    //             }
    //         }
    //     }

    //     // check for keyword-only defaults
    //     if (code.kwonlyargcount() > 0) {
    //         Object kwdefaults = attr<"__kwdefaults__">();
    //         if (!kwdefaults.is(None)) {
    //             Dict temp = {};
    //             for (const Object& key : kwdefaults) {
    //                 if (dict.contains(key)) {
    //                     temp[key] = dict.pop(key);
    //                 }
    //             }
    //             if (dict) {
    //                 throw ValueError("no match for arguments " + Str(List(dict.keys())));
    //             }
    //             kwdefaults |= temp;
    //         } else if (dict) {
    //             throw ValueError("no match for arguments " + Str(List(dict.keys())));
    //         }
    //     } else if (dict) {
    //         throw ValueError("no match for arguments " + Str(List(dict.keys())));
    //     }

    //     // TODO: set defaults to Tuple(positional_defaults.values()) and update kwdefaults

    // }

    /* Get a read-only dictionary holding type annotations for the function. */
    inline MappingProxy<Dict<Str, Object>> annotations() const {
        PyObject* result = PyFunction_GetAnnotations(self());
        if (result == nullptr) {
            return MappingProxy(Dict<Str, Object>{});
        }
        return reinterpret_borrow<MappingProxy<Dict<Str, Object>>>(result);
    }

    // /* Set the type annotations for the function.  If nullopt is provided, then the
    // current annotations will be cleared.  Otherwise, the values in the dictionary will
    // be used to update the current values in-place. */
    // inline void annotations(std::optional<Dict> annotations) {
    //     if (!annotations.has_value()) {  // clear all annotations
    //         if (PyFunction_SetAnnotations(self(), Py_None)) {
    //             Exception::from_python();
    //         }

    //     } else if (!annotations.value()) {  // do nothing
    //         return;

    //     } else {  // update annotations in-place
    //         Code code = this->code();
    //         Tuple<Str> args = code.varnames()[{0, code.argcount() + code.kwonlyargcount()}];
    //         MappingProxy existing = this->annotations();

    //         // build new dict
    //         Dict result = {};
    //         for (const Object& arg : args) {
    //             if (annotations.value().contains(arg)) {
    //                 result[arg] = annotations.value().pop(arg);
    //             } else if (existing.contains(arg)) {
    //                 result[arg] = existing[arg];
    //             }
    //         }

    //         // account for return annotation
    //         static const Str s_return = "return";
    //         if (annotations.value().contains(s_return)) {
    //             result[s_return] = annotations.value().pop(s_return);
    //         } else if (existing.contains(s_return)) {
    //             result[s_return] = existing[s_return];
    //         }

    //         // check for unmatched keys
    //         if (annotations.value()) {
    //             throw ValueError(
    //                 "no match for arguments " +
    //                 Str(List(annotations.value().keys()))
    //             );
    //         }

    //         // push changes
    //         if (PyFunction_SetAnnotations(self(), result.ptr())) {
    //             Exception::from_python();
    //         }
    //     }
    // }

    /* Get the closure associated with the function.  This is a Tuple of cell objects
    containing data captured by the function. */
    inline Tuple<Object> closure() const {
        PyObject* result = PyFunction_GetClosure(self());
        if (result == nullptr) {
            return {};
        }
        return reinterpret_borrow<Tuple<Object>>(result);
    }

    /* Set the closure associated with the function.  If nullopt is given, then the
    closure will be deleted. */
    inline void closure(std::optional<Tuple<Object>> closure) {
        PyObject* item = closure ? closure.value().ptr() : Py_None;
        if (PyFunction_SetClosure(self(), item)) {
            Exception::from_python();
        }
    }

};


///////////////////////////
////    CLASSMETHOD    ////
///////////////////////////


template <std::derived_from<ClassMethod> T>
struct __getattr__<T, "__func__">                               : Returns<Function> {};
template <std::derived_from<ClassMethod> T>
struct __getattr__<T, "__wrapped__">                            : Returns<Function> {};


/* Represents a statically-typed Python `classmethod` object in C++.  Note that this
is a pure descriptor class, and is not callable by itself.  It behaves similarly to the
@classmethod decorator, and can be attached to py::Type objects through normal
attribute assignment. */
class ClassMethod : public Object {
    using Base = Object;

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return std::derived_from<T, ClassMethod>;
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
                obj.ptr(),
                (PyObject*) &PyClassMethodDescr_Type
            );
            if (result == -1) {
                Exception::from_python();
            }
            return result;

        } else {
            return false;
        }
    }

    ClassMethod(Handle h, const borrowed_t& t) : Base(h, t) {}
    ClassMethod(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    ClassMethod(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    ClassMethod(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<ClassMethod>(accessor).release(), stolen_t{})
    {}

    /* Wrap an existing Python function as a classmethod descriptor. */
    ClassMethod(Function func) : Base(PyClassMethod_New(func.ptr()), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
    }

    /* Get the underlying function. */
    inline Function function() const {
        return reinterpret_steal<Function>(Object(attr<"__func__">()).release());
    }

};


////////////////////////////
////    STATICMETHOD    ////
////////////////////////////


template <std::derived_from<StaticMethod> T>
struct __getattr__<T, "__func__">                               : Returns<Function> {};
template <std::derived_from<StaticMethod> T>
struct __getattr__<T, "__wrapped__">                            : Returns<Function> {};


/* Represents a statically-typed Python `staticmethod` object in C++.  Note that this
is a pure descriptor class, and is not callable by itself.  It behaves similarly to the
@staticmethod decorator, and can be attached to py::Type objects through normal
attribute assignment. */
class StaticMethod : public Object {
    using Base = Object;

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return std::derived_from<T, StaticMethod>;
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
                obj.ptr(),
                (PyObject*) &PyStaticMethod_Type
            );
            if (result == -1) {
                Exception::from_python();
            }
            return result;

        } else {
            return false;
        }
    }

    StaticMethod(Handle h, const borrowed_t& t) : Base(h, t) {}
    StaticMethod(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    StaticMethod(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    StaticMethod(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<StaticMethod>(accessor).release(), stolen_t{})
    {}

    /* Wrap an existing Python function as a staticmethod descriptor. */
    StaticMethod(Function func) : Base(PyStaticMethod_New(func.ptr()), stolen_t{}) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
    }

    /* Get the underlying function. */
    inline Function function() const {
        return reinterpret_steal<Function>(Object(attr<"__func__">()).release());
    }

};


////////////////////////
////    PROPERTY    ////
////////////////////////


namespace impl {
    static const Type PyProperty = reinterpret_borrow<Type>(
        reinterpret_cast<PyObject*>(&PyProperty_Type)
    );
}


template <std::derived_from<Property> T>
struct __getattr__<T, "fget">                                   : Returns<Function> {};
template <std::derived_from<Property> T>
struct __getattr__<T, "fset">                                   : Returns<Function> {};
template <std::derived_from<Property> T>
struct __getattr__<T, "fdel">                                   : Returns<Function> {};
template <std::derived_from<Property> T>
struct __getattr__<T, "getter">                                 : Returns<Function> {};
template <std::derived_from<Property> T>
struct __getattr__<T, "setter">                                 : Returns<Function> {};
template <std::derived_from<Property> T>
struct __getattr__<T, "deleter">                                : Returns<Function> {};


/* Represents a statically-typed Python `property` object in C++.  Note that this is a
pure descriptor class, and is not callable by itself.  It behaves similarly to the
@property decorator, and can be attached to py::Type objects through normal attribute
assignment. */
class Property : public Object {
    using Base = Object;

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return std::derived_from<T, Property>;
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
                obj.ptr(),
                impl::PyProperty.ptr());
            if (result == -1) {
                Exception::from_python();
            }
            return result;

        } else {
            return false;
        }
    }

    Property(Handle h, const borrowed_t& t) : Base(h, t) {}
    Property(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    Property(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    Property(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<Property>(accessor).release(), stolen_t{})
    {}

    /* Wrap an existing Python function as a getter in a property descriptor. */
    Property(const Function& getter) :
        Base(impl::PyProperty(getter).release(), stolen_t{})
    {}

    /* Wrap existing Python functions as getter and setter in a property descriptor. */
    Property(const Function& getter, const Function& setter) :
        Base(impl::PyProperty(getter, setter).release(), stolen_t{})
    {}

    /* Wrap existing Python functions as getter, setter, and deleter in a property
    descriptor. */
    Property(const Function& getter, const Function& setter, const Function& deleter) :
        Base(impl::PyProperty(getter, setter, deleter).release(), stolen_t{})
    {}

    /* Get the function being used as a getter. */
    inline Function fget() const {
        return reinterpret_steal<Function>(Object(attr<"fget">()).release());
    }

    /* Get the function being used as a setter. */
    inline Function fset() const {
        return reinterpret_steal<Function>(Object(attr<"fset">()).release());
    }

    /* Get the function being used as a deleter. */
    inline Function fdel() const {
        return reinterpret_steal<Function>(Object(attr<"fdel">()).release());
    }

};


}  // namespace py
}  // namespace bertrand


namespace pybind11 {
namespace detail {

template <bertrand::py::impl::is_callable_any T>
struct type_caster<T> {
    PYBIND11_TYPE_CASTER(T, _("callable"));

    /* Convert a Python object into a C++ callable. */
    inline bool load(handle src, bool convert) {
        return false;
    }

    /* Convert a C++ callable into a Python object. */
    inline static handle cast(const T& src, return_value_policy policy, handle parent) {
        return bertrand::py::Function(src).release();
    }

};

}  // namespace detail
}  // namespace pybind11


#endif  // BERTRAND_PYTHON_FUNC_H

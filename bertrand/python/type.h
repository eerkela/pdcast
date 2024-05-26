#if !defined(BERTRAND_PYTHON_INCLUDED) && !defined(LINTER)
#error "This file should not be included directly.  Please include <bertrand/python.h> instead."
#endif

#ifndef BERTRAND_PYTHON_TYPE_H
#define BERTRAND_PYTHON_TYPE_H

#include "common.h"
#include "tuple.h"
#include "dict.h"
#include "str.h"


namespace bertrand {
namespace py {


/* Represents a statically-typed Python type object in C++.  Note that new types can be
created on the fly by invoking the `type` metaclass directly, using an optional name,
bases, and namespace. */
class Type : public Object {
    using Base = Object;

    PyTypeObject* self() const noexcept {
        return reinterpret_cast<PyTypeObject*>(m_ptr);
    }

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return impl::type_like<T>;
    }

    template <typename T>
    static constexpr bool check(const T& obj) {
        if constexpr (impl::cpp_like<T>) {
            return check<T>();
        } else if constexpr (check<T>()) {
            return obj.ptr() != nullptr;
        } else if constexpr (impl::is_object_exact<T>) {
            return obj.ptr() != nullptr && PyType_Check(obj.ptr());
        } else {
            return false;
        }
    }

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    Type(Handle h, const borrowed_t& t) : Base(h, t) {}
    Type(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    Type(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    Type(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<Type>(accessor).release(), stolen_t{})
    {}

    /* Default constructor.  Initializes to the built-in type metaclass. */
    Type() : Base((PyObject*) &PyType_Type, borrowed_t{}) {}

    /* Explicitly detect the type of an arbitrary Python object. */
    template <impl::python_like T>
    explicit Type(const T& obj) :
        Base(reinterpret_cast<PyObject*>(Py_TYPE(obj.ptr())), borrowed_t{})
    {}

    /* Dynamically create a new Python type by calling the type() metaclass. */
    explicit Type(
        const Str& name,
        const Tuple<Type>& bases = {},
        const Dict<Str, Object>& dict = {}
    );

    /////////////////////////////
    ////    C++ INTERFACE    ////
    /////////////////////////////

    /* Get the Python type of a registered pybind11 extension type. */
    template <typename T>
    static Type of() {
        return reinterpret_steal<Type>(pybind11::type::of<T>().release());
    }

    #if (PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 9)

        /* Get the module that the type is defined in.  Can throw if called on a
        static type rather than a heap type (one that was created using
        PyType_FromModuleAndSpec() or higher). */
        auto module_() const {
            PyObject* result = PyType_GetModule(self());
            if (result == nullptr) {
                Exception::from_python();
            }
            return reinterpret_steal<Module>(result);
        }

    #endif

    #if (PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 11)

        /* Get the type's qualified name. */
        Str qualname() const;

    #endif

    /* Get type's tp_name slot. */
    auto name() const noexcept {
        return self()->tp_name;
    }

    /* Get the type's tp_basicsize slot. */
    auto basicsize() const noexcept {
        return self()->tp_basicsize;
    }

    /* Get the type's tp_itemsize slot. */
    auto itemsize() const noexcept {
        return self()->tp_itemsize;
    }

    /* Get the type's tp_dealloc slot. */
    auto dealloc() const noexcept {
        return self()->tp_dealloc;
    }

    /* Get the type's tp_as_async slot. */
    auto as_async() const noexcept {
        return self()->tp_as_async;
    }

    /* Get the type's tp_repr slot. */
    auto repr() const noexcept {
        return self()->tp_repr;
    }

    /* Get the type's tp_as_number slot. */
    auto as_number() const noexcept {
        return self()->tp_as_number;
    }

    /* Get the type's tp_as_sequence slot. */
    auto as_sequence() const noexcept {
        return self()->tp_as_sequence;
    }

    /* Get the type's tp_as_mapping slot. */
    auto as_mapping() const noexcept {
        return self()->tp_as_mapping;
    }

    /* Get the type's tp_hash slot. */
    auto hash() const noexcept {
        return self()->tp_hash;
    }

    /* Get the type's tp_call slot. */
    auto call() const noexcept {
        return self()->tp_call;
    }

    /* Get the type's tp_str slot. */
    auto str() const noexcept {
        return self()->tp_str;
    }

    /* Get the type's tp_getattro slot. */
    auto getattro() const noexcept {
        return self()->tp_getattro;
    }

    /* Get the type's tp_setattro slot. */
    auto setattro() const noexcept {
        return self()->tp_setattro;
    }

    /* Get the type's tp_as_buffer slot. */
    auto as_buffer() const noexcept {
        return self()->tp_as_buffer;
    }

    /* Get the type's tp_flags slot. */
    auto flags() const noexcept {
        return self()->tp_flags;
    }

    /* Get the type's tp_doc slot. */
    auto doc() const noexcept {
        return self()->tp_doc;
    }

    /* Get the type's tp_traverse slot. */
    auto traverse() const noexcept {
        return self()->tp_traverse;
    }

    /* Get the type's tp_clear slot. */
    auto clear() const noexcept {
        return self()->tp_clear;
    }

    /* Get the type's tp_richcompare slot. */
    auto richcompare() const noexcept {
        return self()->tp_richcompare;
    }

    /* Get the type's tp_iter slot. */
    auto iter() const noexcept {
        return self()->tp_iter;
    }

    /* Get the type's tp_iternext slot. */
    auto iternext() const noexcept {
        return self()->tp_iternext;
    }

    /* Get the type's tp_methods slot. */
    auto methods() const noexcept {
        return self()->tp_methods;
    }

    /* Get the type's tp_members slot. */
    auto members() const noexcept {
        return self()->tp_members;
    }

    /* Get the type's tp_getset slot. */
    auto getset() const noexcept {
        return self()->tp_getset;
    }

    /* Get the type's tp_base slot. */
    auto base() const noexcept {
        return reinterpret_borrow<Type>(reinterpret_cast<PyObject*>(self()->tp_base));
    }

    /* Get the type's tp_dict slot. */
    auto dict() const noexcept {
        return reinterpret_borrow<Dict<Str, Object>>(self()->tp_dict);
    }

    /* Get the type's tp_descr_get slot. */
    auto descr_get() const noexcept {
        return self()->tp_descr_get;
    }

    /* Get the type's tp_descr_set slot. */
    auto descr_set() const noexcept {
        return self()->tp_descr_set;
    }

    /* Get the type's tp_bases slot. */
    auto bases() const noexcept {
        return reinterpret_borrow<Tuple<Type>>(self()->tp_bases);
    }

    /* Get the type's tp_mro slot. */
    auto mro() const noexcept {
        return reinterpret_borrow<Tuple<Type>>(self()->tp_mro);
    }

    /* Get the type's tp_finalize slot. */
    auto finalize() const noexcept {
        return self()->tp_finalize;
    }

    /* Get the type's tp_vectorcall slot. */
    auto vectorcall() const noexcept {
        return self()->tp_vectorcall;
    }

    /* Get the type's tp_vectorcall_offset slot. */
    auto vectorcall_offset() const noexcept {
        return self()->tp_vectorcall_offset;
    }

    /* Clear the lookup cache for the type and all of its subtypes.  This method should
    be called after any manual modification to the attributes or this class or any of
    its bases at the C++ level, in order to synchronize them with the Python
    interpreter.  Most users will never need to use this in practice. */
    void clear_cache() const noexcept {
        PyType_Modified(reinterpret_cast<PyTypeObject*>(this->ptr()));
    }

};


/* Represents a statically-typed Python `super` object in C++. */
class Super : public Object {
    using Base = Object;

public:
    static const Type type;

    template <typename T>
    static consteval bool check() {
        return std::derived_from<T, Super>;
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
                reinterpret_cast<PyObject*>(&PySuper_Type)
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

    Super(Handle h, const borrowed_t& t) : Base(h, t) {}
    Super(Handle h, const stolen_t& t) : Base(h, t) {}

    template <impl::pybind11_like T> requires (check<T>())
    Super(T&& other) : Base(std::forward<T>(other)) {}

    template <typename Policy>
    Super(const pybind11::detail::accessor<Policy>& accessor) :
        Base(Base::from_pybind11_accessor<Super>(accessor).release(), stolen_t{})
    {}

    /* Default constructor.  Equivalent to Python `super()` with no arguments, which
    uses the calling context's inheritance hierarchy. */
    Super() : Base(
        PyObject_CallNoArgs(reinterpret_cast<PyObject*>(&PySuper_Type)),
        stolen_t{}
    ) {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
    }

    /* Explicit constructor.  Equivalent to Python `super(type, self)` with 2
    arguments. */
    explicit Super(const Type& type, const Handle& self) :
        Base(PyObject_CallFunctionObjArgs(
            reinterpret_cast<PyObject*>(&PySuper_Type),
            type.ptr(),
            self.ptr(),
            nullptr
        ), stolen_t{})
    {
        if (m_ptr == nullptr) {
            Exception::from_python();
        }
    }

};


}  // namespace py
}  // namespace bertrand


#endif  // BERTRAND_PYTHON_TYPE_H

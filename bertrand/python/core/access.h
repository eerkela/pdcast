#ifndef BERTRAND_CORE_ACCESS_H
#define BERTRAND_CORE_ACCESS_H

#include "declarations.h"
#include "object.h"
#include "except.h"
#include "ops.h"


namespace py {


////////////////////////////////
////    ATTRIBUTES/ITEMS    ////
////////////////////////////////


namespace impl {

    /* A proxy for the result of an attribute lookup that is controlled by the
    `__getattr__`, `__setattr__`, and `__delattr__` control structs.

    This is a simple extension of an Object type that intercepts `operator=` and
    assigns the new value back to the attribute using the appropriate API.  Mutating
    the object in any other way will also modify it in-place on the parent. */
    template <typename Self, StaticStr Name>
        requires (__getattr__<Self, Name>::enable)
    struct Attr : std::remove_cvref_t<typename __getattr__<Self, Name>::type> {
    private:
        using Base = std::remove_cvref_t<typename __getattr__<Self, Name>::type>;
        static_assert(
            std::derived_from<Base, Object>,
            "Default attribute access operator must return a subclass of py::Object.  "
            "Check your specialization of __getattr__ for this type and ensure the "
            "Return type derives from py::Object, or define a custom call operator "
            "to override this behavior."
        );

        template <typename S, StaticStr N> requires (__delattr__<S, N>::enable)
        friend void del(Attr<S, N>&& item);
        template <inherits<Object> T>
        friend PyObject* ptr(T&);
        template <inherits<Object> T>
            requires (!std::is_const_v<std::remove_reference_t<T>>)
        friend PyObject* release(T&&);
        template <std::derived_from<Object> T>
        friend T reinterpret_borrow(PyObject*);
        template <std::derived_from<Object> T>
        friend T reinterpret_steal(PyObject*);
        template <typename T>
        friend auto& unwrap(T& obj);
        template <typename T>
        friend const auto& unwrap(const T& obj);

        /* m_self inherits the same const/volatile/reference qualifiers as the original
        object. */
        Self m_self;

        /* The wrapper's `m_ptr` member is lazily evaluated to avoid repeated lookups.
        Replacing it with a computed property will trigger a __getattr__ lookup the
        first time it is accessed. */
        __declspec(property(get = _get_ptr, put = _set_ptr)) PyObject* m_ptr;
        void _set_ptr(PyObject* value) { Base::m_ptr = value; }
        PyObject* _get_ptr() {
            if (Base::m_ptr == nullptr) {
                if constexpr (has_call_operator<__getattr__<Self, Name>>) {
                    Base::m_ptr = release(__getattr__<Self, Name>{}(
                        std::forward<Self>(m_self))
                    );
                } else {
                    PyObject* result = PyObject_GetAttr(
                        ptr(m_self),
                        TemplateString<Name>::ptr
                    );
                    if (result == nullptr) {
                        Exception::from_python();
                    }
                    Base::m_ptr = result;
                }
            }
            return Base::m_ptr;
        }

    public:

        Attr(Self&& self) :
            Base(nullptr, Object::stolen_t{}), m_self(std::forward<Self>(self))
        {}
        Attr(const Attr& other) = delete;
        Attr(Attr&& other) = delete;

        template <typename S, typename Value>
            requires (
                std::is_lvalue_reference_v<S> ||
                !__setattr__<Self, Name, Value>::enable
            )
        Attr& operator=(this S&& self, Value&& value) = delete;
        template <typename S, typename Value>
            requires (
                !std::is_lvalue_reference_v<S> &&
                __setattr__<Self, Name, Value>::enable
            )
        Attr& operator=(this S&& self, Value&& value) {
            using setattr = __setattr__<Self, Name, Value>;
            using Return = typename setattr::type;
            static_assert(
                std::is_void_v<Return>,
                "attribute assignment operator must return void.  Check your "
                "specialization of __setattr__ for these types and ensure the Return "
                "type is set to void."
            );
            if constexpr (has_call_operator<setattr>) {
                setattr{}(std::forward<Self>(self.m_self), std::forward<Value>(value));

            } else if constexpr (has_cpp<Base>) {
                unwrap(self) = unwrap(std::forward<Value>(value));

            } else {
                Base::operator=(std::forward<Value>(value));
                if (PyObject_SetAttr(
                    ptr(self.m_self),
                    TemplateString<Name>::ptr,
                    ptr(self)
                )) {
                    Exception::from_python();
                }
            }
            return self;
        }

    };

    /* A proxy for an item in a Python container that is controlled by the
    `__getitem__`, `__setitem__`, and `__delitem__` control structs.

    This is a simple extension of an Object type that intercepts `operator=` and
    assigns the new value back to the container using the appropriate API.  Mutating
    the object in any other way will also modify it in-place within the container. */
    template <typename Self, typename... Key>
        requires (__getitem__<Self, Key...>::enable)
    struct Item : __getitem__<Self, Key...>::type {
    private:
        using Base = __getitem__<Self, Key...>::type;
        static_assert(sizeof...(Key) > 0, "Item must have at least one key.");
        static_assert(
            std::derived_from<Base, Object>,
            "Default index operator must return a subclass of py::Object.  Check your "
            "specialization of __getitem__ for this type and ensure the Return type "
            "derives from py::Object, or define a custom call operator to override "
            "this behavior."
        );

        template <typename S, typename... K> requires (__delitem__<S, K...>::enable)
        friend void del(Item<S, K...>&& item);
        template <inherits<Object> T>
        friend PyObject* ptr(T&);
        template <inherits<Object> T>
            requires (!std::is_const_v<std::remove_reference_t<T>>)
        friend PyObject* release(T&&);
        template <std::derived_from<Object> T>
        friend T reinterpret_borrow(PyObject*);
        template <std::derived_from<Object> T>
        friend T reinterpret_steal(PyObject*);
        template <typename T>
        friend auto& unwrap(T& obj);
        template <typename T>
        friend const auto& unwrap(const T& obj);

        template <typename... Ts>
        struct KeyType {
            template <typename T>
            struct wrap_references {
                using type = T;
            };
            template <typename T> requires (std::is_reference_v<T>)
            struct wrap_references<T> {
                using type = std::reference_wrapper<std::remove_reference_t<T>>;
            };
            using type = std::tuple<wrap_references<Ts>...>;
        };
        template <typename T>
        struct KeyType<T> {
            using type = T;
        };
        using M_Key = KeyType<Key...>::type;

        /* m_self inherits the same const/volatile/reference qualifiers as the original
        object.  The keys are either moved or copied into m_key if it is a tuple, or
        directly references similar to m_self if it is a single value. */
        Self m_self;
        M_Key m_key;

        /* When the key is stored as a tuple, there needs to be an extra coercion step
        to convert the `reference_wrapper`s back into the original references, and to
        move the keys that were originally supplied as raw values or rvalue
        references. */
        template <size_t I>
        struct maybe_move {
            using type = unpack_type<I, Key...>;
            decltype(auto) static operator()(M_Key& keys) {
                if constexpr (std::is_lvalue_reference_v<type>) {
                    return std::get<I>(keys).get();
                } else if constexpr (std::is_rvalue_reference_v<type>) {
                    return std::move(std::get<I>(keys).get());
                } else {
                    return std::move(std::get<I>(keys));
                }
            }
        };

        /* The wrapper's `m_ptr` member is lazily evaluated to avoid repeated lookups.
        Replacing it with a computed property will trigger a __getitem__ lookup the
        first time it is accessed. */
        __declspec(property(get = _get_ptr, put = _set_ptr)) PyObject* m_ptr;
        void _set_ptr(PyObject* value) { Base::m_ptr = value; }
        PyObject* _get_ptr() {
            if (Base::m_ptr == nullptr) {
                using getitem = __getitem__<Self, Key...>;
                PyObject* result;
                if constexpr (sizeof...(Key) == 1) {
                    if constexpr (has_call_operator<getitem>) {
                        result = release(
                            getitem{}(
                                std::forward<Self>(m_self),
                                std::forward<M_Key>(m_key)
                            )
                        );
                    } else {
                        result = PyObject_GetItem(
                            ptr(m_self),
                            ptr(as_object(std::forward<M_Key>(m_key)))
                        );
                        if (result == nullptr) {
                            Exception::from_python();
                        }
                    }

                } else {
                    if constexpr (has_call_operator<getitem>) {
                        [&]<size_t... I>(std::index_sequence<I...>) {
                            result = release(
                                getitem{}(
                                    std::forward<Self>(m_self),
                                    maybe_move<I>{}(m_key)...
                                )
                            );
                        }(std::index_sequence_for<Key...>{});
                    } else {
                        result = PyObject_GetItem(
                            ptr(m_self),
                            ptr(as_object(m_key))
                        );
                        if (result == nullptr) {
                            Exception::from_python();
                        }
                    }
                }
                Base::m_ptr = result;
            }
            return Base::m_ptr;
        }

    public:

        Item(Self&& self, Key&&... key) :
            Base(nullptr, Object::stolen_t{}), m_self(std::forward<Self>(self)),
            m_key(std::forward<Key>(key)...)
        {}
        Item(const Item& other) = delete;
        Item(Item&& other) = delete;

        template <typename S, typename Value>
            requires (
                std::is_lvalue_reference_v<S> ||
                !__setitem__<Self, Value, Key...>::enable
            )
        Item& operator=(this S&& self, Value&& other) = delete;
        template <typename S, typename Value>
            requires (
                !std::is_lvalue_reference_v<S> &&
                __setitem__<Self, Value, Key...>::enable
            )
        Item& operator=(this S&& self, Value&& value) {
            using setitem = __setitem__<Self, Value, Key...>;
            using Return = typename setitem::type;
            static_assert(
                std::is_void_v<Return>,
                "index assignment operator must return void.  Check your "
                "specialization of __setitem__ for these types and ensure the Return "
                "type is set to void."
            );
            /// TODO: all custom __setitem__ operators must reverse the order of the
            /// value and keys.  Also, they will only ever be called with the
            /// value as a python object.
            if constexpr (sizeof...(Key) == 1) {
                if constexpr (has_call_operator<setitem>) {
                    setitem{}(
                        std::forward<Self>(self.m_self),
                        std::forward<Value>(value),
                        std::forward<M_Key>(self.m_key)
                    );
                } else if constexpr (has_cpp<Base>) {
                    static_assert(
                        supports_item_assignment<Base, Value, Key...>,
                        "__setitem__<Self, Value, Key...> is enabled for operands "
                        "whose C++ representations have no viable overload for "
                        "`Self[Key...] = Value`"
                    );
                    unwrap(self) = unwrap(std::forward<Value>(value));

                } else {
                    Base::operator=(std::forward<Value>(value));
                    if (PyObject_SetItem(
                        ptr(self.m_self),
                        ptr(as_object(std::forward<M_Key>(self.m_key))),
                        ptr(self)
                    )) {
                        Exception::from_python();
                    }
                }

            } else {
                if constexpr (has_call_operator<setitem>) {
                    [&]<size_t... I>(std::index_sequence<I...>) {
                        setitem{}(
                            std::forward<Self>(self.m_self),
                            std::forward<Value>(value),
                            maybe_move<I>{}(self.m_key)...
                        );
                    }(std::index_sequence_for<Key...>{});
                } else if constexpr (has_cpp<Base>) {
                    static_assert(
                        supports_item_assignment<Base, Value, Key...>,
                        "__setitem__<Self, Value, Key...> is enabled for operands "
                        "whose C++ representations have no viable overload for "
                        "`Self[Key...] = Value`"
                    );
                    unwrap(self) = unwrap(std::forward<Value>(value));

                } else {
                    Base::operator=(std::forward<Value>(value));
                    if (PyObject_SetItem(
                        ptr(self.m_self),
                        ptr(as_object(self.m_key)),
                        ptr(self)
                    )) {
                        Exception::from_python();
                    }
                }
            }
            return self;
        }

    };

}


template <typename Self, typename... Key> requires (__getitem__<Self, Key...>::enable)
decltype(auto) Object::operator[](this Self&& self, Key&&... key) {
    using getitem = __getitem__<Self, Key...>;
    if constexpr (std::derived_from<typename getitem::type, Object>) {
        return impl::Item<Self, Key...>(
            std::forward<Self>(self),
            std::forward<Key>(key)...
        );
    } else {
        static_assert(
            std::is_invocable_r_v<typename getitem::type, getitem, const Self&, Key...>,
            "__getitem__ is specialized to return a C++ value, but the call operator "
            "does not accept the correct arguments.  Check your specialization of "
            "__getitem__ for these types and ensure a call operator is defined that "
            "accepts these arguments."
        );
        return getitem{}(std::forward<Self>(self), std::forward<Key>(key)...);
    }
}


/* Replicates Python's `del` keyword for attribute and item deletion.  Note that the
usage of `del` to dereference naked Python objects is not supported - only those uses
which would translate to a `PyObject_DelAttr()` or `PyObject_DelItem()` are considered
valid. */
template <typename Self, StaticStr Name> requires (__delattr__<Self, Name>::enable)
void del(impl::Attr<Self, Name>&& attr) {
    using delattr = __delattr__<Self, Name>;
    using Return = delattr::type;
    static_assert(
        std::is_void_v<Return>,
        "index deletion operator must return void.  Check your specialization "
        "of __delitem__ for these types and ensure the Return type is set to void."
    );
    if constexpr (impl::has_call_operator<delattr>) {
        delattr{}(std::forward<Self>(attr.m_self));
    } else {
        if (PyObject_DelAttr(
            ptr(attr.m_self),
            impl::TemplateString<Name>::ptr
        )) {
            Exception::from_python();
        }
    }
}


/* Replicates Python's `del` keyword for attribute and item deletion.  Note that the
usage of `del` to dereference naked Python objects is not supported - only those uses
which would translate to a `PyObject_DelAttr()` or `PyObject_DelItem()` are considered
valid. */
template <typename Self, typename... Key> requires (__delitem__<Self, Key...>::enable)
void del(impl::Item<Self, Key...>&& item) {
    using delitem = __delitem__<Self, Key...>;
    using Return = delitem::type;
    static_assert(
        std::is_void_v<Return>,
        "index deletion operator must return void.  Check your specialization "
        "of __delitem__ for these types and ensure the Return type is set to void."
    );
    if constexpr (sizeof...(Key) == 1) {
        if constexpr (impl::has_call_operator<delitem>) {
            delitem{}(
                std::forward<Self>(item.m_self),
                std::forward<Key>(item.m_key)...
            );
        } else {
            if (PyObject_DelItem(
                ptr(item.m_self),
                ptr(as_object(std::forward<Key>(item.m_key)))...
            )) {
                Exception::from_python();
            }
        }

    } else {
        if constexpr (impl::has_call_operator<delitem>) {
            [&]<size_t... I>(std::index_sequence<I...>) {
                delitem{}(
                    std::forward<Self>(item.m_self),
                    typename impl::Item<Self, Key...>::template maybe_move<I>{}(
                        item.m_key
                    )...
                );
            }(std::index_sequence_for<Key...>{});
        } else {
            if (PyObject_DelItem(
                ptr(item.m_self),
                ptr(as_object(item.m_key))
            )) {
                Exception::from_python();
            }
        }
    }
}


template <impl::lazily_evaluated From, typename To>
    requires (std::convertible_to<impl::lazy_type<From>, To>)
struct __cast__<From, To>                                   : Returns<To> {
    static To operator()(From&& item) {
        if constexpr (impl::has_cpp<impl::lazy_type<From>>) {
            return impl::implicit_cast<To>(unwrap(std::forward<From>(item)));
        } else {
            return impl::implicit_cast<To>(
                reinterpret_steal<impl::lazy_type<From>>(ptr(item))
            );
        }
    }
};


template <impl::lazily_evaluated From, typename To>
    requires (impl::explicitly_convertible_to<impl::lazy_type<From>, To>)
struct __explicit_cast__<From, To>                          : Returns<To> {
    static To operator()(const From& item) {
        if constexpr (impl::has_cpp<impl::lazy_type<From>>) {
            return static_cast<To>(unwrap(item));
        } else {
            return static_cast<To>(
                reinterpret_steal<impl::lazy_type<From>>(ptr(item))
            );
        }
    }
};


template <typename T, impl::lazily_evaluated Base>
struct __isinstance__<T, Base> : __isinstance__<T, impl::lazy_type<Base>> {};
template <typename T, impl::lazily_evaluated Base>
struct __issubclass__<T, Base> : __issubclass__<T, impl::lazy_type<Base>> {};
template <impl::lazily_evaluated Base, StaticStr Name>
struct __getattr__<Base, Name> : __getattr__<impl::lazy_type<Base>, Name> {};
template <impl::lazily_evaluated Base, StaticStr Name, typename Value>
struct __setattr__<Base, Name, Value> : __setattr__<impl::lazy_type<Base>, Name, Value> {};
template <impl::lazily_evaluated Base, StaticStr Name>
struct __delattr__<Base, Name> : __delattr__<impl::lazy_type<Base>, Name> {};
template <impl::lazily_evaluated Base, typename... Key>
struct __getitem__<Base, Key...> : __getitem__<impl::lazy_type<Base>, Key...> {};
template <impl::lazily_evaluated Base, typename Value, typename... Key>
struct __setitem__<Base, Value, Key...> : __setitem__<impl::lazy_type<Base>, Value, Key...> {};
template <impl::lazily_evaluated Base, typename... Key>
struct __delitem__<Base, Key...> : __delitem__<impl::lazy_type<Base>, Key...> {};
template <impl::lazily_evaluated Base>
struct __len__<Base> : __len__<impl::lazy_type<Base>> {};
template <impl::lazily_evaluated Base>
struct __iter__<Base> : __iter__<impl::lazy_type<Base>> {};
template <impl::lazily_evaluated Base>
struct __reversed__<Base> : __reversed__<impl::lazy_type<Base>> {};
template <impl::lazily_evaluated Base, typename Key>
struct __contains__<Base, Key> : __contains__<impl::lazy_type<Base>, Key> {};
template <impl::lazily_evaluated Base>
struct __hash__<Base> : __hash__<impl::lazy_type<Base>> {};
template <impl::lazily_evaluated Base>
struct __abs__<Base> : __abs__<impl::lazy_type<Base>> {};
template <impl::lazily_evaluated Base>
struct __invert__<Base> : __invert__<impl::lazy_type<Base>> {};
template <impl::lazily_evaluated Base>
struct __pos__<Base> : __pos__<impl::lazy_type<Base>> {};
template <impl::lazily_evaluated Base>
struct __neg__<Base> : __neg__<impl::lazy_type<Base>> {};
template <impl::lazily_evaluated Base>
struct __increment__<Base> : __increment__<impl::lazy_type<Base>> {};
template <impl::lazily_evaluated Base>
struct __decrement__<Base> : __decrement__<impl::lazy_type<Base>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __lt__<L, R> : __lt__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __lt__<L, R> : __lt__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __lt__<L, R> : __lt__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __le__<L, R> : __le__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __le__<L, R> : __le__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __le__<L, R> : __le__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __eq__<L, R> : __eq__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __eq__<L, R> : __eq__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __eq__<L, R> : __eq__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __ne__<L, R> : __ne__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __ne__<L, R> : __ne__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __ne__<L, R> : __ne__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __ge__<L, R> : __ge__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __ge__<L, R> : __ge__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __ge__<L, R> : __ge__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __gt__<L, R> : __gt__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __gt__<L, R> : __gt__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __gt__<L, R> : __gt__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __add__<L, R> : __add__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __add__<L, R> : __add__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __add__<L, R> : __add__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __sub__<L, R> : __sub__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __sub__<L, R> : __sub__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __sub__<L, R> : __sub__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __mul__<L, R> : __mul__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __mul__<L, R> : __mul__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __mul__<L, R> : __mul__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __truediv__<L, R> : __truediv__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __truediv__<L, R> : __truediv__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __truediv__<L, R> : __truediv__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __floordiv__<L, R> : __floordiv__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __floordiv__<L, R> : __floordiv__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __floordiv__<L, R> : __floordiv__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __mod__<L, R> : __mod__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __mod__<L, R> : __mod__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __mod__<L, R> : __mod__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __pow__<L, R> : __pow__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __pow__<L, R> : __pow__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __pow__<L, R> : __pow__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __lshift__<L, R> : __lshift__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __lshift__<L, R> : __lshift__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __lshift__<L, R> : __lshift__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __rshift__<L, R> : __rshift__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __rshift__<L, R> : __rshift__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __rshift__<L, R> : __rshift__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __and__<L, R> : __and__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __and__<L, R> : __and__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __and__<L, R> : __and__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __xor__<L, R> : __xor__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __xor__<L, R> : __xor__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __xor__<L, R> : __xor__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __or__<L, R> : __or__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __or__<L, R> : __or__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __or__<L, R> : __or__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __iadd__<L, R> : __iadd__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __iadd__<L, R> : __iadd__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __iadd__<L, R> : __iadd__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __isub__<L, R> : __isub__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __isub__<L, R> : __isub__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __isub__<L, R> : __isub__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __imul__<L, R> : __imul__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __imul__<L, R> : __imul__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __imul__<L, R> : __imul__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __itruediv__<L, R> : __itruediv__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __itruediv__<L, R> : __itruediv__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __itruediv__<L, R> : __itruediv__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __ifloordiv__<L, R> : __ifloordiv__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __ifloordiv__<L, R> : __ifloordiv__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __ifloordiv__<L, R> : __ifloordiv__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __imod__<L, R> : __imod__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __imod__<L, R> : __imod__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __imod__<L, R> : __imod__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __ipow__<L, R> : __ipow__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __ipow__<L, R> : __ipow__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __ipow__<L, R> : __ipow__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __ilshift__<L, R> : __ilshift__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __ilshift__<L, R> : __ilshift__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __ilshift__<L, R> : __ilshift__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __irshift__<L, R> : __irshift__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __irshift__<L, R> : __irshift__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __irshift__<L, R> : __irshift__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __iand__<L, R> : __iand__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __iand__<L, R> : __iand__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __iand__<L, R> : __iand__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __ixor__<L, R> : __ixor__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __ixor__<L, R> : __ixor__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __ixor__<L, R> : __ixor__<L, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, impl::lazily_evaluated R>
struct __ior__<L, R> : __ior__<impl::lazy_type<L>, impl::lazy_type<R>> {};
template <impl::lazily_evaluated L, typename R> requires (!impl::lazily_evaluated<R>)
struct __ior__<L, R> : __ior__<impl::lazy_type<L>, R> {};
template <typename L, impl::lazily_evaluated R> requires (!impl::lazily_evaluated<L>)
struct __ior__<L, R> : __ior__<L, impl::lazy_type<R>> {};


/////////////////////////
////    ITERATORS    ////
/////////////////////////


template <typename Begin, typename End = void, typename Container = void>
struct Iterator;


template <typename Begin, typename End, typename Container>
struct Interface<Iterator<Begin, End, Container>> : impl::IterTag {
    using begin_t = Begin;
    using end_t = End;
    using container_t = Container;

    decltype(auto) __iter__(this auto&& self);
    decltype(auto) __next__(this auto&& self);
};
template <typename Begin, typename End, typename Container>
struct Interface<Type<Iterator<Begin, End, Container>>> {
    using begin_t = Begin;
    using end_t = End;
    using container_t = Container;

    static decltype(auto) __iter__(auto&& self) { return self.__iter__(); }
    static decltype(auto) __next__(auto&& self) { return self.__next__(); }
};


/* A wrapper around a Python iterator that allows it to be used from C++.

This type has no fixed implementation, and can match any kind of Python iterator.  It
roughly corresponds to the `collections.abc.Iterator` abstract base class in Python,
and allows C++ to call the Python-level `__next__()` hook.  Note that the reverse
(exposing C++ iterators to Python) is done via a separate specialization.

In the interest of performance, no explicit checks are done to ensure that the return
type matches expectations.  As such, this class is one of the rare cases where type
safety may be violated, and should therefore be used with caution.  It is mostly meant
for internal use to back the default result of the `begin()` and `end()` operators when
no specialized C++ iterator can be found.  In that case, its value type is set to the
`T` in an `__iter__<Container> : Returns<T> {};` specialization.  If you want to use
this class and avoid type safety issues, leave the return type set to `Object` (the
default), which will incur a runtime check on conversion. */
template <impl::python Return>
struct Iterator<Return, void, void> : Object, Interface<Iterator<Return, void, void>> {
    struct __python__ : def<__python__, Iterator>, PyObject {
        static Type<Iterator> __import__() {
            PyObject* collections_abc = PyImport_Import(
                impl::TemplateString<"collections.abc">::ptr
            );
            if (collections_abc == nullptr) {
                Exception::from_python();
            }
            PyObject* iterator = PyObject_GetAttr(
                collections_abc,
                impl::TemplateString<"Iterator">::ptr
            );
            Py_DECREF(collections_abc);
            if (iterator == nullptr) {
                Exception::from_python();
            }
            return reinterpret_steal<Type<Iterator>>(iterator);
        }
    };

    Iterator(PyObject* p, borrowed_t t) : Object(p, t) {}
    Iterator(PyObject* p, stolen_t t) : Object(p, t) {}

    template <typename T = Iterator> requires (__initializer__<T>::enable)
    Iterator(const std::initializer_list<typename __initializer__<T>::type>& init) :
        Object(__initializer__<T>{}(init))
    {}

    template <typename... Args> requires (implicit_ctor<Iterator>::template enable<Args...>)
    Iterator(Args&&... args) : Object(
        implicit_ctor<Iterator>{},
        std::forward<Args>(args)...
    ) {}

    template <typename... Args> requires (explicit_ctor<Iterator>::template enable<Args...>)
    explicit Iterator(Args&&... args) : Object(
        explicit_ctor<Iterator>{},
        std::forward<Args>(args)...
    ) {}

};


/* A wrapper around a non-ownding C++ range that allows them to be iterated over from
Python.

This will instantiate a unique Python type with an appropriate `__next__()` method for
every combination of C++ iterators, forwarding to their respective `operator*()`,
`operator++()`, and `operator==()` methods. */
template <std::input_iterator Begin, std::sentinel_for<Begin> End>
    requires (std::convertible_to<decltype(*std::declval<Begin>()), Object>)
struct Iterator<Begin, End, void> : Object, Interface<Iterator<Begin, End, void>> {
    struct __python__ : def<__python__, Iterator>, PyObject {
        inline static bool initialized = false;
        static PyTypeObject __type__;

        std::remove_reference_t<Begin> begin;
        std::remove_reference_t<End> end;

        __python__(auto& container) :
            begin(std::ranges::begin(container)), end(std::ranges::end(container))
        {
            ready();
        }

        __python__(Begin&& begin, End&& end) :
            begin(std::forward(begin)), end(std::forward(end))
        {
            ready();
        }

        static Type<Iterator> __import__() {
            ready();
            return reinterpret_borrow<Type<Iterator>>(&__type__);
        }

        static PyObject* __next__(__python__* self) {
            try {
                if (self->begin == self->end) {
                    return nullptr;
                }
                if constexpr (std::is_lvalue_reference_v<decltype(*(self->begin))>) {
                    auto result = wrap(*(self->begin));  // non-owning obj
                    ++(self->begin);
                    return reinterpret_cast<PyObject*>(release(result));
                } else {
                    auto result = as_object(*(self->begin));  // owning obj
                    ++(self->begin);
                    return reinterpret_cast<PyObject*>(release(result));
                }
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

    private:

        static void ready() {
            if (!initialized) {
                __type__ = {
                    .tp_name = typeid(Iterator).name(),
                    .tp_basicsize = sizeof(__python__),
                    .tp_itemsize = 0,
                    .tp_flags = 
                        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
                    .tp_iter = PyObject_SelfIter,
                    .tp_iternext = reinterpret_cast<iternextfunc>(__next__)
                };
                if (PyType_Ready(&__type__) < 0) {
                    Exception::from_python();
                }
                initialized = true;
            }
        }

    };

    Iterator(PyObject* p, borrowed_t t) : Object(p, t) {}
    Iterator(PyObject* p, stolen_t t) : Object(p, t) {}

    template <typename T = Iterator> requires (__initializer__<T>::enable)
    Iterator(const std::initializer_list<typename __initializer__<T>::type>& init) :
        Object(__initializer__<T>{}(init))
    {}

    template <typename... Args> requires (implicit_ctor<Iterator>::template enable<Args...>)
    Iterator(Args&&... args) : Object(
        implicit_ctor<Iterator>{},
        std::forward<Args>(args)...
    ) {}

    template <typename... Args> requires (explicit_ctor<Iterator>::template enable<Args...>)
    explicit Iterator(Args&&... args) : Object(
        explicit_ctor<Iterator>{},
        std::forward<Args>(args)...
    ) {}

};


/* A wrapper around an owning C++ range that was generated from a temporary container.
The container is moved into the Python iterator object and will remain valid as long as
the iterator object has a nonzero reference count.

This will instantiate a unique Python type with an appropriate `__next__()` method for
every combination of C++ iterators, forwarding to their respective `operator*()`,
`operator++()`, and `operator==()` methods. */
template <std::input_iterator Begin, std::sentinel_for<Begin> End, impl::iterable Container>
    requires (std::convertible_to<decltype(*std::declval<Begin>()), Object>)
struct Iterator<Begin, End, Container> : Object, Interface<Iterator<Begin, End, Container>> {
    struct __python__ : def<__python__, Iterator>, PyObject {
        inline static bool initialized = false;
        static PyTypeObject __type__;

        Container container;
        Begin begin;
        End end;

        __python__(Container&& container) :
            container(std::move(container)),
            begin(std::ranges::begin(this->container)),
            end(std::ranges::end(this->container))
        {
            ready();
        }

        static Type<Iterator> __import__() {
            ready();
            return reinterpret_borrow<Type<Iterator>>(&__type__);
        }

        /// TODO: what if the container yields Python objects?  What about references
        /// to Python objects?

        static PyObject* __next__(__python__* self) {
            try {
                if (self->begin == self->end) {
                    return nullptr;
                }
                if constexpr (std::is_lvalue_reference_v<decltype(*(self->begin))>) {
                    auto result = wrap(*(self->begin));  // non-owning obj
                    ++(self->begin);
                    return reinterpret_cast<PyObject*>(release(result));
                } else {
                    auto result = as_object(*(self->begin));  // owning obj
                    ++(self->begin);
                    return reinterpret_cast<PyObject*>(release(result));
                }
            } catch (...) {
                Exception::to_python();
                return nullptr;
            }
        }

    private:

        static void ready() {
            if (!initialized) {
                __type__ = {
                    .tp_name = typeid(Iterator).name(),
                    .tp_basicsize = sizeof(__python__),
                    .tp_itemsize = 0,
                    .tp_flags = 
                        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_DISALLOW_INSTANTIATION,
                    .tp_iter = PyObject_SelfIter,
                    .tp_iternext = reinterpret_cast<iternextfunc>(__next__)
                };
                if (PyType_Ready(&__type__) < 0) {
                    Exception::from_python();
                }
                initialized = true;
            }
        }
    };

    Iterator(PyObject* p, borrowed_t t) : Object(p, t) {}
    Iterator(PyObject* p, stolen_t t) : Object(p, t) {}

    template <typename T = Iterator> requires (__initializer__<T>::enable)
    Iterator(const std::initializer_list<typename __initializer__<T>::type>& init) :
        Object(__initializer__<T>{}(init))
    {}

    template <typename... Args> requires (implicit_ctor<Iterator>::template enable<Args...>)
    Iterator(Args&&... args) : Object(
        implicit_ctor<Iterator>{},
        std::forward<Args>(args)...
    ) {}

    template <typename... Args> requires (explicit_ctor<Iterator>::template enable<Args...>)
    explicit Iterator(Args&&... args) : Object(
        explicit_ctor<Iterator>{},
        std::forward<Args>(args)...
    ) {}
};


namespace impl {

    template <typename Container>
    struct IterTraits {
        using begin = decltype(std::ranges::begin(std::declval<std::remove_reference_t<Container>&>()));
        using end = decltype(std::ranges::end(std::declval<std::remove_reference_t<Container&>&>()));
        using container = std::remove_reference_t<Container>;
    };

    template <typename Container> requires (std::is_lvalue_reference_v<Container>)
    struct IterTraits<Container> {
        using begin = decltype(std::ranges::begin(std::declval<Container>()));
        using end = decltype(std::ranges::end(std::declval<Container>()));
        using container = void;
    };

}


/* CTAD guide will generate a Python iterator around a pair of raw C++ iterators. */
template <std::input_iterator Begin, std::sentinel_for<Begin> End>
    requires (std::convertible_to<decltype(*std::declval<Begin>()), Object>)
Iterator(Begin, End) -> Iterator<Begin, End, void>;


/* CTAD guide will generate a Python iterator from an arbitrary C++ container, with
correct ownership semantics. */
template <impl::iterable Container> requires (impl::yields<Container, Object>)
Iterator(Container&&) -> Iterator<
    typename impl::IterTraits<Container>::begin,
    typename impl::IterTraits<Container>::end,
    typename impl::IterTraits<Container>::container
>;


/* Implement the CTAD guide for iterable containers.  The container type may be const,
which will be reflected in the deduced iterator types. */
template <impl::iterable Container> requires (impl::yields<Container, Object>)
struct __init__<
    Iterator<
        typename impl::IterTraits<Container>::begin,
        typename impl::IterTraits<Container>::end,
        typename impl::IterTraits<Container>::container
    >,
    Container
> : Returns<Iterator<
    typename impl::IterTraits<Container>::begin,
    typename impl::IterTraits<Container>::end,
    typename impl::IterTraits<Container>::container
>> {
    static auto operator()(Container&& self) {
        return impl::construct<Iterator<
            typename impl::IterTraits<Container>::begin,
            typename impl::IterTraits<Container>::end,
            typename impl::IterTraits<Container>::container
        >>(std::forward<Container>(self));
    }
};


/* Construct a Python iterator from a pair of C++ iterators. */
template <std::input_iterator Begin, std::sentinel_for<Begin> End>
    requires (std::convertible_to<decltype(*std::declval<Begin>()), Object>)
struct __init__<Iterator<Begin, End, void>, Begin, End> : Returns<Iterator<Begin, End, void>> {
    static auto operator()(auto&& begin, auto&& end) {
        return impl::construct<Iterator<Begin, End, void>>(
            std::forward<decltype(begin)>(begin),
            std::forward<decltype(end)>(end)
        );
    }
};


template <impl::python T, impl::python Return>
struct __isinstance__<T, Iterator<Return, void, void>>      : Returns<bool> {
    static constexpr bool operator()(T&& obj) {
        if constexpr (impl::dynamic<T>) {
            return PyIter_Check(ptr(obj));
        } else {
            return issubclass<T, Iterator<Return, void, void>>();
        }
    }
};


template <impl::python T, impl::python Return>
struct __issubclass__<T, Iterator<Return, void, void>>      : Returns<bool> {
    static constexpr bool operator()() {
        return
            impl::inherits<T, impl::IterTag> &&
            std::convertible_to<impl::iter_type<T>, Return>;
    }
};


template <
    impl::python T,
    std::input_iterator Begin,
    std::sentinel_for<Begin> End,
    typename Container
>
struct __isinstance__<T, Iterator<Begin, End, Container>>   : Returns<bool> {};


template <
    impl::python T,
    std::input_iterator Begin,
    std::sentinel_for<Begin> End,
    typename Container
>
struct __issubclass__<T, Iterator<Begin, End, Container>>   : Returns<bool> {};


/* Traversing a Python iterator requires a customized C++ iterator type. */
template <typename T>
struct __iter__<Iterator<T, void, void>>                    : Returns<T> {
    using iterator_category = std::input_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = T;
    using pointer           = T*;
    using reference         = T&;

    Iterator<T> iter;
    T curr;

    __iter__(const Iterator<T>& self) :
        iter(self), curr(reinterpret_steal<T>(nullptr))
    {}
    __iter__(Iterator<T>&& self) :
        iter(std::move(self)), curr(reinterpret_steal<T>(nullptr))
    {}

    __iter__(const Iterator<T>& self, bool) : __iter__(self) { ++(*this); }
    __iter__(Iterator<T>&& self, bool) : __iter__(std::move(self)) { ++(*this); }

    __iter__(const __iter__&) = delete;
    __iter__(__iter__&& other) :
        iter(std::move(other.iter)), curr(std::move(other.curr))
    {}

    __iter__& operator=(const __iter__&) = delete;
    __iter__& operator=(__iter__&& other) {
        if (&other != this) {
            iter = std::move(other.iter);
            curr = std::move(other.curr);
        }
        return *this;
    }

    [[nodiscard]] T& operator*() { return curr; }
    [[nodiscard]] T* operator->() { return &curr; }
    [[nodiscard]] const T& operator*() const { return curr; }
    [[nodiscard]] const T* operator->() const { return &curr; }

    __iter__& operator++() {
        PyObject* next = PyIter_Next(ptr(iter));
        if (PyErr_Occurred()) {
            Exception::from_python();
        }
        curr = reinterpret_steal<T>(next);
        return *this;
    }

    [[nodiscard]] bool operator==(const __iter__& other) const {
        return ptr(curr) == ptr(other.curr);
    }

    [[nodiscard]] bool operator!=(const __iter__& other) const {
        return ptr(curr) != ptr(other.curr);
    }

};
/// NOTE: can't iterate over a const Iterator<T> because the iterator itself must be
/// mutable.


/* py::Iterator<Begin, End, ...> is special cased in the begin() and end() operators to
extract the internal C++ iterators rather than creating yet another layer of
indirection. */
template <std::input_iterator Begin, std::sentinel_for<Begin> End, typename Container>
struct __iter__<Iterator<Begin, End, Container>> : Returns<decltype(*std::declval<Begin>())> {};


template <typename T, typename Begin, typename End, typename Container>
struct __contains__<T, Iterator<Begin, End, Container>> : Returns<bool> {};


/// TODO: the attributes can only be defined after functions are defined


// template <impl::inherits<impl::IterTag> Self>
// struct __getattr__<Self, "__iter__"> : Returns<
//     Function<impl::qualify<Self(std::remove_cvref_t<Self>::*)(), Self>>
// > {};
// template <impl::inherits<impl::IterTag> Self>
// struct __getattr__<Self, "__next__"> : Returns<
//     Function<impl::qualify<
//         std::conditional_t<
//             std::is_void_v<typename std::remove_reference_t<Self>::end_t>,
//             std::remove_reference_t<decltype(
//                 *std::declval<typename std::remove_reference_t<Self>::begin_t>()
//             )>,
//             decltype(
//                 *std::declval<typename std::remove_reference_t<Self>::begin_t>()
//             )
//         >(std::remove_cvref_t<Self>::*)(),
//         Self
//     >>
// > {};
// template <impl::inherits<impl::IterTag> Self>
// struct __getattr__<Type<Self>, "__iter__"> : Returns<Function<
//     Self(*)(Self)
// >> {};
// template <impl::inherits<impl::IterTag> Self>
// struct __getattr__<Type<Self>, "__next__"> : Returns<Function<
//     std::conditional_t<
//         std::is_void_v<typename std::remove_reference_t<Self>::end_t>,
//         std::remove_reference_t<decltype(
//             *std::declval<typename std::remove_reference_t<Self>::begin_t>()
//         )>,
//         decltype(
//             *std::declval<typename std::remove_reference_t<Self>::begin_t>()
//         )
//     >(*)(Self)
// >> {};


template <typename Begin, typename End, typename Container>
decltype(auto) Interface<Iterator<Begin, End, Container>>::__iter__(this auto&& self) {
    return std::forward<decltype(self)>(self);
}


template <typename Begin, typename End, typename Container>
decltype(auto) Interface<Iterator<Begin, End, Container>>::__next__(this auto&& self) {
    using Iter = std::remove_reference_t<decltype(self)>;

    if constexpr (std::is_void_v<typename Iter::end_t>) {
        PyObject* next = PyIter_Next(ptr(self));
        if (next == nullptr) {
            if (PyErr_Occurred()) {
                Exception::from_python();
            }
            throw StopIteration();
        }
        return reinterpret_steal<typename Iter::begin_t>(next);

    } else {
        if (self->begin == self->end) {
            throw StopIteration();
        }
        ++(self->begin);
        if (self->begin == self->end) {
            throw StopIteration();
        }
        return *(self->begin);
    }
}


/* Begin iteration operator.  Both this and the end iteration operator are
controlled by the __iter__ control struct, whose return type dictates the
iterator's dereference type. */
template <typename Self> requires (__iter__<Self>::enable)
[[nodiscard]] decltype(auto) begin(Self&& self) {
    if constexpr (std::is_constructible_v<__iter__<Self>, Self, int>) {
        static_assert(
            std::is_constructible_v<__iter__<Self>, Self>,
            "__iter__<T> specializes the begin iterator, but not the end iterator.  "
            "Did you forget to define an `__iter__(T&&)` constructor?"
        );
        return __iter__<Self>(std::forward<Self>(self), 0);
    } else {
        static_assert(
            !std::is_constructible_v<__iter__<Self>, Self>,
            "__iter__<T> specializes the end iterator, but not the begin iterator.  "
            "Did you forget to define an `__iter__(T&&, int)` constructor?"
        );
        if constexpr (impl::has_cpp<Self>) {
            return std::ranges::begin(unwrap(std::forward<Self>(self)));

        } else {
            auto result = [](Self&& self) {
                using Return = typename __iter__<Self>::type;
                static_assert(
                    std::derived_from<Return, Object>,
                    "iterator must dereference to a subclass of Object.  Check your "
                    "specialization of __iter__ for this types and ensure the Return type "
                    "is a subclass of py::Object."
                );
                PyObject* iter = PyObject_GetIter(ptr(self));
                if (iter == nullptr) {
                    Exception::from_python();
                }
                return __iter__<Iterator<Return>>{
                    reinterpret_steal<Iterator<Return>>(iter),
                    0
                };
            };
            if constexpr (impl::inherits<Self, impl::IterTag>) {
                if constexpr (!std::is_void_v<typename std::remove_reference_t<Self>::end_t>) {
                    return self->begin;
                } else {
                    return result(std::forward<Self>(self));
                }
            } else {
                return result(std::forward<Self>(self));
            }
        }
    }
}


/* Const iteration operator.  Python has no distinction between mutable and
immutable iterators, so this is fundamentally the same as the ordinary begin()
method.  Some libraries assume the existence of this method. */
template <typename Self>
    requires (__iter__<Self>::enable && std::is_const_v<std::remove_reference_t<Self>>)
[[nodiscard]] decltype(auto) cbegin(Self&& self) {
    return begin(std::forward<Self>(self));
}


/* End iteration operator.  This terminates the iteration and is controlled by the
__iter__ control struct. */
template <typename Self> requires (__iter__<Self>::enable)
[[nodiscard]] decltype(auto) end(Self&& self) {
    if constexpr (std::is_constructible_v<__iter__<Self>, const Self&>) {
        static_assert(
            std::is_constructible_v<__iter__<Self>, Self, int>,
            "__iter__<T> specializes the begin iterator, but not the end iterator.  "
            "Did you forget to define an `__iter__(const T&)` constructor?"
        );
        return __iter__<Self>(std::forward<Self>(self));
    } else {
        static_assert(
            !std::is_constructible_v<__iter__<Self>, Self, int>,
            "__iter__<T> specializes the end iterator, but not the begin iterator.  "
            "Did you forget to define an `__iter__(const T&, int)` constructor?"
        );
        if constexpr (impl::has_cpp<Self>) {
            return std::ranges::end(unwrap(std::forward<Self>(self)));

        } else {
            auto result = [](Self&& self) {
                using Return = typename __iter__<Self>::type;
                static_assert(
                    std::derived_from<Return, Object>,
                    "iterator must dereference to a subclass of Object.  Check your "
                    "specialization of __iter__ for this types and ensure the Return type "
                    "is a subclass of py::Object."
                );
                return __iter__<Iterator<Return>>{
                    reinterpret_steal<Iterator<Return>>(nullptr)
                };
            };
            if constexpr (impl::inherits<Self, impl::IterTag>) {
                if constexpr (!std::is_void_v<typename std::remove_reference_t<Self>::end_t>) {
                    return self->end;
                } else {
                    return result(std::forward<Self>(self));
                }
            } else {
                return result(std::forward<Self>(self));
            }
        }
    }
}


/* Const end operator.  Similar to `cbegin()`, this is identical to `end()`. */
template <typename Self>
    requires (__iter__<Self>::enable && std::is_const_v<std::remove_reference_t<Self>>)
[[nodiscard]] decltype(auto) cend(Self&& self) {
    return end(std::forward<Self>(self));
}


/* Reverse iteration operator.  Both this and the reverse end operator are
controlled by the __reversed__ control struct, whose return type dictates the
iterator's dereference type. */
template <typename Self> requires (__reversed__<Self>::enable)
[[nodiscard]] decltype(auto) rbegin(Self&& self) {
    if constexpr (std::is_constructible_v<__reversed__<Self>, const Self&, int>) {
        static_assert(
            std::is_constructible_v<__reversed__<Self>, Self>,
            "__reversed__<T> specializes the begin iterator, but not the end "
            "iterator.  Did you forget to define a `__reversed__(const T&)` "
            "constructor?"
        );
        return __reversed__<Self>(std::forward<Self>(self), 0);
    } else {
        static_assert(
            !std::is_constructible_v<__reversed__<Self>, Self>,
            "__reversed__<T> specializes the end iterator, but not the begin "
            "iterator.  Did you forget to define a `__reversed__(const T&, int)` "
            "constructor?"
        );
        if constexpr (impl::has_cpp<Self>) {
            return std::ranges::rbegin(unwrap(std::forward<Self>(self)));

        } else {
            using Return = typename __reversed__<Self>::type;
            static_assert(
                std::derived_from<Return, Object>,
                "iterator must dereference to a subclass of Object.  Check your "
                "specialization of __reversed__ for this types and ensure the Return "
                "type is a subclass of py::Object."
            );
            PyObject* iter = PyObject_CallMethodNoArgs(
                ptr(self),
                impl::TemplateString<"__reversed__">::ptr
            );
            if (iter == nullptr) {
                Exception::from_python();
            }
            return __iter__<Iterator<Return>>{
                reinterpret_steal<Iterator<Return>>(iter),
                0
            };
        }
    }
}


/* Const reverse iteration operator.  Python has no distinction between mutable
and immutable iterators, so this is fundamentally the same as the ordinary
rbegin() method.  Some libraries assume the existence of this method. */
template <typename Self>
    requires (__reversed__<Self>::enable && std::is_const_v<std::remove_reference_t<Self>>)
[[nodiscard]] decltype(auto) crbegin(Self&& self) {
    return rbegin(std::forward<Self>(self));
}


/* Reverse end operator.  This terminates the reverse iteration and is controlled
by the __reversed__ control struct. */
template <typename Self> requires (__reversed__<Self>::enable)
[[nodiscard]] decltype(auto) rend(Self&& self) {
    if constexpr (std::is_constructible_v<__reversed__<Self>, Self>) {
        static_assert(
            std::is_constructible_v<__reversed__<Self>, Self, int>,
            "__reversed__<T> specializes the begin iterator, but not the end "
            "iterator.  Did you forget to define a `__reversed__(const T&)` "
            "constructor?"
        );
        return __reversed__<Self>(std::forward<Self>(self));
    } else {
        static_assert(
            !std::is_constructible_v<__reversed__<Self>, Self, int>,
            "__reversed__<T> specializes the end iterator, but not the begin "
            "iterator.  Did you forget to define a `__reversed__(const T&, int)` "
            "constructor?"
        );
        if constexpr (impl::has_cpp<Self>) {
            return std::ranges::rend(unwrap(std::forward<Self>(self)));

        } else {
            using Return = typename __reversed__<Self>::type;
            static_assert(
                std::derived_from<Return, Object>,
                "iterator must dereference to a subclass of Object.  Check your "
                "specialization of __reversed__ for this types and ensure the Return "
                "type is a subclass of py::Object."
            );
            return __iter__<Iterator<Return>>{
                reinterpret_steal<Iterator<Return>>(nullptr)
            };
        }
    }
}


/* Const reverse end operator.  Similar to `crbegin()`, this is identical to
`rend()`. */
template <typename Self>
    requires (__reversed__<const Self>::enable && std::is_const_v<std::remove_reference_t<Self>>)
[[nodiscard]] decltype(auto) crend(Self&& self) {
    return rend(std::forward<Self>(self));
}


namespace impl {

    /* A range adaptor that concatenates a sequence of subranges into a single view.
    Every element in the input range must yield another range, which will be flattened
    into a single output range. */
    template <std::ranges::input_range View>
        requires (
            std::ranges::view<View> &&
            std::ranges::input_range<std::ranges::range_value_t<View>>
        )
    struct Comprehension : BertrandTag, std::ranges::view_base {
    private:
        using InnerView = std::ranges::range_value_t<View>;

        View m_view;

        struct Sentinel;

        struct Iterator {
        private:

            void skip_empty_views() {
                while (inner_begin == inner_end) {
                    if (++outer_begin == outer_end) {
                        break;
                    }
                    inner_begin = std::ranges::begin(*outer_begin);
                    inner_end = std::ranges::end(*outer_begin);
                }
            }

        public:
            using iterator_category = std::input_iterator_tag;
            using value_type = std::ranges::range_value_t<InnerView>;
            using difference_type = std::ranges::range_difference_t<InnerView>;
            using pointer = value_type*;
            using reference = value_type&;

            std::ranges::iterator_t<View> outer_begin;
            std::ranges::iterator_t<View> outer_end;
            std::ranges::iterator_t<InnerView> inner_begin;
            std::ranges::iterator_t<InnerView> inner_end;

            Iterator() = default;
            Iterator(
                std::ranges::iterator_t<View>&& outer_begin,
                std::ranges::iterator_t<View>&& outer_end
            ) : outer_begin(std::move(outer_begin)), outer_end(std::move(outer_end))
            {
                if (outer_begin != outer_end) {
                    inner_begin = std::ranges::begin(*outer_begin);
                    inner_end = std::ranges::end(*outer_begin);
                    skip_empty_views();
                }
            }

            Iterator& operator++() {
                if (++inner_begin == inner_end) {
                    if (++outer_begin != outer_end) {
                        inner_begin = std::ranges::begin(*outer_begin);
                        inner_end = std::ranges::end(*outer_begin);
                        skip_empty_views();
                    }
                }
                return *this;
            }

            decltype(auto) operator*() const {
                return *inner_begin;
            }

            bool operator==(const Sentinel&) const {
                return outer_begin == outer_end;
            }

            bool operator!=(const Sentinel&) const {
                return outer_begin != outer_end;
            }

        };

        struct Sentinel {
            bool operator==(const Iterator& iter) const {
                return iter.outer_begin == iter.outer_end;
            }
            bool operator!=(const Iterator& iter) const {
                return iter.outer_begin != iter.outer_end;
            }
        };

    public:

        Comprehension() = default;
        Comprehension(const Comprehension&) = default;
        Comprehension(Comprehension&&) = default;
        Comprehension(View&& view) : m_view(std::move(view)) {}

        Iterator begin() {
            return Iterator(std::ranges::begin(m_view), std::ranges::end(m_view));
        }

        Sentinel end() {
            return {};
        };

    };

    template <typename View>
    Comprehension(View&&) -> Comprehension<std::remove_cvref_t<View>>;

}


/* Apply a C++ range adaptor to a Python object.  This is similar to the C++-style `|`
operator for chaining range adaptors, but uses the `->*` operator to avoid conflicts
with Python and apply higher precedence than typical binary operators. */
template <impl::python Self, std::ranges::view View> requires (impl::iterable<Self>)
[[nodiscard]] auto operator->*(Self&& self, View&& view) {
    return std::views::all(std::forward<Self>(self)) | std::forward<View>(view);
}


/* Generate a C++ range adaptor that approximates a Python-style list comprehension.
This is done by piping a raw function pointer or lambda in place of a C++ range
adaptor, which will be applied to each element in the sequence.  The function must be
callable with the container's value type, and may return any type.

If the function returns a range adaptor, then the adaptor's output will be flattened
into the parent range, similar to a nested `for` loop within a comprehension.
Returning a range with no elements will effectively filter out the current element,
similar to a Python `if` clause within a comprehension.

Here's an example:

    py::List list = {1, 2, 3, 4, 5};
    py::List new_list = list->*[](const py::Int& x) {
        return std::views::repeat(x, x % 2 ? 0 : x);
    };
    py::print(new_list);  // [2, 2, 4, 4, 4, 4]
*/
template <impl::python Self, typename Func>
    requires (
        impl::iterable<Self> &&
        !std::ranges::view<Func> &&
        std::is_invocable_v<Func, impl::iter_type<Self>>
    )
[[nodiscard]] auto operator->*(Self&& self, Func&& func) {
    using Return = std::invoke_result_t<Func, impl::iter_type<Self>>;
    if constexpr (std::ranges::view<Return>) {
        return impl::Comprehension(
            std::views::all(std::forward<Self>(self))) |
            std::views::transform(std::forward<Func>(func)
        );
    } else {
        return
            std::views::all(std::forward<Self>(self)) |
            std::views::transform(std::forward<Func>(func));
    }
}


////////////////////////
////    OPTIONAL    ////
////////////////////////


/// TODO: operators should now be in a state where they can return arbitrary types
/// from control structures that supply a custom call operator, which means I should
/// be able to make all operations monadic.


template <std::derived_from<Object> T>
struct Optional;


namespace impl {
    struct OptionalTag : BertrandTag {};
}


template <typename T>
struct Interface<Optional<T>> : impl::OptionalTag {
    using __wrapped__ = T;

    [[nodiscard]] T value(this auto&& self) {
        if (self.is(None)) {
            throw TypeError("optional is empty");
        }
        return reinterpret_borrow<T>(ptr(self->m_value));
    }
};
template <typename T>
struct Interface<Type<Optional<T>>> : impl::OptionalTag {
    using __wrapped__ = T;

    template <impl::inherits<impl::OptionalTag> U>
    [[nodiscard]] T value(U&& self) {
        if (self.is(None)) {
            throw TypeError("optional is empty");
        }
        return reinterpret_borrow<T>(ptr(self->m_value));
    }
};


template <std::derived_from<Object> T>
struct Optional : Object, Interface<Optional<T>> {
    struct __python__ : def<__python__, Optional>, PyObject {
        static constexpr StaticStr __doc__ =
R"doc()doc";

        Object m_value;

        __python__(const T& value) : m_value(value) {}
        __python__(T&& value) : m_value(std::move(value)) {}
        __python__(const NoneType& value) : m_value(value) {}
        __python__(NoneType&& value) : m_value(std::move(value)) {}

        template <StaticStr ModName>
        static Type<Optional> __export__(Module<ModName>& mod);
        static Type<Optional> __import__();

        /// TODO: Python should forward attribute/item access/assignment/etc. to the
        /// underlying object in a monadic fashion, just like in C++

        static PyObject* value(__python__* self) {
            if (self->m_value.is(None)) {
                throw TypeError("optional is empty");
            }
            return Py_NewRef(ptr(self->m_value));
        }

    private:

        inline static PyMethodDef methods[] = {
            {
                "value",
                reinterpret_cast<PyCFunction>(value),
                METH_NOARGS,
                PyDoc_STR(
R"doc(Get the value stored in the optional, or raise an error if it
is empty.

Returns
-------
T
    The value stored in the optional.

Raises
------
TypeError
    If the optional currently holds `None`.
)doc"
                )
            },
            {nullptr}
        };

    };

    Optional(PyObject* p, borrowed_t t) : Object(p, t) {}
    Optional(PyObject* p, stolen_t t) : Object(p, t) {}

    template <typename Self = Optional> requires (__initializer__<Self>::enable)
    Optional(const std::initializer_list<typename __initializer__<Self>::type>& init) :
        Object(__initializer__<Self>{}(init))
    {}

    template <typename... Args> requires (implicit_ctor<Optional>::template enable<Args...>)
    Optional(Args&&... args) : Object(
        implicit_ctor<Optional>{},
        std::forward<Args>(args)...
    ) {}

    template <typename... Args> requires (explicit_ctor<Optional>::template enable<Args...>)
    explicit Optional(Args&&... args) : Object(
        explicit_ctor<Optional>{},
        std::forward<Args>(args)...
    ) {}

};


template <typename T>
struct __template__<Optional<T>>                            : Returns<Object> {
    static Type<T> operator()() { return {}; }
};


template <typename Derived, impl::inherits<impl::OptionalTag> Base>
struct __isinstance__<Derived, Base>                         : Returns<bool> {
    using Wrapped = std::remove_reference_t<Base>::__wrapped__;
    static constexpr bool operator()(Derived&& obj) {
        if constexpr (impl::dynamic<Derived>) {
            return
                obj.is(None) ||
                isinstance<Wrapped>(std::forward<Derived>(obj));
        } else {
            return
                impl::none_like<Derived> ||
                isinstance<Wrapped>(std::forward<Derived>(obj));
        }
    }
    template <typename T = Wrapped>
        requires (std::is_invocable_v<__isinstance__<Derived, T>, Derived, T>)
    static constexpr bool operator()(Derived&& obj, Base base) {
        if (base.is(None)) {
            return false;
        } else {
            return isinstance(std::forward<Derived>(obj), base.value());
        }
    }
};


template <typename Derived, impl::inherits<impl::OptionalTag> Base>
struct __issubclass__<Derived, Base>                         : Returns<bool> {
    using Wrapped = std::remove_reference_t<Base>::__wrapped__;
    static constexpr bool operator()() {
        return impl::none_like<Derived> || issubclass<Derived, Wrapped>();
    }
    template <typename T = Wrapped>
        requires (std::is_invocable_v<__issubclass__<Derived, T>, Derived>)
    static constexpr bool operator()(Derived&& obj) {
        if constexpr (impl::dynamic<Derived>) {
            return
                obj.is(None) ||
                issubclass<Wrapped>(std::forward<Derived>(obj));
        } else {
            return
                impl::none_like<Derived> ||
                issubclass<Wrapped>(std::forward<Derived>(obj));
        }
    }
    template <typename T = Wrapped>
        requires (std::is_invocable_v<__issubclass__<Derived, T>, Derived, T>)
    static constexpr bool operator()(Derived&& obj, Base base) {
        if (base.is(None)) {
            return false;
        } else {
            return issubclass(std::forward<Derived>(obj), base.value());
        }
    }
};


/// NOTE: constructors are forwarded to the underlying object, except for the default
/// constructor, which initializes to None


template <typename T> requires (__initializer__<T>::enable)
struct __initializer__<Optional<T>> : Returns<typename __initializer__<T>::type> {
    using Element = __initializer__<T>::type;
    static Optional<T> operator()(const std::initializer_list<Element>& init) {
        return T(init);
    }
};


template <typename T>
struct __init__<Optional<T>>                                : Returns<Optional<T>> {
    static Optional<T> operator()() {
        return impl::construct<Optional<T>>(None);
    }
};


template <typename T, typename... Args> requires (std::constructible_from<T, Args...>)
struct __init__<Optional<T>, Args...>                        : Returns<Optional<T>> {
    static Optional<T> operator()(Args&&... args) {
        return impl::construct<Optional<T>>(T(std::forward<Args>(args)...));
    }
};


/* Implicitly cast a non-empty input into the underlying type if possible. */
template <typename From, typename To> requires (std::convertible_to<From, To>)
struct __cast__<From, Optional<To>>                         : Returns<Optional<To>> {
    static Optional<To> operator()(From&& from) {
        return impl::construct<Optional<To>>(To(std::forward<From>(from)));
    }
};


/// NOTE: implicit casts are allowed to and from std::optional, pointers, and smart
/// pointers, as well as from 


template <impl::is_optional T> requires (__cast__<impl::optional_type<T>>::enable)
struct __cast__<T> : Returns<Optional<typename __cast__<impl::optional_type<T>>::type>> {};
template <std::convertible_to<Object> T>
struct __cast__<T*> : Returns<Optional<obj<T>>> {};
template <impl::is_shared_ptr T> requires (std::convertible_to<impl::shared_ptr_type<T>, Object>)
struct __cast__<T> : Returns<Optional<obj<impl::shared_ptr_type<T>>>> {};
template <impl::is_unique_ptr T> requires (std::convertible_to<impl::unique_ptr_type<T>, Object>)
struct __cast__<T> : Returns<Optional<obj<impl::unique_ptr_type<T>>>> {};


template <impl::is_optional From, typename To>
    requires (std::convertible_to<impl::optional_type<From>, To>)
struct __cast__<From, Optional<To>>                         : Returns<Optional<To>> {
    static Optional<To> operator()(From from) {
        if (!from.has_value()) {
            return None;
        } else {
            if constexpr (std::is_lvalue_reference_v<From>) {
                return To(from.value());
            } else {
                return To(std::move(from.value()));
            }
        }
    }
};


template <impl::is_ptr From, typename To>
    requires (std::convertible_to<impl::ptr_type<From>, To>)
struct __cast__<From, Optional<To>>                         : Returns<Optional<To>> {
    static Optional<To> operator()(From from) {
        if (from == nullptr) {
            return None;
        } else {
            return To(*from);
        }
    }
};


template <impl::is_shared_ptr From, typename To>
    requires (std::convertible_to<impl::shared_ptr_type<From>, To>)
struct __cast__<From, Optional<To>>                         : Returns<Optional<To>> {
    static Optional<To> operator()(From&& from) {
        if (from == nullptr) {
            return None;
        } else {
            return To(*from);
        }
    }
};


template <impl::is_unique_ptr From, typename To>
    requires (std::convertible_to<impl::unique_ptr_type<From>, To>)
struct __cast__<From, Optional<To>>                         : Returns<Optional<To>> {
    static Optional<To> operator()(From&& from) {
        if (from == nullptr) {
            return None;
        } else {
            return To(*from);
        }
    }
};


template <impl::inherits<impl::OptionalTag> From, typename To>
    requires (std::convertible_to<
        typename std::remove_reference_t<From>::__wrapped__,
        To
    >)
struct __cast__<From, std::optional<To>>                    : Returns<std::optional<To>> {
    static std::optional<To> operator()(From from) {
        if (from.is(None)) {
            return std::nullopt;
        } else {
            return To(from.value());
        }
    }
};


template <impl::inherits<impl::OptionalTag> From, typename To>
    requires (std::same_as<
        typename std::remove_reference_t<From>::__wrapped__,
        std::remove_cvref_t<To>
    >)
struct __cast__<From, To*>                                  : Returns<To*> {
    static To* operator()(From from) {
        if (from.is(None)) {
            return nullptr;
        } else {
            return &from.value();
        }
    }
};


template <impl::inherits<impl::OptionalTag> From, typename To>
    requires (std::convertible_to<
        typename std::remove_reference_t<From>::__wrapped__,
        To
    >)
struct __cast__<From, std::shared_ptr<To>>                  : Returns<std::shared_ptr<To>> {
    static std::shared_ptr<To> operator()(From from) {
        if (from.is(None)) {
            return nullptr;
        } else {
            return std::make_shared<To>(from.value());
        }
    }
};


template <impl::inherits<impl::OptionalTag> From, typename To>
    requires (std::convertible_to<
        typename std::remove_reference_t<From>::__wrapped__,
        To
    >)
struct __cast__<From, std::unique_ptr<To>>                  : Returns<std::unique_ptr<To>> {
    static std::unique_ptr<To> operator()(From from) {
        if (from.is(None)) {
            return nullptr;
        } else {
            return std::make_unique<To>(from.value());
        }
    }
};


/// NOTE: all other operations are monadic


template <impl::inherits<impl::OptionalTag> Self, StaticStr Name>
    requires (__getattr__<
        typename std::remove_reference_t<Self>::__wrapped__,
        Name
    >::enable)
struct __getattr__<Self, Name>                              : Returns<Optional<
    typename __getattr__<typename std::remove_reference_t<Self>::__wrapped__, Name>::type
>> {
    using Wrapped = std::remove_reference_t<Self>::__wrapped__;
    using Return = __getattr__<Wrapped, Name>::type;
    static Optional<Return> operator()(Self self) {
        if (self.is(None)) {
            return None;
        } else {
            return getattr<Name>(self.value());
        }
    }
};


template <impl::inherits<impl::OptionalTag> Self, StaticStr Name, typename Value>
    requires (__setattr__<
        typename std::remove_reference_t<Self>::__wrapped__,
        Name,
        Value
    >::enable)
struct __setattr__<Self, Name, Value>             : Returns<void> {
    static void operator()(Self self, Value&& value) {
        if (!self.is(None)) {
            setattr<Name>(self.value(), std::forward<Value>(value));
        }
    }
};


template <impl::inherits<impl::OptionalTag> Self, StaticStr Name>
    requires (__delattr__<
        typename std::remove_reference_t<Self>::__wrapped__,
        Name
    >::enable)
struct __delattr__<Self, Name>                              : Returns<void> {
    static void operator()(Self self) {
        if (!self.is(None)) {
            delattr<Name>(self.value());
        }
    }
};


template <impl::inherits<impl::OptionalTag> Self>
    requires (__repr__<typename std::remove_reference_t<Self>::__wrapped__>::enable)
struct __repr__<Self>                                       : Returns<Str> {
    static auto operator()(Self self) {
        return self.is(None) ? std::string("None") : repr(self.value());
    }
};


template <impl::inherits<impl::OptionalTag> Self, typename... Args>
    requires (__call__<
        typename std::remove_reference_t<Self>::__wrapped__,
        Args...
    >::enable)
struct __call__<Self, Args...>                              : Returns<Optional<
    typename __call__<
        typename std::remove_reference_t<Self>::__wrapped__,
        Args...
    >::type    
>> {
    using Wrapped = std::remove_reference_t<Self>::__wrapped__;
    using Return = __call__<Wrapped, Args...>::type;
    static Optional<Return> operator()(Self self, Args&&... args) {
        if (self.is(None)) {
            return None;
        } else {
            return self.value()(std::forward<Args>(args)...);
        }
    }
};


template <impl::inherits<impl::OptionalTag> Self, typename... Key>
    requires (__getitem__<
        typename std::remove_reference_t<Self>::__wrapped__,
        Key...
    >::enable)
struct __getitem__<Self, Key...>                             : Returns<Optional<
    typename __getitem__<
        typename std::remove_reference_t<Self>::__wrapped__,
        Key...
    >::type
>> {
    using Wrapped = std::remove_reference_t<Self>::__wrapped__;
    using Return = __getitem__<Wrapped, Key...>::type;
    static Optional<Return> operator()(Self self, Key&&... key) {
        if (self.is(None)) {
            return None;
        } else {
            return self.value()[std::forward<Key>(key)...];
        }
    }
};


template <impl::inherits<impl::OptionalTag> Self, typename Value, typename... Key>
    requires (__setitem__<
        typename std::remove_reference_t<Self>::__wrapped__,
        Value,
        Key...
    >::enable)
struct __setitem__<Self, Value, Key...>                         : Returns<void> {
    static void operator()(Self self, Value&& value, Key&&... key) {
        if (!self.is(None)) {
            self.value()[std::forward<Key>(key)...] = std::forward<Value>(value);
        }
    }
};


template <impl::inherits<impl::OptionalTag> Self, typename... Key>
    requires (__delitem__<
        typename std::remove_reference_t<Self>::__wrapped__,
        Key...
    >::enable)
struct __delitem__<Self, Key...>                               : Returns<void> {
    static void operator()(Self self, Key&&... key) {
        if (!self.is(None)) {
            del(self.value()[std::forward<Key>(key)...]);
        }
    }
};


template <impl::inherits<impl::OptionalTag> Self>
    requires (__len__<typename std::remove_reference_t<Self>::__wrapped__>::enable)
struct __len__<Self>                                        : Returns<size_t> {
    static size_t operator()(Self self) {
        return self.is(None) ? 0 : len(self.value());
    }
};


template <impl::inherits<impl::OptionalTag> Self>
    requires (__iter__<typename std::remove_reference_t<Self>::__wrapped__>::enable)
struct __iter__<Self>                                       : Returns<Optional<
    typename __iter__<typename std::remove_reference_t<Self>::__wrapped__>::type
>> {
    /// TODO: complicated
};



template <impl::inherits<impl::OptionalTag> Self>
    requires (__reversed__<typename std::remove_reference_t<Self>::__wrapped__>::enable)
struct __reversed__<Self>                                   : Returns<Optional<
    typename __reversed__<typename std::remove_reference_t<Self>::__wrapped__>::type
>> {
    /// TODO: complicated
};


template <impl::inherits<impl::OptionalTag> Self, typename Key>
    requires (__contains__<
        typename std::remove_reference_t<Self>::__wrapped__,
        Key
    >::enable)
struct __contains__<Self, Key>                              : Returns<bool> {
    static bool operator()(Self self, Key&& key) {
        return self.is(None) ? false : self.value().contains(std::forward<Key>(key));
    }
};


template <impl::inherits<impl::OptionalTag> Self>
    requires (__hash__<typename std::remove_reference_t<Self>::__wrapped__>::enable)
struct __hash__<Self>                                       : Returns<size_t> {
    static size_t operator()(Self self) {
        if (self.is(None)) {
            return hash(None);
        } else {
            return hash(self.value());
        }
    }
};


template <impl::inherits<impl::OptionalTag> Self>
    requires (__abs__<typename std::remove_reference_t<Self>::__wrapped__>::enable)
struct __abs__<Self>                                        : Returns<Optional<
    typename __abs__<typename std::remove_reference_t<Self>::__wrapped__>::type
>> {
    using Wrapped = std::remove_reference_t<Self>::__wrapped__;
    using Return = __abs__<Wrapped>::type;
    static Optional<Return> operator()(Self self) {
        if (self.is(None)) {
            return None;
        } else {
            return abs(self.value());
        }
    }
};


template <impl::inherits<impl::OptionalTag> Self>
struct __invert__<Self>                                     : Returns<Optional<
    typename __invert__<typename std::remove_reference_t<Self>::__wrapped__>::type
>> {
    using Wrapped = std::remove_reference_t<Self>::__wrapped__;
    using Return = __invert__<Wrapped>::type;
    static Optional<Return> operator()(Self self) {
        if (self.is(None)) {
            return None;
        } else {
            return ~self.value();
        }
    }
};


template <impl::inherits<impl::OptionalTag> Self>
struct __pos__<Self>                                        : Returns<Optional<
    typename __pos__<typename std::remove_reference_t<Self>::__wrapped__>::type
>> {
    using Wrapped = std::remove_reference_t<Self>::__wrapped__;
    using Return = __pos__<Wrapped>::type;
    static Optional<Return> operator()(Self self) {
        if (self.is(None)) {
            return None;
        } else {
            return +self.value();
        }
    }
};


template <impl::inherits<impl::OptionalTag> Self>
struct __neg__<Self>                                        : Returns<Optional<
    typename __neg__<typename std::remove_reference_t<Self>::__wrapped__>::type
>> {
    using Wrapped = std::remove_reference_t<Self>::__wrapped__;
    using Return = __neg__<Wrapped>::type;
    static Optional<Return> operator()(Self self) {
        if (self.is(None)) {
            return None;
        } else {
            return -self.value();
        }
    }
};




/// TODO: there's something weird about reference types here.  The same will be true
/// for inplace operators in the future.


template <impl::inherits<impl::OptionalTag> Self>
    requires (__increment__<typename std::remove_reference_t<Self>::__wrapped__>::enable)
struct __increment__<Self>                                  : Returns<
    std::remove_reference_t<Self>&
> {
    static std::remove_reference_t<Self>& operator()(Self self) {
        if (!self.is(None)) {
            ++self.value();
        }
        return self;
    }
};


/// TODO: same as for __increment__


template <impl::inherits<impl::OptionalTag> Self>
    requires (__decrement__<typename std::remove_reference_t<Self>::__wrapped__>::enable)
struct __decrement__<Self>                                  : Returns<
    std::remove_reference_t<Self>&
> {
    static std::remove_reference_t<Self>& operator()(Self self) {
        if (!self.is(None)) {
            --self.value();
        }
        return self;
    }
};


/// TODO: __lt__, etc. should not be constrained to return bool.  Also binary operators
/// are really hard in this case.


/// TODO: all the math operators as monads



/////////////////////
////    UNION    ////
/////////////////////


template <std::derived_from<Object>... Types>
struct Union;


namespace impl {
    struct UnionTag : BertrandTag {};

    template <typename T>
    struct VariantToUnion {
        static constexpr bool enable = false;
    };
    template <std::convertible_to<Object>... Ts>
    struct VariantToUnion<std::variant<Ts...>> {
        static constexpr bool enable = true;
        using type = Union<obj<Ts>...>;
    };

}


template <typename... Types>
struct Interface<Union<Types...>> : impl::UnionTag {
    using __wrapped__ = std::variant<Types...>;
};
template <typename... Types>
struct Interface<Type<Union<Types...>>> : impl::UnionTag {
    using __wrapped__ = std::variant<Types...>;
};


template <std::derived_from<Object>... Types>
struct Union : Object, Interface<Union<Types...>> {
    struct __python__ : def<__python__, Union>, PyObject {
        static constexpr StaticStr __doc__ =
R"doc()doc";

        std::variant<Types...> value;

    };

    Union(PyObject* p, borrowed_t t) : Object(p, t) {}
    Union(PyObject* p, stolen_t t) : Object(p, t) {}

    template <typename Self = Union> requires (__initializer__<Self>::enable)
    Union(const std::initializer_list<typename __initializer__<Self>::type>& init) :
        Object(__initializer__<Self>{}(init))
    {}

    template <typename... Args> requires (implicit_ctor<Union>::template enable<Args...>)
    Union(Args&&... args) : Object(
        implicit_ctor<Union>{},
        std::forward<Args>(args)...
    ) {}

    template <typename... Args> requires (explicit_ctor<Union>::template enable<Args...>)
    explicit Union(Args&&... args) : Object(
        explicit_ctor<Union>{},
        std::forward<Args>(args)...
    ) {}

};


template <impl::is_variant T> requires (impl::VariantToUnion<T>::enable)
struct __cast__<T> : Returns<typename impl::VariantToUnion<T>::type> {};


template <impl::is_variant From, typename... Ts>
    requires (impl::VariantToUnion<From>::enable)
struct __cast__<From, Union<Ts...>>                         : Returns<Union<Ts...>> {
    /// TODO: need to assert that the variant types are convertible to the union types
};




}


#endif

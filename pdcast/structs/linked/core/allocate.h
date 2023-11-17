// include guard: BERTRAND_STRUCTS_LINKED_CORE_ALLOCATE_H
#ifndef BERTRAND_STRUCTS_LINKED_CORE_ALLOCATE_H
#define BERTRAND_STRUCTS_LINKED_CORE_ALLOCATE_H

#include <cstddef>  // size_t
#include <cstdlib>  // malloc(), calloc(), free()
#include <functional>  // std::hash, std::equal_to
#include <iostream>  // std::cout, std::endl
#include <optional>  // std::optional
#include <sstream>  // std::ostringstream
#include <stdexcept>  // std::invalid_argument
#include <Python.h>  // CPython API
#include "../../util/except.h"  // catch_python(), TypeError()
#include "../../util/math.h"  // next_power_of_two()
#include "../../util/python.h"  // std::hash<PyObject*>, eq(), len()
#include "../../util/repr.h"  // repr()
#include "../../util/name.h"  // PyName
#include "node.h"  // NodeTraits


namespace bertrand {
namespace structs {
namespace linked {


/////////////////////////
////    CONSTANTS    ////
/////////////////////////


/* DEBUG=TRUE adds print statements for every call to malloc()/free() in order
to help catch memory leaks. */
const bool DEBUG = true;


////////////////////
////    BASE    ////
////////////////////


/* Empty tag class marking a node allocator for a linked data structure.

NOTE: this class is inherited by all allocators, and can be used for easy SFINAE checks
via std::is_base_of, without requiring any foreknowledge of template parameters. */
class AllocatorTag {};


/* Base class that implements shared functionality for all allocators and provides the
minimum necessary attributes for compatibility with higher-level views. */
template <typename NodeType>
class BaseAllocator : public AllocatorTag {
public:
    using Node = NodeType;

protected:
    // bitfield for storing boolean flags related to allocator state, as follows:
    // 0b001: if set, allocator is temporarily frozen for memory stability
    // 0b010: if set, allocator does not support dynamic resizing
    // 0b100: if set, nodes cannot be re-specialization after list construction
    enum {
        FROZEN                  = 0b001,
        FIXED_SIZE              = 0b010,
        STRICTLY_TYPED          = 0b100
    } BITFLAGS;
    unsigned char flags;
    union { mutable Node _temp; };  // temporary node for internal use

    /* Allocate a contiguous block of uninitialized nodes with the specified size. */
    inline static Node* allocate_array(size_t capacity) {
        Node* result = static_cast<Node*>(malloc(capacity * sizeof(Node)));
        if (result == nullptr) {
            throw std::bad_alloc();
        }
        return result;
    }

    /* Initialize an uninitialized node for use in the list. */
    template <typename... Args>
    inline void init_node(Node* node, Args&&... args) {
        using util::repr;

        // variadic dispatch to node constructor
        new (node) Node(std::forward<Args>(args)...);

        // check python specialization if enabled
        if (specialization != nullptr && !node->typecheck(specialization)) {
            std::ostringstream msg;
            msg << repr(node->value()) << " is not of type ";
            msg << repr(specialization);
            node->~Node();  // in-place destructor
            throw util::TypeError(msg.str());
        }
        if constexpr (DEBUG) {
            std::cout << "    -> create: " << repr(node->value()) << std::endl;
        }
    }

    /* Destroy all nodes contained in the list. */
    inline void destroy_list() noexcept {
        using util::repr;

        Node* curr = head;
        while (curr != nullptr) {
            Node* next = curr->next();
            if constexpr (DEBUG) {
                std::cout << "    -> recycle: " << repr(curr->value()) << std::endl;
            }
            curr->~Node();  // in-place destructor
            curr = next;
        }
    }

    /* Throw an error indicating that the allocator cannot grow past its current
    size. */
    inline auto cannot_grow() const {
        std::ostringstream msg;
        msg << "array is frozen at size: " << capacity;
        return util::MemoryError(msg.str());
    }

public:
    Node* head;  // head of the list
    Node* tail;  // tail of the list
    size_t capacity;  // number of nodes in the array
    size_t occupied;  // number of nodes currently in use - equivalent to list.size()
    PyObject* specialization;  // type specialization for PyObject* values

    // TODO: capacity is optional and defaults to nullopt.  If FIXED_SIZE is set, then
    // capacity must be specified.  Otherwise, capacity is set to DEFAULT_CAPACITY.

    /* Create an allocator with an optional fixed size. */
    BaseAllocator(
        size_t capacity,
        bool fixed,
        PyObject* specialization
        // bool permanent
    ) : flags(FIXED_SIZE * fixed),  // fixed=true turns off dynamic resizing
        head(nullptr),
        tail(nullptr),
        capacity(capacity),
        occupied(0),
        specialization(specialization)
    {
        if constexpr (DEBUG) {
            std::cout << "    -> allocate: " << this->capacity << " nodes" << std::endl;
        }
        if (specialization != nullptr) {
            Py_INCREF(specialization);
        }
    }

    /* Copy constructor. */
    BaseAllocator(const BaseAllocator& other) :
        flags(other.flags),
        head(nullptr),
        tail(nullptr),
        capacity(other.capacity),
        occupied(other.occupied),
        specialization(other.specialization)
    {
        if constexpr (DEBUG) {
            std::cout << "    -> allocate: " << capacity << " nodes" << std::endl;
        }
        Py_XINCREF(specialization);
    }

    /* Move constructor. */
    BaseAllocator(BaseAllocator&& other) noexcept :
        flags(other.flags),
        head(other.head),
        tail(other.tail),
        capacity(other.capacity),
        occupied(other.occupied),
        specialization(other.specialization)
    {
        other.flags = 0;
        other.head = nullptr;
        other.tail = nullptr;
        other.capacity = 0;
        other.occupied = 0;
        other.specialization = nullptr;
    }

    /* Copy assignment operator. */
    BaseAllocator& operator=(const BaseAllocator& other) {
        // check for self-assignment
        if (this == &other) {
            return *this;
        }

        // destroy current
        Py_XDECREF(specialization);
        if (head != nullptr) {
            destroy_list();  // calls destructors
            head = nullptr;
            tail = nullptr;
        }
        if constexpr (DEBUG) {
            std::cout << "    -> deallocate: " << capacity << " nodes" << std::endl;
        }

        // copy from other
        flags = other.flags;
        capacity = other.capacity;
        occupied = other.occupied;
        specialization = Py_XNewRef(other.specialization);

        return *this;
    }

    /* Move assignment operator. */
    BaseAllocator& operator=(BaseAllocator&& other) noexcept {
        // check for self-assignment
        if (this == &other) {
            return *this;
        }

        // destroy current
        Py_XDECREF(specialization);
        if (head != nullptr) {
            destroy_list();  // calls destructors
        }
        if constexpr (DEBUG) {
            std::cout << "    -> deallocate: " << capacity << " nodes" << std::endl;
        }

        // transfer ownership
        flags = other.flags;
        head = other.head;
        tail = other.tail;
        capacity = other.capacity;
        occupied = other.occupied;
        specialization = other.specialization;

        // reset other
        other.flags = 0;
        other.head = nullptr;
        other.tail = nullptr;
        other.capacity = 0;
        other.occupied = 0;
        other.specialization = nullptr;
        return *this;
    }

    /* Destroy an allocator and release its resources. */
    ~BaseAllocator() noexcept {
        Py_XDECREF(specialization);
    }

    ////////////////////////
    ////    ABSTRACT    ////
    ////////////////////////

    /* Construct a new node for the list. */
    template <typename... Args>
    Node* create(Args&&...);

    /* Release a node from the list. */
    void recycle(Node* node) {
        // manually call destructor
        if constexpr (DEBUG) {
            std::cout << "    -> recycle: " << util::repr(node->value()) << std::endl;
        }
        node->~Node();
        --occupied;
    }

    /* Remove all elements from the list. */
    void clear() noexcept {
        // destroy all nodes
        destroy_list();

        // reset list parameters
        head = nullptr;
        tail = nullptr;
        occupied = 0;
    }

    /* Resize the allocator to store a specific number of nodes. */
    void reserve(size_t new_size) {
        // ensure new capacity is large enough to store all existing nodes
        if (new_size < this->occupied) {
            throw std::invalid_argument(
                "new capacity cannot be smaller than current size"
            );
        }
    }

    /* Attempt to resize the allocator based on an optional size. */
    void try_reserve(std::optional<size_t> new_size);

    /* Attempt to reserve memory to hold all the elements of a given container if it
    implements a `size()` method or is a Python object with a corresponding `__len__()`
    attribute. */
    template <typename Container>
    void try_reserve(Container& container);

    /* Rearrange the nodes in memory to reduce fragmentation. */
    void defragment() {
        // ensure list is not frozen for memory stability
        if (frozen()) {
            std::ostringstream msg;
            msg << "array cannot be reallocated while a MemGuard is active";
            throw util::MemoryError(msg.str());
        }
    }

    /////////////////////////
    ////    INHERITED    ////
    /////////////////////////

    /* Enforce strict type checking for python values within the list. */
    void specialize(PyObject* spec) {
        // handle null assignment
        if (spec == nullptr) {
            if (specialization != nullptr) {
                Py_DECREF(specialization);  // release old spec
                specialization = nullptr;
            }
            return;
        }

        // early return if new spec is same as old spec
        if (specialization != nullptr) {
            int comp = PyObject_RichCompareBool(spec, specialization, Py_EQ);
            if (comp == -1) {  // comparison raised an exception
                throw util::catch_python<util::TypeError>();
            } else if (comp == 1) {
                return;
            }
        }

        // check the contents of the list
        Node* curr = head;
        while (curr != nullptr) {
            if (!curr->typecheck(spec)) {
                std::ostringstream msg;
                msg << util::repr(curr->value()) << " is not of type ";
                msg << util::repr(spec);
                throw util::TypeError(msg.str());
            }
            curr = curr->next();
        }

        // replace old specialization
        Py_INCREF(spec);
        if (specialization != nullptr) {
            Py_DECREF(specialization);
        }
        specialization = spec;
    }

    /* Check whether the allocator supports dynamic resizing. */
    inline bool dynamic() const noexcept {
        return !static_cast<bool>(flags & FIXED_SIZE);
    }

    /* Check whether the allocator is temporarily frozen for memory stability. */
    inline bool frozen() const noexcept {
        return static_cast<bool>(flags & FROZEN);
    }

    /* Get a temporary node for internal use. */
    inline Node* temp() const noexcept {
        return &_temp;
    }

    /* Get the total amount of dynamic memory allocated by this allocator. */
    inline size_t nbytes() const {
        return (1 + this->capacity) * sizeof(Node);  // account for temporary node
    }

};


/* An RAII-style memory guard that temporarily prevents an allocator from being resized
or defragmented within a certain context. */
template <typename Allocator>
class MemGuard {
    static_assert(
        std::is_base_of_v<BaseAllocator<typename Allocator::Node>, Allocator>,
        "Allocator must inherit from BaseAllocator"
    );

    friend Allocator;
    Allocator* allocator;

    /* Create an active MemGuard for an allocator, freezing it at its current
    capacity. */
    MemGuard(Allocator* allocator) noexcept : allocator(allocator) {
        allocator->flags |= Allocator::FROZEN;  // set frozen bitflag
        if constexpr (DEBUG) {
            std::cout << "FREEZE: " << allocator->capacity << " NODES";
            std::cout << std::endl;
        }
    }

    /* Create an inactive MemGuard for an allocator. */
    MemGuard() noexcept : allocator(nullptr) {}

    /* Destroy the outermost MemGuard. */
    inline void destroy() noexcept {
        allocator->flags &= ~(Allocator::FROZEN);  // clear frozen bitflag
        if constexpr (DEBUG) {
            std::cout << "UNFREEZE: " << allocator->capacity << " NODES";
            std::cout << std::endl;
        }
        if (allocator->dynamic()) {
            allocator->shrink();  // every allocator implements this
        }
    }

public:

    /* Copy constructor/assignment operator deleted for safety. */
    MemGuard(const MemGuard& other) = delete;
    MemGuard& operator=(const MemGuard& other) = delete;

    /* Move constructor. */
    MemGuard(MemGuard&& other) noexcept : allocator(other.allocator) {
        other.allocator = nullptr;
    }

    /* Move assignment operator. */
    MemGuard& operator=(MemGuard&& other) noexcept {
        // check for self-assignment
        if (this == &other) {
            return *this;
        }

        // destroy current
        if (active()) destroy();

        // transfer ownership
        allocator = other.allocator;
        other.allocator = nullptr;
        return *this;
    }

    /* Unfreeze and potentially shrink the allocator when the outermost MemGuard falls
    out of scope. */
    ~MemGuard() noexcept { if (active()) destroy(); }

    /* Check whether the guard is active. */
    inline bool active() const noexcept { return allocator != nullptr; }

};


/* A Python wrapper around a MemGuard that allows it to be used as a context manager. */
template <typename Allocator>
class PyMemGuard {
    using MemGuard = linked::MemGuard<Allocator>;

    PyObject_HEAD
    Allocator* allocator;
    size_t capacity;
    bool has_guard;
    union { MemGuard guard; };

public:

    /* Disable instantiation from C++. */
    PyMemGuard() = delete;
    PyMemGuard(const PyMemGuard&) = delete;
    PyMemGuard(PyMemGuard&&) = delete;
    PyMemGuard& operator=(const PyMemGuard&) = delete;
    PyMemGuard& operator=(PyMemGuard&&) = delete;

    /* Construct a Python MemGuard for a C++ allocator. */
    inline static PyObject* construct(Allocator* allocator, size_t capacity) {
        // allocate
        PyMemGuard* self = PyObject_New(PyMemGuard, &Type);
        if (self == nullptr) {
            PyErr_SetString(
                PyExc_RuntimeError,
                "failed to allocate PyMemGuard"
            );
            return nullptr;
        }

        // initialize
        self->allocator = allocator;
        self->capacity = capacity;
        self->has_guard = false;
        return reinterpret_cast<PyObject*>(self);
    }

    /* Enter the context manager's block, freezing the allocator. */
    static PyObject* __enter__(PyMemGuard* self, PyObject* /* ignored */) {
        // check if the allocator is already frozen
        if (self->has_guard) {
            PyErr_SetString(
                PyExc_RuntimeError,
                "allocator is already frozen"
            );
            return nullptr;
        }

        // freeze the allocator
        try {
            new (&self->guard) MemGuard(self->allocator->reserve(self->capacity));

        // translate C++ exceptions into Python errors
        } catch (...) {
            util::throw_python();
            return nullptr;
        }

        // return new reference
        self->has_guard = true;
        return Py_NewRef(self);
    }

    /* Exit the context manager's block, unfreezing the allocator. */
    inline static PyObject* __exit__(PyMemGuard* self, PyObject* /* ignored */) {
        if (self->has_guard) {
            self->guard.~MemGuard();
            self->has_guard = false;
        }
        Py_RETURN_NONE;
    }

    /* Check if the allocator is currently frozen. */
    inline static PyObject* active(PyMemGuard* self, void* /* ignored */) {
        return PyBool_FromLong(self->has_guard);
    }

private:

    /* Unfreeze the allocator when the context manager is garbage collected, if it
    hasn't been unfrozen already. */
    inline static void __dealloc__(PyMemGuard* self) {
        if (self->has_guard) {
            self->guard.~MemGuard();
            self->has_guard = false;
        }
        Type.tp_free(self);
    }

    /* Docstrings for public attributes of PyMemGuard. */
    struct docs {

        static constexpr std::string_view PyMemGuard {R"doc(
A Python-compatible wrapper around a C++ MemGuard that allows it to be used as
a context manager.

Notes
-----
This class is only meant to be instantiated via the ``reserve()`` method of a
linked data structure.  It is directly equivalent to constructing a C++
RAII-style MemGuard within the guarded context.  The C++ guard is automatically
destroyed upon exiting the context.
)doc"
        };

        static constexpr std::string_view __enter__ {R"doc(
Enter the context manager's block, freezing the allocator.

Returns
-------
PyMemGuard
    The context manager itself, which may be aliased using the `as` keyword.
)doc"
        };

        static constexpr std::string_view __exit__ {R"doc(
Exit the context manager's block, unfreezing the allocator.
)doc"
        };

        static constexpr std::string_view active {R"doc(
Check if the allocator is currently frozen.

Returns
-------
bool
    True if the allocator is currently frozen, False otherwise.
)doc"
        };

    };

    /* Vtable containing Python @properties for the context manager. */
    inline static PyGetSetDef properties[] = {
        {"active", (getter) active, NULL, docs::active.data()},
        {NULL}  // sentinel
    };

    /* Vtable containing Python methods for the context manager. */
    inline static PyMethodDef methods[] = {
        {"__enter__", (PyCFunction) __enter__, METH_NOARGS, docs::__enter__.data()},
        {"__exit__", (PyCFunction) __exit__, METH_VARARGS, docs::__exit__.data()},
        {NULL}  // sentinel
    };

    /* Initialize a PyTypeObject to represent the guard from Python. */
    static PyTypeObject init_type() {
        PyTypeObject slots = {
            .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
            .tp_name = util::PyName<MemGuard>.data(),
            .tp_basicsize = sizeof(PyMemGuard),
            .tp_dealloc = (destructor) __dealloc__,
            .tp_flags = (
                Py_TPFLAGS_DEFAULT | Py_TPFLAGS_IMMUTABLETYPE |
                Py_TPFLAGS_DISALLOW_INSTANTIATION
            ),
            .tp_doc = docs::PyMemGuard.data(),
            .tp_methods = methods,
            .tp_getset = properties,
        };

        // register Python type
        if (PyType_Ready(&slots) < 0) {
            throw std::runtime_error("could not initialize PyMemGuard type");
        }
        return slots;
    }

public:

    /* Final Python type. */
    inline static PyTypeObject Type = init_type();

};


////////////////////
////    LIST    ////
////////////////////


/* A custom allocator that uses a dynamic array to manage memory for each node. */
template <typename NodeType>
class ListAllocator : public BaseAllocator<NodeType> {
public:
    using Node = NodeType;
    using MemGuard = linked::MemGuard<ListAllocator>;
    static constexpr size_t DEFAULT_CAPACITY = 8;  // minimum array size

private:
    friend MemGuard;
    using Base = BaseAllocator<Node>;

    Node* array;  // dynamic array of nodes
    std::pair<Node*, Node*> free_list;  // singly-linked list of open nodes

    /* When we allocate new nodes, we fill the dynamic array from left to right.
    If a node is removed from the middle of the array, then we add it to the free
    list.  The next time a node is allocated, we check the free list and reuse a
    node if possible.  Otherwise, we initialize a new node at the end of the
    occupied section.  If this causes the array to exceed its capacity, then we
    allocate a new array with twice the length and copy all nodes in the same order
    as they appear in the list. */

    /* Copy/move the nodes from this allocator into the given array. */
    template <bool move>
    std::pair<Node*, Node*> transfer(Node* other) const {
        Node* new_head = nullptr;
        Node* new_tail = nullptr;

        // copy over existing nodes
        Node* curr = this->head;
        size_t idx = 0;
        while (curr != nullptr) {
            // remember next node in original list
            Node* next = curr->next();

            // initialize new node in array
            Node* other_curr = &other[idx++];
            if constexpr (move) {
                new (other_curr) Node(std::move(*curr));
            } else {
                new (other_curr) Node(*curr);
            }

            // link to previous node in array
            if (curr == this->head) {
                new_head = other_curr;
            } else {
                Node::join(new_tail, other_curr);
            }
            new_tail = other_curr;
            curr = next;
        }

        // return head/tail pointers for new list
        return std::make_pair(new_head, new_tail);
    }

    /* Allocate a new array of a given size and transfer the contents of the list. */
    void resize(size_t new_capacity) {
        // allocate new array
        Node* new_array = Base::allocate_array(new_capacity);
        if constexpr (DEBUG) {
            std::cout << "    -> allocate: " << new_capacity << " nodes" << std::endl;
        }

        // move nodes into new array
        std::pair<Node*, Node*> bounds(transfer<true>(new_array));
        this->head = bounds.first;
        this->tail = bounds.second;

        // replace old array
        free(array);  // bypasses destructors
        if constexpr (DEBUG) {
            std::cout << "    -> deallocate: " << this->capacity << " nodes" << std::endl;
        }
        array = new_array;
        this->capacity = new_capacity;
        free_list.first = nullptr;
        free_list.second = nullptr;
    }

    /* Shrink a dynamic allocator if it is under the minimum load factor.  This is
    called automatically by recycle() as well as when a MemGuard falls out of scope,
    guaranteeing the load factor is never less than 25% of the list's capacity. */
    inline bool shrink() {
        if (this->capacity != DEFAULT_CAPACITY && this->occupied <= this->capacity / 4) {
            size_t size = util::next_power_of_two(2 * this->occupied);
            size = size < DEFAULT_CAPACITY ? DEFAULT_CAPACITY : size;
            resize(size);
            return true;
        }
        return false;
    }

public:

    /* Create an allocator with an optional fixed size. */
    ListAllocator(
        size_t capacity,
        bool fixed,
        PyObject* specialization
    ) : Base(capacity, fixed, specialization),
        array(Base::allocate_array(this->capacity)),
        free_list(std::make_pair(nullptr, nullptr))
    {}

    /* Copy constructor. */
    ListAllocator(const ListAllocator& other) :
        Base(other),
        array(Base::allocate_array(this->capacity)),
        free_list(std::make_pair(nullptr, nullptr))
    {
        if (this->occupied) {
            std::pair<Node*, Node*> bounds(other.transfer<false>(array));
            this->head = bounds.first;
            this->tail = bounds.second;
        }
    }

    /* Move constructor. */
    ListAllocator(ListAllocator&& other) noexcept :
        Base(std::move(other)),
        array(other.array),
        free_list(std::move(other.free_list))
    {
        other.array = nullptr;
        other.free_list.first = nullptr;
        other.free_list.second = nullptr;
    }

    // TODO: assignment operators should check for frozen state?

    /* Copy assignment operator. */
    ListAllocator& operator=(const ListAllocator& other) {
        if (this == &other) return *this;  // check for self-assignment

        // invoke parent operator
        Base::operator=(other);

        // destroy array
        free_list.first = nullptr;
        free_list.second = nullptr;
        if (array != nullptr) {
            free(array);
        }

        // copy array
        array = Base::allocate_array(this->capacity);
        if (this->occupied != 0) {
            std::pair<Node*, Node*> bounds(other.transfer<false>(array));
            this->head = bounds.first;
            this->tail = bounds.second;
        } else {
            this->head = nullptr;
            this->tail = nullptr;
        }
        return *this;
    }

    /* Move assignment operator. */
    ListAllocator& operator=(ListAllocator&& other) noexcept {
        if (this == &other) return *this;  // check for self-assignment

        // invoke parent
        Base::operator=(std::move(other));

        // destroy array
        if (array != nullptr) {
            free(array);
        }

        // transfer ownership
        array = other.array;
        free_list.first = other.free_list.first;
        free_list.second = other.free_list.second;

        // reset other
        other.array = nullptr;
        other.free_list.first = nullptr;
        other.free_list.second = nullptr;
        return *this;
    }

    /* Destroy an allocator and release its resources. */
    ~ListAllocator() noexcept {
        if (this->occupied) {
            Base::destroy_list();  // calls destructors
        }
        if (array != nullptr) {
            free(array);
            if constexpr (DEBUG) {
                std::cout << "    -> deallocate: " << this->capacity << " nodes";
                std::cout << std::endl;
            }
        }
    }

    /* Construct a new node for the list. */
    template <typename... Args>
    Node* create(Args&&... args) {
        // check free list
        if (free_list.first != nullptr) {
            Node* node = free_list.first;
            Node* temp = node->next();
            try {
                Base::init_node(node, std::forward<Args>(args)...);
            } catch (...) {
                node->next(temp);  // restore free list
                throw;  // propagate
            }
            free_list.first = temp;
            if (free_list.first == nullptr) free_list.second = nullptr;
            ++this->occupied;
            return node;
        }

        // check if we need to grow the array
        if (this->occupied == this->capacity) {
            if (this->frozen() || !this->dynamic()) {
                throw Base::cannot_grow();
            }
            resize(this->capacity * 2);
        }

        // append to end of allocated section
        Node* node = &array[this->occupied];
        Base::init_node(node, std::forward<Args>(args)...);
        ++this->occupied;
        return node;
    }

    /* Release a node from the list. */
    void recycle(Node* node) {
        // invoke parent method
        Base::recycle(node);

        // shrink array if necessary, else add to free list
        if (this->frozen() || !this->dynamic() || !this->shrink()) {
            if (free_list.first == nullptr) {
                free_list.first = node;
                free_list.second = node;
            } else {
                free_list.second->next(node);
                free_list.second = node;
            }
        }
    }

    /* Remove all elements from the list. */
    void clear() noexcept {
        // invoke parent method
        Base::clear();

        // reset free list and shrink to default capacity
        free_list.first = nullptr;
        free_list.second = nullptr;
        if (!this->frozen() && this->dynamic()) {
            this->capacity = DEFAULT_CAPACITY;
            free(array);
            array = Base::allocate_array(this->capacity);
        }
    }

    /* Resize the array to store a specific number of nodes. */
    MemGuard reserve(size_t new_size) {
        // invoke parent method
        Base::reserve(new_size);

        // if frozen, check new size against current capacity
        if (this->frozen() || !this->dynamic()) {
            if (new_size > this->capacity) {
                throw Base::cannot_grow();  // throw error
            } else {
                return MemGuard();  // return empty guard
            }
        }

        // otherwise, grow the array if necessary
        size_t new_cap = util::next_power_of_two(new_size);
        if (new_cap > DEFAULT_CAPACITY && new_cap > this->capacity) {
            resize(new_cap);
        }

        // freeze allocator until guard falls out of scope
        return MemGuard(this);
    }

    /* Attempt to resize the array based on an optional size. */
    MemGuard try_reserve(std::optional<size_t> new_size) {
        if (!new_size.has_value()) {
            return MemGuard();
        }
        return reserve(new_size.value());
    }

    /* Attempt to reserve memory to hold all the elements of a given container if it
    implements a `size()` method or is a Python object with a corresponding `__len__()`
    attribute.  Otherwise, produce an empty MemGuard. */
    template <typename Container>
    inline MemGuard try_reserve(Container& container) {
        std::optional<size_t> length = util::len(container);
        if (!length.has_value()) {
            return MemGuard();
        }
        return reserve(this->occupied + length.value());
    }

    /* Consolidate the nodes within the array, arranging them in the same order as
    they appear within the list. */
    inline void defragment() {
        Base::defragment();
        resize(this->capacity);  // in-place resize
    }

    /* Check whether the referenced node is being managed by this allocator. */
    inline bool owns(Node* node) const noexcept {
        return node >= array && node < array + this->capacity;  // pointer arithmetic
    }

};


//////////////////////////////
////    SET/DICTIONARY    ////
//////////////////////////////


/* A custom allocator that uses a hash table to manage memory for each node. */
template <typename NodeType>
class HashAllocator : public BaseAllocator<NodeType> {
public:
    using Node = NodeType;
    static constexpr size_t DEFAULT_CAPACITY = 16;  // minimum array size

private:
    using Base = BaseAllocator<NodeType>;
    using Value = typename Node::Value;
    static constexpr unsigned char DEFAULT_EXPONENT = 4;  // log2(DEFAULT_CAPACITY)
    static constexpr float MAX_LOAD_FACTOR = 0.5;  // grow if load factor exceeds threshold
    static constexpr float MIN_LOAD_FACTOR = 0.125;  // shrink if load factor drops below threshold
    static constexpr float MAX_TOMBSTONES = 0.125;  // clear tombstones if threshold is exceeded
    static constexpr size_t PRIMES[29] = {  // prime numbers to use for double hashing
        // HASH PRIME   // TABLE SIZE               // AI AUTOCOMPLETE
        13,             // 16 (2**4)                13
        23,             // 32 (2**5)                23
        47,             // 64 (2**6)                53
        97,             // 128 (2**7)               97
        181,            // 256 (2**8)               193
        359,            // 512 (2**9)               389
        719,            // 1024 (2**10)             769
        1439,           // 2048 (2**11)             1543
        2879,           // 4096 (2**12)             3079
        5737,           // 8192 (2**13)             6151
        11471,          // 16384 (2**14)            12289
        22943,          // 32768 (2**15)            24593
        45887,          // 65536 (2**16)            49157
        91753,          // 131072 (2**17)           98317
        183503,         // 262144 (2**18)           196613
        367007,         // 524288 (2**19)           393241
        734017,         // 1048576 (2**20)          786433
        1468079,        // 2097152 (2**21)          1572869
        2936023,        // 4194304 (2**22)          3145739
        5872033,        // 8388608 (2**23)          6291469
        11744063,       // 16777216 (2**24)         12582917
        23488103,       // 33554432 (2**25)         25165843
        46976221,       // 67108864 (2**26)         50331653
        93952427,       // 134217728 (2**27)        100663319
        187904861,      // 268435456 (2**28)        201326611
        375809639,      // 536870912 (2**29)        402653189
        751619321,      // 1073741824 (2**30)       805306457
        1503238603,     // 2147483648 (2**31)       1610612741
        3006477127,     // 4294967296 (2**32)       3221225473
        // NOTE: HASH PRIME is the first prime number larger than 0.7 * TABLE_SIZE
    };

    // TODO: flags should be renamed

    Node* array;  // dynamic array of nodes
    uint32_t* flags;  // bit array indicating whether a node is occupied or deleted
    size_t tombstones;  // number of nodes marked for deletion
    size_t prime;  // prime number used for double hashing
    unsigned char exponent;  // log2(capacity) - log2(DEFAULT_CAPACITY)

    /* Adjust the input to a constructor's `capacity` argument to account for double
    hashing, maximum load factor, and strict power of two table sizes. */
    inline static std::optional<size_t> adjust_size(std::optional<size_t> capacity) {
        if (!capacity.has_value()) {
            return std::nullopt;  // do not adjust
        }

        // check if capacity is a power of two
        size_t value = capacity.value();
        if (!util::is_power_of_two(value)) {
            std::ostringstream msg;
            msg << "capacity must be a power of two, got " << value;
            throw std::invalid_argument(msg.str());
        }

        // double capacity to ensure load factor is at most 0.5
        return std::make_optional(value * 2);
    }

    /* Allocate an auxiliary bit array indicating whether a given index is currently
    occupied or is marked as a tombstone. */
    inline static uint32_t* allocate_flags(const size_t capacity) {
        size_t num_flags = (capacity - 1) / 16;
        uint32_t* result = static_cast<uint32_t*>(calloc(num_flags, sizeof(uint32_t)));
        if (result == nullptr) {
            throw std::bad_alloc();
        }
        return result;
    }

    /* A fast modulo operator that exploits a power of two table capacity. */
    inline static size_t modulo(const size_t a, const size_t b) {
        return a & (b - 1);  // works for any power of two b
    }

    /* Copy/move the nodes from this allocator into the given array. */
    template <bool move>
    std::pair<Node*, Node*> transfer(Node* other, uint32_t* other_flags) const {
        Node* new_head = nullptr;
        Node* new_tail = nullptr;

        // move nodes into new array
        Node* curr = this->head;
        std::equal_to<Value> eq;
        while (curr != nullptr) {
            // rehash node
            size_t hash;
            if constexpr (NodeTraits<Node>::has_hash) {
                hash = curr->hash();
            } else {
                hash = std::hash<Value>()(curr->value());
            }

            // get index in new array
            size_t idx = modulo(hash, this->capacity);
            size_t step = (hash % prime) | 1;
            Node* lookup = &other[idx];
            size_t div = idx / 16;
            size_t mod = idx % 16;
            uint32_t bits = other_flags[div] >> mod;

            // handle collisions (NOTE: no need to check for tombstones)
            while (bits & 0b10) {  // node is constructed
                idx = modulo(idx + step, this->capacity);
                lookup = &other[idx];
                div = idx / 16;
                mod = idx % 16;
                bits = other_flags[div] >> mod;
            }

            // transfer node into new array
            Node* next = curr->next();
            if constexpr (move) {
                new (lookup) Node(std::move(*curr));
            } else {
                new (lookup) Node(*curr);
            }
            other_flags[div] |= 0b10 << mod;  // mark as constructed

            // update head/tail pointers
            if (curr == this->head) {
                new_head = lookup;
            } else {
                Node::join(new_tail, lookup);
            }
            new_tail = lookup;
            curr = next;
        }

        // return head/tail pointers for new list
        return std::make_pair(new_head, new_tail);
    }

    /* Allocate a new array of a given size and transfer the contents of the list. */
    void resize(const unsigned char new_exponent) {
        // allocate new array
        size_t new_capacity = 1 << (new_exponent + DEFAULT_EXPONENT);
        if constexpr (DEBUG) {
            std::cout << "    -> allocate: " << new_capacity << " nodes" << std::endl;
        }
        Node* new_array = Base::allocate_array(new_capacity);
        uint32_t* new_flags = allocate_flags(new_capacity);

        // update table parameters
        size_t old_capacity = this->capacity;
        this->capacity = new_capacity;
        tombstones = 0;
        prime = PRIMES[new_exponent];
        exponent = new_exponent;

        // move nodes into new array
        std::pair<Node*, Node*> bounds(transfer<true>(new_array, new_flags));
        this->head = bounds.first;
        this->tail = bounds.second;

        // replace arrays
        free(array);
        free(flags);
        array = new_array;
        flags = new_flags;
        if constexpr (DEBUG) {
            std::cout << "    -> deallocate: " << old_capacity << " nodes" << std::endl;
        }
    }

    /* Look up a value in the hash table by providing an explicit hash. */
    Node* _search(const size_t hash, const Value& value) const {
        // get index and step for double hashing
        size_t step = (hash % prime) | 1;
        size_t idx = modulo(hash, this->capacity);
        Node& lookup = array[idx];
        uint32_t& bits = flags[idx / 16];
        unsigned char flag = (bits >> (idx % 16)) & 0b11;

        // NOTE: first bit flag indicates whether bucket is occupied by a valid node.
        // Second indicates whether it is a tombstone.  Both are mutually exclusive.

        // handle collisions
        std::equal_to<Value> eq;
        while (flag > 0b00) {
            if (flag == 0b10 && eq(lookup->value(), value)) return &lookup;

            // advance to next slot
            idx = modulo(idx + step, this->capacity);
            lookup = array[idx];
            bits = flags[idx / 16];
            flag = (bits >> (idx % 16)) & 0b11;
        }

        // value not found
        return nullptr;
    }

public:

    /* Create an allocator with an optional fixed size. */
    HashAllocator(
        std::optional<size_t> capacity,
        bool frozen,
        PyObject* specialization
    ) : Base(adjust_size(capacity), frozen, specialization),
        array(Base::allocate_array(this->capacity)),
        flags(allocate_flags(this->capacity)),
        tombstones(0),
        prime(PRIMES[0]),
        exponent(0)
    {}

    /* Copy constructor. */
    HashAllocator(const HashAllocator& other) :
        Base(other),
        array(Base::allocate_array(this->capacity)),
        flags(allocate_flags(this->capacity)),
        tombstones(other.tombstones),
        prime(other.prime),
        exponent(other.exponent)
    {
        // copy over existing nodes in correct list order (head -> tail)
        if (this->occupied != 0) {
            std::pair<Node*, Node*> bounds(other.transfer<false>(array));
            this->head = bounds.first;
            this->tail = bounds.second;
        }
    }

    /* Move constructor. */
    HashAllocator(HashAllocator&& other) noexcept :
        Base(std::move(other)),
        array(other.array),
        flags(other.flags),
        tombstones(other.tombstones),
        prime(other.prime),
        exponent(other.exponent)
    {
        other.array = nullptr;
        other.flags = nullptr;
        other.tombstones = 0;
        other.prime = 0;
        other.exponent = 0;
    }

    /* Copy assignment operator. */
    HashAllocator& operator=(const HashAllocator& other) {
        // check for self-assignment
        if (this == &other) {
            return *this;
        }

        // invoke parent
        Base::operator=(other);

        // destroy array
        if (array != nullptr) {
            free(array);
            free(flags);
        }

        // copy array
        array = Base::allocate_array(this->capacity);
        flags = allocate_flags(this->capacity);
        if (this->occupied != 0) {
            std::pair<Node*, Node*> bounds(other.transfer<false>(array));
            this->head = bounds.first;
            this->tail = bounds.second;
        } else {
            this->head = nullptr;
            this->tail = nullptr;
        }
        return *this;
    }

    /* Move assignment operator. */
    HashAllocator& operator=(HashAllocator&& other) noexcept {
        // check for self-assignment
        if (this == &other) {
            return *this;
        }

        // invoke parent
        Base::operator=(std::move(other));

        // destroy array
        if (array != nullptr) {
            free(array);
            free(flags);
        }

        // transfer ownership
        array = other.array;
        flags = other.flags;
        tombstones = other.tombstones;
        prime = other.prime;
        exponent = other.exponent;

        // reset other
        other.array = nullptr;
        other.free_list.first = nullptr;
        other.free_list.second = nullptr;
        return *this;
    }

    /* Destroy an allocator and release its resources. */
    ~HashAllocator() noexcept {
        if (this->head != nullptr) {
            Base::destroy_list();  // calls destructors
        }
        if (array != nullptr) {
            free(array);
            free(flags);
            if constexpr (DEBUG) {
                std::cout << "    -> deallocate: " << this->capacity << " nodes" << std::endl;
            }
        }
    }

    /* Construct a new node for the list. */
    template <bool exist_ok = false, typename... Args>
    Node* create(Args&&... args) {
        // allocate into temporary node
        Base::init_node(this->temp(), std::forward<Args>(args)...);

        // check if we need to grow the array
        size_t total_load = this->occupied + tombstones;
        if (total_load >= this->capacity / 2) {
            if (this->frozen()) {
                if (this->occupied == this->capacity / 2) {
                    throw Base::cannot_grow();
                } else if (tombstones > this->capacity / 16) {
                    resize(exponent);  // clear tombstones
                }
                // NOTE: allow a small amount of hysteresis (load factor up to 0.5625)
                // to avoid pessimistic rehashing
            } else {
                resize(exponent + 1);  // grow array
            }
        }

        // search for node within hash table
        size_t hash = this->temp()->hash();
        size_t step = (hash % prime) | 1;  // double hashing
        size_t idx = modulo(hash, this->capacity);
        Node& lookup = array[idx];
        size_t div = idx / 16;
        size_t mod = idx % 16;
        uint32_t& bits = flags[div];
        unsigned char flag = (bits >> mod) & 0b11;

        // NOTE: first bit flag indicates whether bucket is occupied by a valid node.
        // Second indicates whether it is a tombstone.  Both are mutually exclusive.

        // handle collisions, replacing tombstones if they are encountered
        while (flag == 0b10) {
            if (lookup.eq(this->temp()->value())) {
                this->temp()->~Node();  // in-place destructor
                if constexpr (exist_ok) {
                    return &lookup;
                } else {
                    std::ostringstream msg;
                    msg << "duplicate key: " << util::repr(this->temp()->value());
                    throw std::invalid_argument(msg.str());
                }
            }

            // advance to next index
            idx = modulo(idx + step, this->capacity);
            lookup = array[idx];
            div = idx / 16;
            mod = idx % 16;
            bits = flags[div];
            flag = (bits >> mod) & 0b11;
        }

        // move temp into empty bucket/tombstone
        new (&lookup) Node(std::move(*this->temp()));
        bits |= (0b10 << mod);  // mark constructed flag
        if (flag == 0b01) {
            flag &= 0b10 << mod;  // clear tombstone flag
            --tombstones;
        }
        ++this->occupied;
        return &lookup;
    }

    /* Release a node from the list. */
    void recycle(Node* node) {
        if constexpr (DEBUG) {
            std::cout << "    -> recycle: " << util::repr(node->value()) << std::endl;
        }

        // look up node in hash table
        size_t hash = node->hash();
        size_t step = (hash % prime) | 1;  // double hashing
        size_t idx = modulo(hash, this->capacity);
        Node& lookup = array[idx];
        size_t div = idx / 16;
        size_t mod = idx % 16;
        uint32_t& bits = flags[div];
        unsigned char flag = (bits >> mod) & 0b11;

        // NOTE: first bit flag indicates whether bucket is occupied by a valid node.
        // Second indicates whether it is a tombstone.  Both are mutually exclusive.

        // handle collisions
        while (flag > 0b00) {
            if (flag == 0b10 && lookup.eq(node->value())) {
                // mark as tombstone
                lookup.~Node();  // in-place destructor
                bits &= (0b01 << mod);  // clear constructed flag
                bits |= (0b01 << mod);  // set tombstone flag
                ++tombstones;
                --this->occupied;

                // check whether to shrink array or clear out tombstones
                size_t threshold = this->capacity / 8;
                if (!this->frozen() &&
                    this->capacity != DEFAULT_CAPACITY &&
                    this->occupied < threshold  // load factor < 0.125
                ) {
                    resize(exponent - 1);
                } else if (tombstones > threshold) {  // tombstones > 0.125
                    resize(exponent);
                }
                return;
            }

            // advance to next index
            idx = modulo(idx + step, this->capacity);
            lookup = array[idx];
            div = idx / 16;
            mod = idx % 16;
            bits = flags[div];
            flag = (bits >> mod) & 0b11;
        }

        // node not found
        std::ostringstream msg;
        msg << "key not found: " << util::repr(node->value());
        throw std::invalid_argument(msg.str());
    }

    /* Remove all elements from the list. */
    void clear() noexcept {
        // destroy all nodes
        Base::destroy_list();

        // reset list parameters
        this->head = nullptr;
        this->tail = nullptr;
        this->occupied = 0;
        tombstones = 0;
        prime = PRIMES[0];
        exponent = 0;
        if (!this->frozen()) {
            this->capacity = DEFAULT_CAPACITY;
            free(array);
            free(flags);
            array = Base::allocate_array(this->capacity);
            flags = allocate_flags(this->capacity);
        }
    }

    /* Resize the array to store a specific number of nodes. */
    void reserve(size_t new_size) {
        // ensure new capacity is large enough to store all nodes
        if (new_size < this->occupied) {
            throw std::invalid_argument(
                "new capacity must not be smaller than current size"
            );
        }

        // do not shrink the array
        if (new_size <= this->capacity / 2) {
            return;
        }

        // frozen arrays cannot grow
        if (this->frozen()) {
            throw Base::cannot_grow();
        }

        // resize to the next power of two
        size_t rounded = util::next_power_of_two(new_size);
        unsigned char new_exponent = 0;
        while (rounded >>= 1) ++new_exponent;
        new_exponent += 1;  // account for max load factor (0.5)
        resize(new_exponent - DEFAULT_EXPONENT);
    }

    /* Rehash the nodes within the array, removing tombstones. */
    inline void defragment() {
        resize(exponent);  // in-place resize
    }

    /* Check whether the referenced node is being managed by this allocator. */
    inline bool owns(Node* node) const noexcept {
        return node >= array && node < array + this->capacity;  // pointer arithmetic
    }

    /* Search for a node by reusing a hash from another node. */
    template <typename N, std::enable_if_t<std::is_base_of_v<NodeTag, N>, int> = 0>
    inline Node* search(N* node) const {
        if constexpr (NodeTraits<N>::has_hash) {
            return _search(node->hash(), node->value());
        } else {
            size_t hash = std::hash<typename N::Value>{}(node->value());
            return _search(hash, node->value());
        }
    }

    /* Search for a node by its value directly. */
    inline Node* search(Value& key) const {
        return _search(std::hash<Value>{}(key), key);
    }

    /* Get the total amount of dynamic memory being managed by this allocator. */
    inline size_t nbytes() const {
        // NOTE: bit flags take 2 extra bits per node (4 nodes per byte)
        return Base::nbytes() + this->capacity / 4;
    }

};


}  // namespace linked
}  // namespace structs
}  // namespace bertrand


#endif  // BERTRAND_STRUCTS_LINKED_CORE_ALLOCATE_H

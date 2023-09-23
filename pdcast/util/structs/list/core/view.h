// include guard prevents multiple inclusion
#ifndef BERTRAND_STRUCTS_CORE_VIEW_H
#define BERTRAND_STRUCTS_CORE_VIEW_H

#include <cstddef>  // size_t
#include <optional>  // std::optional
#include <sstream>
#include <stdexcept>  // std::invalid_argument
#include <tuple>  // std::tuple
#include <Python.h>  // CPython API
#include "node.h"  // Hashed<>, Mapped<>
#include "allocate.h"  // Allocator
#include "index.h"  // IndexFactory
#include "iter.h"  // IteratorFactory
#include "slice.h"  // SliceFactory
#include "table.h"  // HashTable
#include "thread.h"  // BasicLock/DiagnosticLock
#include "util.h"  // CoupledIterator<>, Bidirectional<>, PyIterable


// TODO: importing core/sort.h causes a circular import error.  We need to separate
// the views from the public interface to avoid this.  We can do this by making
// the public C++ interface a separate class that contains a view as a member.  This
// can hold all the functors and other methods that are exposed to the user, and can
// separate the low-level view methods from the high-level interface.


// TODO: the views are relagated to node allocation/deallocation and linking/unlinking.
// In fact, it can absorb some of the complexity from Allocator and make NodeFactory
// obsolete.



/*

bertrand/
    structs/
        core/
            allocate.h
            iter.h
            node.h
            search.h
            thread.h
            util.h
        list/
            cython.h
            index.h
            list.h
            list.pxd
            list.pyx
            list.pyi
            slice.h
            sort.h
        set/
            cython.h
            relative.h
            set.h
            set.pxd
            set.pyx
            set.pyi
        dict/
            cython.h
            dict.h
            dict.pxd
            dict.pyx
            dict.pyi

*/


/*
    LinkedList (python)
        VariantList
            LinkedList (C++)
                ListView

    ListView:
        head
        tail
        size
        max_size
        specialization
        node()
        recycle()
        copy()
        clear()
        link()
        unlink()
        specialize()
        nbytes()

    LinkedList (C++):
        append()
        insert()
        extend()
        index()
        count()
        contains()
        remove()
        pop()
        clear()
        copy()
        sort()
        reverse()
        rotate()
        position()
        position.forward()
        position.backward()
        position().get()
        position().set()
        position().del()
        slice()
        slice.normalize()
        slice().get()
        slice().set()
        slice().del()
        iter()
        iter.reverse()
        iter.begin()
        iter.end()
        iter.rbegin()
        iter.rend()
        begin()
        end()
        rbegin()
        rend()
        lock()
        lock.context()
        lock.diagnostics()
        lock.count()
        lock.duration()
        lock.contention()
        lock.reset_diagnostics()
        specialize()
        specialization()
        size()
        max_size()
        nbytes()

*/


// TODO: Might be able to bring back inheritance between views.


//////////////////////
////    MIXINS    ////
//////////////////////


// TODO: delete this as soon as the C++ LinkedList implementation is debugged.


/* A mixin class that implements the full Python list interface. */
template <
    template <typename, template <typename> class> class ViewType,
    typename NodeType,
    template <typename> class Allocator
>
class ListInterface {
public:
    using View = ViewType<NodeType, Allocator>;

    /* Append an item to the end of a list. */
    void append(PyObject* item, const bool left = false) {
        using Node = typename View::Node;  // compiler error if declared outside method
        View& view = self();

        // allocate a new node
        Node* node = view.node(item);
        if (node == nullptr) {
            return;  // propagate error
        }

        // link to beginning/end of list
        if (left) {
            view.link(nullptr, node, view.head);
        } else {
            view.link(view.tail, node, nullptr);
        }
        if (PyErr_Occurred()) {
            view.recycle(node);  // clean up allocated node
        }
    }

    /* Insert an item into a list at the specified index. */
    template <typename T>
    void insert(T index, PyObject* item) {
        using Node = typename View::Node;
        View& view = self();

        // get iterator to index
        auto iter = view.position(index);
        if (!iter.has_value()) {
            return;  // propagate error
        }

        // allocate a new node
        Node* node = view.node(item);
        if (node == nullptr) {
            return;  // propagate error
        }

        // attempt to insert
        iter.value().insert(node);
        if (PyErr_Occurred()) {
            view.recycle(node);  // clean up staged node before propagating
        }
    }

    /* Extend a list by appending elements from the iterable. */
    void extend(PyObject* items, const bool left = false) {
        using Node = typename View::Node;
        View& view = self();

        // note original head/tail in case of error
        Node* original;
        if (left) {
            original = view.head;
        } else {
            original = view.tail;
        }

        // proceed with extend
        try {
            PyIterable sequence(items);
            for (PyObject* item : sequence) {
                // allocate a new node
                Node* node = view.node(item);
                if (node == nullptr) {
                    throw std::invalid_argument("could not allocate node");
                }

                // link to beginning/end of list
                if (left) {
                    view.link(nullptr, node, view.head);
                } else {
                    view.link(view.tail, node, nullptr);
                }
                if (PyErr_Occurred()) {
                    view.recycle(node);  // clean up allocated node
                    throw std::invalid_argument("could not link node");
                }
            }
        } catch (std::invalid_argument&) {
            // NOTE: this branch can also be triggered if the iterator raises an
            // exception during `iter()` or `next()`.
            if (left) {
                // if we added nodes to the left, then we just remove until we reach
                // the original head
                Node* curr = view.head;
                while (curr != original) {
                    Node* next = static_cast<Node*>(curr->next);
                    view.unlink(nullptr, curr, curr->next);
                    view.recycle(curr);
                    curr = next;
                }
            } else {
                // otherwise, we start from the original tail and remove until we reach
                // the end of the list
                Node* curr = static_cast<Node*>(original->next);
                while (curr != nullptr) {
                    Node* next = static_cast<Node*>(curr->next);
                    view.unlink(original, curr, next);
                    view.recycle(curr);
                    curr = next;
                }
            }
            return;  // propagate Python error
        }
    }

    /* Get the index of an item within a list. */
    template <typename T>
    std::optional<size_t> index(PyObject* item, T start = 0, T stop = -1) const {
        const View& view = self();

        // normalize start/stop indices
        std::optional<size_t> opt_start = view.position.normalize(start, true);
        std::optional<size_t> opt_stop = view.position.normalize(stop, true);
        if (!opt_start.has_value() || !opt_stop.has_value()) {
            return std::nullopt;  // propagate error
        }
        size_t norm_start = opt_start.value();
        size_t norm_stop = opt_stop.value();
        if (norm_start > norm_stop) {
            PyErr_Format(
                PyExc_ValueError,
                "start index cannot be greater than stop index"
            );
            return std::nullopt;
        }

        // if list is doubly-linked and stop is closer to tail than start is to head,
        // then we iterate backward from the tail
        if constexpr (view.doubly_linked) {
            if ((view.size - 1 - norm_stop) < norm_start) {
                // get backwards iterator to stop index
                auto iter = view.iter.reverse();
                size_t idx = view.size - 1;
                while (idx > norm_stop) {
                    ++iter;
                    --idx;
                }

                // search until we hit start index
                bool found = false;
                size_t last_observed;
                while (idx >= norm_start) {
                    int comp = PyObject_RichCompareBool((*iter)->value, item, Py_EQ);
                    if (comp == -1) {
                        return std::nullopt;  // propagate error
                    } else if (comp == 1) {
                        found = true;
                        last_observed = idx;
                    }
                    ++iter;
                    --idx;
                }
                if (found) {
                    return std::make_optional(last_observed);
                }
                PyErr_Format(PyExc_ValueError, "%R is not in list", item);
                return std::nullopt;
            }
        }

        // otherwise, we iterate forward from the head
        auto iter = view.iter();
        size_t idx = 0;
        while (idx < norm_start) {
            ++iter;
            ++idx;
        }

        // search until we hit item or stop index
        while (idx < norm_stop) {
            int comp = PyObject_RichCompareBool((*iter)->value, item, Py_EQ);
            if (comp == -1) {
                return std::nullopt;  // propagate error
            } else if (comp == 1) {
                return std::make_optional(idx);
            }
            ++iter;
            ++idx;
        }
        PyErr_Format(PyExc_ValueError, "%R is not in list", item);
        return std::nullopt;
    }

    /* Count the number of occurrences of an item within a list. */
    template <typename T>
    std::optional<size_t> count(PyObject* item, T start = 0, T stop = -1) const {
        const View& view = self();

        // normalize start/stop indices
        std::optional<size_t> opt_start = view.position.normalize(start, true);
        std::optional<size_t> opt_stop = view.position.normalize(stop, true);
        if (!opt_start.has_value() || !opt_stop.has_value()) {
            return std::nullopt;  // propagate error
        }
        size_t norm_start = opt_start.value();
        size_t norm_stop = opt_stop.value();
        if (norm_start > norm_stop) {
            PyErr_Format(
                PyExc_ValueError,
                "start index cannot be greater than stop index"
            );
            return std::nullopt;
        }

        // if list is doubly-linked and stop is closer to tail than start is to head,
        // then we iterate backward from the tail
        if constexpr (view.doubly_linked) {
            if ((view.size - 1 - norm_stop) < norm_start) {
                // get backwards iterator to stop index
                auto iter = view.iter.reverse();
                size_t idx = view.size - 1;
                while (idx > norm_stop) {
                    ++iter;
                    --idx;
                }

                // search until we hit start index
                size_t count = 0;
                while (idx >= norm_start) {
                    int comp = PyObject_RichCompareBool((*iter)->value, item, Py_EQ);
                    if (comp == -1) {
                        return std::nullopt;  // propagate error
                    } else if (comp == 1) {
                        ++count;
                    }
                    ++iter;
                    --idx;
                }
                return std::make_optional(count);
            }
        }

        // otherwise, we iterate forward from the head
        auto iter = view.iter();
        size_t idx = 0;
        while (idx < norm_start) {
            ++iter;
            ++idx;
        }

        // search until we hit item or stop index
        size_t count = 0;
        while (idx < norm_stop) {
            int comp = PyObject_RichCompareBool((*iter)->value, item, Py_EQ);
            if (comp == -1) {
                return std::nullopt;  // propagate error
            } else if (comp == 1) {
                ++count;
            }
            ++iter;
            ++idx;
        }
        return std::make_optional(count);
    }

    /* Check if the list contains a certain item. */
    std::optional<bool> contains(PyObject* item) const {
        const View& view = self();

        for (auto node : view) {
            // C API equivalent of the == operator
            int comp = PyObject_RichCompareBool(node->value, item, Py_EQ);
            if (comp == -1) {  // == comparison raised an exception
                return std::nullopt;
            } else if (comp == 1) {  // found a match
                return std::make_optional(true);
            }
        }

        // item not found
        return std::make_optional(false);
    }

    /* Remove the first occurrence of an item from a list. */
    void remove(PyObject* item) {
        View& view = self();

        // find item in list
        for (auto iter = view.iter(); iter != iter.end(); ++iter) {
            // C API equivalent of the == operator
            int comp = PyObject_RichCompareBool((*iter)->value, item, Py_EQ);
            if (comp == -1) {  // == comparison raised an exception
                return;  // propagate error
            } else if (comp == 1) {  // found a match
                view.recycle(iter.remove());
                return;
            }
        }

        // item not found
        PyErr_Format(PyExc_ValueError, "%R is not in list", item);        
    }

    /* Remove an item from a list and return its value. */
    template <typename T>
    PyObject* pop(T index) {
        using Node = typename View::Node;
        View& view = self();

        // get an iterator to the specified index
        auto iter = view.position(index);
        if (!iter.has_value()) {
            return nullptr;  // propagate error
        }

        // remove node at index and return its value
        Node* node = iter.value().remove();
        PyObject* result = node->value;
        Py_INCREF(result);  // ensure value is not garbage collected during recycle()
        view.recycle(node);
        return result;
    }

    // copy()/clear() implemented on view

    // const MergeSort sort;

    /* Reverse a list in-place. */
    void reverse() {
        using Node = typename View::Node;
        View& view = self();

        // save original `head` pointer
        Node* head = view.head;
        Node* curr = head;
        
        if constexpr (view.doubly_linked) {
            // swap all `next`/`prev` pointers
            while (curr != nullptr) {
                Node* next = static_cast<Node*>(curr->next);
                curr->next = static_cast<Node*>(curr->prev);
                curr->prev = next;
                curr = next;
            }
        } else {
            // swap all `next` pointers
            Node* prev = nullptr;
            while (curr != nullptr) {
                Node* next = static_cast<Node*>(curr->next);
                curr->next = prev;
                prev = curr;
                curr = next;
            }
        }

        // swap `head`/`tail` pointers
        view.head = view.tail;
        view.tail = head;
    }

    /* Rotate a list to the right by the specified number of steps. */
    void rotate(long long steps = 1) {
        using Node = typename View::Node;
        View& view = self();

        // normalize steps
        size_t norm_steps = llabs(steps) % view.size;
        if (norm_steps == 0) {
            return;  // rotated list is identical to original
        }

        // get index at which to split the list
        size_t index;
        size_t rotate_left = (steps < 0);
        if (rotate_left) {  // count from head
            index = norm_steps;
        } else {  // count from tail
            index = view.size - norm_steps;
        }

        Node* new_head;
        Node* new_tail;

        // identify new head and tail of rotated list
        if constexpr (view.doubly_linked) {
            // NOTE: if the list is doubly-linked, then we can iterate in either
            // direction to find the junction point.
            if (index > view.size / 2) {  // backward traversal
                new_head = view.tail;
                for (size_t i = view.size - 1; i > index; i--) {
                    new_head = static_cast<Node*>(new_head->prev);
                }
                new_tail = static_cast<Node*>(new_head->prev);

                // split list at junction and join previous head/tail
                Node::split(new_tail, new_head);
                Node::join(view.tail, view.head);

                // update head/tail pointers
                view.head = new_head;
                view.tail = new_tail;
                return;
            }
        }

        // forward traversal
        new_tail = view.head;
        for (size_t i = 1; i < index; i++) {
            new_tail = static_cast<Node*>(new_tail->next);
        }
        new_head = static_cast<Node*>(new_tail->next);

        // split at junction and join previous head/tail
        Node::split(new_tail, new_head);
        Node::join(view.tail, view.head);

        // update head/tail pointers
        view.head = new_head;
        view.tail = new_tail;
    }

private:

    inline View& self() {
        return static_cast<View&>(*this);
    }

    inline const View& self() const {
        return static_cast<const View&>(*this);
    }

};


////////////////////////
////    LISTVIEW    ////
////////////////////////


/* A pure C++ linked list data structure with customizable node types and allocation
strategies. */
template <
    typename NodeType = DoubleNode,
    template <typename> class AllocatorPolicy = DynamicAllocator
>
class ListView {
public:
    template <typename T>
    using AllocatorType = AllocatorPolicy<T>;  // in case we need to change it later

    using View = ListView<NodeType, AllocatorPolicy>;
    using Node = NodeType;
    using Allocator = AllocatorPolicy<Node>;
    using Iter = IteratorFactory<View>;
    inline static constexpr bool doubly_linked = has_prev<Node>::value;

    template <Direction dir>
    using Iterator = typename Iter::template Iterator<dir>;

    template <Direction dir>
    using IteratorPair = typename Iter::template IteratorPair<dir>;



    /* A custom allocator that uses a dynamic array to manage memory for each node. */
    class ArrayAllocator {        
    protected:
        friend ListView;
        Node* head;  // head of the list
        Node* tail;  // tail of the list
        size_t capacity;  // number of nodes in the array
        size_t occupied;  // number of nodes currently in use
        const bool frozen;  // indicates if the array is frozen at its current capacity
        Node* array;  // dynamic array of nodes
        std::pair<Node*, Node*> free_list;  // singly-linked list of open nodes

        /* When we allocate new nodes, we fill the dynamic array from left to right.
        If a node is removed from the middle of the array, then we add it to the free
        list.  The next time a node is allocated, we check the free list and reuse a
        node if possible.  Otherwise, we initialize a new node at the end of the
        occupied section.  If this causes the array to exceed its capacity, then we
        allocate a new array with twice the length and copy all nodes in the same order
        as they appear in the list. */

        /* Allocate a raw array of uninitialized nodes with the specified size. */
        inline static Node* allocate_array(size_t capacity) {
            Node* result = static_cast<Node*>(malloc(capacity * sizeof(Node)));
            if (result == nullptr) {
                throw std::bad_alloc();
            }
            return result;
        }

        /* Copy the contents of the list into a new array in list order. */
        void resize(size_t new_capacity) {
            Node* new_array = allocate_array(new_capacity);

            // copy over existing nodes in correct list order (head -> tail)
            Node* new_prev = nullptr;  // previous node in new array
            Node* curr = head;  // current node in old array
            for (size_t i = 0; i < occupied; i++) {
                // move node into new array
                Node* new_curr = &new_array[i];
                new (new_curr) Node(std::move(curr));  // placement new

                // link to previous node
                Node::join(new_prev, new_curr);
                new_prev = new_curr;
                curr = static_cast<Node*>(curr->next);
            }

            // replace old array
            free(array);  // does not call destructors
            array = new_array;
            capacity = new_capacity;
            free_list.first = nullptr;  // clear free list
            free_list.second = nullptr;

            // update head/tail pointers
            if (occupied == 0) {
                head = nullptr;
                tail = nullptr;
            } else {
                head = &array[0];
                tail = &array[occupied - 1];
            }
        }

    public:
        static const size_t DEFAULT_CAPACITY = 8;

        /* Create an allocator with an optional fixed size. */
        ArrayAllocator(size_t capacity = DEFAULT_CAPACITY, const bool frozen = false) :
            head(nullptr), tail(nullptr), capacity(capacity), occupied(0),
            frozen(frozen), array(allocate_array(capacity)),
            free_list(std::make_pair(nullptr, nullptr))
        {
            if constexpr (DEBUG) {
                printf("    -> preallocate: %zu nodes\n", capacity);
            }
        }

        /* Copy constructor. */
        ArrayAllocator(const ArrayAllocator& other) :
            head(nullptr), tail(nullptr), capacity(other.capacity),
            occupied(other.occupied), frozen(other.frozen),
            array(allocate_array(capacity)),
            free_list(std::make_pair(nullptr, nullptr))
        {
            if constexpr (DEBUG) {
                printf("    -> preallocate: %zu nodes\n", capacity);
            }

            // copy over existing nodes in correct list order (head -> tail)
            if (occupied != 0) {
                Node* new_prev = nullptr;  // previous node in new array
                Node* curr = other.head;  // current node in old array
                for (size_t i = 0; i < occupied; i++) {
                    // move node into new array
                    Node* new_curr = &array[i];
                    new (new_curr) Node(*curr);  // placement new

                    // link to previous node
                    Node::join(new_prev, new_curr);
                    new_prev = new_curr;
                    curr = static_cast<Node*>(curr->next);
                }

                // update head/tail pointers
                head = &array[0];
                tail = &array[occupied - 1];
            }
        }

        /* Move constructor. */
        ArrayAllocator(ArrayAllocator&& other) noexcept :
            head(other.head), tail(other.tail), capacity(other.capacity),
            occupied(other.occupied), frozen(other.frozen), array(other.array),
            free_list(other.free_list)
        {
            // reset other allocator
            other.head = nullptr;
            other.tail = nullptr;
            other.capacity = 0;
            other.occupied = 0;
            other.frozen = false;
            other.array = nullptr;
            other.free_list.first = nullptr;
            other.free_list.second = nullptr;
        }

        /* Release the allocated nodes. */
        ~ArrayAllocator() {
            if constexpr (DEBUG) {
                printf("    -> deallocate: %zu nodes\n", capacity);
            }
            free(array);  // does not call destructors
        }

        /* Construct a new node for the list. */
        template <typename... Args>
        Node* create(Args... args) {
            // check free list
            if (free_list.first != nullptr) {
                Node* node = free_list.first;
                Node* temp = static_cast<Node*>(node->next);
                try {
                    new (node) Node(std::forward<Args>(args)...);  // placement new
                } catch (...) {
                    node->next = temp;  // restore free list
                    throw;  // propagate
                }
                free_list.first = temp;
                if (temp == nullptr) {
                    free_list.second = nullptr;
                }
                if constexpr (DEBUG) {
                    printf("    -> create: %s\n", repr(node->value));
                }
                ++occupied;
                return node;
            }

            // check if we need to grow the array
            if (occupied == capacity) {
                if (frozen) {
                    std::ostringstream msg;
                    msg << "array cannot grow beyond size " << capacity;
                    throw std::runtime_error(msg.str());
                }
                resize(capacity * 2);
            }

            // append to end of allocated section
            Node* node = &array[occupied++];
            new (node) Node(std::forward<Args>(args)...);  // placement new
            if constexpr (DEBUG) {
                printf("    -> create: %s\n", repr(node->value));
            }
            return node;
        }

        /* Release a node from the list. */
        void recycle(Node* node) {
            if constexpr (DEBUG) {
                printf("    -> recycle: %s\n", repr(node->value));
            }

            // manually call destructor
            node->~Node();

            // check if we need to shrink the array
            if (!frozen && capacity != DEFAULT_CAPACITY && occupied == capacity / 4) {
                resize(capacity / 2);
            } else {
                if (free_list.first == nullptr) {
                    free_list.first = node;
                    free_list.second = node;
                } else {
                    free_list.second->next = node;
                    free_list.second = node;
                }
            }
            --occupied;
        }

        /* Consolidate the nodes within the array, arranging them in the same order as
        they appear within the list. */
        inline void consolidate() {
            resize(capacity);  // in-place resize
        }

        /* Check whether the referenced node is being managed by this allocator. */
        inline bool owns(Node* node) const {
            return node >= array && node < array + capacity;  // pointer arithmetic
        }

    };


    // TODO: view.size() should get the private view.occupied field.

    // TODO: nbytes() should do the calculation internally.

    // /* Get the overall memory usage of the allocator. */
    // inline size_t nbytes() const { return sizeof(*this) + capacity * sizeof(Node); }


    // TODO: head/tail should be tracked in the allocator itself?
    // -> this would require us to expose head() and tail() methods on the view, but
    // would avoid the need to provide a view to the allocator and remove a reference.


    // TODO: we could probably bake the specialization into the allocator.create()
    // method.


    Node* head;
    Node* tail;
    size_t size;
    Py_ssize_t max_size;
    PyObject* specialization;

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    /* Construct an empty ListView. */
    ListView(Py_ssize_t max_size = -1, PyObject* spec = nullptr) :
        head(nullptr), tail(nullptr), size(0), max_size(max_size),
        specialization(spec), iter(*this), allocator(max_size)
    {
        if (spec != nullptr) {
            Py_INCREF(spec);
        }
    }

    /* Construct a ListView from an input iterable. */
    ListView(
        PyObject* iterable,
        bool reverse = false,
        Py_ssize_t max_size = -1,
        PyObject* spec = nullptr
    ) : head(nullptr), tail(nullptr), size(0), max_size(max_size),
        specialization(spec), iter(*this), allocator(max_size)
    {
        if (spec != nullptr) {
            Py_INCREF(spec);
        }

        // unpack Python iterable into ListView
        try {
            PyIterable sequence(iterable);
            for (PyObject* item : sequence) {
                // allocate a new node
                Node* curr = node(item);
                if (curr == nullptr) {  // error during node initialization
                    if constexpr (DEBUG) {
                        // QoL - nothing has been allocated, so we don't actually free
                        printf("    -> free: %s\n", repr(item));
                    }
                    throw std::runtime_error("could not allocate node");
                }

                // link the node to the staged list
                if (reverse) {
                    link(nullptr, curr, head);
                } else {
                    link(tail, curr, nullptr);
                }
                if (PyErr_Occurred()) {
                    recycle(curr);  // clean up allocated node
                    throw std::runtime_error("could not link node");
                }
            }
        } catch (...) {
            this->~ListView();
            throw;  // propagate
        }
    }

    /* Move constructor: transfer ownership from one ListView to another. */
    ListView(ListView&& other) noexcept :
        head(other.head), tail(other.tail), size(other.size), max_size(other.max_size),
        specialization(other.specialization), iter(*this),
        allocator(std::move(other.allocator))
    {
        // reset other ListView
        other.head = nullptr;
        other.tail = nullptr;
        other.size = 0;
        other.max_size = 0;
        other.specialization = nullptr;
    }

    /* Move assignment: transfer ownership from one ListView to another. */
    ListView& operator=(ListView&& other) noexcept {
        // check for self-assignment
        if (this == &other) {
            return *this;
        }

        // free old nodes/specialization
        this->~ListView();

        // transfer ownership of nodes
        head = other.head;
        tail = other.tail;
        size = other.size;
        max_size = other.max_size;
        specialization = other.specialization;  // no need to INCREF/DECREF
        allocator = std::move(other.allocator);

        // reset other ListView
        other.head = nullptr;
        other.tail = nullptr;
        other.size = 0;
        other.max_size = 0;
        other.specialization = nullptr;

        return *this;
    }

    /* Copy constructors. These are disabled for the sake of efficiency, preventing us
    from unintentionally copying data.  Use the explicit copy() method instead. */
    ListView(const ListView& other) = delete;
    ListView& operator=(const ListView&) = delete;

    /* Destroy a ListView and free all its nodes. */
    ~ListView() noexcept {
        this->clear();
        if (specialization != nullptr) {
            Py_DECREF(specialization);
        };
    }

    /* Construct a new node for the list. */
    template <typename... Args>
    inline Node* node(Args... args) const {
        // variadic dispatch to Node::init()
        Node* result = allocator.create(args...);
        if (specialization != nullptr && result != nullptr) {
            if (!Node::typecheck(result, specialization)) {
                recycle(result);  // clean up allocated node
                return nullptr;  // propagate TypeError()
            }
        }
        return result;
    }

    /* Copy a node in the list. */
    inline Node* node(Node* node) const {
        return allocator.copy(node);
    }

    /* Release a node, returning it to the allocator. */
    inline void recycle(Node* node) const {
        allocator.recycle(node);
    }

    /* Copy a node in the list. */
    inline Node* copy(Node* node) const {
        return allocator.copy(node);
    }

    /* Make a shallow copy of the entire list. */
    std::optional<View> copy() const {
        View result(max_size, specialization);

        // copy every node into result
        for (Node* node : *this) {
            // allocate a new node
            Node* copied = result.copy(node);
            if (copied == nullptr) {
                return std::nullopt;  // propagate error
            }

            // link to end of copied list
            result.link(result.tail, copied, nullptr);
            if (PyErr_Occurred()) {
                return std::nullopt;  // propagate error
            }
        }

        return std::make_optional(std::move(result));
    }

    /* Remove all elements from a list. */
    void clear() noexcept {
        // store temporary reference to head
        Node* curr = head;

        // reset list parameters
        head = nullptr;
        tail = nullptr;
        size = 0;

        // recycle all nodes
        while (curr != nullptr) {
            // NOTE: since we're clearing the entire list, we can omit some of the
            // checks that would normally be necessary in `unlink()`.  This causes the
            // deleted nodes to retain dangling pointers to their deleted neighbors,
            // but that's okay because we're about to recycle them anyways.
            Node* next = static_cast<Node*>(curr->next);
            recycle(curr);
            curr = next;
        }
    }

    /* Link a node to its neighbors to form a linked list. */
    inline void link(Node* prev, Node* curr, Node* next) {
        // delegate to node-specific link() helper
        Node::link(prev, curr, next);

        // update list parameters
        size++;
        if (prev == nullptr) {
            head = curr;
        }
        if (next == nullptr) {
            tail = curr;
        }
    }

    /* Unlink a node from its neighbors. */
    inline void unlink(Node* prev, Node* curr, Node* next) {
        // delegate to node-specific unlink() helper
        Node::unlink(prev, curr, next);

        // update list parameters
        size--;
        if (prev == nullptr) {
            head = next;
        }
        if (next == nullptr) {
            tail = prev;
        }
    }

    /////////////////////////////
    ////    EXTRA METHODS    ////
    /////////////////////////////

    /* Enforce strict type checking for elements of this list. */
    void specialize(PyObject* spec) {
        // handle null assignment
        if (spec == nullptr) {
            if (specialization != nullptr) {
                Py_DECREF(specialization);  // remember to release old spec
                specialization = nullptr;
            }
            return;
        }

        // early return if new spec is same as old spec
        if (specialization != nullptr) {
            int comp = PyObject_RichCompareBool(spec, specialization, Py_EQ);
            if (comp == -1) {  // comparison raised an exception
                return;  // propagate error
            } else if (comp == 1) {  // spec is identical
                return;  // do nothing
            }
        }

        // check the contents of the list
        for (auto node : *this) {
            if (!Node::typecheck(node, spec)) {
                return;  // propagate TypeError()
            }
        }

        // replace old specialization
        Py_INCREF(spec);
        if (specialization != nullptr) {
            Py_DECREF(specialization);
        }
        specialization = spec;
    }

    /* Get the total memory consumed by the list (in bytes). */
    inline size_t nbytes() const {
        return allocator.nbytes() + sizeof(*this);
    }

    /////////////////////////////////
    ////    ITERATOR PROTOCOL    ////
    /////////////////////////////////

    /* An IteratorFactory functor that allows iteration over the list. */
    const Iter iter;

    /* Create an iterator to the start of the list. */
    inline auto begin() const {
        return iter.begin();
    }

    /* Create an iterator to the end of the list. */
    inline auto end() const {
        return iter.end();
    }

    /* Create a reverse iterator to the end of the list. */
    inline auto rbegin() const {
        return iter.rbegin();
    }

    /* Create a reverse iterator to the start of the list. */
    inline auto rend() const {
        return iter.rend();
    }

protected:
    mutable Allocator allocator;
};


///////////////////////
////    SETVIEW    ////
///////////////////////


template <typename NodeType, template <typename> class Allocator>
class SetView : public ListView<Hashed<NodeType>, Allocator> {
public:
    using Base = ListView<Hashed<NodeType>, Allocator>;
    using View = SetView<NodeType, Allocator>;
    using Node = Hashed<NodeType>;

    /* Construct an empty SetView. */
    SetView(Py_ssize_t max_size = -1, PyObject* spec = nullptr) :
        Base(max_size, spec), table()
    {}

    /* Construct a SetView from an input iterable. */
    SetView(
        PyObject* iterable,
        bool reverse = false,
        Py_ssize_t max_size = -1,
        PyObject* spec = nullptr
    ) : Base(max_size, spec), table()
    {
        // C API equivalent of iter(iterable)
        PyObject* iterator = PyObject_GetIter(iterable);
        if (iterator == nullptr) {  // TypeError()
            this->self_destruct();
            throw std::invalid_argument("Value is not iterable");
        }

        // unpack iterator into SetView
        PyObject* item;
        while (true) {
            // C API equivalent of next(iterator)
            item = PyIter_Next(iterator);
            if (item == nullptr) { // end of iterator or error
                if (PyErr_Occurred()) {
                    Py_DECREF(iterator);
                    this->self_destruct();
                    throw std::invalid_argument("could not get item from iterator");
                }
                break;
            }

            // allocate a new node and link it to the list
            stage(item, reverse);
            if (PyErr_Occurred()) {
                Py_DECREF(iterator);
                Py_DECREF(item);
                this->self_destruct();
                throw std::invalid_argument("could not stage item");
            }

            // advance to next item
            Py_DECREF(item);
        }

        // release reference on iterator
        Py_DECREF(iterator);
    }

    /* Move ownership from one SetView to another (move constructor). */
    SetView(SetView&& other) : Base(std::move(other)), table(std::move(other.table)) {}

    /* Move ownership from one SetView to another (move assignment). */
    SetView& operator=(SetView&& other) {
        // check for self-assignment
        if (this == &other) {
            return *this;
        }

        // call parent move assignment operator
        Base::operator=(std::move(other));

        // transfer ownership of hash table
        table = std::move(other.table);
        return *this;
    }

    /* Copy a node in the list. */
    inline Node* copy(Node* node) const {
        return Base::copy(node);
    }

    /* Make a shallow copy of the entire list. */
    std::optional<View> copy() const {
        try {
            View result(this->max_size, this->specialization);

            // copy nodes into new list
            Base::copy_into(result);
            if (PyErr_Occurred()) {
                return std::nullopt;
            }

            return std::make_optional(std::move(result));
        } catch (std::invalid_argument&) {
            return std::nullopt;
        }
    }

    /* Clear the list and reset the associated hash table. */
    inline void clear() {
        Base::clear();  // free all nodes
        table.reset();  // reset hash table
    }

    /* Link a node to its neighbors to form a linked list. */
    void link(Node* prev, Node* curr, Node* next) {
        // add node to hash table
        table.remember(curr);
        if (PyErr_Occurred()) {  // node is already present in table
            return;
        }

        // delegate to ListView
        Base::link(prev, curr, next);
    }

    /* Unlink a node from its neighbors. */
    void unlink(Node* prev, Node* curr, Node* next) {
        // remove node from hash table
        table.forget(curr);
        if (PyErr_Occurred()) {  // node is not present in table
            return;
        }

        // delegate to ListView
        Base::unlink(prev, curr, next);
    }

    /* Search for a node by its value. */
    template <typename T>
    inline Node* search(T* key) const {
        // NOTE: T can be either a PyObject or node pointer.  If a node is provided,
        // then its precomputed hash will be reused if available.  Otherwise, the value
        // will be passed through `PyObject_Hash()` before searching the table.
        return table.search(key);
    }

    /* A proxy class that allows for operations relative to a particular value
    within the set. */
    class RelativeProxy {
    public:
        using View = SetView<NodeType, Allocator>;
        using Node = View::Node;

        View* view;
        Node* sentinel;
        Py_ssize_t offset;

        // TODO: truncate could be handled at the proxy level.  It would just be
        // another constructor argument

        /* Construct a new RelativeProxy for the set. */
        RelativeProxy(View* view, Node* sentinel, Py_ssize_t offset) :
            view(view), sentinel(sentinel), offset(offset)
        {}

        // TODO: relative() could just return a RelativeProxy by value, which would
        // be deleted as soon as it falls out of scope.  This means we create a new
        // proxy every time a variant method is called, but we can reuse them in a
        // C++ context.

        /* Execute a function with the RelativeProxy as its first argument. */
        template <typename Func, typename... Args>
        auto execute(Func func, Args... args) {
            // function pointer must accept a RelativeProxy* as its first argument
            using ReturnType = decltype(func(std::declval<RelativeProxy*>(), args...));

            // call function with proxy
            if constexpr (std::is_void_v<ReturnType>) {
                func(this, args...);
            } else {
                return func(this, args...);
            }
        }


        // TODO: these could maybe just get the proxy's curr(), prev(), and next()
        // nodes, respectively.  We can then derive the other nodes from whichever one
        // is populated.  For example, if we've already found and cached the prev()
        // node, then curr() is just generated by getting prev()->next, and same with
        // next() and curr()->next.  If none of the nodes are cached, then we have to
        // iterate like we do now to find and cache them.  This means that in any
        // situation where we need to get all three nodes, we should always start with
        // prev().

        /* Return the node at the proxy's current location. */
        Node* walk(Py_ssize_t offset, bool truncate) {
            // check for no-op
            if (offset == 0) {
                return sentinel;
            }

            // TODO: introduce caching for the proxy's current position.  Probably
            // need to use std::optional<Node*> for these, since nullptr might be a
            // valid value.

            // if we're traversing forward from the sentinel, then the process is the
            // same for both singly- and doubly-linked lists
            if (offset > 0) {
                curr = sentinel;
                for (Py_ssize_t i = 0; i < offset; i++) {
                    if (curr == nullptr) {
                        if (truncate) {
                            return view->tail;  // truncate to end of list
                        } else {
                            return nullptr;  // index out of range
                        }
                    }
                    curr = static_cast<Node*>(curr->next);
                }
                return curr;
            }

            // if the list is doubly-linked, we can traverse backward just as easily
            if constexpr (has_prev<Node>::value) {
                curr = sentinel;
                for (Py_ssize_t i = 0; i > offset; i--) {
                    if (curr == nullptr) {
                        if (truncate) {
                            return view->head;  // truncate to beginning of list
                        } else {
                            return nullptr;  // index out of range
                        }
                    }
                    curr = static_cast<Node*>(curr->prev);
                }
                return curr;
            }

            // Otherwise, we have to iterate from the head of the list.  We do this
            // using a two-pointer approach where the `lookahead` pointer is offset
            // from the `curr` pointer by the specified number of steps.  When it
            // reaches the sentinel, then `curr` will be at the correct position.
            Node* lookahead = view->head;
            for (Py_ssize_t i = 0; i > offset; i--) {  // advance lookahead to offset
                if (lookahead == sentinel) {
                    if (truncate) {
                        return view->head;  // truncate to beginning of list
                    } else {
                        return nullptr;  // index out of range
                    }
                }
                lookahead = static_cast<Node*>(lookahead->next);
            }

            // advance both pointers until lookahead reaches sentinel
            curr = view->head;
            while (lookahead != sentinel) {
                curr = static_cast<Node*>(curr->next);
                lookahead = static_cast<Node*>(lookahead->next);
            }
            return curr;
        }

        /* Find the left and right bounds for an insertion. */
        std::pair<Node*, Node*> junction(Py_ssize_t offset, bool truncate) {
            // get the previous node for the insertion point
            prev = walk(offset - 1, truncate);

            // apply truncate rule
            if (prev == nullptr) {  // walked off end of list
                if (!truncate) {
                    return std::make_pair(nullptr, nullptr);  // error code
                }
                if (offset < 0) {
                    return std::make_pair(nullptr, view->head);  // beginning of list
                }
                return std::make_pair(view->tail, nullptr);  // end of list
            }

            // return the previous node and its successor
            curr = static_cast<Node*>(prev->next);
            return std::make_pair(prev, curr);
        }

        /* Find the left and right bounds for a removal. */
        std::tuple<Node*, Node*, Node*> neighbors(Py_ssize_t offset, bool truncate) {
            // NOTE: we can't reuse junction() here because we need access to the node
            // preceding the tail in the event that we walk off the end of the list and
            // truncate=true.
            curr = sentinel;

            // NOTE: this is trivial for doubly-linked lists
            if constexpr (has_prev<Node>::value) {
                if (offset > 0) {  // forward traversal
                    next = static_cast<Node*>(curr->next);
                    for (Py_ssize_t i = 0; i < offset; i++) {
                        if (next == nullptr) {
                            if (truncate) {
                                break;  // truncate to end of list
                            } else {
                                return std::make_tuple(nullptr, nullptr, nullptr);
                            }
                        }
                        curr = next;
                        next = static_cast<Node*>(curr->next);
                    }
                    prev = static_cast<Node*>(curr->prev);
                } else {  // backward traversal
                    prev = static_cast<Node*>(curr->prev);
                    for (Py_ssize_t i = 0; i > offset; i--) {
                        if (prev == nullptr) {
                            if (truncate) {
                                break;  // truncate to beginning of list
                            } else {
                                return std::make_tuple(nullptr, nullptr, nullptr);
                            }
                        }
                        curr = prev;
                        prev = static_cast<Node*>(curr->prev);
                    }
                    next = static_cast<Node*>(curr->next);
                }
                return std::make_tuple(prev, curr, next);
            }

            // NOTE: It gets significantly more complicated if the list is singly-linked.
            // In this case, we can only optimize the forward traversal branch if we're
            // advancing at least one node and the current node is not the tail of the
            // list.
            if (truncate && offset > 0 && curr == view->tail) {
                offset = 0;  // skip forward iteration branch
            }

            // forward iteration (efficient)
            if (offset > 0) {
                prev = nullptr;
                next = static_cast<Node*>(curr->next);
                for (Py_ssize_t i = 0; i < offset; i++) {
                    if (next == nullptr) {  // walked off end of list
                        if (truncate) {
                            break;
                        } else {
                            return std::make_tuple(nullptr, nullptr, nullptr);
                        }
                    }
                    if (prev == nullptr) {
                        prev = curr;
                    }
                    curr = next;
                    next = static_cast<Node*>(curr->next);
                }
                return std::make_tuple(prev, curr, next);
            }

            // backward iteration (inefficient)
            Node* lookahead = view->head;
            for (size_t i = 0; i > offset; i--) {  // advance lookahead to offset
                if (lookahead == curr) {
                    if (truncate) {  // truncate to beginning of list
                        next = static_cast<Node*>(view->head->next);
                        return std::make_tuple(nullptr, view->head, next);
                    } else {  // index out of range
                        return std::make_tuple(nullptr, nullptr, nullptr);
                    }
                }
                lookahead = static_cast<Node*>(lookahead->next);
            }

            // advance both pointers until lookahead reaches sentinel
            prev = nullptr;
            Node* temp = view->head;
            while (lookahead != curr) {
                prev = temp;
                temp = static_cast<Node*>(temp->next);
                lookahead = static_cast<Node*>(lookahead->next);
            }
            next = static_cast<Node*>(temp->next);
            return std::make_tuple(prev, temp, next);
        }

    private:
        // cache the proxy's current position in the set
        Node* prev;
        Node* curr;
        Node* next;
    };

    /* Generate a proxy for a set that allows operations relative to a particular
    sentinel value. */
    template <typename T, typename Func, typename... Args>
    auto relative(T* sentinel, Py_ssize_t offset, Func func, Args... args) {
        // function pointer must accept a RelativeProxy* as its first argument
        using ReturnType = decltype(func(std::declval<RelativeProxy*>(), args...));

        // search for sentinel
        Node* sentinel_node = search(sentinel);
        if (sentinel_node == nullptr) {  // sentinel not found
            PyErr_Format(PyExc_KeyError, "%R is not contained in the set", sentinel);
            return nullptr;  // propagate
        }

        // stack-allocate a temporary proxy for the set (memory-safe)
        RelativeProxy proxy(this, sentinel_node, offset);

        // call function with proxy
        if constexpr (std::is_void_v<ReturnType>) {
            func(&proxy, args...);
        } else {
            return func(&proxy, args...);
        }
    }

    /* Clear all tombstones from the hash table. */
    inline void clear_tombstones() {
        table.clear_tombstones();
    }

    /* Get the total amount of memory consumed by the set (in bytes).  */
    inline size_t nbytes() const {
        return Base::nbytes() + table.nbytes();
    }

protected:

    /* Allocate a new node for the item and add it to the set, discarding it in
    the event of an error. */
    inline void stage(PyObject* item, bool reverse) {
        // allocate a new node
        Node* curr = this->node(item);
        if (curr == nullptr) {  // error during node initialization
            if constexpr (DEBUG) {
                // QoL - nothing has been allocated, so we don't actually free
                printf("    -> free: %s\n", repr(item));
            }
            return;
        }

        // search for node in hash table
        Node* existing = search(curr);
        if (existing != nullptr) {  // item already exists
            if constexpr (has_mapped<Node>::value) {
                // update mapped value
                Py_DECREF(existing->mapped);
                Py_INCREF(curr->mapped);
                existing->mapped = curr->mapped;
            }
            this->recycle(curr);
            return;
        }

        // link the node to the staged list
        if (reverse) {
            link(nullptr, curr, this->head);
        } else {
            link(this->tail, curr, nullptr);
        }
        if (PyErr_Occurred()) {
            this->recycle(curr);  // clean up allocated node
            return;
        }
    }

private:
    HashTable<Node> table;  // stack allocated
};


////////////////////////
////    DICTVIEW    ////
////////////////////////


// TODO: we can add a separate specialization for an LRU cache that is always
// implemented as a DictView<DoubleNode, PreAllocator>.  This would allow us to
// use a pure C++ implementation for the LRU cache, which isn't even wrapped
// in a Cython class.  We could export this as a Cython alias for use in type
// inference.  Maybe the instance factories have one of these as a C-level
// member.



// TODO: If we inherit from SetView<Mapped<NodeType>, Allocator>, then we need
// to remove the hashing-related code from Mapped<>.


template <typename NodeType, template <typename> class Allocator>
class DictView {
public:
    using Node = Mapped<NodeType>;
    Node* head;
    Node* tail;
    size_t size;

    /* Disabled copy/move constructors.  These are dangerous because we're
    manually managing memory for each node. */
    DictView(const DictView& other) = delete;       // copy constructor
    DictView& operator=(const DictView&) = delete;  // copy assignment
    DictView(DictView&&) = delete;                  // move constructor
    DictView& operator=(DictView&&) = delete;       // move assignment

    /* Construct an empty DictView. */
    DictView(Py_ssize_t max_size = -1) :
        head(nullptr), tail(nullptr), size(0), specialization(nullptr),
        table(), allocator(max_size) {}

    /* Construct a DictView from an input iterable. */
    DictView(
        PyObject* iterable,
        bool reverse = false,
        PyObject* spec = nullptr,
        Py_ssize_t max_size = -1
    ) : head(nullptr), tail(nullptr), size(0), specialization(nullptr),
        table(), allocator(max_size)
    {
        // C API equivalent of iter(iterable)
        PyObject* iterator = PyObject_GetIter(iterable);
        if (iterator == nullptr) {
            throw std::invalid_argument("Value is not iterable");
        }

        // hold reference to specialization, if given
        if (spec != nullptr) {
            Py_INCREF(spec);
        }

        // unpack iterator into DictView
        PyObject* item;
        while (true) {
            // C API equivalent of next(iterator)
            item = PyIter_Next(iterator);
            if (item == nullptr) { // end of iterator or error
                if (PyErr_Occurred()) {
                    Py_DECREF(iterator);
                    self_destruct();
                    throw std::runtime_error("could not get item from iterator");
                }
                break;  // end of iterator
            }

            // allocate a new node and link it to the list
            stage(item, reverse);
            if (PyErr_Occurred()) {
                Py_DECREF(iterator);
                Py_DECREF(item);
                self_destruct();
                throw std::runtime_error("could not stage item");
            }

            // advance to next item
            Py_DECREF(item);
        }

        // release reference on iterator
        Py_DECREF(iterator);
    };

    /* Destroy a DictView and free all its resources. */
    ~DictView() {
        self_destruct();
    }

    /* Construct a new node for the list. */
    template <typename... Args>
    inline Node* node(PyObject* value, Args... args) const {
        // variadic dispatch to Node::init()
        Node* result = allocator.create(value, args...);
        if (specialization != nullptr && result != nullptr) {
            if (!Node::typecheck(result, specialization)) {
                recycle(result);  // clean up allocated node
                return nullptr;  // propagate TypeError()
            }
        }

        return result;
    }

    /* Release a node, returning it to the allocator. */
    inline void recycle(Node* node) const {
        allocator.recycle(node);
    }

    /* Copy a single node in the list. */
    inline Node* copy(Node* node) const {
        return allocator.copy(node);
    }

    /* Make a shallow copy of the list. */
    DictView<NodeType, Allocator>* copy() const {
        DictView<NodeType, Allocator>* copied = new DictView<NodeType, Allocator>();
        Node* old_node = head;
        Node* new_node = nullptr;
        Node* new_prev = nullptr;

        // copy each node in list
        while (old_node != nullptr) {
            new_node = copy(old_node);  // copy node
            if (new_node == nullptr) {  // error during copy()
                delete copied;  // discard staged list
                return nullptr;
            }

            // link to tail of copied list
            copied->link(new_prev, new_node, nullptr);
            if (PyErr_Occurred()) {  // error during link()
                delete copied;  // discard staged list
                return nullptr;
            }

            // advance to next node
            new_prev = new_node;
            old_node = static_cast<Node*>(old_node->next);
        }

        // return copied view
        return copied;
    }

    /* Clear the list and reset the associated hash table. */
    inline void clear() {
        purge_list();  // free all nodes
        table.reset();  // reset hash table to initial size
    }

    /* Link a node to its neighbors to form a linked list. */
    void link(Node* prev, Node* curr, Node* next) {
        // add node to hash table
        table.remember(curr);
        if (PyErr_Occurred()) {
            return;
        }

        // delegate to node-specific link() helper
        Node::link(prev, curr, next);

        // update list parameters
        size++;
        if (prev == nullptr) {
            head = curr;
        }
        if (next == nullptr) {
            tail = curr;
        }
    }

    /* Unlink a node from its neighbors. */
    void unlink(Node* prev, Node* curr, Node* next) {
        // remove node from hash table
        table.forget(curr);
        if (PyErr_Occurred()) {
            return;
        }

        // delegate to node-specific unlink() helper
        Node::unlink(prev, curr, next);

        // update list parameters
        size--;
        if (prev == nullptr) {
            head = next;
        }
        if (next == nullptr) {
            tail = prev;
        }
    }

    /* Enforce strict type checking for elements of this list. */
    void specialize(PyObject* spec) {
        // check the contents of the list
        if (spec != nullptr) {
            Node* curr = head;
            for (size_t i = 0; i < size; i++) {
                if (!Node::typecheck(curr, spec)) {
                    return;  // propagate TypeError()
                }
                curr = static_cast<Node*>(curr->next);
            }
            Py_INCREF(spec);
        }

        // replace old specialization
        if (specialization != nullptr) {
            Py_DECREF(specialization);
        }
        specialization = spec;
    }

    /* Get the type specialization for elements of this list. */
    inline PyObject* get_specialization() const {
        if (specialization != nullptr) {
            Py_INCREF(specialization);
        }
        return specialization;  // return a new reference or NULL
    }

    /* Search for a node by its value. */
    inline Node* search(PyObject* value) const {
        return table.search(value);
    }

    /* Search for a node by its value. */
    inline Node* search(Node* value) const {
        return table.search(value);
    }

    /* Search for a node and move it to the front of the list at the same time. */
    inline Node* lru_search(PyObject* value) {
        // move node to head of list
        Node* curr = table.search(value);
        if (curr != nullptr && curr != head) {
            if (curr == tail) {
                tail = static_cast<Node*>(curr->prev);
            }
            Node* prev = static_cast<Node*>(curr->prev);
            Node* next = static_cast<Node*>(curr->next);
            Node::unlink(prev, curr, next);
            Node::link(nullptr, curr, head);
            head = curr;
        }

        return curr;
    }

    /* Clear all tombstones from the hash table. */
    inline void clear_tombstones() {
        table.clear_tombstones();
    }

    /* Get the total amount of memory consumed by the dictionary (in bytes). */
    inline size_t nbytes() const {
        return allocator.nbytes() + table.nbytes() + sizeof(*this);
    }

private:
    PyObject* specialization;  // specialized type for elements of this list
    mutable Allocator<Node>allocator;  // stack allocated
    HashTable<Node> table;  // stack allocated

    /* Allocate a new node for the item and append it to the list, discarding
    it in the event of an error. */
    void stage(PyObject* item, bool reverse) {
        // allocate a new node
        Node* curr = node(item);
        if (PyErr_Occurred()) {
            // QoL - nothing has been allocated, so we don't actually free anything
            if constexpr (DEBUG) {
                printf("    -> free: %s\n", repr(item));
            }
            return;
        }

        // link the node to the staged list
        if (reverse) {
            link(nullptr, curr, head);
        } else {
            link(tail, curr, nullptr);
        }
        if (PyErr_Occurred()) {  // node already exists
            recycle(curr);
            return;
        }
    }

    /* Clear all nodes in the list. */
    void purge_list() {
        // NOTE: this does not reset the hash table, and is therefore unsafe.
        // It should only be used to destroy a DictView or clear its contents.
        Node* curr = head;  // store temporary reference to head

        // reset list parameters
        head = nullptr;
        tail = nullptr;
        size = 0;

        // recycle all nodes
        while (curr != nullptr) {
            Node* next = static_cast<Node*>(curr->next);
            recycle(curr);
            curr = next;
        }
    }

    /* Release the resources being managed by the DictView. */
    inline void self_destruct() {
        // NOTE: allocator and table are stack allocated, so they don't need to
        // be freed here.  Their destructors will be called automatically when
        // the DictView is destroyed.
        purge_list();
        if (specialization != nullptr) {
            Py_DECREF(specialization);
        }
    }

};


///////////////////////////
////    VIEW TRAITS    ////
///////////////////////////


/* A trait that detects whether the templated view is set-like (i.e. has a
search() method). */
template <typename View>
struct is_setlike {
private:
    // Helper template to detect whether View has a search() method
    template <typename T>
    static auto test(T* t) -> decltype(t->search(nullptr), std::true_type());

    // Overload for when View does not have a search() method
    template <typename T>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<View>(nullptr))::value;
};


///////////////////////////////
////    VIEW DECORATORS    ////
///////////////////////////////


// TODO: Sorted<> becomes a decorator for a view, not a node.  It automatically
// converts a view of any type into a sorted view, which stores its nodes in a
// skip list.  This makes the sortedness immutable, and blocks operations that
// would unsort the list.  Every node in the list is decorated with a key value
// that is supplied by the user.  This key is provided in the constructor, and
// is cached on the node itself under a universal `key` attribute.  The SortKey
// template parameter defines what is stored in this key, and under what
// circumstances it is modified.

// using MFUCache = typename Sorted<DictView<DoubleNode>, Frequency, Descending>;

// This would create a doubly-linked skip list where each node maintains a
// value, mapped value, frequency count, hash, and prev/next pointers.  The
// view itself would maintain a hash map for fast lookups.  If the default
// SortKey is used, then we can also make the the index() method run in log(n)
// by exploiting the skip list.  These can be specific overloads in the methods
// themselves.

// This decorator can be extended to any of the existing views.



// template <
//     template <typename> class ViewType,
//     typename NodeType,
//     typename SortKey = Value,
//     typename SortOrder = Ascending
// >
// class Sorted : public ViewType<NodeType> {
// public:
//     /* A node decorator that maintains vectors of next and prev pointers for use in
//     sorted, skip list-based data structures. */
//     struct Node : public ViewType::Node {
//         std::vector<Node*> skip_next;
//         std::vector<Node*> skip_prev;

//         /* Initialize a newly-allocated node. */
//         inline static Node* init(Node* node, PyObject* value) {
//             node = static_cast<Node*>(NodeType::init(node, value));
//             if (node == nullptr) {  // Error during decorated init()
//                 return nullptr;  // propagate
//             }

//             // NOTE: skip_next and skip_prev are stack-allocated, so there's no
//             // need to initialize them here.

//             // return initialized node
//             return node;
//         }

//         /* Initialize a copied node. */
//         inline static Node* init_copy(Node* new_node, Node* old_node) {
//             // delegate to templated init_copy() method
//             new_node = static_cast<Node*>(NodeType::init_copy(new_node, old_node));
//             if (new_node == nullptr) {  // Error during templated init_copy()
//                 return nullptr;  // propagate
//             }

//             // copy skip pointers
//             new_node->skip_next = old_node->skip_next;
//             new_node->skip_prev = old_node->skip_prev;
//             return new_node;
//         }

//         /* Tear down a node before freeing it. */
//         inline static void teardown(Node* node) {
//             node->skip_next.clear();  // reset skip pointers
//             node->skip_prev.clear();
//             NodeType::teardown(node);
//         }

//         // TODO: override link() and unlink() to update skip pointers and maintain
//         // sorted order
//     }
// }


// ////////////////////////
// ////    POLICIES    ////
// ////////////////////////


// // TODO: Value and Frequency should be decorators for nodes to give them full
// // type information.  They can even wrap


// /* A SortKey that stores a reference to a node's value in its key. */
// struct Value {
//     /* Decorate a freshly-initialized node. */
//     template <typename Node>
//     inline static void decorate(Node* node) {
//         node->key = node->value;
//     }

//     /* Clear a node's sort key. */
//     template <typename Node>
//     inline static void undecorate(Node* node) {
//         node->key = nullptr;
//     }
// };


// /* A SortKey that stores a frequency counter as a node's key. */
// struct Frequency {
//     /* Initialize a node's sort key. */
//     template <typename Node>
//     inline static void decorate(Node* node) {
//         node->key = 0;
//     }

//     /* Clear a node's sort key */

// };


// /* A Sorted<> policy that sorts nodes in ascending order based on key. */
// template <typename SortValue>
// struct Ascending {
//     /* Check whether two keys are in sorted order relative to one another. */
//     template <typename KeyValue>
//     inline static bool compare(KeyValue left, KeyValue right) {
//         return left <= right;
//     }

//     /* A specialization for compare to use with Python objects as keys. */
//     template <>
//     inline static bool compare(PyObject* left, PyObject* right) {
//         return PyObject_RichCompareBool(left, right, Py_LE);  // -1 signals error
//     }
// };


// /* A specialized version of Ascending that compares PyObject* references. */
// template <>
// struct Ascending<Value> {
    
// };


// /* A Sorted<> policy that sorts nodes in descending order based on key. */
// struct Descending {
//     /* Check whether two keys are in sorted order relative to one another. */
//     template <typename KeyValue>
//     inline static int compare(KeyValue left, KeyValue right) {
//         return left >= right;
//     }

//     /* A specialization for compare() to use with Python objects as keys. */
//     template <>
//     inline static int compare(PyObject* left, PyObject* right) {
//         return PyObject_RichCompareBool(left, right, Py_GE);  // -1 signals error
//     }
// };


#endif // BERTRAND_STRUCTS_CORE_VIEW_H include guard
